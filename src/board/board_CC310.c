/*
 * Board_CC310.c
 *
 *  Created on: 11 nov 2022
 *      Author: Francisco
 */

#include "board.h"
#include "simplelink.h"

#include "FreeRTOS.h"
#include "semphr.h"

/* Convert to struct ? */
static SemaphoreHandle_t tx_semaphore = NULL;
static SemaphoreHandle_t rx_semaphore = NULL;

SPIDRV_HandleData_t cc3100_usart;

SPIDRV_Init_t cc3100_usart_init_data =
{ .port = WIFI_SERIAL_PORT, .portLocation = _USART_ROUTE_LOCATION_LOC0,
		.bitRate = WIFI_SPI_BAUDRATE, .frameLength = 8, .dummyTxValue = 0xFF,
		.type = spidrvMaster, .bitOrder = spidrvBitOrderMsbFirst, .clockMode =
				spidrvClockMode0, .csControl = spidrvCsControlApplication,
		.slaveStartMode = spidrvSlaveStartImmediate,

};

struct transfer_status_s
{
	//struct SPIDRV_HandleData_t * handle;
	Ecode_t transferStatus; // status
	int itemsTransferred; // items_transferred
};

static volatile P_EVENT_HANDLER interrupt_handler_callback = NULL;
static void *interrupt_handler_pValue = NULL;

static void cc3100_interrupt_callback(uint8_t intNo)
{
	(void) intNo; // Validate interrupt ?

	if (NULL != interrupt_handler_callback)
	{
		interrupt_handler_callback(interrupt_handler_pValue);
	}
}

static void recieve_callback(struct SPIDRV_HandleData *handle,
		Ecode_t transferStatus, int itemsTransferred)
{
	(void) handle; // Validate handle ?
	struct transfer_status_s transfer_status_information =
	{ transferStatus, itemsTransferred };

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if (NULL != rx_semaphore)
	{
		xQueueSendFromISR(rx_semaphore, &transfer_status_information,
				&xHigherPriorityTaskWoken);

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}

}

static void send_callback(struct SPIDRV_HandleData *handle,
		Ecode_t transferStatus, int itemsTransferred)
{
	(void) handle; // Validate handle ?
	struct transfer_status_s transfer_status_information =
	{ transferStatus, itemsTransferred };

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if (NULL != tx_semaphore)
	{
		xQueueSendFromISR(tx_semaphore, &transfer_status_information,
				&xHigherPriorityTaskWoken);

		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

static void cc3100_spi_select(void)
{
	GPIO_PinOutClear(WIFI_CSN_PORT, WIFI_CSN_PIN);
}

static void cc3100_spi_deselect(void)
{
	GPIO_PinOutSet(WIFI_CSN_PORT, WIFI_CSN_PIN);
}

void Board_CC3100_Init(void)
{
	GPIO_PinModeSet(VDD_WIFI_EN_PORT, VDD_WIFI_EN_PIN, VDD_WIFI_EN_MODE, 1);
	GPIO_PinModeSet(WIFI_NRESET_PORT, WIFI_NRESET_PIN, WIFI_NRESET_MODE, 1);
	GPIO_PinModeSet(WIFI_CSN_PORT, WIFI_CSN_PIN, WIFI_CSN_MODE, 1);
	GPIO_PinModeSet(WIFI_INT_PORT, WIFI_INT_PIN, WIFI_INT_MODE, 0);
	GPIO_PinModeSet(WIFI_NHIB_PORT, WIFI_NHIB_PIN, WIFI_NHIB_MODE, 1);
	GPIO_PinModeSet(WIFI_SPI0_MISO_PORT, WIFI_SPI0_MISO_PIN,
	WIFI_SPI0_MISO_MODE, 0);
	GPIO_PinModeSet(WIFI_SPI0_MOSI_PORT, WIFI_SPI0_MOSI_PIN,
	WIFI_SPI0_MOSI_MODE, 0);
	GPIO_PinModeSet(WIFI_SPI0_SCK_PORT, WIFI_SPI0_SCK_PIN, WIFI_SPI0_SCK_MODE,
			0);

	GPIOINT_CallbackRegister(WIFI_INT_PIN, cc3100_interrupt_callback);

	GPIO_ExtIntConfig(WIFI_INT_PORT,
	WIFI_INT_PIN,
	WIFI_INT_PIN,
	true,
	true,
	false);
}

void CC3100_DeviceEnablePreamble(void)
{
	GPIO_PinOutClear(VDD_WIFI_EN_PORT, VDD_WIFI_EN_PIN);
	GPIO_PinOutSet(WIFI_NHIB_PORT, WIFI_NHIB_PIN);
	BOARD_msDelay(WIFI_SUPPLY_SETTING_DELAY_MS);

	GPIO_PinOutClear(WIFI_NRESET_PORT, WIFI_NRESET_PIN);
	BOARD_msDelay(WIFI_MIN_RESET_DELAY_MS);
	GPIO_PinOutSet(WIFI_NRESET_PORT, WIFI_NRESET_PIN);
	BOARD_msDelay(WIFI_PWRON_HW_WAKEUP_DELAY_MS);
	BOARD_msDelay(WIFI_INIT_DELAY_MS);
}

void CC3100_DeviceEnable(void)
{
	if (tx_semaphore == NULL)
	{
		tx_semaphore = xQueueCreate(1, sizeof(struct transfer_status_s));
	}

	if (rx_semaphore == NULL)
	{
		rx_semaphore = xQueueCreate(1, sizeof(struct transfer_status_s));
	}

	GPIO_PinOutSet(WIFI_NHIB_PORT, WIFI_NHIB_PIN);
	GPIO_PinModeSet(WIFI_INT_PORT, WIFI_INT_PIN, WIFI_INT_MODE, 0);
	GPIO_ExtIntConfig(WIFI_INT_PORT,
		WIFI_INT_PIN,
		WIFI_INT_PIN,
		true,
		false,
		true);
}

void CC3100_DeviceDisable(void)
{
	GPIO_PinOutClear(WIFI_NHIB_PORT, WIFI_NHIB_PIN);
	GPIO_ExtIntConfig(WIFI_INT_PORT,
		WIFI_INT_PIN,
		WIFI_INT_PIN,
		true,
		false,
		false);
	BOARD_msDelay(1); // Very important (!)
//	GPIO_PinModeSet(VDD_WIFI_EN_PORT, VDD_WIFI_EN_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_CSN_PORT, WIFI_CSN_PIN, gpioModeDisabled, 0);
	GPIO_PinModeSet(WIFI_INT_PORT, WIFI_INT_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_NHIB_PORT, WIFI_NHIB_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_NRESET_PORT, WIFI_NRESET_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_SPI0_MISO_PORT, WIFI_SPI0_MISO_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_SPI0_MOSI_PORT, WIFI_SPI0_MOSI_PIN, gpioModeDisabled, 0);
//	GPIO_PinModeSet(WIFI_SPI0_SCK_PORT, WIFI_SPI0_SCK_PIN, gpioModeDisabled, 0);
}

int CC3100_IfOpen(char *pIfName, unsigned long flags)
{
	(void)pIfName;
	(void)flags;
	// Success: 0
	// Failure: -1

	SPIDRV_Init(&cc3100_usart, &cc3100_usart_init_data);

	GPIO_PinOutSet(WIFI_NHIB_PORT, WIFI_NHIB_PIN); // Clear Hybernate
	BOARD_msDelay(50);
	cc3100_spi_deselect();
	return (int) 0;
}

int CC3100_IfClose(Fd_t Fd)
{
	(void)Fd;
	// Success: 0
	// Failure; -1
	cc3100_spi_deselect();
	SPIDRV_DeInit(&cc3100_usart);
	return 0;
}

int CC3100_IfRead(Fd_t Fd, uint8_t *pBuff, int Len)
{
	(void) Fd;
	int retVal = -1;
	struct transfer_status_s transfer_status =
	{ ECODE_EMDRV_SPIDRV_PARAM_ERROR, 0 };
	Ecode_t ecode = ECODE_EMDRV_SPIDRV_PARAM_ERROR;

	cc3100_spi_select();

	if (rx_semaphore != NULL) // Change to check if scheduler is active
	{
		ecode = SPIDRV_MReceive(&cc3100_usart, pBuff, Len, recieve_callback);
		if (ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			(void) xQueueReceive(rx_semaphore, &transfer_status, portMAX_DELAY);
			if (ECODE_EMDRV_SPIDRV_OK == transfer_status.transferStatus)
			{
				retVal = transfer_status.itemsTransferred;
			}
			else
			{
				retVal = -1;
			}
		}
	}
	else
	{
		ecode = SPIDRV_MReceiveB(&cc3100_usart, pBuff, Len);
		if (ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			retVal = Len;
		}
	}

	cc3100_spi_deselect();
	return retVal;

}

int CC3100_IfWrite(Fd_t Fd, uint8_t *pBuff, int Len)
{
	(void) Fd;
	int retVal = -1;
	struct transfer_status_s transfer_status =
	{ ECODE_EMDRV_SPIDRV_PARAM_ERROR, 0 };
	Ecode_t ecode = ECODE_EMDRV_SPIDRV_PARAM_ERROR;

	cc3100_spi_select();

	if (rx_semaphore != NULL) // Change to check if scheduler is active
	{
		ecode = SPIDRV_MTransmit(&cc3100_usart, pBuff, Len, send_callback);
		if (ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			(void) xQueueReceive(tx_semaphore, &transfer_status, portMAX_DELAY);
			if (ECODE_EMDRV_SPIDRV_OK == transfer_status.transferStatus)
			{
				retVal = transfer_status.itemsTransferred;
			}
			else
			{
				retVal = -1;
			}
		}
	}
	else
	{
		ecode = SPIDRV_MTransmitB(&cc3100_usart, pBuff, Len);
		if (ECODE_EMDRV_SPIDRV_OK == ecode)
		{
			retVal = Len;
		}
	}

	cc3100_spi_deselect();
	return retVal;
}

void CC3100_IfRegIntHdlr(P_EVENT_HANDLER interruptHdl, void *pValue)
{
	interrupt_handler_callback = interruptHdl;
	interrupt_handler_pValue = pValue;
}

CC3100_MaskIntHdlr(void)
{
	;
}

CC3100_UnmaskIntHdlr(void)
{
	;
}

/* General Event Handler */
void CC3100_GeneralEvtHdlr(SlDeviceEvent_t *slGeneralEvent)
{
	switch (slGeneralEvent->Event)
	{
	case SL_DEVICE_GENERAL_ERROR_EVENT:
		// General Error
	case SL_DEVICE_ABORT_ERROR_EVENT:
	case SL_DEVICE_DRIVER_ASSERT_ERROR_EVENT:
	case SL_DEVICE_DRIVER_TIMEOUT_CMD_COMPLETE:
	case SL_DEVICE_DRIVER_TIMEOUT_SYNC_PATTERN:
	case SL_DEVICE_DRIVER_TIMEOUT_ASYNC_EVENT:
	default:
		break;
	}

}


void CC3100_HttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent,
		SlHttpServerResponse_t *pSlHttpServerResponse)
{
	;
}
void CC3100_SockEvtHdlr(SlSockEvent_t *pSlSockEvent)
{
	switch (pSlSockEvent->Event)
	{
	case SL_SOCKET_TX_FAILED_EVENT:
		printf("tx failed\r\n");
	case SL_SOCKET_ASYNC_EVENT:
		printf("socket async event\r\n");
	default:
		break;
	}
}
