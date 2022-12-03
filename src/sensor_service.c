/*
 * sensor_service.c
 *
 *  Created on: 1 dic 2022
 *      Author: Francisco
 */
#include "uiso.h"

#include "board_i2c_sensors.h"


#define SENSOR_TASK_PRIORITY    (UBaseType_t)( uiso_task_runtime_services )

TaskHandle_t sensor_task_handle = NULL;

//TimerHandle_t user_timer = NULL;
//TimerHandle_t clock_timer = NULL;
//SemaphoreHandle_t user_semaphore = NULL;

enum
{
	sensor_event_bme280 = (1 << 0),
	sensor_event_bma280 = (1 << 1),
	sensor_event_bmm150 = (1 << 2),
	sensor_event_bmg160 = (1 << 3),
	sensor_event_bmi160 = (1 << 4),

};

#define SENSOR_EVENT_TIMEOUT	(10*1000)

TimerHandle_t bme280_sampling_timer;

int initialize_bma280(void);

int initialize_bme280(void);

static void sensor_task(void *param);

void bme280_sampling_timer_callback( TimerHandle_t xTimer )
{
	xTaskNotify( sensor_task_handle, (uint32_t)sensor_event_bme280, eSetBits );
}

static void sensor_task(void *param)
{
	(void) param;

	struct bme280_data bme280_sensor_data;
	struct bma2_sensor_data bma280_sensor_data;
	struct bmm150_mag_data bmm150_sensor_data;



	initialize_bma280();

	initialize_bme280();

	xTimerStart( bme280_sampling_timer, portMAX_DELAY);

	do
	{
		uint32_t notification_value = 0;

		if(pdTRUE == xTaskNotifyWait(0, UINT32_MAX, &notification_value, SENSOR_EVENT_TIMEOUT))
		{
			if((uint32_t)sensor_event_bme280 == (notification_value & (uint32_t)sensor_event_bme280))
			{
				int ret = -1;
				uint32_t bme280_delay = bme280_cal_meas_delay(&(board_bme280.settings));

				ret = bme280_set_sensor_mode(BME280_FORCED_MODE, &board_bme280);

				vTaskDelay(pdMS_TO_TICKS(bme280_delay));

				ret = bme280_get_sensor_data(BME280_ALL, &bme280_sensor_data, &board_bme280);

						//printf("AX: %d, AY: %d, AZ: %d, MX: %d, MY: %d, MZ: %d, T: %d, P: %d, Rh: %d\n\r", bma280_sensor_data.x, bma280_sensor_data.y, bma280_sensor_data.z, bmm150_sensor_data.x, bmm150_sensor_data.y, bmm150_sensor_data.z, (int)bme280_sensor_data.temperature, (int)bme280_sensor_data.pressure, (int)bme280_sensor_data.humidity);
				printf("Sensor Event (!)\n\r");
			}
			if((uint32_t)sensor_event_bma280 == (notification_value & (uint32_t)sensor_event_bma280))
			{
				int ret = -1;
				ret = bma2_get_accel_data(&bma280_sensor_data, &board_bma280);


			}

		}
		else
		{
			// Timeout
		}
	}while(1);



}


void create_sensor_task(void)
{
	xTaskCreate(sensor_task, "Sensor Task", configMINIMAL_STACK_SIZE, NULL,
			SENSOR_TASK_PRIORITY, &sensor_task_handle);

	bme280_sampling_timer = xTimerCreate("sampling timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, bme280_sampling_timer_callback);
}


int initialize_bma280(void)
{
	int ret = -1;
	/* BMA280 Init */
	int8_t self_test_result = -1;

	board_bma280_enable();

	ret = bma2_init(&board_bma280);

	if(BMA2_OK == ret)
	{
		ret = bma2_soft_reset(&board_bma280);
	}

	if(BMA2_OK == ret)
	{
		ret = bma2_perform_accel_selftest(&self_test_result, &board_bma280);
	}

	if(BMA2_OK == ret)
	{
		ret = bma2_set_power_mode(BMA2_NORMAL_MODE , &board_bma280);
	}

	if(BMA2_OK == ret)
	{
		struct bma2_acc_conf bma2_config;

		ret = bma2_get_accel_conf(&bma2_config, &board_bma280);
		if(BMA2_OK == ret)
		{
			bma2_config.bw = BMA2_ACC_RANGE_4G;
		    bma2_config.range = BMA2_ACC_BW_125_HZ;
			bma2_config.shadow_dis = BMA2_SHADOWING_ENABLE;
		    bma2_config.data_high_bw = BMA2_FILTERED_DATA;

		    ret = bma2_set_accel_conf(&bma2_config, &board_bma280);
		}
	}

	return ret;
}


int initialize_bme280(void)
{
	int ret = -1;

	board_bme280_enable();

	ret = bme280_init(&board_bme280);

	if(BME280_OK == ret)
	{
		ret = bme280_set_sensor_mode(BME280_SLEEP_MODE, &board_bme280);
	}

	if(BME280_OK == ret)
	{
		ret = bme280_get_sensor_settings(&board_bme280);

		board_bme280.settings.filter = BME280_FILTER_COEFF_OFF;
		board_bme280.settings.standby_time = BME280_STANDBY_TIME_0_5_MS;
		board_bme280.settings.osr_h = BME280_OVERSAMPLING_16X;
		board_bme280.settings.osr_p = BME280_OVERSAMPLING_16X;
		board_bme280.settings.osr_t = BME280_OVERSAMPLING_16X;

		if(BME280_OK == ret)
		{
			ret = bme280_set_sensor_settings(BME280_ALL_SETTINGS_SEL, &board_bme280);
		}
	}

	return ret;
}
