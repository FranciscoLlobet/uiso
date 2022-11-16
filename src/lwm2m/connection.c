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

#include "simplelink.h"
#include <sys/types.h>


int create_socket(const char * portStr, int addressFamily)
{
	int s = -1;
	int res = -1;

	SlSockAddrIn_t lwm2m_client = { .sin_family = SL_AF_INET, .sin_port = 0, .sin_addr = { .s_addr = 0 } };

	lwm2m_client.sin_addr.s_addr = 0;
	lwm2m_client.sin_port = 0;

	s = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, IPPROTO_UDP);
	if(s >= 0)
	{
		res = sl_Bind(s, (SlSockAddr_t*) &lwm2m_client, sizeof(lwm2m_client));
		if(res < 0)
		{
			(void)sl_Close(s);
			s = -1;
		}
	}

    return s;
}

connection_t * connection_find(connection_t * connList,
                               struct sockaddr_in * addr,
                               size_t addrLen)
{
    connection_t * connP;

    connP = connList;
    while (connP != NULL)
    {
    	/* Modified this for IPv4 */
    	if((connP->addrLen == addrLen) && (connP->addr.sin_port == addr->sin_port ) && (connP->addr.sin_addr.s_addr == addr->sin_addr.s_addr))
    	{
    		return connP;
    	}
        connP = connP->next;
    }

    return connP;
}

connection_t * connection_new_incoming(connection_t * connList,
                                       int sock,
                                       struct sockaddr * addr,
                                       size_t addrLen)
{
    connection_t * connP;

    connP = (connection_t *)lwm2m_malloc(sizeof(connection_t));
    if (connP != NULL)
    {
        connP->sock = sock;
        memcpy(&(connP->addr), addr, addrLen);
        connP->addrLen = addrLen;
        connP->next = connList;
    }

    return connP;
}

connection_t * connection_create(connection_t * connList,
                                 int sock,
                                 char * host,
                                 char * port,
                                 int addressFamily)
{
	connection_t * connP = NULL;

	SlSockAddrIn_t lwm2m_server_addr =
		{ .sin_family = SL_AF_INET, .sin_port = 0, .sin_addr =
		{ .s_addr = 0 } };

	_i16 dns_status = -1;
	int s = -1;

	uint32_t dns_resolved_host = 0;
	dns_status = sl_NetAppDnsGetHostByName((_i8*)host, strlen(host), &(lwm2m_server_addr.sin_addr.s_addr), SL_AF_INET);

	if(dns_status >=0)
	{
		lwm2m_server_addr.sin_addr.s_addr = __REV(lwm2m_server_addr.sin_addr.s_addr);
		lwm2m_server_addr.sin_port = __REV16(5683);

		s = sl_Socket(SL_AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(s >= 0)
		{
			if(0 > sl_Connect(s, &lwm2m_server_addr, sizeof(lwm2m_server_addr)))
			{
				(void)sl_Close(s);
				s = -1;
			}
		}

	    if (s >= 0)
	    {
	        connP = connection_new_incoming(connList, sock, &lwm2m_server_addr, sizeof(lwm2m_server_addr));
	        close(s);
	    }
	}

    return connP;
}

void connection_free(connection_t * connList)
{
    while (connList != NULL)
    {
        connection_t * nextP;

        nextP = connList->next;
        lwm2m_free(connList);

        connList = nextP;
    }
}

int connection_send(connection_t *connP,
                    uint8_t * buffer,
                    size_t length)
{
    int nbSent;
    size_t offset;

#ifdef LWM2M_WITH_LOGS
    char s[INET6_ADDRSTRLEN];
    in_port_t port;

    s[0] = 0;

    if (AF_INET == connP->addr.sin6_family)
    {
        struct sockaddr_in *saddr = (struct sockaddr_in *)&connP->addr;
        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
        port = saddr->sin_port;
    }
    else if (AF_INET6 == connP->addr.sin6_family)
    {
        struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&connP->addr;
        inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
        port = saddr->sin6_port;
    } else {
        return -1;
    }

    fprintf(stderr, "Sending %lu bytes to [%s]:%hu\r\n", length, s, ntohs(port));

    output_buffer(stderr, buffer, length, 0);
#endif

    offset = 0;
    while (offset != length)
    {
        nbSent = sendto(connP->sock, buffer + offset, length - offset, 0, (struct sockaddr *)&(connP->addr), connP->addrLen);
        if (nbSent == -1) return -1;
        offset += nbSent;
    }
    return 0;
}

uint8_t lwm2m_buffer_send(void * sessionH,
                          uint8_t * buffer,
                          size_t length,
                          void * userdata)
{
    connection_t * connP = (connection_t*) sessionH;

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

    return (session1 == session2);
}
