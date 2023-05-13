/*
 * monitor_service.c
 *
 *  Created on: 13 abr 2023
 *      Author: Francisco
 */


#include "uiso.h"
#include "user_task.h"
#include "mqtt_client.h"
#include "sl_sleeptimer.h"
#include "ff.h"

#define SYSTEM_MONITOR_TASK_PRIO    (UBaseType_t)( uiso_task_event_services )

TimerHandle_t system_monitor_timer_handle = NULL;
TaskHandle_t system_monitor_handle;

static void sytem_monitor_timer( TimerHandle_t xTimer )
{
	(void)xTaskNotify( system_monitor_handle, 1, eIncrement);
}

extern TaskHandle_t xSimpleLinkSpawnTaskHndl;
extern TaskHandle_t network_monitor_task_handle;
extern TaskHandle_t wifi_task_handle;
TaskStatus_t task_status[10];



static void system_monitor(void *param)
{
	(void) param;

	xTimerStart( system_monitor_timer_handle, portMAX_DELAY );
	char system_monitor_msg[100];

	do
	{
		uint32_t notification_value = 0;
		if(pdTRUE == xTaskNotifyWait( 0x0, 0x0, &notification_value, portMAX_DELAY))
		{
			unsigned int timestamp = sl_sleeptimer_get_time();
			//uint32_t n_tasks = uxTaskGetNumberOfTasks();
			unsigned int stack_high_watermark_lwm2m = uxTaskGetStackHighWaterMark(get_user_task_handle());
			unsigned int stack_high_watermark_mqtt = uxTaskGetStackHighWaterMark(get_mqtt_client_task_handle());
			unsigned int stack_high_watermark_wifi = uxTaskGetStackHighWaterMark(wifi_task_handle);
			unsigned int stack_high_watermark_network_monitor = uxTaskGetStackHighWaterMark(network_monitor_task_handle);


			sprintf(system_monitor_msg, "%u, %u, %u, %u, %u, %u, %u\r\n", notification_value, timestamp,
					stack_high_watermark_lwm2m, stack_high_watermark_mqtt, stack_high_watermark_wifi, stack_high_watermark_network_monitor, xPortGetFreeHeapSize());

			FSIZE_t fSize = 0;
			FRESULT fRes = FR_OK;
			FATFS fs;
			UINT fWrite = 0;
			FIL file;

			//fRes = f_mount(&fs, "SD", 1);
			if (FR_OK == fRes)
			{
				fRes = f_open(&file, "SD:/LOG.TXT", (FA_WRITE | FA_OPEN_APPEND));
			}

			if (FR_OK == fRes)
			{
				fRes = f_lseek(&file, f_size(&file));
			}

			if (FR_OK == fRes)
			{
				fRes = f_write(&file, system_monitor_msg, strlen(system_monitor_msg), &fWrite);
			}

			if(FR_OK == fRes)
			{
				fRes = f_sync(&file);
			}

			if (FR_OK == fRes)
			{
				fRes = f_close(&file);
			}
			else
			{
				(void) f_close(&file);
			}

//			if (FR_OK == fRes)
//			{
//				fRes = f_unmount("SD");
//			}
//			else
//			{
//				(void) f_unmount("SD");
//			}
		}
	}while(1);


}

void create_system_monitor(void)
{
	xTaskCreate(system_monitor, "System_Monitor", configMINIMAL_STACK_SIZE + 256, NULL,
			SYSTEM_MONITOR_TASK_PRIO, &system_monitor_handle);

	system_monitor_timer_handle = xTimerCreate("system_monitor_timer",(60*1000), true, NULL, sytem_monitor_timer);

}
