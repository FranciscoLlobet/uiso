/*
 * network.c
 *
 *  Created on: 8 abr 2023
 *      Author: Francisco
 */


#include "uiso.h"

#include "mbedtls/error.h"

#include "wifi_service.h"

//#include "simplelink.h"
#include "sl_sleeptimer.h"

#define NETWORK_MONITOR_TASK    (UBaseType_t)( uiso_task_runtime_services )

#define SIMPLELINK_MAX_SEND_MTU 	1472

/* Internal socket management */
struct socket_management_s
{
	int16_t sd; /* Socket Descriptor */
	int16_t pad;
	uint32_t deadline_s;
	SemaphoreHandle_t queue_handle;
	SemaphoreHandle_t signal_semaphore;
};


struct uiso_sockets_s
{
    int32_t sd; /* Socket Descriptor */
    int32_t protocol;

    /* Wait deadlines */
    uint32_t rx_wait_deadline_s;
    uint32_t tx_wait_deadline_s;

    /* RX-TX Wait */
    SemaphoreHandle_t rx_signal;
    SemaphoreHandle_t tx_signal;

    /* local address */
    SlSockAddrIn_t local;
    SlSocklen_t local_len;

    /* Peer address */
    SlSockAddrIn_t peer;
    SlSocklen_t peer_len;

    mbedtls_ssl_context * ssl_context;

    uint32_t last_send_time;
    uint32_t last_recv_time;

    void * app_ctx;
    uint32_t app_param;
};

static struct uiso_sockets_s system_sockets[wifi_service_max]; /* new system sockets */

static struct socket_management_s rx_sockets[wifi_service_max];
static struct socket_management_s tx_sockets[wifi_service_max];
static SemaphoreHandle_t network_monitor_mutex = NULL;

static SemaphoreHandle_t network_mutex = NULL;

static void select_task(void *param);
static void initialize_socket_management(void);
static int register_deadline(struct socket_management_s * ctx, int sd, uint32_t timeout_s);
static void wifi_service_register_rx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);
static void wifi_service_register_tx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);

TaskHandle_t network_monitor_task_handle = NULL;



/* mbedTLS Support */
static int _network_send(uiso_network_ctx_t ctx, unsigned char *buf, size_t len);
static int _network_recv(uiso_network_ctx_t ctx, unsigned char *buf, size_t len);
static int _network_close(uiso_network_ctx_t ctx);
static int _set_mbedtls_bio(uiso_network_ctx_t ctx);

static void initialize_socket_management(void)
{
	memset(rx_sockets, 0, sizeof(rx_sockets));
	memset(tx_sockets, 0, sizeof(tx_sockets));

	for (size_t i = 0; i < (size_t) wifi_service_max; i++)
	{
		rx_sockets[i].sd = -1;
		rx_sockets[i].deadline_s = 0;
		rx_sockets[i].queue_handle = xSemaphoreCreateMutex();
		rx_sockets[i].signal_semaphore = xSemaphoreCreateBinary();
	}
	for (size_t i = 0; i < (size_t) wifi_service_max; i++)
	{
		tx_sockets[i].sd = -1;
		tx_sockets[i].deadline_s = 0;
		tx_sockets[i].queue_handle = xSemaphoreCreateMutex();
		tx_sockets[i].signal_semaphore = xSemaphoreCreateBinary();
	}
}

static int register_deadline(struct socket_management_s * ctx, int sd, uint32_t timeout_s)
{
	int ret = 0;

	(void)xSemaphoreTake(network_monitor_mutex, portMAX_DELAY);

	if(NULL != ctx)
	{
		ctx->sd = sd;
		ctx->deadline_s = (uint32_t) sl_sleeptimer_get_time() + timeout_s;
	}
	else
	{
		ret = -1;
	}

	(void)xSemaphoreGive(network_monitor_mutex);
	return ret;
}


void wifi_service_register_rx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	(void)register_deadline(&rx_sockets[(size_t)id], sd, timeout_s);
}

void wifi_service_register_tx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	(void)register_deadline(&tx_sockets[(size_t)id], sd, timeout_s);
}

int create_network_mediator(void)
{
	int ret = 0;

	if(pdFALSE == xTaskCreate(select_task, "SelectTask", configMINIMAL_STACK_SIZE + 100, NULL,
		NETWORK_MONITOR_TASK, &network_monitor_task_handle))
	{
		ret = -1;
	}

	if(0 == ret)
	{
		initialize_socket_management();
	}
	if(0 == ret)
	{
		network_monitor_mutex =  xSemaphoreCreateMutex();
		if(NULL == network_monitor_mutex) return -1;
	}

	return ret;
}

int enqueue_select_rx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;
	wifi_service_register_rx_socket(id, sd, timeout_s);

	(void)xSemaphoreTake( rx_sockets[(size_t)id].signal_semaphore, 0);
	(void) xTaskNotifyIndexed(network_monitor_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( rx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s + 2) ))
	{
		ret_value = 0;
	}

	return ret_value;
}

int enqueue_select_tx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;
	wifi_service_register_tx_socket(id, sd, timeout_s);

	(void)xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 0);
	(void) xTaskNotifyIndexed(network_monitor_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s + 2) ))
	{
		ret_value = 0;
	}

	return ret_value;
}

/* RX-TX-Monitor Task */
static void select_task(void *param)
{
	(void) param;

	// Add network mutex
	vTaskSuspend(NULL);

	fd_set read_fd_set;
	fd_set write_fd_set;
	uint32_t notification_counter = 0;

	do
	{
		// Reset the pointers
		fd_set * read_set_ptr = NULL;
		fd_set * write_fd_set_ptr = NULL;

		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);

		size_t i = 0;

		uint32_t timeout_min = MONITOR_MAX_RESPONSE_S;
		uint32_t current_time = sl_sleeptimer_get_time();

		// Start Cycle

		(void)xSemaphoreTake(network_monitor_mutex, portMAX_DELAY);

		for (i = 0; i < (size_t) wifi_service_max; i++)
		{
			uint32_t current_timeout = 0;

			struct socket_management_s *rx_socket = &rx_sockets[i];
			struct socket_management_s *tx_socket = &tx_sockets[i];

			if (rx_socket->sd > 0)
			{
				// Get the timeout
				if(rx_socket->deadline_s != 0)
				{
					if(rx_socket->deadline_s >= current_time)
					{

						current_timeout = rx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(rx_socket->sd, &read_fd_set);
						read_set_ptr = &read_fd_set;
					}
					else if((rx_socket->deadline_s + 1) == current_time)
					{
						timeout_min = 0;
						FD_SET(rx_socket->sd, &read_fd_set);
						read_set_ptr = &read_fd_set;
					}
					else
					{
						// rx_socket->sd = -1;
						rx_socket->deadline_s = 0; /* Deadline expired */
					}
				}

			} // Checking tx sockets
			if (tx_socket->sd > 0)
			{
				// Get the timeout
				if (tx_socket->deadline_s != 0)
				{
					if((tx_socket->deadline_s)>=current_time)
					{
						current_timeout = tx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(tx_socket->sd, &write_fd_set);
						write_fd_set_ptr = &write_fd_set;
					}
					else if((tx_socket->deadline_s+1) == current_time)
					{
						timeout_min = 0;
						FD_SET(tx_socket->sd, &write_fd_set);
						write_fd_set_ptr = &write_fd_set;
					}
					else
					{
						// tx_sockets->sd = -1;
						tx_socket->deadline_s = 0; /* Deadline expired */
					}
				}
			} // Checking tx sockets

		} // End for
		(void)xSemaphoreGive(network_monitor_mutex);

		if ((read_set_ptr != NULL) || (write_fd_set_ptr != NULL))
		{
			// Start second cycle
			struct timeval tv =
			{ .tv_sec = timeout_min, .tv_usec = 0 };
			int result = sl_Select(FD_SETSIZE, read_set_ptr, write_fd_set_ptr, NULL, &tv);
			if(result > 0)
			{
				(void)xSemaphoreTake(network_monitor_mutex, portMAX_DELAY);
				if (read_set_ptr != NULL)
				{
					for (i = 0; i < (size_t) wifi_service_max; i++)
					{
						if(rx_sockets[i].sd > 0)
						{
							if (FD_ISSET(rx_sockets[i].sd, read_set_ptr))
							{
								rx_sockets[i].deadline_s = 0;
								// rx_sockets[i].sd = -1;
								xSemaphoreGive( rx_sockets[i].signal_semaphore );
							}
						}
					}
				}
				if (write_fd_set_ptr != NULL)
				{
					for (i = 0; i < (size_t) wifi_service_max; i++)
					{
						if(tx_sockets[i].sd > 0)
						{
							if (FD_ISSET(tx_sockets[i].sd, write_fd_set_ptr))
							{
								tx_sockets[i].deadline_s = 0;
								// tx_sockets[i].sd = -1;
								xSemaphoreGive( tx_sockets[i].signal_semaphore );
							}
						}
					}
				}
				(void)xSemaphoreGive(network_monitor_mutex);
			}
			else
			{
				//
			}
			// End second cycle // Return mutex
		} else
		{
			// Return mutex
			(void)xTaskGenericNotifyWait( 0, 0, UINT32_MAX, &notification_counter, portMAX_DELAY);
		}


	} while (1);
}


/* Basic network operations */
static int _network_connect(uiso_network_ctx_t ctx, const char *host,
		const char *port, enum uiso_protocol proto)
{
	int ret = (int)UISO_NETWORK_GENERIC_ERROR;
	_i16 dns_status = -1;

	SlSockAddrIn_t *host_addr = &(ctx->peer);

	/* Only resolve for IPv4 */
	memset(host_addr, 0, sizeof(SlSockAddrIn_t));

	dns_status = sl_NetAppDnsGetHostByName((_i8*) host, strlen(host),
			&(host_addr->sin_addr.s_addr), SL_AF_INET);

	if (dns_status < 0)
	{
		return (int)UISO_NETWORK_DNS_ERROR;
	}
	else
	{
		ctx->peer.sin_family = SL_AF_INET;
		ctx->peer.sin_port = __REV16(atoi(port));
		ctx->peer.sin_addr.s_addr = __REV(host_addr->sin_addr.s_addr);
		ctx->peer_len = sizeof(struct SlSockAddrIn_t);
	}

	_i16 type = 0;
	_i16 protocol = 0;

	switch (proto)
	{
	case uiso_protocol_dtls_ip4:
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
		break;
	case uiso_protocol_tls_ip4:
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
		break;
	case uiso_protocol_udp_ip4:
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
		break;
	case uiso_protocol_tcp_ip4:
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
		break;
	default:
		type = 0;
		protocol = 0;
		break;
	}

	/* Open the socket */
	ctx->protocol = (int32_t)proto;
	ctx->sd = (int32_t) socket(SL_AF_INET, type, protocol);
	if (ctx->sd >= (int32_t) 0)
	{
		ret = (int)UISO_NETWORK_OK;
	}
	else
	{
		ret = (int)UISO_NETWORK_SOCKET_ERROR;
	}

	if (ret == (int)UISO_NETWORK_OK)
	{
		SlSockNonblocking_t enableOption =
		{ .NonblockingEnabled = 1 };
		(void) sl_SetSockOpt(ctx->sd, SL_SOL_SOCKET, SL_SO_NONBLOCKING,
				(_u8*) &enableOption, sizeof(enableOption));
	}

	// Bind to local port
	//
	//


	if (ret == (int)UISO_NETWORK_OK)
	{
		if (0
				< connect(ctx->sd, (SlSockAddr_t*) &(ctx->peer),
						sizeof(ctx->peer)))
		{
			ret = (int)UISO_NETWORK_CONNECT_ERROR;
			(void) close(ctx->sd);
			ctx->sd = UISO_NETWORK_INVALID_SOCKET;
		}
	}

	return ret;
}

/* support function for mbedTLS */
static int _network_recv(uiso_network_ctx_t ctx, unsigned char *buf, size_t len)
{
	int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

	if(NULL == ctx)
	{
		return UISO_NETWORK_NULL_CTX;
	}
	else if(0 >= (ctx->sd))
	{
		return UISO_NETWORK_NEGATIVE_SD;
	}

	if(ctx->protocol & (int32_t)uiso_protocol_udp_ip4)
	{
		ctx->peer_len = sizeof(SlSockAddrIn_t);
		ret = (int) sl_RecvFrom((_i16)ctx->sd, (void *)buf, (_i16)len, (_i16)0, (SlSockAddr_t *)&(ctx->peer), (SlSocklen_t*)&(ctx->peer_len));
	}
	else if(ctx->protocol & (int32_t)uiso_protocol_tcp_ip4)
	{
		ret = (int)sl_Recv((_i16)ctx->sd, (char*)buf, (_i16)len, (_i16)0);
	}
	else
	{
		ret = (int)UISO_NETWORK_UNKNOWN_PROTOCOL;
	}

	if (ret < 0)
	{
		if (ret == (int)SL_EAGAIN)
		{
			ret = (int)MBEDTLS_ERR_SSL_WANT_READ;
		}
		else
		{
			ret = UISO_NETWORK_RECV_ERROR;
		}
	}

	return (ret);
}

/* support function for mbedTLS */
static int _network_send(uiso_network_ctx_t ctx, unsigned char *buf, size_t len)
{
	int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

	if(NULL == ctx)
	{
		return UISO_NETWORK_NULL_CTX;
	}
	else if(0 >= (ctx->sd))
	{
		return UISO_NETWORK_NEGATIVE_SD;
	}

	if(ctx->protocol & (int32_t)uiso_protocol_udp_ip4)
	{
		ret = (int)sl_SendTo((_i16)ctx->sd, (void *)buf, (_i16) len, (_i16) 0,  (SlSockAddr_t *)&(ctx->peer),	sizeof(SlSockAddrIn_t));
	}
	else if(ctx->protocol & (int32_t)uiso_protocol_tcp_ip4)
	{
		ret = (int)sl_Send((_i16)ctx->sd, (void *)buf, (_i16)len, (_i16)0);
	}
	else
	{
		ret = (int)UISO_NETWORK_UNKNOWN_PROTOCOL;
	}

	return ret;
}


static int _set_mbedtls_bio(uiso_network_ctx_t ctx)
{
	if(NULL == ctx)
	{
		return (int)UISO_NETWORK_NULL_CTX;
	}
	else if(NULL == ctx->ssl_context)
	{
		return (int)UISO_NETWORK_NULL_CTX;
	}

	mbedtls_ssl_set_bio(ctx->ssl_context, (void *)ctx,
			(mbedtls_ssl_send_t *)_network_send,
			(mbedtls_ssl_recv_t *)_network_recv,
			(mbedtls_ssl_recv_timeout_t*)NULL);

	mbedtls_ssl_set_mtu(ctx->ssl_context,
			SIMPLELINK_MAX_SEND_MTU); /* Set MTU to match simple-link */

	return UISO_NETWORK_OK;
}


/*
 * Close the connection
 */
static int _network_close(uiso_network_ctx_t ctx)
{
	if(NULL == ctx)
	{
		return UISO_NETWORK_NULL_CTX;
	}
	else if(0 >= (ctx->sd))
	{
		return UISO_NETWORK_NEGATIVE_SD;
	}

	int ret = (int)sl_Close((_i16)ctx->sd);

	ctx->sd = UISO_NETWORK_INVALID_SOCKET;

	return ret;
}

int uiso_network_register_ssl_context(uiso_network_ctx_t ctx, mbedtls_ssl_context * ssl_ctx)
{
	if(NULL == ctx)
	{
		return UISO_NETWORK_NULL_CTX;
	}
	else if(NULL == ssl_ctx)
	{
		return UISO_NETWORK_NULL_CTX;
	}

	ctx->ssl_context = ssl_ctx;
	return UISO_NETWORK_OK;
}



int uiso_create_network_connection(uiso_network_ctx_t ctx, const char *host,
		const char *port, enum uiso_protocol proto)
{
	// GET NETWORK MUTEX
	int ret = (int)UISO_NETWORK_OK;

	/* Prepare connection */
	if((UISO_SECURITY_BIT_MASK & (int32_t)proto) == UISO_SECURITY_BIT_MASK)
	{
		ret = _set_mbedtls_bio(ctx);
	}

	if(ret >= 0)
	{
		ret = _network_connect(ctx, host, port, proto);
	}

	if(ret >= 0)
	{
		if((UISO_SECURITY_BIT_MASK & (int32_t)proto) == UISO_SECURITY_BIT_MASK)
		{
			/* Perform mbedTLS handshake */
			do
			{
				ret = mbedtls_ssl_handshake(ctx->ssl_context);
			} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
		}
	}

	if(ret >= 0)
	{
		ctx->last_recv_time = sl_sleeptimer_get_time();
		ctx->last_send_time = ctx->last_recv_time;
	}

	// RETURN NETWORK MUTEX
	return ret;
}


int uiso_network_send_udp(uiso_network_ctx_t ctx, uint8_t *buffer, size_t length)
{
	// GET NETWORK MUTEX
	int n_bytes_sent = 0;
	int ret = (int)UISO_NETWORK_OK;
	size_t offset = 0;

	uint32_t current_time = sl_sleeptimer_get_time();
	int cid_enabled = MBEDTLS_SSL_CID_DISABLED;

	ret = mbedtls_ssl_get_peer_cid(ctx->ssl_context, &cid_enabled, NULL, 0);
	if (MBEDTLS_SSL_CID_DISABLED == cid_enabled)
	{
		if ((current_time - ctx->last_send_time) > 120)
		{
			/* Attempt re-negotiation */
			do
			{
				ret = mbedtls_ssl_renegotiate(ctx->ssl_context);
			} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret)
					|| (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS == ret));

			if (0 != ret)
			{
				/* This code tries to handle issues seen when the server does not support renegociation */
				ret = mbedtls_ssl_close_notify(ctx->ssl_context);
				if (0 == ret)
				{
					ret = mbedtls_ssl_session_reset(ctx->ssl_context);
				}
				else
				{
					(void) mbedtls_ssl_session_reset(ctx->ssl_context);
				}

				if (0 == ret)
				{
					do
					{
						ret = mbedtls_ssl_handshake(
								ctx->ssl_context);
					} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
							|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
				}
			}

			if (0 != ret)
			{
				return (int)UISO_NETWORK_DTLS_RENEGOCIATION_FAIL;
			}
		}
	}

	/* Actual sending */
	if(ctx->protocol == uiso_protocol_dtls_ip4)
	{
		while(offset != length)
		{
			n_bytes_sent = mbedtls_ssl_write(ctx->ssl_context, buffer + offset,
					length - offset);
			if ((MBEDTLS_ERR_SSL_WANT_READ == n_bytes_sent)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == n_bytes_sent)
					|| (MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS == n_bytes_sent)
					|| (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS == n_bytes_sent))
			{
				/* These mbedTLS return codes mean that the write operation must be retried */
				n_bytes_sent = 0;
			}
			else if(0 > n_bytes_sent)
			{
				ret = UISO_NETWORK_GENERIC_ERROR;
				do
				{
					ret = mbedtls_ssl_close_notify(ctx->ssl_context);
				} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
										|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));

				ret = mbedtls_ssl_session_reset(ctx->ssl_context);
				if (0 == ret)
				{
					do
					{
						ret = mbedtls_ssl_handshake(ctx->ssl_context);
					} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
				}

				if (0 == ret)
				{
					n_bytes_sent = 0;
				}
				else
				{
					return ret;
				}

			}

			if(0 > n_bytes_sent){
				return ret;
			}
			offset += n_bytes_sent;
		}
	}
	else if(ctx->protocol == uiso_protocol_udp_ip4)
	{

		while(offset != length)
		{
			n_bytes_sent = _network_send(ctx, buffer + offset, length - offset);
			if(0 >= n_bytes_sent)
			{
				return n_bytes_sent;
			}
			offset += n_bytes_sent;
		}
	}
	else
	{
		return (int)UISO_NETWORK_GENERIC_ERROR;
	}

	// RETURN NETWORK MUTEX
	return n_bytes_sent;
}

int uiso_network_send_tcp(uiso_network_ctx_t ctx, uint8_t *buffer, size_t length)
{
	int ret = (int)UISO_NETWORK_OK;


	return ret;
}

