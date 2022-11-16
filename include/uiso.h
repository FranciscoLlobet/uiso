/*
 * uiso.h
 *
 *  Created on: 12 nov 2022
 *      Author: Francisco
 */

#ifndef UISO_H_
#define UISO_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"


#include "sl_iostream.h"
#include "sl_iostream_swo.h"

enum
{
	uiso_rtos_prio_idle = 0, /* Lowest priority. Reserved for idle task  */
	uiso_rtos_prio_low = 1,
	uiso_rtos_prio_below_normal = 2,
	uiso_rtos_prio_normal = 3,
	uiso_rtos_prio_above_normal = 4,
	uiso_rtos_prio_high = 5, /* Very High priority. */
	uiso_rtos_prio_highest = 6
};


enum
{
	uiso_task_prio_idle = uiso_rtos_prio_idle,
	uiso_task_housekeeping_services = uiso_rtos_prio_low,
	uiso_task_runtime_services = uiso_rtos_prio_normal,
	uiso_task_event_services = uiso_rtos_prio_above_normal,
	uiso_task_connectivity_service = uiso_rtos_prio_above_normal,
	uiso_task_prio_timer_deamon = uiso_rtos_prio_high, /* Reserved for the timer */
	uiso_task_prio_hardware_service = uiso_rtos_prio_highest,
};


#endif /* UISO_H_ */
