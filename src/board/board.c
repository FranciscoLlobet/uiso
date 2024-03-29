/*
 * board.c
 *
 *  Created on: 8 nov 2022
 *      Author: Francisco
 */
#include "FreeRTOS.h"
#include "task.h"

#include "board.h"

#include "timer.h"

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

void BOARD_SysTick_Enable(void)
{
	SysTick_Config(SystemCoreClock / BOARD_SYSTICK_FREQUENCY);
}

void BOARD_SysTick_Disable(void)
{
	SysTick->CTRL = (SysTick_CTRL_CLKSOURCE_Msk | (0 & SysTick_CTRL_TICKINT_Msk)
			| (0 & SysTick_CTRL_ENABLE_Msk));

	NVIC_ClearPendingIRQ(SysTick_IRQn);
}

void BOARD_usDelay(uint32_t delay_in_us)
{
	sl_udelay_wait((unsigned) delay_in_us);
}


void BOARD_msDelay(uint32_t delay_in_ms)
{
	if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState())
	{
		vTaskDelay(delay_in_ms);
	}
	else
	{
		sl_sleeptimer_delay_millisecond(delay_in_ms);
	}

}

void BOARD_Init(void)
{
	CHIP_Init();
	MSC_Init();

	/* Initialize mcu peripherals */
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

	CMU_ClockEnable(cmuClock_GPIO, true);


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

	sl_led_turn_off(&led_red);
	sl_led_turn_off(&led_orange);
	sl_led_turn_off(&led_yellow);

	/* BUTTON Set Group */
	sl_button_init(&button1);
	sl_button_init(&button2);

	DMADRV_Init();

	/* Initialize SPI peripherals */
	BOARD_SD_Card_Init();
	Board_CC3100_Init();

	/* Initialize I2C peripheral */
	board_i2c_init();

	BOARD_Watchdog_Init();

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
}


void BOARD_MCU_Reset(void)
{
	/* Disable the IRQs that may interfere now */
	__disable_irq();

	/* Data Synch Barrier */
	__DSB();

	/* Instruction Synch Barrier */
	__ISB();

	/* Perform Chip Reset*/
	CHIP_Reset();
}
