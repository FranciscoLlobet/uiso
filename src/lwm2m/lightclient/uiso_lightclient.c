/*******************************************************************************
 *
 * Copyright (c) 2013, 2014, 2015 Intel Corporation and others.
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
 *    Benjamin Cabé - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Axel Lorente - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    Christian Renz - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>
 Bosch Software Innovations GmbH - Please refer to git log

 */

#include "liblwm2m.h"
#include "../connection.h"

#include "uiso_net_sockets.h"
//#include "simplelink.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* Extern declarations */
extern lwm2m_object_t* get_object_device(void);
extern void free_object_device(lwm2m_object_t *objectP);
extern lwm2m_object_t* get_server_object(void);
extern void free_server_object(lwm2m_object_t *object);
extern lwm2m_object_t* get_security_object(void);
extern void free_security_object(lwm2m_object_t *objectP);
extern char* get_server_uri(lwm2m_object_t *objectP, uint16_t secObjInstID);
extern lwm2m_object_t* get_test_object(void);
extern void free_test_object(lwm2m_object_t *object);

#define MAX_PACKET_SIZE 2048

/* RX buffer */
static uint8_t buffer[MAX_PACKET_SIZE];

uint8_t* get_rx_buffer(void)
{
	return &buffer[0];
}

int g_reboot = 0;

typedef struct
{
	lwm2m_object_t *securityObjP;
	int sock;
	connection_t connList;
	int addressFamily;
	connection_t connection_context;
} client_data_t;

/* Net context for the LWM2M client */
uiso_mbedtls_context_t net_context;

/* SSL Timer for the LWM2M connection */
uiso_mbedtls_timing_delay_t ssl_timer;

/* SSL Context */
mbedtls_ssl_context ssl_context;

/* SSL Config */
mbedtls_ssl_config ssl_config;

/* DRBG Context*/
mbedtls_ctr_drbg_context drbg_context;

/* Initialize entropy */
uint32_t entropy = 0x55555555;

/* Entropy Context */
mbedtls_entropy_context entropy_context;

int ciphersuites[] = {
        MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8,
		MBEDTLS_TLS_PSK_WITH_AES_128_CCM,
		0
};

extern unsigned char * get_connection_psk(lwm2m_object_t * objectP, uint16_t secObjInstID, size_t * psk_len);
extern unsigned char * get_public_identiy(lwm2m_object_t * objectP, uint16_t secObjInstID, size_t * public_identity_len);

void* lwm2m_connect_server(uint16_t secObjInstID, void *userData)
{
	client_data_t *dataP;
	char *uri;
	char *host;
	char *port;
	connection_t newConnP = NULL;
	int ret = -1;

	dataP = (client_data_t*) userData;

	uri = get_server_uri(dataP->securityObjP, secObjInstID);

	if (uri == NULL)
		return NULL;

	fprintf(stdout, "Connecting to %s\r\n", uri);

	// parse uri in the form "coaps://[host]:[port]"
	if (0 == strncmp(uri, "coaps://", strlen("coaps://")))
	{
		host = uri + strlen("coaps://");
	}
	else if (0 == strncmp(uri, "coap://", strlen("coap://")))
	{
		host = uri + strlen("coap://");
	}
	else
	{
		goto exit;
	}
	port = strrchr(host, ':');
	if (port == NULL)
		goto exit;
	// remove brackets
	if (host[0] == '[')
	{
		host++;
		if (*(port - 1) == ']')
		{
			*(port - 1) = 0;
		}
		else
			goto exit;
	}
	// split strings
	*port = 0;
	port++;



	uiso_mbedtls_net_init(&net_context);
	uiso_mbedtls_init_timer(&ssl_timer);
	UISO_MBED_TLS_THREADING_SET_ALT();
	mbedtls_ssl_init(&ssl_context);
	mbedtls_ssl_config_init(&ssl_config);
	mbedtls_ctr_drbg_init(&drbg_context);
	mbedtls_entropy_init(&entropy_context);


	// mbedtls_entropy_add_source(&entropy_context, mbedtls_entropy_f_source_ptr f_source, void *p_source, size_t threshold, MBEDTLS_ENTROPY_SOURCE_STRONG );
	mbedtls_ctr_drbg_seed(&drbg_context, mbedtls_entropy_func, &entropy, NULL, 0);

	/* Set Configuration*/
	mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM,	MBEDTLS_SSL_PRESET_DEFAULT);
	mbedtls_ssl_conf_max_frag_len(&ssl_config, MBEDTLS_SSL_MAX_FRAG_LEN_1024);
	mbedtls_ssl_conf_authmode(&ssl_config, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_rng(&ssl_config, mbedtls_ctr_drbg_random, &drbg_context);
	mbedtls_ssl_conf_read_timeout(&ssl_config, 2000);

	size_t psk_len = 0;
	unsigned char * psk = get_connection_psk(dataP->securityObjP, secObjInstID, &psk_len);

	size_t public_identity_len = 0;
	unsigned char * public_identity = get_public_identiy(dataP->securityObjP, secObjInstID, &public_identity_len);

	ret = mbedtls_ssl_conf_psk(&ssl_config, psk, psk_len, public_identity, strlen(public_identity));

	mbedtls_ssl_conf_ciphersuites(&ssl_config, ciphersuites);

	ret = mbedtls_ssl_setup(&ssl_context, &ssl_config);
	mbedtls_ssl_set_timer_cb(&ssl_context, &ssl_timer,
			uiso_mbedtls_timing_set_delay, uiso_mbedtls_timing_get_delay);

	net_context.ssl_context = &ssl_context;


	newConnP = connection_create(dataP->connList, dataP->sock, host, port,
			(int) uiso_protocol_dtls_ip4);

	if (newConnP == NULL)
	{
		fprintf(stderr, "Connection creation failed.\r\n");
	}
	else
	{
		dataP->connList = newConnP;
	}

	exit: lwm2m_free(uri);
	return (void*) newConnP;
}

void lwm2m_close_connection(void *sessionH, void *userData)
{
	client_data_t *app_data;
	connection_t targetP;

	app_data = (client_data_t*) userData;
	targetP = (connection_t) sessionH;

    mbedtls_ssl_close_notify(&ssl_context);

    mbedtls_net_close(&net_context);
    mbedtls_entropy_free(&entropy_context);
    mbedtls_ssl_free(&ssl_context);
    mbedtls_ssl_config_free(&ssl_config);
    uiso_mbedtls_deinit_timer(&ssl_timer);
    mbedtls_threading_free_alt();


	if (targetP == app_data->connList)
	{
		app_data->connList = targetP->next;
		lwm2m_free(targetP);
	}
	else
	{
		connection_t parentP;

		parentP = app_data->connList;
		while (parentP != NULL && parentP->next != targetP)
		{
			parentP = parentP->next;
		}
		if (parentP != NULL)
		{
			parentP->next = targetP->next;
			lwm2m_free(targetP);
		}
	}
}

void print_state(lwm2m_context_t *lwm2mH)
{
	lwm2m_server_t *targetP;

	fprintf(stderr, "State: ");
	switch (lwm2mH->state)
	{
	case STATE_INITIAL:
		fprintf(stderr, "STATE_INITIAL");
		break;
	case STATE_BOOTSTRAP_REQUIRED:
		fprintf(stderr, "STATE_BOOTSTRAP_REQUIRED");
		break;
	case STATE_BOOTSTRAPPING:
		fprintf(stderr, "STATE_BOOTSTRAPPING");
		break;
	case STATE_REGISTER_REQUIRED:
		fprintf(stderr, "STATE_REGISTER_REQUIRED");
		break;
	case STATE_REGISTERING:
		fprintf(stderr, "STATE_REGISTERING");
		break;
	case STATE_READY:
		fprintf(stderr, "STATE_READY");
		break;
	default:
		fprintf(stderr, "Unknown !");
		break;
	}
	fprintf(stderr, "\r\n");

	targetP = lwm2mH->bootstrapServerList;

	if (lwm2mH->bootstrapServerList == NULL)
	{
		fprintf(stderr, "No Bootstrap Server.\r\n");
	}
	else
	{
		fprintf(stderr, "Bootstrap Servers:\r\n");
		for (targetP = lwm2mH->bootstrapServerList; targetP != NULL; targetP =
				targetP->next)
		{
			fprintf(stderr, " - Security Object ID %d", targetP->secObjInstID);
			fprintf(stderr, "\tHold Off Time: %lu s",
					(unsigned long) targetP->lifetime);
			fprintf(stderr, "\tstatus: ");
			switch (targetP->status)
			{
			case STATE_DEREGISTERED:
				fprintf(stderr, "DEREGISTERED\r\n");
				break;
			case STATE_BS_HOLD_OFF:
				fprintf(stderr, "CLIENT HOLD OFF\r\n");
				break;
			case STATE_BS_INITIATED:
				fprintf(stderr, "BOOTSTRAP INITIATED\r\n");
				break;
			case STATE_BS_PENDING:
				fprintf(stderr, "BOOTSTRAP PENDING\r\n");
				break;
			case STATE_BS_FINISHED:
				fprintf(stderr, "BOOTSTRAP FINISHED\r\n");
				break;
			case STATE_BS_FAILED:
				fprintf(stderr, "BOOTSTRAP FAILED\r\n");
				break;
			default:
				fprintf(stderr, "INVALID (%d)\r\n", (int) targetP->status);
			}
			fprintf(stderr, "\r\n");
		}
	}

	if (lwm2mH->serverList == NULL)
	{
		fprintf(stderr, "No LWM2M Server.\r\n");
	}
	else
	{
		fprintf(stderr, "LWM2M Servers:\r\n");
		for (targetP = lwm2mH->serverList; targetP != NULL;
				targetP = targetP->next)
		{
			fprintf(stderr, " - Server ID %d", targetP->shortID);
			fprintf(stderr, "\tstatus: ");
			switch (targetP->status)
			{
			case STATE_DEREGISTERED:
				fprintf(stderr, "DEREGISTERED\r\n");
				break;
			case STATE_REG_PENDING:
				fprintf(stderr, "REGISTRATION PENDING\r\n");
				break;
			case STATE_REGISTERED:
				fprintf(stderr,
						"REGISTERED\tlocation: \"%s\"\tLifetime: %lus\r\n",
						targetP->location, (unsigned long) targetP->lifetime);
				break;
			case STATE_REG_UPDATE_PENDING:
				fprintf(stderr, "REGISTRATION UPDATE PENDING\r\n");
				break;
			case STATE_REG_UPDATE_NEEDED:
				fprintf(stderr, "REGISTRATION UPDATE REQUIRED\r\n");
				break;
			case STATE_DEREG_PENDING:
				fprintf(stderr, "DEREGISTRATION PENDING\r\n");
				break;
			case STATE_REG_FAILED:
				fprintf(stderr, "REGISTRATION FAILED\r\n");
				break;
			default:
				fprintf(stderr, "INVALID (%d)\r\n", (int) targetP->status);
			}
			fprintf(stderr, "\r\n");
		}
	}
}

#define OBJ_COUNT 4

#define COMPLETE_SERVER_URI    "coap://leshan.eclipseprojects.io:5683"

lwm2m_context_t *lwm2mH = NULL;
lwm2m_object_t *objArray[OBJ_COUNT];

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t user_task_handle;

client_data_t data;

int lwm2m_client_task_runner(void *param1)
{
	(void) param1;

	lwm2m_context_t *lwm2mH = NULL;
	lwm2m_object_t *objArray[OBJ_COUNT];

	const char *localPort = "56830";
	char *name = "wakaama_xdk110";

	int result;
	int opt;

	/* Reset the client_data_object */
	memset(&data, 0, sizeof(client_data_t));
	data.connection_context = (connection_t) &net_context;
	data.connection_context->next = NULL;
	data.addressFamily = AF_INET;
	data.sock = -1;
	data.connList = (connection_t) NULL;
	data.connList->next = NULL;

	/*
	 *This call an internal function that create an IPv6 socket on the port 5683.
	 */

	fprintf(stderr, "Trying to bind LWM2M Client to port %s\r\n", localPort);
	data.sock = create_socket(localPort, data.addressFamily);
	if (data.sock < 0)
	{
		fprintf(stderr, "Failed to open socket: %d %s\r\n", errno,
				strerror(errno));
		return -1;
	}

	/*
	 * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
	 * Those functions are located in their respective object file.
	 */
	objArray[0] = get_security_object();
	if (NULL == objArray[0])
	{
		fprintf(stderr, "Failed to create security object\r\n");
		return -1;
	}
	data.securityObjP = objArray[0];

	objArray[1] = get_server_object();
	if (NULL == objArray[1])
	{
		fprintf(stderr, "Failed to create server object\r\n");
		return -1;
	}

	objArray[2] = get_object_device();
	if (NULL == objArray[2])
	{
		fprintf(stderr, "Failed to create Device object\r\n");
		return -1;
	}

	objArray[3] = get_test_object();
	if (NULL == objArray[3])
	{
		fprintf(stderr, "Failed to create Test object\r\n");
		return -1;
	}

	/*
	 * The liblwm2m library is now initialized with the functions that will be in
	 * charge of communication
	 */
	lwm2mH = lwm2m_init(&data);
	if (NULL == lwm2mH)
	{
		fprintf(stderr, "lwm2m_init() failed\r\n");
		return -1;
	}

	/*
	 * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
	 * the number of objects we will be passing through and the objects array
	 */
	result = lwm2m_configure(lwm2mH, name, NULL, NULL, OBJ_COUNT, objArray);
	if (result != 0)
	{
		fprintf(stderr, "lwm2m_configure() failed: 0x%X\r\n", result);
		return -1;
	}

	fprintf(stdout,
			"LWM2M Client \"%s\" started on port %s.\r\nUse Ctrl-C to exit.\r\n\n",
			name, localPort);

	/*
	 * We now enter in a while loop that will handle the communications from the server
	 */
	do
	{
		struct timeval tv;
		fd_set readfds;

		uint32_t notification_value = 0;
		if (pdTRUE == xTaskNotifyWait(0, UINT32_MAX, &notification_value, 0))
		{
			if (notification_value & (1 << 0))
			{
				lwm2m_update_registration(lwm2mH, 0, false);
			}
			if (notification_value & (1 << 1))
			{
				lwm2m_uri_t uri =
				{ .objectId = LWM2M_DEVICE_OBJECT_ID, .instanceId = 0,
						.resourceId = 13 };
				lwm2m_resource_value_changed(lwm2mH, &uri);
			}
		}

		print_state(lwm2mH);

		/* Perform LWM2M step*/
		time_t timeout_val = 60;

		result = lwm2m_step(lwm2mH, &timeout_val);

		/* Check if something is pending */
		if (result == 0)
		{
			FD_ZERO(&readfds);
			//FD_SET(data.sock, &readfds);
			FD_SET(data.connection_context->fd, &readfds);
			tv.tv_sec = timeout_val;
			tv.tv_usec = 0;

			result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
		}
		else
		{
			fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
			// Go to error condition
			break;
		}

		if (result == 0)
		{
			fprintf(stderr, "No LWM2M event\r\n");
		}
		if (result < 0)
		{
			if (errno != EINTR)
			{
				fprintf(stderr, "Error in select(): %d %s\r\n", errno,
						strerror(errno));
			}
		}
		else if (result > 0)
		{
			ssize_t numBytes = 0;

			/*
			 * If an event happens on the socket
			 */
			if (FD_ISSET(data.connection_context->fd, &readfds))
			{
				SlSockAddr_t addr;
				socklen_t addrLen = 0;

				addrLen = sizeof(addr);

				/*
				 * We retrieve the data received
				 */
				//numBytes = recvfrom(data.sock, get_rx_buffer(), MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrLen);
				numBytes = mbedtls_ssl_read(
						data.connection_context->ssl_context, get_rx_buffer(),
						MAX_PACKET_SIZE);

				if (0 > numBytes)
				{
//                	if(numByter == MBEDTLS_ERR_SSL_WANT_READ
// *                 #MBEDTLS_ERR_SSL_WANT_WRITE,
// *                 #MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS,
// *                 #MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS or
// *                 #MBEDTLS_ERR_SSL_CLIENT_RECONNECT,)

					fprintf(stderr, "Error in recvfrom(): %d %s\r\n", errno,
							strerror(errno));
				}
				else if (numBytes >= MAX_PACKET_SIZE)
				{
					fprintf(stderr, "Received packet >= MAX_PACKET_SIZE\r\n");
				}
				else if (0 < numBytes)
				{
					/* mbedtls (!) */
					// For some reason, the connection list gets corrupted
					//connection_t connP = connection_find(data.connList, (struct sockaddr_in *)&(data.connection_context->host_addr), data.connection_context->host_addr_len);
					connection_t connP = data.connection_context;

					if (connP != NULL)
					{
						/*
						 * Let liblwm2m respond to the query depending on the context
						 */
						lwm2m_handle_packet(lwm2mH, buffer, (size_t) numBytes,
								connP);
					}
					else
					{
						/*
						 * This packet comes from an unknown peer
						 */
						fprintf(stderr, "received bytes ignored!\r\n");
					}
				}
			}
		}
	} while (1);

	/*
	 * Finally when the loop is left, we unregister our client from it
	 */
	lwm2m_close(lwm2mH);
	close(data.sock);
	connection_free(data.connList);

	free_security_object(objArray[0]);
	free_server_object(objArray[1]);
	free_object_device(objArray[2]);
	free_test_object(objArray[3]);

	fprintf(stdout, "\r\n\n");

	return 0;
}
