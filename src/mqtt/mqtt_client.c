#include "uiso.h"

#include "mqtt_client.h"
#include "../wifi_service.h"
/* Standard includes. */


#include "MQTTClient.h"


enum
{
	mqtt_notify_message_reception = (1 << 1),
	mqtt_notify_sensor_timer = ( 1<< 2 )
};

static TaskHandle_t mqtt_client_task = NULL;

TimerHandle_t publish_timer = NULL;

TaskHandle_t get_mqtt_client_task_handle(void)
{
	return mqtt_client_task;
}


void messageArrived(MessageData* data)
{
	printf("Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len, data->topicName->lenstring.data,
		data->message->payloadlen, data->message->payload);
}

static void prvMQTTEchoTask(void *pvParameters)
{
	(void)pvParameters;
	char* address = "test.mosquitto.org";

	/* Suspend myself */
	vTaskSuspend(NULL);

	/* connect to m2m.eclipse.org, subscribe to a topic, send and receive messages regularly every 1 sec */
	MQTTClient client;
	Network network;
	unsigned char sendbuf[80], readbuf[80];
	int rc = 0,
		count = 0;
	MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
	connectData.keepAliveInterval = (10 * 60); // 10 Minutes
	connectData.cleansession = 1;

	pvParameters = 0;
	init_network(&network);
	MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));


	if ((rc = NetworkConnect(&network, address, 1883)) != 0)
		printf("Return code from network connect is %d\n", rc);

	connectData.MQTTVersion = 3;
	connectData.clientID.cstring = "LlobetianTecnologica";

	if ((rc = MQTTConnect(&client, &connectData)) != 0)
		printf("Return code from MQTT connect is %d\n", rc);
	else
		printf("MQTT Connected\n");

	if ((rc = MQTTSubscribe(&client, "FreeRTOS/sample/#", 1, messageArrived)) != 0)
		printf("Return code from MQTT subscribe is %d\n", rc);

	xTimerStart( publish_timer, portMAX_DELAY);

	do
	{
		int rc = 0;
		MQTTMessage message;
		char payload[30];

		message.qos = 1;
		message.retained = 0;
		message.payload = payload;
		sprintf(payload, "message number %d", count++);
		message.payloadlen = strlen(payload);

		// wait for recieve


		uint32_t notification_value = 0;
		if(pdTRUE == xTaskNotifyWait(0, UINT32_MAX, &notification_value, 0))
		{
			if(notification_value & (uint32_t)mqtt_notify_message_reception)
			{
				(void)MQTTYield(&client, 500);
			}
			if(notification_value & (uint32_t)mqtt_notify_sensor_timer)
			{
				MQTTPublish(&client, "FreeRTOS/sample/a", &message);
			}
		}

		if(0 == enqueue_select_rx(wifi_service_mqtt_socket, network.my_socket, 5))
		{
			(void)xTaskNotify(mqtt_client_task, (uint32_t)mqtt_notify_message_reception, eSetBits);
		}


	}
	while(1);

	/* do not return */
}



/*-----------------------------------------------------------*/

#define MQTT_CLIENT_TASK_PRIORITY    (UBaseType_t)( uiso_task_runtime_services )

void publish_timer_callback(TimerHandle_t xTimer)
{
	(void)xTimer;

	(void)xTaskNotify(mqtt_client_task, (uint32_t)mqtt_notify_sensor_timer, eSetBits);
}

int initialize_mqtt_client(void)
{
	xTaskCreate(prvMQTTEchoTask,
			"MQTTEcho0",
			2048,
			NULL,
			MQTT_CLIENT_TASK_PRIORITY,
			&mqtt_client_task);

	publish_timer = xTimerCreate("MQTT publish timer", pdMS_TO_TICKS(5000), pdTRUE, NULL, publish_timer_callback);



	return 0;
}

