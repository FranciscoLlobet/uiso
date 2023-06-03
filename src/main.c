/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include "diag/trace.h"
#include "timer.h"

// ----------------------------------------------------------------------------
//
// Print a greeting message on the trace device and enter a loop
// to count seconds.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the ITM output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace-impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//
// ----------------------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

/* emdrv */
#include "board.h"
#include "uiso.h"


/* Silicon Labs Drivers */
#include "sl_iostream_debug.h"
#include "sl_debug_swo.h"
#include "sl_iostream_swo.h"
#include "sl_sleeptimer.h"
#include "sl_power_manager.h"
#include "dmadrv.h"


/* Silicon Labs Device Initializations */
#include "sl_device_init_emu.h"
#include "sl_device_init_nvic.h"
#include "sl_device_init_hfxo.h"
#include "sl_device_init_hfrco.h"
#include "sl_device_init_lfxo.h"

#define JSMN_STATIC
#include "uiso_config.h"
//#include "ff.h"
//#include "diskio.h"

#include "simplelink.h"
#include "mbedtls/ssl.h"

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	CHIP_Init();
	MSC_Init();

	sl_device_init_nvic();
	sl_device_init_hfxo();
	sl_device_init_hfrco();
	sl_device_init_lfxo();
	sl_device_init_emu();

	NVIC_SetPriorityGrouping((uint32_t) 3);/* Set priority grouping to group 4*/

	/* Set core NVIC priorities */
	NVIC_SetPriority(SVCall_IRQn, 0);
	NVIC_SetPriority(DebugMonitor_IRQn, 0);
	NVIC_SetPriority(PendSV_IRQn, 0);
	NVIC_SetPriority(SysTick_IRQn, 0);

	CMU_OscillatorEnable(cmuOsc_HFXO, true, true);
	CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
	CMU_OscillatorEnable(cmuOsc_HFRCO, false, false);
	CMU_ClockEnable(cmuClock_HFPER, true);
	CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
	CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
	CMU_ClockEnable(cmuClock_HFLE, true);

	/* ENABLE SWO */
	sl_debug_swo_init();

	sl_iostream_swo_init(); // Send printf to swo

	/* */
	CMU_ClockEnable(cmuClock_GPIO, true);

	{
		/* Setup Interrupts */
		NVIC_SetPriority(GPIO_ODD_IRQn, 5);
		NVIC_SetPriority(GPIO_EVEN_IRQn, 5);

		GPIOINT_Init();
	}

	/* Wifi NRSET port */
	GPIO_PinModeSet(gpioPortA, 15, gpioModeWiredAnd, 0);

	/* Enable 2v5 */
	GPIO_PinModeSet(gpioPortF, 5, gpioModeWiredOr, 0);

	/* Enable 3v3 supply */
	GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 0);

	GPIO_DriveModeSet(gpioPortE, gpioDriveModeLow);
	GPIO_DriveModeSet(gpioPortD, gpioDriveModeHigh);

	GPIO_PinOutClear(PWR_2V5_SNOOZE_PORT, PWR_2V5_SNOOZE_PIN);
	GPIO_PinOutSet(PWR_3V3_EN_PORT, PWR_3V3_EN_PIN);

	/* LED Set Group */
	sl_led_init(&led_red);
	sl_led_init(&led_orange);
	sl_led_init(&led_yellow);

	/* BUTTON Set Group */
	sl_button_init(&button1);
	sl_button_init(&button2);

	DMADRV_Init();

	/* Initialize SPI peripherals */
	BOARD_SD_Card_Init();
	Board_CC3100_Init();

	/* Initialize I2C peripheral */
	board_i2c_init();


	SysTick_Config(SystemCoreClock / TIMER_FREQUENCY_HZ);

	/* Start Services */
	sl_power_manager_init();

	sl_sleeptimer_date_t date;

	(void)sl_sleeptimer_build_datetime(
			&date,
			(2022 - 1900), /* DEFAULT YEAR */
            MONTH_NOVEMBER, /* DEFAULT MONTH */
			11, /* DEFAULT DAY */
            10, /* DEFAULT HOUR */
            0, /* DEFAULT MINUTES */
            0, /* DEFAULT SECONDS */
            1); /* DEFAULT TZ */

	(void)sl_sleeptimer_set_datetime(&date);

	//create_user_task();
	sl_iostream_printf(sl_iostream_swo_handle, "Starting FreeRTOS\n\r");

	create_user_task(); /* Create LWM2M task */

	initialize_mqtt_client(); /* Create MQTT task */

	create_wifi_service_task();

	create_network_mediator();

	create_sensor_task();

	VStartSimpleLinkSpawnTask((unsigned portBASE_TYPE)uiso_task_connectivity_service);

	create_system_monitor();

	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook( void )
{
	// Tick Hook
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
	NVIC_SystemReset();
	 asm volatile ("bkpt 0");
}

void vApplicationMallocFailedHook(void)
{
	NVIC_SystemReset();
	 asm volatile ("bkpt 0");
}

void vApplicationDaemonTaskStartupHook(void)
{
	sl_iostream_printf(sl_iostream_swo_handle, "Started FreeRTOS");

	uiso_load_config();

}

void vApplicationIdleHook( void )
{
	// Application Idle Hook
}


void __attribute__ ((section(".after_vectors")))
SysTick_Handler(void)
{
	if(taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
	{
		xPortSysTickHandler();
	}
}

static void _perform_reset(void* param1, uint32_t param2)
{
	(void)param1;
	(void)param2;

	taskENTER_CRITICAL();

	CHIP_Reset();

	taskEXIT_CRITICAL();
}

void uiso_reset(void)
{
	 (void)xTimerPendFunctionCall(_perform_reset, NULL, 0, portMAX_DELAY);
}


#include <sys/time.h>
extern int _gettimeofday(struct timeval* ptimeval, void * ptimezone);
/* implementing system time */
int _gettimeofday(struct timeval* ptimeval, void * ptimezone)
{
	struct timezone * timezone_ptr = (struct timezone * )ptimezone;

	if(NULL != ptimeval)
	{
		ptimeval->tv_sec = sl_sleeptimer_get_time();
		ptimeval->tv_usec = 0;
	}

	if(NULL != timezone_ptr)
	{
		timezone_ptr->tz_dsttime = 0;
		timezone_ptr->tz_minuteswest = 0;
	}

	return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
