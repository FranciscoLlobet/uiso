/*
 * network.c
 *
 *  Created on: 8 abr 2023
 *      Author: Francisco
 */


#include "uiso.h"


#include "wifi_service.h"

#include "simplelink.h"
#include "sl_sleeptimer.h"

#define NETWORK_MONITOR_TASK    (UBaseType_t)( uiso_task_runtime_services )

/* Internal socket management */
struct socket_management_s
{
	int16_t sd; /* Socket Descriptor */
	uint32_t deadline_s;
	SemaphoreHandle_t queue_handle;
	SemaphoreHandle_t signal_semaphore;
};

static struct socket_management_s rx_sockets[wifi_service_max];
static struct socket_management_s tx_sockets[wifi_service_max];


static void select_task(void *param);
static void initialize_socket_management(void);
static int register_deadline(struct socket_management_s * ctx, int sd, uint32_t timeout_s);
static void wifi_service_register_rx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);
static void wifi_service_register_tx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);

TaskHandle_t network_monitor_task_handle = NULL;

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

	if(NULL != ctx)
	{
		ctx->sd = sd;
		/* Register a deadline plus a tolerance */
		ctx->deadline_s = (uint32_t) sl_sleeptimer_get_time() + timeout_s;
	}
	else
	{
		ret = -1;
	}

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


	return ret;
}

int enqueue_select_rx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;

	wifi_service_register_rx_socket(id, sd, timeout_s);

	(void) xTaskNotifyIndexed(network_monitor_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( rx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s) ))
	{
		ret_value = 0;
	}

	return ret_value;
}

int enqueue_select_tx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;
	(void)xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 0);

	wifi_service_register_tx_socket(id, sd, timeout_s);

	(void) xTaskNotifyIndexed(network_monitor_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s) ))
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
					if((rx_socket->deadline_s)>= current_time)
					{
						current_timeout = rx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(rx_socket->sd, &read_fd_set);
						read_set_ptr = &read_fd_set;
					}
					else
					{
						rx_socket->deadline_s = 0; /* Deadline expired */
					}
				}

			} // Checking rx sockets
			if (tx_socket->sd > 0)
			{
				// Get the timeout
				if (tx_socket->deadline_s != 0)
				{
					if((tx_socket->deadline_s))
					{
						current_timeout = tx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(tx_socket->sd, &write_fd_set);
						write_fd_set_ptr = &write_fd_set;
					}
					else
					{
						tx_socket->deadline_s = 0; /* Deadline expired */
					}
				}
			} // Checking tx sockets

		}

		if ((read_set_ptr != NULL) || (write_fd_set_ptr != NULL))
		{
			struct timeval tv =
			{ .tv_sec = timeout_min, .tv_usec = 0 };
			int result = sl_Select(FD_SETSIZE, read_set_ptr, write_fd_set_ptr, NULL, &tv);
			if(result > 0)
			{
				if (read_set_ptr != NULL)
				{
					for (i = 0; i < (size_t) wifi_service_max; i++)
					{
						if(rx_sockets[i].sd > 0)
						{
							if (FD_ISSET(rx_sockets[i].sd, read_set_ptr))
							{
								rx_sockets[i].deadline_s = 0;
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
								xSemaphoreGive( tx_sockets[i].signal_semaphore );
							}
						}
					}
				}
			}
			else
			{
				//
			}
		} else
		{
			uint32_t notification_counter;
			(void)xTaskGenericNotifyWait( 0, 0, UINT32_MAX, &notification_counter, 10*1000);
			//printf("RX counter %d\n\r", notification_counter);
		}
	} while (1);
}
