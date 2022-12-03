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
extern void trigger_registration_update(void);

void user_timer_callback( TimerHandle_t pxTimer )
{
	(void)pxTimer;
	xTaskNotify(user_task_handle, (1<<0), eSetBits);
}

void clock_timer_callback( TimerHandle_t pxTimer )
{
	(void)pxTimer;
	xTaskNotify(user_task_handle, (1<<1), eSetBits);
}

#include "ff.h"

FATFS fs; /* File System pointer */

#include "board_i2c_sensors.h"
#include "bma2.h"



void user_task(void *param)
{
	(void) param;
    volatile int ret = 0;
	uint32_t ulNotifiedValue = 0;



	/* BMI 150 code */
	board_bmm150_enable();

	ret = bmm150_init(&board_bmm150);

	if(BMM150_OK == ret)
	{
		ret = bmm150_soft_reset(&board_bmm150);
	}

	if(BMM150_OK == ret)
	{
		ret = bmm150_perform_self_test(BMM150_SELF_TEST_NORMAL, &board_bmm150);
	}

	if(BMM150_OK == ret)
	{
		struct bmm150_settings settings;
		uint16_t desired_settings = BMM150_SEL_DATA_RATE | BMM150_SEL_CONTROL_MEASURE | BMM150_SEL_XY_REP | BMM150_SEL_Z_REP;
		ret = bmm150_get_sensor_settings(&settings, &board_bmm150);

		settings.data_rate = BMM150_DATA_RATE_20HZ;
		settings.preset_mode = BMM150_PRESETMODE_ENHANCED;
		settings.pwr_mode = BMM150_POWERMODE_NORMAL;
		settings.xy_rep = BMM150_REPXY_REGULAR;
		settings.xyz_axes_control = BMM150_XYZ_CHANNEL_ENABLE;
		settings.z_rep = BMM150_REPZ_ENHANCED;

		if(BMM150_OK == ret)
		{
			ret = bmm150_set_sensor_settings(desired_settings, &settings, &board_bmm150);
		}
		if(BMM150_OK == ret)
		{
			ret =  bmm150_set_op_mode(&settings, &board_bmm150);
		}
		if(BMM150_OK == ret)
		{
			ret =  bmm150_set_presetmode(&settings, &board_bmm150);
		}
	}




//	vTaskSuspend(NULL);



	do{



		ret = bmm150_read_mag_data(&bmm150_sensor_data, &board_bmm150);




		// lwm2m_client_task_runner(NULL);


	    vTaskDelay(1000);



	}while(1);

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

