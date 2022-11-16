/*
 * sd_card.c
 *
 *  Created on: 9 nov 2022
 *      Author: Francisco
 */

#include "board.h"

#include "ff.h"
#include "diskio.h"

/* OS resources */
#include "FreeRTOS.h"
#include "semphr.h"

#include "em_usart.h"
#include "sd_card.h"

enum SD_Card_Return_E {
	SD_CARD_SUCCESS = 0,
	SD_CARD_ERROR = 1,
	SD_CARD_NOT_INSERTED,
	SD_CARD_TIMEOUT
};

/* Detect SD Card */
static void card_detect_callback(uint8_t intNo);

/**
 * Poll if card is inserted
 */
static enum SD_Card_Return_E is_card_inserted(void);

/**
 * Wait for card to be ready
 */
static enum SD_Card_Return_E wait_for_card_to_be_ready(void);



static void card_detect_callback(uint8_t intNo)
{
	(void)intNo;
	if(0 == GPIO_PinInGet(SD_DETECT_PORT,SD_DETECT_PIN))
	{
		// SD Card inserted
		sl_led_turn_on(&led_yellow);
	}
	else
	{
		// Disconnect SD Card
		sl_led_turn_off(&led_yellow);
	}
}


void BOARD_SD_Card_Init(void)
{
    GPIO_PinModeSet(SD_CARD_CS_PORT, SD_CARD_CS_PIN, SD_CARD_CS_MODE, 1);
    GPIO_PinModeSet(SD_CARD_LS_PORT, SD_CARD_LS_PIN, SD_CARD_LS_MODE, 1);
    GPIO_PinModeSet(SD_DETECT_PORT, SD_DETECT_PIN, SD_DETECT_MODE, 1);
    GPIO_PinModeSet(SD_CARD_SPI1_MISO_PORT, SD_CARD_SPI1_MISO_PIN, SD_CARD_SPI1_MISO_MODE, 0);
    GPIO_PinModeSet(SD_CARD_SPI1_MOSI_PORT, SD_CARD_SPI1_MOSI_PIN, SD_CARD_SPI1_MOSI_MODE, 0);
    GPIO_PinModeSet(SD_CARD_SPI1_SCK_PORT, SD_CARD_SPI1_SCK_PIN, SD_CARD_SPI1_SCK_MODE, 0);

    GPIOINT_CallbackRegister(SD_DETECT_PIN, card_detect_callback);

    GPIO_ExtIntConfig(SD_DETECT_PORT,
    				  SD_DETECT_PIN,
					  SD_DETECT_PIN,
                      true,
                      true,
                      true);
}

// Mutex
static SemaphoreHandle_t tx_semaphore = NULL;
static SemaphoreHandle_t rx_semaphore = NULL;

void BOARD_SD_Card_Enable(void)
{
	if(tx_semaphore == NULL)
	{
		tx_semaphore = xQueueCreate( 1, sizeof(Ecode_t) );
	}

	if(rx_semaphore == NULL)
	{
		rx_semaphore = xQueueCreate( 1, sizeof(Ecode_t) );
	}

}

void BOARD_SD_Card_Disable(void)
{
	SPIDRV_DeInit(&sd_card_usart);

	GPIO_PinModeSet(SD_CARD_CS_PORT, SD_CARD_CS_PIN, gpioModeDisabled, 1);
	GPIO_PinModeSet(SD_CARD_LS_PORT, SD_CARD_CS_PIN, gpioModeDisabled, 1);
	GPIO_PinModeSet(SD_DETECT_PORT, SD_DETECT_PIN, gpioModeDisabled, 0);
	GPIO_PinModeSet(SD_CARD_SPI1_MISO_PORT, SD_CARD_SPI1_MISO_PIN, gpioModeDisabled, 0);
	GPIO_PinModeSet(SD_CARD_SPI1_MOSI_PORT, SD_CARD_SPI1_MOSI_PIN, gpioModeDisabled, 0);
	GPIO_PinModeSet(SD_CARD_SPI1_SCK_PORT, SD_CARD_SPI1_SCK_PIN, gpioModeDisabled,0);
}



void BOARD_SD_CARD_Deselect(void)
{
	GPIO_PinOutSet(SD_CARD_CS_PORT, SD_CARD_CS_PIN);
}

void BOARD_SD_CARD_Select(void)
{
	GPIO_PinOutClear(SD_CARD_CS_PORT, SD_CARD_CS_PIN);
}


void BOARD_SD_CARD_SetFastBaudrate(void)
{
	SPIDRV_SetBitrate(&sd_card_usart, BOARD_SD_CARD_BITRATE);
}

void BOARD_SD_CARD_SetSlowBaudrate(void)
{
	SPIDRV_SetBitrate(&sd_card_usart, BOARD_SD_CARD_WAKEUP_BITRATE);
}



static void sd_card_recieve_callback(struct SPIDRV_HandleData *handle,
                                  Ecode_t transferStatus,
                                  int itemsTransferred)
{
	(void)handle;
	(void)itemsTransferred;

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if(NULL != rx_semaphore) xQueueSendFromISR( rx_semaphore, &transferStatus, &xHigherPriorityTaskWoken );

	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

static void sd_card_send_callback(struct SPIDRV_HandleData *handle,
                                  Ecode_t transferStatus,
                                  int itemsTransferred)
{
	(void)handle;
	(void)itemsTransferred;

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if(NULL != tx_semaphore) xQueueSendFromISR( tx_semaphore, &transferStatus, &xHigherPriorityTaskWoken );

	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}


uint32_t BOARD_SD_CARD_Recieve(void * buffer, int count)
{
	Ecode_t ecode = ECODE_EMDRV_SPIDRV_PARAM_ERROR;
	if(rx_semaphore != NULL) // Change to check if scheduler is active
	{
		ecode = SPIDRV_MReceive(&sd_card_usart, buffer, count, sd_card_recieve_callback);
		if(ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			(void)xQueueReceive( rx_semaphore, &ecode, portMAX_DELAY);
		}
	}
	else
	{
		ecode = SPIDRV_MReceiveB(&sd_card_usart, buffer, count);
	}

	return (uint32_t)ecode;
}

uint32_t BOARD_SD_CARD_Send(const void * buffer, int count)
{
	Ecode_t ecode = ECODE_EMDRV_SPIDRV_PARAM_ERROR;
	if(tx_semaphore != NULL) // Change to check if scheduler is active
	{
		ecode = SPIDRV_MTransmit(&sd_card_usart, buffer, count, sd_card_send_callback);
		if(ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			(void)xQueueReceive( tx_semaphore, &ecode, portMAX_DELAY);
		}
	}
	else
	{
		ecode = SPIDRV_MTransmitB(&sd_card_usart, buffer, count);
	}

	return (uint32_t)ecode;
}

#if 0

static enum SD_Card_Return_E wait_for_card_to_be_ready(void)
{
	enum SD_Card_Return_E returnValue = SD_CARD_TIMEOUT;

	uint8_t rxValue = 0;

	/* Card should be available in 500ms*/
	for(uint32_t cycles = (uint32_t)WAIT_CYCLES; cycles>0; cycles--)
	{
		if(ECODE_EMDRV_SPIDRV_OK == SPIDRV_MTransferSingleItemB(&sd_card_usart, UINT32_C(0), &rxValue))
		{
			if(0xFF == rxValue)
			{
				return SD_CARD_SUCCESS;
			}
		}
		else
		{
			return SD_CARD_ERROR;
		}
	}

	return returnValue;
}

static enum SD_Card_Return_E is_card_inserted(void)
{
	enum SD_Card_Return_E value = SD_CARD_NOT_INSERTED;

	if(0 == GPIO_PinInGet(SD_DETECT_PORT,SD_DETECT_PIN))
	{
		value = SD_CARD_SUCCESS;
	}

	return value;
}

#endif




#include "sl_sleeptimer.h"
uint32_t get_fattime (void)
{
    sl_sleeptimer_date_t date;

    (void)sl_sleeptimer_get_datetime(&date);

    return (DWORD)(date.year - 80) << 25 |
           (DWORD)(date.month + 1) << 21 |
           (DWORD)date.month_day << 16 |
           (DWORD)date.hour << 11 |
           (DWORD)date.min << 5 |
           (DWORD)date.sec >> 1;
}

