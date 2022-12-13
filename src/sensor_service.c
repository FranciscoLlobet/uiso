/*
 * sensor_service.c
 *
 *  Created on: 1 dic 2022
 *      Author: Francisco
 */
#include <lwm2m_client.h>
#include "uiso.h"
#include "sensor_statistics.h"
#include "board_i2c_sensors.h"
#include "gpiointerrupt.h"


#define SENSOR_TASK_PRIORITY    (UBaseType_t)( uiso_task_runtime_services )

TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t bme280_task_handle = NULL;

TimerHandle_t bme280_sampling_timer;
TimerHandle_t bma280_timer;

//TimerHandle_t user_timer = NULL;
//TimerHandle_t clock_timer = NULL;
//SemaphoreHandle_t user_semaphore = NULL;

/* 32 Frames at 3 Axis with 2 Bytes per  axis */
static uint8_t bma280_fifo_buffer[BMA2_FIFO_BUFFER] = { 0 };

struct bma2_sensor_data bma280_accelerometer_data[BMA2_FIFO_BUFFER / BMA2_FIFO_XYZ_AXIS_FRAME_SIZE];

enum
{
	sensor_event_bme280 = (1 << 0),
	sensor_event_bma280_int1 = (1 << 1),
	sensor_event_bma280_int2 = (1 << 2),
	sensor_event_bma280_timer = (1 << 3),
	sensor_event_bmm150 = (1 << 4),
	sensor_event_bmg160 = (1 << 5),
	sensor_event_bmi160 = (1 << 6),

};

#define SENSOR_EVENT_TIMEOUT	(10*1000) /* BMA280 No-Event Timeout */



int initialize_bma280(void);

int initialize_bme280(void);

static void sensor_task(void *param);

static void bme280_sampling_timer_callback(TimerHandle_t xTimer)
{
	(void) pvTimerGetTimerID(xTimer);

	xTaskNotify(bme280_task_handle, (uint32_t )sensor_event_bme280, eSetBits);
}

static void bma280_timer_callback(TimerHandle_t xTimer)
{
	(void) pvTimerGetTimerID(xTimer);

	xTaskNotify(sensor_task_handle, (uint32_t )sensor_event_bma280_timer, eSetBits);
}


static void bma280_int1_callback(uint8_t intNo)
{
	(void) intNo;

	BaseType_t higherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(sensor_task_handle, (uint32_t )sensor_event_bma280_int1,
			eSetBits, &higherPriorityTaskWoken);

	portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void bme280_task(void *param)
{
	(void) param;
	struct bme280_data bme280_sensor_data;

	initialize_bme280();

	xTimerStart(bme280_sampling_timer, portMAX_DELAY);

	do
	{
		uint32_t notification_value = 0;

		if (pdTRUE
				== xTaskNotifyWait(0, UINT32_MAX, &notification_value,
						SENSOR_EVENT_TIMEOUT))
		{
			if ((uint32_t) sensor_event_bme280
					== (notification_value & (uint32_t) sensor_event_bme280))
			{
				int ret = -1;
				uint32_t bme280_delay = bme280_cal_meas_delay(
						&(board_bme280.settings));

				ret = bme280_set_sensor_mode(BME280_FORCED_MODE, &board_bme280);

				vTaskDelay(pdMS_TO_TICKS(bme280_delay));

				ret = bme280_get_sensor_data(BME280_ALL, &bme280_sensor_data,
						&board_bme280);

				printf("T: %d, P: %d, Rh: %d\n\r",
						(int) bme280_sensor_data.temperature,
						(int) bme280_sensor_data.pressure,
						(int) bme280_sensor_data.humidity);

				lwm2m_client_update_temperature((float)bme280_sensor_data.temperature);
			}
		}
	} while (1);

}

static void sensor_task(void *param)
{
	(void) param;

	uint32_t count = 0;
	struct bma2_sensor_data bma280_sensor_data;
	struct bmm150_mag_data bmm150_sensor_data;

	struct bma2_fifo_frame bma280_fifo;
	struct accelerometer_statistics_s accelerometer_statistics;

	initialize_bma280();

	xTimerStart(bma280_timer, portMAX_DELAY);

	do
	{
		int ret = -1;
		uint32_t notification_value = 0;

		if (pdTRUE
				== xTaskNotifyWait(0, UINT32_MAX, &notification_value,
						SENSOR_EVENT_TIMEOUT))
		{

			if (notification_value & (uint32_t) sensor_event_bma280_int1)
			{
				struct bma2_int_status interrupt_status;

				ret = bma2_get_int_status(&interrupt_status, &board_bma280);
				if (BMA2_OK == ret)
				{
					if (interrupt_status.int_status_1
							& BMA2_INT_1_ASSERTED_FIFO_FULL)
					{
						uint16_t accelerometer_values = 0;
						bma280_fifo.data = &(bma280_fifo_buffer[0]);
						bma280_fifo.length = sizeof(bma280_fifo_buffer);

						ret = bma2_read_fifo_data(&bma280_fifo, &board_bma280);

						if(BMA2_OK == ret)
						{
							ret = bma2_extract_accel(&(bma280_accelerometer_data[0]), &accelerometer_values, &bma280_fifo);
						}
						if(BMA2_OK == ret)
						{
							mean_and_variance(&accelerometer_statistics, &(bma280_accelerometer_data[0]), accelerometer_values);
							printf("X:%d, Y:%d, Z:%d\r\n", accelerometer_statistics.mean.x,accelerometer_statistics.mean.y, accelerometer_statistics.mean.z);
							lwm2m_client_update_accel(accelerometer_statistics.mean.x, accelerometer_statistics.mean.y, accelerometer_statistics.mean.z);
						}
					}
					else
					{
						// Other interrupt reasons
					}
				}

				if(BMA2_OK != ret)
				{
					// Error
				}
			}

			if (notification_value & (uint32_t) sensor_event_bma280_timer)
			{
				int8_t temp_val = 0;
				ret = bma2_get_temperature_data((uint8_t *)&temp_val, &board_bma280);
				if(BMA2_OK == ret)
				{
					printf("BMA280 T: %d\n\r", temp_val/2 + 23);
				}

				if(BMA2_OK != ret)
				{
					// Error
				}
			}



		}
		else
		{
			// Timeout
		}

	} while (1);

}

void create_sensor_task(void)
{
	xTaskCreate(sensor_task, "Sensor Task", configMINIMAL_STACK_SIZE, NULL,
			uiso_task_housekeeping_services, &sensor_task_handle);

	xTaskCreate(bme280_task, "Temp Collector", configMINIMAL_STACK_SIZE,
			NULL,
			uiso_task_housekeeping_services, &bme280_task_handle);

	bme280_sampling_timer = xTimerCreate("sampling timer", pdMS_TO_TICKS(1000),
			pdTRUE, NULL, bme280_sampling_timer_callback);

	bma280_timer = xTimerCreate("bma280 timer", pdMS_TO_TICKS(1000),
			pdTRUE, NULL, bma280_timer_callback);
}

int initialize_bma280(void)
{
	int ret = -1;
	/* BMA280 Init */
	int8_t self_test_result = -1;

	board_bma280_enable();

	ret = bma2_init(&board_bma280);

	if (BMA2_OK == ret)
	{
		ret = bma2_soft_reset(&board_bma280);
	}

	if (BMA2_OK == ret)
	{
		ret = bma2_perform_accel_selftest(&self_test_result, &board_bma280);
	}

	if (BMA2_OK == ret)
	{
		ret = bma2_set_power_mode(BMA2_STANDBY_MODE, &board_bma280);
	}

	if (BMA2_OK == ret)
	{
		struct bma2_acc_conf bma2_config;

		ret = bma2_get_accel_conf(&bma2_config, &board_bma280);
		if (BMA2_OK == ret)
		{
			/**
			 * Configuration is two FIFO per second
			 */
			bma2_config.bw = BMA2_ACC_BW_62_5_HZ; //BMA2_ACC_BW_31_25_HZ;
			bma2_config.range = BMA2_ACC_RANGE_4G;
			bma2_config.shadow_dis = BMA2_SHADOWING_ENABLE;
			bma2_config.data_high_bw = BMA2_FILTERED_DATA;

			ret = bma2_set_accel_conf(&bma2_config, &board_bma280);
		}
	}

	if (BMA2_OK == ret)
	{
		(void) bma2_int_rst_latch(BMA2_RESET_LATCHED_INT, &board_bma280);
	}

	/* Configure Interrupts*/
	if (BMA2_OK == ret)
	{

		struct bma2_int_pin interrupt_pin_config;

		if (BMA2_OK == ret)
		{
			ret = bma2_get_int_out_ctrl(&interrupt_pin_config, &board_bma280);
		}

		if (BMA2_OK == ret)
		{
			interrupt_pin_config.int1_lvl = BMA2_ACTIVE_LOW;
			interrupt_pin_config.int1_od = BMA2_PUSH_PULL;
			interrupt_pin_config.latch_int = BMA2_NON_LATCHED;

			ret = bma2_set_int_out_ctrl(&interrupt_pin_config, &board_bma280);
		}

		if (BMA2_OK == ret)
		{
			ret = bma2_set_int_mapping(BMA2_INT_MAP, BMA2_INT1_MAP_FIFO_FULL,
					&board_bma280);
		}

		if (BMA2_OK == ret)
		{
			GPIOINT_CallbackRegister(BMA280_INT1_PIN, bma280_int1_callback);
		}

		if (BMA2_OK == ret)
		{
			GPIO_ExtIntConfig(BMA280_INT1_PORT,
			BMA280_INT1_PIN,
			BMA280_INT1_PIN,
			false,
			true,
			true);
		}

		if (BMA2_OK == ret)
		{
			uint32_t enabled_interrupts = 0;
			(void) bma2_get_enabled_interrupts(&enabled_interrupts,
					&board_bma280);

			enabled_interrupts = BMA2_INT_EN_FIFO_FULL;

			ret = bma2_enable_interrupt(enabled_interrupts, &board_bma280);
		}
	}

	if (BMA2_OK == ret)
	{
		ret = bma2_set_power_mode(BMA2_NORMAL_MODE, &board_bma280);
	}

	/* Configure FIFO */
	if (BMA2_OK == ret)
	{
		struct bma2_fifo_frame fifo_config;

		(void) bma2_get_fifo_config(&fifo_config, &board_bma280);

		fifo_config.length = BMA2_FIFO_BUFFER;
		fifo_config.fifo_data_select = BMA2_XYZ_AXES;
		fifo_config.fifo_mode_select = BMA2_MODE_FIFO;

		ret = bma2_set_fifo_config(&fifo_config, &board_bma280);
	}

	return ret;
}

int initialize_bme280(void)
{
	int ret = -1;

	board_bme280_enable();

	ret = bme280_init(&board_bme280);

	if (BME280_OK == ret)
	{
		ret = bme280_set_sensor_mode(BME280_SLEEP_MODE, &board_bme280);
	}

	if (BME280_OK == ret)
	{
		ret = bme280_get_sensor_settings(&board_bme280);

		board_bme280.settings.filter = BME280_FILTER_COEFF_OFF;
		board_bme280.settings.standby_time = BME280_STANDBY_TIME_0_5_MS;
		board_bme280.settings.osr_h = BME280_OVERSAMPLING_16X;
		board_bme280.settings.osr_p = BME280_OVERSAMPLING_16X;
		board_bme280.settings.osr_t = BME280_OVERSAMPLING_16X;

		if (BME280_OK == ret)
		{
			ret = bme280_set_sensor_settings(BME280_ALL_SETTINGS_SEL,
					&board_bme280);
		}
	}

	return ret;
}
