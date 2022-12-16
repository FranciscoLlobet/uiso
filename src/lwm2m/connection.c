/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Pascal Rieux - Please refer to git log
 *
 *******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "connection.h"
#include "commandline.h"
#include "uiso_net_sockets.h"
#include "mbedtls/net_sockets.h"
#include <sys/types.h>

#define SIMPLE_LINK_MAX_SEND_MTU 	1472

extern mbedtls_ssl_context ssl_context;
extern uiso_mbedtls_context_t net_context;

connection_t connection_find(connection_t connList, struct sockaddr_in *addr,
		size_t addrLen)
{
	connection_t connP;

	connP = connList;
	while (connP != NULL)
	{
		/* Modified this for IPv4 */
		if (uiso_net_compare_addresses_ipv4((SlSockAddrIn_t*) addr,
				&(connP->host_addr)))
		{
			return connP;
		}

		connP = connP->next;
	}

	return connP;
}

connection_t connection_new_incoming(connection_t connList,
		struct uiso_mbedtls_context_s *connection)
{
	connection_t connP;

	connP = (connection_t) lwm2m_malloc(sizeof(connection_t));
	if (connP != NULL)
	{
		/* Prepend to list */
		memmove(connP, connection, sizeof(struct uiso_mbedtls_context_s));
		connP->next = connList;
	}

	return connP;
}

connection_t connection_create(connection_t connList, char *host, char *port,
		int protocol)
{
	(void) connList;
	int ret = UISO_NET_GENERIC_ERROR;

	connection_t connP = NULL;

	ret = uiso_mbedtls_net_connect(&net_context, host, port, protocol);

	if (UISO_NET_OK == ret)
	{
		if (uiso_protocol_dtls_ip4 == protocol)
		{
			mbedtls_ssl_set_bio(net_context.ssl_context, &net_context,
					mbedtls_net_send, mbedtls_net_recv, NULL);
			mbedtls_ssl_set_mtu(net_context.ssl_context,
					SIMPLE_LINK_MAX_SEND_MTU); /* Set MTU to match simple-link */

			do
			{
				ret = mbedtls_ssl_handshake(net_context.ssl_context);
			} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
		}
	}

	if (UISO_NET_OK == ret)
	{
		//connP = connection_new_incoming(connList, &net_context);
		net_context.next = NULL;
		connP = &net_context;
		net_context.last_send_time = lwm2m_gettime();
	}

	return connP;
}

void connection_free(connection_t connList)
{
	int ret = UISO_NET_GENERIC_ERROR;

	do
	{
		ret = mbedtls_ssl_close_notify(net_context.ssl_context);
	} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
			|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));

	mbedtls_net_close(&net_context);
}

int connection_send(connection_t connP, uint8_t *buffer, size_t length)
{
	int nbSent;
	size_t offset;

	offset = 0;
	time_t current_time = lwm2m_gettime();

	/* Re-negotiation after a certain timeout */
	if (connP->protocol == uiso_protocol_dtls_ip4)
	{
		int ret = -1;
		int enabled = MBEDTLS_SSL_CID_DISABLED;

		/* Check if CID is available from peer */
		ret = mbedtls_ssl_get_peer_cid(&ssl_context, &enabled, NULL, 0);
		if (MBEDTLS_SSL_CID_DISABLED == enabled)
		{
			if ((current_time - connP->last_send_time) > 120)
			{
				/* Attempt re-negotiation */
				do
				{
					ret = mbedtls_ssl_renegotiate(&ssl_context);
				} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
						|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret)
						|| (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS == ret));

				if (0 != ret)
				{
					/* This code tries to handle issues seen when the server does not support renegociation */
					ret = mbedtls_ssl_close_notify(&ssl_context);
					if (0 == ret)
					{
						ret = mbedtls_ssl_session_reset(&ssl_context);
					}
					else
					{
						(void) mbedtls_ssl_session_reset(&ssl_context);
					}

					if (0 == ret)
					{
						do
						{
							ret = mbedtls_ssl_handshake(
									net_context.ssl_context);
						} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
								|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
					}
				}

				if (0 != ret)
				{
					printf("Error: %d", ret);
					return -1;
				}
			}
		}
	}

	while (offset != length)
	{
		if (connP->protocol == uiso_protocol_dtls_ip4)
		{
			nbSent = mbedtls_ssl_write(&ssl_context, buffer + offset,
					length - offset);
			if ((MBEDTLS_ERR_SSL_WANT_READ == nbSent)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == nbSent)
					|| (MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS == nbSent)
					|| (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS == nbSent))
			{
				nbSent = 0;
			}
			else if (0 > nbSent)
			{
#if 1
				int ret = -1;
				do
				{
					ret = mbedtls_ssl_close_notify(&ssl_context);
				} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
						|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));

				ret = mbedtls_ssl_session_reset(&ssl_context);
				if (0 == ret)
				{
					do
					{
						ret = mbedtls_ssl_handshake(net_context.ssl_context);
					} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
							|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
				}
				if (0 == ret)
				{
					nbSent = 0;
				}
#endif
			}
		}
		else if (connP->protocol == uiso_protocol_udp_ip4)
		{
			nbSent = sendto(connP->fd, buffer + offset, length - offset, 0,
					(struct sockaddr*) &(connP->host_addr),
					connP->host_addr_len);
		}
		else
		{
			nbSent = -1; /* Unsupported protocol */
		}

		if (nbSent < 0)
			return -1;
		offset += nbSent;
	}

	connP->last_send_time = current_time;
	return 0;
}

int connection_renegociate(connection_t connP)
{
	int ret = -1;

	/* Attempt re-negotiation */
	do
	{
		ret = mbedtls_ssl_renegotiate(&ssl_context);
	} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
			|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret)
			|| (MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS == ret));

	if (0 != ret)
	{
		/* This code tries to handle issues seen when the server does not support renegociation */
		do
		{
			ret = mbedtls_ssl_close_notify(&ssl_context);
		} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
				|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));

		ret = mbedtls_ssl_session_reset(&ssl_context);

		if (0 == ret)
		{
			do
			{
				ret = mbedtls_ssl_handshake(net_context.ssl_context);
			} while ((MBEDTLS_ERR_SSL_WANT_READ == ret)
					|| (MBEDTLS_ERR_SSL_WANT_WRITE == ret));
		}

	}

	return ret;
}

uint8_t lwm2m_buffer_send(void *sessionH, uint8_t *buffer, size_t length,
		void *userdata)
{
	connection_t connP = (connection_t) sessionH;

	(void) userdata; /* unused */

	if (connP == NULL)
	{
		return COAP_500_INTERNAL_SERVER_ERROR ;
	}

	if (-1 == connection_send(connP, buffer, length))
	{
		return COAP_500_INTERNAL_SERVER_ERROR ;
	}

	return COAP_NO_ERROR ;
}

bool lwm2m_session_is_equal(void *session1, void *session2, void *userData)
{
	(void) userData; /* unused */

	return true; //(session1 == session2);
}
