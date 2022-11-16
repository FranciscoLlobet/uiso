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
SemaphoreHandle_t user_semaphore = NULL;

void user_timer_callback( TimerHandle_t pxTimer )
{
	(void)pxTimer;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	xSemaphoreGive( user_semaphore );
}

#include "ff.h"

FATFS fs; /* File System pointer */

extern int lwm2m_client_task_runner(void * param1);

void user_task(void *param)
{
	(void) param;

	uint32_t ulNotifiedValue = 0;

	vTaskSuspend(NULL);

	lwm2m_client_task_runner(param);
}

void create_user_task(void)
{
	xTaskCreate(user_task, "User Task", configMINIMAL_STACK_SIZE + 1000, NULL,
			USER_TASK_PRIORITY, &user_task_handle);

	user_timer = xTimerCreate("blink timer", 1000, pdTRUE, NULL, user_timer_callback);
	xTimerStart( user_timer, 0);
	user_semaphore = xSemaphoreCreateBinary();
}

