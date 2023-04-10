/*
 * MQTTuiso.h
 *
 *  Created on: 1 abr 2023
 *      Author: Francisco
 */

#ifndef MQTT_MQTTUISO_H_
#define MQTT_MQTTUISO_H_

#include "uiso.h"
//#include "network.h"

//#include "netapp.h"
//#include "socket.h"

#define MQTT_TASK    (1)


typedef struct Timer
{
	TickType_t xTicksToWait;
	TimeOut_t xTimeOut;
} Timer;

typedef struct Mutex
{
	SemaphoreHandle_t sem;
} Mutex;

typedef struct Thread
{
	TaskHandle_t task;
} Thread;

typedef struct Network Network;

struct Network
{
	int my_socket;
	int (*mqttread) (Network*, unsigned char*, int, int);
	int (*mqttwrite) (Network*, unsigned char*, int, int);
	void (*disconnect) (Network*);
};

void TimerInit(Timer*);
char TimerIsExpired(Timer*);
void TimerCountdownMS(Timer*, unsigned int);
void TimerCountdown(Timer*, unsigned int);
int TimerLeftMS(Timer*);

void MutexInit(Mutex*);
int MutexLock(Mutex*);
int MutexUnlock(Mutex*);

int ThreadStart(Thread*, void (*fn)(void*), void* arg);

void init_network(Network* n);

int wait_for_message(void);


#endif /* MQTT_MQTTUISO_H_ */
