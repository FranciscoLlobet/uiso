/*
 * mqtt_client.h
 *
 *  Created on: 1 abr 2023
 *      Author: Francisco
 */

#ifndef MQTT_MQTT_CLIENT_H_
#define MQTT_MQTT_CLIENT_H_

#include "uiso.h"


TaskHandle_t get_mqtt_client_task_handle(void);

int initialize_mqtt_client(void);


#endif /* MQTT_MQTT_CLIENT_H_ */
