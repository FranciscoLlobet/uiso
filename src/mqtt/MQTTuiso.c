/*
 * MQTTuiso.c
 *
 *  Created on: 1 abr 2023
 *      Author: Francisco
 */

#include "MQTTuiso.h"
#include "../wifi_service.h"

static int uiso_mqtt_read(Network *n, unsigned char *buffer, int len, int timeout_ms);
static int uiso_mqtt_write(Network *n, unsigned char *buffer, int len, int timeout_ms);
static void uiso_mqtt_disconnect(Network *n);

TaskHandle_t mqtt_task = NULL;

void TimerInit(Timer *timer)
{
	if (NULL != timer)
	{
		timer->xTicksToWait = 0;
		memset(&timer->xTimeOut, '\0', sizeof(timer->xTimeOut));
	} else
	{
		/* error */
	}
}

char TimerIsExpired(Timer *timer)
{
	return xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait) == pdTRUE;
}

void TimerCountdownMS(Timer *timer, unsigned int timeout_ms)
{
	if (NULL != timer)
	{
		timer->xTicksToWait = timeout_ms / portTICK_PERIOD_MS; /* convert milliseconds to ticks */
		vTaskSetTimeOutState(&timer->xTimeOut); /* Record the time at which this function was entered. */
	} else
	{
		/* error */
	}

}

void TimerCountdown(Timer *timer, unsigned int timeout_s)
{
	if (NULL != timer)
	{
		TimerCountdownMS(timer, timeout_s * 1000);
	} else
	{
		/* error */
	}
}

int TimerLeftMS(Timer *timer)
{
	xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait); /* updates xTicksToWait to the number left */
	return (timer->xTicksToWait < 0) ? 0 : (timer->xTicksToWait * portTICK_PERIOD_MS);
}

void MutexInit(Mutex *mutex)
{
	if (NULL != mutex)
	{
		mutex->sem = xSemaphoreCreateMutex();
		if (NULL == mutex->sem)
		{
			// error
		}
	}
}

int MutexLock(Mutex *mutex)
{
	if (NULL != mutex)
	{
		return xSemaphoreTake(mutex->sem, portMAX_DELAY);
	}

	return pdFALSE;
}

int MutexUnlock(Mutex *mutex)
{
	if (NULL != mutex)
	{
		return xSemaphoreGive(mutex->sem);
	}

	return pdFALSE;
}

int ThreadStart(Thread *thread, void (*fn)(void*), void *arg)
{
	int rc = 0;
	uint16_t usTaskStackSize = (configMINIMAL_STACK_SIZE * 5);
	UBaseType_t uxTaskPriority = uxTaskPriorityGet(NULL); /* set the priority as the same as the calling task*/

	rc = xTaskCreate(fn, /* The function that implements the task. */
	"MQTT_Task", /* Just a text name for the task to aid debugging. */
	usTaskStackSize, /* The stack size is defined in FreeRTOSIPConfig.h. */
	arg, /* The task parameter, not used in this case. */
	uxTaskPriority, /* The priority assigned to the task is defined in FreeRTOSConfig.h. */
	&(thread->task)); /* The task handle is not used. */

	mqtt_task = thread->task;

	return rc;
}

static int uiso_mqtt_read(Network *n, unsigned char *buffer, int len, int timeout_ms)
{
	int rc = 0;
	int recvLen = 0;
	uint32_t timeout_s = 1;
	printf("rto: %d\n\r", timeout_ms);

	if(0 == recvLen)
	{
		do
		{
			rc = sl_Recv(n->my_socket, buffer + recvLen, len - recvLen, 0);
			if (rc <= 0)
				break;
			recvLen += rc;
		} while (recvLen < len);
	}

	return recvLen;
}

static int uiso_mqtt_write(Network *n, unsigned char *buffer, int len, int timeout_ms)
{
	int rc = 0;

	if(0 == rc)
	{
		rc = sl_Send(n->my_socket, buffer, len, 0);
	}

	return rc;
}

static void uiso_mqtt_disconnect(Network *n)
{
	sl_Close(n->my_socket);
}

void init_network(Network *n)
{
	if (NULL != n)
	{
		n->my_socket = -1;
		n->mqttread = uiso_mqtt_read;
		n->mqttwrite = uiso_mqtt_write;
		n->disconnect = uiso_mqtt_disconnect;
	}

	//return 0;
}

int NetworkConnect(Network *n, char *addr, int port)
{
	SlSockAddrIn_t sAddr;
	int addrSize;
	int retVal;
	unsigned long ipAddress;

	sl_NetAppDnsGetHostByName(addr, strlen(addr), &ipAddress, AF_INET);

	sAddr.sin_family = AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short) port);
	sAddr.sin_addr.s_addr = sl_Htonl(ipAddress);

	addrSize = sizeof(SlSockAddrIn_t);

	n->my_socket = sl_Socket(SL_AF_INET, SL_SOCK_STREAM, SL_IPPROTO_TCP);
	if (n->my_socket < 0)
	{
		// error
		return -1;
	}

	retVal = sl_Connect(n->my_socket, (SlSockAddr_t*) &sAddr, addrSize);
	if (retVal < 0)
	{
		// error
		sl_Close(n->my_socket);
		return retVal;
	}

	return retVal;
}

