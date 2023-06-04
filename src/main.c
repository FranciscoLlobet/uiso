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

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

/* emdrv */
#include "board.h"
#include "uiso.h"

#define JSMN_STATIC
#include "uiso_config.h"

#include "simplelink.h"
#include "mbedtls/ssl.h"

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	BOARD_Init();

	//create_user_task();
	sl_iostream_printf(sl_iostream_swo_handle, "Starting FreeRTOS\n\r");

	create_user_task(); /* Create LWM2M task */

	initialize_mqtt_client(); /* Create MQTT task */

	create_wifi_service_task();

	create_network_mediator();

	create_sensor_task();

	VStartSimpleLinkSpawnTask((unsigned portBASE_TYPE)uiso_task_connectivity_service);

	create_system_monitor();

	BOARD_Watchdog_Enable();

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

	sl_led_turn_on(&led_red);

	BOARD_msDelay(500);

	sl_led_turn_on(&led_orange);

	BOARD_msDelay(500);

	sl_led_turn_on(&led_yellow);

	BOARD_msDelay(500);

	sl_led_turn_off(&led_red);

	BOARD_msDelay(500);

	sl_led_turn_off(&led_orange);

	BOARD_msDelay(500);

	sl_led_turn_off(&led_yellow);
}

void vApplicationIdleHook( void )
{
	BOARD_Watchdog_Feed();
}


extern void xPortSysTickHandler( void );

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

	sl_led_turn_on(&led_red);
	sl_led_turn_on(&led_yellow);
	sl_led_turn_on(&led_orange);

	/* Here we are currently in the Timer service routine. */

	// ... Do some Board deinitialization things...


	/* Enter the critical section */
	taskENTER_CRITICAL();

	BOARD_MCU_Reset();

	taskEXIT_CRITICAL(); /* Unreachable */
}

void uiso_reset(void)
{
	 if(pdTRUE != xTimerPendFunctionCall(_perform_reset, NULL, 0, portMAX_DELAY))
	 {
		 // If the reset callback cannot be enqueued, then the reset shall be performed immediately.
		 taskENTER_CRITICAL();

		 BOARD_MCU_Reset();

		 taskEXIT_CRITICAL();
	 }
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
