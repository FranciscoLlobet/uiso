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
//#include "simplelink.h"
#include <sys/types.h>

extern mbedtls_ssl_context ssl_context;
extern uiso_mbedtls_context_t net_context;

int create_socket(const char * portStr, int addressFamily)
{
	(int)addressFamily;
	int s = -1;
	int res = -1;

	SlSockAddrIn_t local_socket = { .sin_family = SL_AF_INET, .sin_port = 0, .sin_addr = { .s_addr = 0 } };

	local_socket.sin_addr.s_addr = 0;
	if(NULL != portStr){
		local_socket.sin_port = __REV16(atoi(portStr));
	}
	else
	{
		local_socket.sin_port = 0;
	}

	s = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, IPPROTO_UDP);
	if(s >= 0)
	{
		res = sl_Bind(s, (SlSockAddr_t*) &local_socket, sizeof(local_socket));
		if(res < 0)
		{
			(void)sl_Close(s);
			s = -1;
		}
	}

    return s;
}

connection_t connection_find(connection_t connList,
                               struct sockaddr_in * addr,
                               size_t addrLen)
{
    connection_t connP;

    connP = connList;
    while (connP != NULL)
    {
    	/* Modified this for IPv4 */
    	if(uiso_net_compare_addresses_ipv4( (SlSockAddrIn_t *)addr, &(connP->host_addr)))
    	{
    		return connP;
    	}

        connP = connP->next;
    }

    return connP;
}

connection_t connection_new_incoming(connection_t connList, struct uiso_mbedtls_context_s * connection)
{
    connection_t connP;

    connP = (connection_t)lwm2m_malloc(sizeof(connection_t));
    if (connP != NULL)
    {
    	/* Prepend to list */
        memmove(connP, connection, sizeof(struct uiso_mbedtls_context_s));
        connP->next = connList;
    }

    return connP;
}



connection_t connection_create(connection_t connList,
                                 int sock,
                                 char * host,
                                 char * port,
                                 int protocol)
{
	(void)sock;
	int ret = UISO_NET_GENERIC_ERROR;


	connection_t connP = NULL;

    ret = uiso_mbedtls_net_connect(&net_context, host, port, protocol);

	if(UISO_NET_OK == ret)
	{
		if(uiso_protocol_dtls_ip4 == protocol)
		{
			mbedtls_ssl_set_bio( net_context.ssl_context, &net_context, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout );
			mbedtls_ssl_set_mtu( net_context.ssl_context, 1460);

			do {
				ret = mbedtls_ssl_handshake( net_context.ssl_context );
			}while( (ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE) );
		}
	}

	if(UISO_NET_OK == ret)
	{
		//connP = connection_new_incoming(connList, &net_context);
		net_context.next = NULL;
		connP = &net_context;
	}


    return connP;
}




void connection_free(connection_t connList)
{
    while (connList != NULL)
    {
        connection_t nextP;

        nextP = connList->next;
        lwm2m_free(connList);

        connList = nextP;
    }
}


int connection_send(connection_t connP,
                    uint8_t * buffer,
                    size_t length)
{
    int nbSent;
    size_t offset;

    offset = 0;
    while (offset != length)
    {
    	if(connP->protocol == uiso_protocol_dtls_ip4)
    	{
    		nbSent = mbedtls_ssl_write(&ssl_context, buffer + offset, length - offset);
    	}
    	else if(connP->protocol == uiso_protocol_udp_ip4)
    	{
    		nbSent = sendto(connP->fd, buffer + offset, length - offset, 0, (struct sockaddr *)&(connP->host_addr), connP->host_addr_len);
    	}
    	else
    	{
    		/* Unsupported protocol */
    		nbSent = -1;
    	}

        if (nbSent < 0) return -1;
        offset += nbSent;
    }
    return 0;
}



uint8_t lwm2m_buffer_send(void * sessionH,
                          uint8_t * buffer,
                          size_t length,
                          void * userdata)
{
    connection_t connP = (connection_t) sessionH;

    (void)userdata; /* unused */

    if (connP == NULL)
    {
        fprintf(stderr, "#> failed sending %lu bytes, missing connection\r\n", length);
        return COAP_500_INTERNAL_SERVER_ERROR ;
    }

    if (-1 == connection_send(connP, buffer, length))
    {
        fprintf(stderr, "#> failed sending %lu bytes\r\n", length);
        return COAP_500_INTERNAL_SERVER_ERROR ;
    }

    return COAP_NO_ERROR;
}

bool lwm2m_session_is_equal(void * session1,
                            void * session2,
                            void * userData)
{
    (void)userData; /* unused */

    return true; //(session1 == session2);
}
