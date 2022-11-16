/*
 * event_processor.c
 *
 *  Created on: 9 nov 2022
 *      Author: Francisco
 */
#include "FreeRTOS.h"
#include "task.h"

// Events
enum{
	/* Group 1: Buttons*/
	EVENT_BUTTON1_PRESS,
	EVENT_BUTTON1_RELEASE,

	EVENT_BUTTON2_PRESS,
	EVENT_BUTTON2_RELEASE,

	/* Group 2: SD Card*/
	EVENT_SD_CARD_INSERTED,
	EVENT_SD_CARD_REMOVED,

	/* Group 3: Wifi HW */

	/* Group 4: Sensors */

	/* Group 5: Wifi Connectivity */


	/* Group 6: */
};



#define EVENT_PROCESSOR_TASK_PRIORITY    ( tskIDLE_PRIORITY + 2 )

TaskHandle_t event_task_handle = NULL;

void event_task(void *param)
{
	(void) param;

	do
	{

		vTaskDelay(1000);

	} while (1);

}

void create_event_processor_task(void)
{
	xTaskCreate(event_task, "Event processor", configMINIMAL_STACK_SIZE, NULL,
			EVENT_PROCESSOR_TASK_PRIORITY, &event_task_handle);

}
