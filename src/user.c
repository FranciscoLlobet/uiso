/*
 * user.c
 *
 *  Created on: 9 nov 2022
 *      Author: Francisco
 */

#include "user_task.h"
#include "sl_iostream.h"
#include "sl_iostream_swo.h"
#include "semphr.h"

#define USER_TASK_PRIORITY    (UBaseType_t)( uiso_task_runtime_services )

TaskHandle_t user_task_handle = NULL;
TimerHandle_t user_timer = NULL;
TimerHandle_t clock_timer = NULL;
SemaphoreHandle_t user_semaphore = NULL;

extern int lwm2m_client_task_runner(void * param1);
//extern void trigger_registration_update(void);

void user_timer_callback( TimerHandle_t pxTimer )
{
	(void)pxTimer;
	xTaskNotify(user_task_handle, (uint32_t)(1<<0), eSetBits);
}

void clock_timer_callback( TimerHandle_t pxTimer )
{
	(void)pxTimer;
	xTaskNotify(user_task_handle, (uint32_t)(1<<1), eSetBits);
}

void user_task(void *param)
{
	(void) param;

	vTaskSuspend(NULL);

	lwm2m_client_task_runner(NULL);
}

void create_user_task(void)
{
	xTaskCreate(user_task, "User Task", 4096 + 2048, NULL,
			USER_TASK_PRIORITY, &user_task_handle);

	user_timer = xTimerCreate("Registration Update", pdMS_TO_TICKS(5*60*1000), pdTRUE, NULL, user_timer_callback);
	clock_timer = xTimerCreate("Timer Object Update", pdMS_TO_TICKS(1*60*1000), pdTRUE, NULL, clock_timer_callback);
	xTimerStart( user_timer, portMAX_DELAY);
	xTimerStart( clock_timer, portMAX_DELAY);
	user_semaphore = xSemaphoreCreateBinary();
}

