/*
 * lwm2m_client.h
 *
 *  Created on: 4 dic 2022
 *      Author: Francisco
 */

#ifndef LWM2M_LIGHTCLIENT_LWM2M_CLIENT_H_
#define LWM2M_LIGHTCLIENT_LWM2M_CLIENT_H_

#include "uiso.h"
#include "liblwm2m.h"


/**
 * Notify of temperature value change
 */
void lwm2m_client_update_temperature(float temperature);
void lwm2m_client_update_accel(float x, float y, float z);


#endif /* LWM2M_LIGHTCLIENT_LWM2M_CLIENT_H_ */
