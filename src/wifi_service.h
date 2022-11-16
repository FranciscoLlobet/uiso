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

void create_wifi_service_task(void);

#endif /* WIFI_SERVICE_H_ */
