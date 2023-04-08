/*
 * wifi_service.h
 *
 *  Created on: 12 nov 2022
 *      Author: Francisco
 */

#ifndef WIFI_SERVICE_H_
#define WIFI_SERVICE_H_

#include "uiso.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timer.h"
#include "semphr.h"

enum wifi_socket_id_e
{
	wifi_service_ntp_socket = 0,
	wifi_service_lwm2m_socket = 1,
	wifi_service_mqtt_socket = 2,
	wifi_service_http_socket = 3,
	wifi_service_max
};

void wifi_service_register_rx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);

int enqueue_select_rx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);
int enqueue_select_tx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s);

void create_wifi_service_task(void);

#endif /* WIFI_SERVICE_H_ */
