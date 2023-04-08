/*
 * wifi_service.c
 *
 *  Created on: 12 nov 2022
 *      Author: Francisco
 */
#include "uiso_config.h"
#include "wifi_service.h"

#include "board_cc3100.h"

#include "simplelink.h"
#include "uiso_ntp.h"
#include <stdbool.h>
#include "sl_sleeptimer.h"

#include "mqtt/mqtt_client.h"

#define WIFI_TASK_PRIORITY      (UBaseType_t)( uiso_rtos_prio_above_normal )
#define SELECT_TASK_PRIORITY    (UBaseType_t)( uiso_task_runtime_services )

extern int lwm2m_client_task_runner(void *param1);

TimerHandle_t sntp_sync_timer = NULL;
TaskHandle_t wifi_task_handle = NULL;
TaskHandle_t select_task_handle = NULL;
SemaphoreHandle_t wifi_event_semaphore = NULL;

/* Wifi Queue Handle */
QueueHandle_t wifi_queue_handle = NULL;

#define WIFI_INITIAL_STATE			0
#define WIFI_PENDING_STATE          (1 << 0) // Pending bit
#define WIFI_DISCONNECTED_STATE 	(1 << 1)
#define WIFI_CONNECTING_STATE  		(1 << 2)
#define WIFI_CONNECTED_STATE  		(1 << 3)

#define WIFI_IP_ACQUIRED			(1 << 4) /* IP ACQUIRED MASK */
#define WIFI_IPV4_ACQUIRED			(1 << 5) /* IP VERSION MASK */
#define WIFI_IPV6_ACQUIRED			(1 << 6) /* */
#define WIFI_IP_RELEASED            (1 << 7)
#define WIFI_IP_MASK 				(WIFI_IP_ACQUIRED | WIFI_IPV4_ACQUIRED | WIFI_IPV6_ACQUIRED | WIFI_IP_RELEASED)

#define WIFI_NTP_SYNC_TIMER			(1 << 8)
#define WIFI_NTP_TIME_SYNCED		(1 << 9)
#define WIFI_NTP_REQUEST_SENT       (1 << 10)

#define WIFI_RX_DATA				(1 << 11)
#define WIFI_RX_NTP_DATA			(1 << 12)
#define WIFI_RX_LWM2M_DATA			(1 << 13)
#define WIFI_RX_MQTT_DATA			(1 << 14)
#define WIFI_RX_HTTP_DATA			(1 << 15)
#define WIFI_EVENT_MANAGER_TIMEOUT    (pdMS_TO_TICKS(24000))

#define WIFI_NTP_SYNC_PERIOD		(12*3600*1000) /* Sync for at least one hour */

extern TaskHandle_t user_task_handle;

enum wifi_connection_state_e
{
	wifi_initial = WIFI_INITIAL_STATE, wifi_disconnected = WIFI_DISCONNECTED_STATE, /* not connected */
	wifi_connecting = WIFI_CONNECTING_STATE, /* connection process starting */
	wifi_connected = WIFI_CONNECTED_STATE, /* connected to access point */
};

enum wifi_ip_acquired_state_e
{
	wifi_ip_not_acquired = 0,
	wifi_ip_acquired = WIFI_IP_ACQUIRED,
	wifi_ip_v4_acquired = (WIFI_IP_ACQUIRED | WIFI_IPV4_ACQUIRED) & WIFI_IP_MASK,
	wifi_ip_v6_acquired = (WIFI_IP_ACQUIRED | WIFI_IPV6_ACQUIRED) & WIFI_IP_MASK,
	wifi_ip_released = (WIFI_IP_RELEASED) & WIFI_IP_MASK
};

enum
{
	wifi_ntp_timer = WIFI_NTP_SYNC_TIMER,
	wifi_ntp_synced_event = WIFI_NTP_TIME_SYNCED,
	wifi_ntp_request_sent = WIFI_NTP_REQUEST_SENT,
};

enum
{
	wifi_connected_and_ip_acquired = (wifi_connected | wifi_ip_acquired)
};

struct wifi_status_s
{
	enum wifi_connection_state_e connection;
	enum wifi_ip_acquired_state_e ip;
} wifi_status;

struct socket_management_s
{
	int16_t sd; /* Socket Descriptor */
	uint32_t deadline_s;
	SemaphoreHandle_t queue_handle;
	SemaphoreHandle_t signal_semaphore;
};

struct socket_management_s rx_sockets[wifi_service_max];
struct socket_management_s tx_sockets[wifi_service_max];

static void initialize_socket_management(void)
{
	for (size_t i = 0; i < (size_t) wifi_service_max; i++)
	{
		rx_sockets[i].sd = -1;
		rx_sockets[i].deadline_s = 0;
		rx_sockets[i].queue_handle = xSemaphoreCreateMutex();
		rx_sockets[i].signal_semaphore = xSemaphoreCreateBinary();
		(void)xSemaphoreTake( rx_sockets[i].signal_semaphore,0 );

	}
	for (size_t i = 0; i < (size_t) wifi_service_max; i++)
	{
		tx_sockets[i].sd = -1;
		tx_sockets[i].deadline_s = 0;
		tx_sockets[i].queue_handle = xSemaphoreCreateMutex();
		tx_sockets[i].signal_semaphore = xSemaphoreCreateBinary();
		(void)xSemaphoreTake( rx_sockets[i].signal_semaphore,0 );
	}
}

void wifi_service_register_rx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	rx_sockets[(size_t) id].sd = sd;
	rx_sockets[(size_t) id].deadline_s = (uint32_t) sl_sleeptimer_get_time() + timeout_s;
}

void wifi_service_register_tx_socket(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	tx_sockets[(size_t) id].sd = sd;
	tx_sockets[(size_t) id].deadline_s = (uint32_t) sl_sleeptimer_get_time() + timeout_s;
}

struct rx_queue_s
{
	int32_t fd;
	uint32_t timeout_s;
	uint32_t timeout_ms;
	TaskHandle_t task_handle;
	QueueHandle_t queue_handle;
	uint32_t task_notification_value;
	char *rx_tx_buf;
	int32_t rx_tx_buf_len;

	uint32_t deadline_s; /* Deadline */

};

static QueueHandle_t rx_queue = NULL;

static void select_task(void *param)
{
	(void) param;
	struct rx_queue_s rx_queue_item;

	memset(&rx_queue_item, 0, sizeof(rx_queue_item));

	// Add network mutex
	vTaskSuspend(NULL);

	fd_set read_fd_set;
	fd_set write_fd_set;

	do
	{
		// Reset the pointers
		fd_set * read_set_ptr = NULL;
		fd_set * write_fd_set_ptr = NULL;
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);

		size_t i = 0;

		uint32_t timeout_min = 5; //UINT32_MAX;
		uint32_t current_time = sl_sleeptimer_get_time();
		for (i = 0; i < (size_t) wifi_service_max; i++)
		{
			uint32_t current_timeout = 0;
			struct socket_management_s *rx_socket = &rx_sockets[i];
			struct socket_management_s *tx_socket = &tx_sockets[i];

			if (rx_socket->sd > 0)
			{
				// Get the timeout
				if(rx_socket->deadline_s != 0)
				{
					if(rx_socket->deadline_s >= current_time)
					{
						current_timeout = rx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(rx_socket->sd, &read_fd_set);
						read_set_ptr = &read_fd_set;
					}
					else
					{
						rx_socket->deadline_s = 0;
					}
				}

			} // Checking rx sockets
			if (tx_socket->sd > 0)
			{
				// Get the timeout
				if (tx_socket->deadline_s != 0)
				{
					if(tx_socket->deadline_s >= current_time)
					{
						current_timeout = tx_socket->deadline_s - current_time;
						if (timeout_min > current_timeout)
						{
							timeout_min = current_timeout;
						}

						FD_SET(tx_socket->sd, &write_fd_set);
						write_fd_set_ptr = &write_fd_set;
					}
					else
					{
						tx_socket->deadline_s = 0;
					}
				}
			} // Checking tx sockets

		}

		if ((read_set_ptr != NULL) || (write_fd_set_ptr != NULL))
		{
			struct timeval tv =
			{ .tv_sec = timeout_min, .tv_usec = 0 };
			int result = sl_Select(FD_SETSIZE, read_set_ptr, write_fd_set_ptr, NULL, &tv);
			if(result > 0)
			{
				if (read_set_ptr != NULL)
				{
					for (i = 0; i < (size_t) wifi_service_max; i++)
					{
						if(rx_sockets[i].sd > 0)
						{
							if (FD_ISSET(rx_sockets[i].sd, read_set_ptr))
							{
								rx_sockets[i].deadline_s = 0;
								xSemaphoreGive( rx_sockets[i].signal_semaphore );
							}
						}
					}
				}
				if (write_fd_set_ptr != NULL)
				{
					for (i = 0; i < (size_t) wifi_service_max; i++)
					{
						if(tx_sockets[i].sd > 0)
						{
							if (FD_ISSET(tx_sockets[i].sd, write_fd_set_ptr))
							{
								tx_sockets[i].deadline_s = 0;
								xSemaphoreGive( tx_sockets[i].signal_semaphore );
							}
						}
					}
				}
			}
			else
			{
				//
			}
		} else
		{
			uint32_t notification_counter;
			(void)xTaskGenericNotifyWait( 0, 0, UINT32_MAX, &notification_counter, portMAX_DELAY);
			printf("RX counter %d\n\r", notification_counter);
		}






	} while (1);

#if 0
	do
	{
		uint32_t notification_value = 0;

		if (pdTRUE == xTaskNotifyWait((uint32_t )0, UINT32_MAX, &notification_value, portMAX_DELAY))
		{
			/* Clear Select TX queue */
			while (pdTRUE == xQueueReceive(tx_queue, &rx_queue_item, 0)) // expiration time?
			{
				//


				bool send_to = ((rx_queue_item.rx_tx_buf != NULL)&&(rx_queue_item.rx_tx_buf_len != 0));
				uint32_t task_notification_value = 0;
				if(rx_queue_item.timeout_s != 0)
				{
					FD_ZERO(&write_fd_set);
					FD_SET(rx_queue_item.fd, &write_fd_set);
					struct timeval tv =
					{ .tv_sec = rx_queue_item.timeout_s, .tv_usec = 0 };
					int result = sl_Select(FD_SETSIZE, NULL, &write_fd_set, NULL, &tv);
					if (result > 0)
					{
						if (FD_ISSET(rx_queue_item.fd, &write_fd_set))
						{
							task_notification_value = rx_queue_item.task_notification_value;
						}
						else
						{
							send_to = false;
							task_notification_value = UINT32_MAX;
						}
					}
				}
				if(send_to)
				{
					//
				}
				if(NULL != rx_queue_item.task_handle)
				{
					xTaskNotifyIndexed(rx_queue_item.task_handle, 1, notification_value, eSetBits);
				}
			}
			/* Clear Select RX queue */
			while (pdTRUE == xQueueReceive(rx_queue, &rx_queue_item, 0))
			{
				bool do_recv = (rx_queue_item.rx_tx_buf != NULL);
				uint32_t task_notification_value = 0;
				if(rx_queue_item.timeout_s != 0)
				{
					FD_ZERO(&read_fd_set);
					FD_SET(rx_queue_item.fd, &read_fd_set);
					struct timeval tv =
					{ .tv_sec = rx_queue_item.timeout_s, .tv_usec = 0 };
					int result = sl_Select(FD_SETSIZE, &read_fd_set, NULL, NULL, &tv);
					if (result > 0)
					{
						if (FD_ISSET(rx_queue_item.fd, &read_fd_set))
						{
							task_notification_value = rx_queue_item.task_notification_value;
						}
						else
						{
							task_notification_value = UINT32_MAX;
							do_recv = false;
						}
					}
				}
				if(do_recv)
				{
					int ret = sl_Recv(rx_queue_item.fd, rx_queue_item.rx_tx_buf,
															(_i16) rx_queue_item.rx_tx_buf_len, 0);
				}
				if(NULL != rx_queue_item.task_handle)
				{
					xTaskNotifyIndexed(rx_queue_item.task_handle, 1,
									task_notification_value, eSetBits);
				}
			} // While select rx
		}

	} while (1);
#endif
}

int wifi_service_recv(int sd, void *recv_buf, uint32_t recv_len, uint32_t timeout_ms)
{
	int ret_value = -1;
	struct rx_queue_s rx_queue_item;
	rx_queue_item.fd = sd;
	rx_queue_item.task_handle = xTaskGetCurrentTaskHandle();
	rx_queue_item.timeout_ms = timeout_ms;
	rx_queue_item.rx_tx_buf = recv_buf;
	rx_queue_item.rx_tx_buf_len = recv_len;

	if (pdTRUE != xQueueSend(rx_queue, &rx_queue_item, portMAX_DELAY))
	{
		ret_value = -1;
	}

	return ret_value;
}

int enqueue_select_rx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;

	wifi_service_register_rx_socket(id, sd, timeout_s);

	(void) xTaskNotifyIndexed(select_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( rx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s) ))
	{
		ret_value = 0;
	}

	return ret_value;
}

int enqueue_select_tx(enum wifi_socket_id_e id, int sd, uint32_t timeout_s)
{
	int ret_value = -1;
	(void)xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 0);

	wifi_service_register_tx_socket(id, sd, timeout_s);

	(void) xTaskNotifyIndexed(select_task_handle, 0, 1, eIncrement);

	if(pdTRUE == xSemaphoreTake( tx_sockets[(size_t)id].signal_semaphore, 1000*(timeout_s) ))
	{
		ret_value = 0;
	}

	return ret_value;
}

void sntp_sync_timer_callback(TimerHandle_t pxTimer)
{
	(void) pxTimer;

	if (NULL != wifi_task_handle)
	{
		xTaskNotify(wifi_task_handle, (WIFI_NTP_SYNC_TIMER | WIFI_PENDING_STATE), eSetBits);
	}
}

void wifi_task(void *param)
{
	(void) param;

	uint32_t ulNotifiedValue = 0;

	volatile _i16 role = 0;

	wifi_status.connection = wifi_disconnected;

	initialize_socket_management();

	role = sl_Start(NULL, (_i8*) CC3100_DEVICE_NAME, NULL);
	switch ((SlWlanMode_e) role)
	{
		case ROLE_STA:
			// Station Role
			break;
		case ROLE_UNKNOWN:
		case ROLE_AP:
		case ROLE_P2P:
		case ROLE_STA_ERR:
		case ROLE_AP_ERR:
		case ROLE_P2P_ERR:
		case INIT_CALIB_FAIL:
		default:
			// Error cases
			break;
	}

	/* Get the device's version-information */
	SlVersionFull ver =
	{ 0 };
	_u8 configOpt = SL_DEVICE_GENERAL_VERSION;
	_u8 configLen = sizeof(ver);
	_i32 retVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (_u8*) (&ver));

	/* Set connection policy to Auto + SmartConfig (Device's default connection policy) */
	retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);

	//retVal = sl_WlanProfileDel(0xFF);

	retVal = sl_WlanDisconnect();
	if (0 == retVal)
	{
		//	xTaskNotifyWait( 0, UINT32_MAX, &ulNotifiedValue, portMAX_DELAY );
	}

	_u8 val = 1;
	retVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &val);

	/* Disable scan */
	configOpt = SL_SCAN_POLICY(0);
	retVal = sl_WlanPolicySet(SL_POLICY_SCAN, configOpt, NULL, 0);

	/* Set Tx power level for station mode
	 Number between 0-15, as dB offset from max power - 0 will set maximum power */
	_u8 power = 0;
	retVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
	WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (_u8*) &power);

	/* Set power management policy to normal */
	retVal = sl_WlanPolicySet(SL_POLICY_PM, SL_NORMAL_POLICY, NULL, 0);

	/* Unregister mDNS services */
	retVal = sl_NetAppMDNSUnRegisterService(0, 0);

	/* Remove  all 64 filters (8*8) */
	_WlanRxFilterOperationCommandBuff_t RxFilterIdMask =
	{ 0 };
	memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
	retVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8*) &RxFilterIdMask,
			sizeof(_WlanRxFilterOperationCommandBuff_t));
	//    ASSERT_ON_ERROR(retVal);

	retVal = sl_Stop(0xff);

	role = sl_Start(NULL, (_i8*) CC3100_DEVICE_NAME, NULL);

	//sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &macLen, mac_);

	retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 1, 0, 0, 1), NULL, 0);

	// Think about scan parameters

	/* Start Wifi Connection */
	wifi_status.connection = wifi_connecting;
	/*_u8 MacAddr[6] =  0xD0, 0x5F, 0xB8, 0x4B, 0xD3, 0x74 }; */
	SlSecParams_t secParams;
	secParams.Key = (signed char*) config_get_wifi_key();
	secParams.KeyLen = strlen(config_get_wifi_key());
	secParams.Type = SL_SEC_TYPE_WPA_WPA2;

	retVal = sl_WlanConnect((_i8*) config_get_wifi_ssid(), (_i16) strlen(config_get_wifi_ssid()),
	NULL, &secParams, NULL);

	/* Connection manager logic */
	do
	{
		if (pdTRUE
				== xTaskNotifyWait(WIFI_PENDING_STATE, UINT32_MAX, &ulNotifiedValue,
						WIFI_EVENT_MANAGER_TIMEOUT))
		{
			sl_iostream_printf(sl_iostream_swo_handle, "Event: %x\n\r", ulNotifiedValue);

			if (ulNotifiedValue & (uint32_t) wifi_connected)
			{
				if (0 == sntp_is_synced())
				{
					xTimerChangePeriod(sntp_sync_timer, (5 * 1000), portMAX_DELAY);
				}

				sl_iostream_printf(sl_iostream_swo_handle, "Wifi Connected %x\n\r",
						ulNotifiedValue);
			}

			if (ulNotifiedValue & (uint32_t) wifi_disconnected)
			{
				// When disconnected, stop ntp sync timer
				if (pdTRUE == xTimerIsTimerActive(sntp_sync_timer))
				{
					xTimerStop(sntp_sync_timer, portMAX_DELAY);
				}

				// Suspend LWM2M Task
				vTaskSuspend(user_task_handle);
				vTaskSuspend(select_task_handle);
				vTaskSuspend(get_mqtt_client_task_handle());
				sl_iostream_printf(sl_iostream_swo_handle, "Wifi Disconnected %x\n\r",
						ulNotifiedValue);
			}

			if (ulNotifiedValue & (uint32_t) wifi_ip_v4_acquired)
			{
				sl_iostream_printf(sl_iostream_swo_handle, "IPv4 Acquired %x\n\r", ulNotifiedValue);
			}

			if (ulNotifiedValue & (uint32_t) wifi_ip_v6_acquired)
			{
				sl_iostream_printf(sl_iostream_swo_handle, "IPv6 Acquired %x\n\r", ulNotifiedValue);
			}

			if (ulNotifiedValue & ((uint32_t) wifi_ip_v4_acquired | (uint32_t) wifi_ip_v6_acquired))
			{
				vTaskResume(select_task_handle);
			}

			if (ulNotifiedValue & (uint32_t) wifi_ip_released)
			{
				sl_iostream_printf(sl_iostream_swo_handle, "IP Released %x\n\r", ulNotifiedValue);
			}

			if (ulNotifiedValue & (uint32_t) wifi_ntp_timer)
			{
				if (pdTRUE == xTimerIsTimerActive(sntp_sync_timer))
				{
					xTimerStop(sntp_sync_timer, portMAX_DELAY);
				}

				/* Execute the ntp timer */
				uint32_t time_to_next_sync = 0;
				enum sntp_return_codes_e sntp_rcode = sntp_success;

				sntp_server_t *server = select_server_from_list();

				sntp_rcode = uiso_sntp_request(server, &time_to_next_sync);

				if (time_to_next_sync < WIFI_NTP_SYNC_PERIOD)
				{
					time_to_next_sync = WIFI_NTP_SYNC_PERIOD;
				}

				/* Restart the NTP sync timer */
				if (sntp_rcode == sntp_success)
				{
					xTimerChangePeriod(sntp_sync_timer, pdMS_TO_TICKS(time_to_next_sync),
							portMAX_DELAY);
					xTaskNotify(wifi_task_handle,
							((uint32_t)wifi_ntp_synced_event | WIFI_PENDING_STATE), eSetBits);
				} else
				{
					xTimerChangePeriod(sntp_sync_timer, pdMS_TO_TICKS(16*1000), portMAX_DELAY);
				}

				sl_iostream_printf(sl_iostream_swo_handle, "SNTP Sync: %d, %d\n\r", sntp_rcode,
						time_to_next_sync);
			}

			if (ulNotifiedValue & (uint32_t) wifi_ntp_synced_event)
			{
				// Resume LWM2M Task
				vTaskResume(user_task_handle);
				vTaskResume(get_mqtt_client_task_handle());
			}
		} else
		{
			sl_iostream_printf(sl_iostream_swo_handle, "No event\n\r");
		}

	} while (1);

}

void create_wifi_service_task(void)
{
	xTaskCreate(wifi_task, "WifiService", configMINIMAL_STACK_SIZE + 1024, NULL,
	WIFI_TASK_PRIORITY, &wifi_task_handle);

	xTaskCreate(select_task, "SelectTask", configMINIMAL_STACK_SIZE + 100, NULL,
	SELECT_TASK_PRIORITY, &select_task_handle);

	sntp_sync_timer = xTimerCreate("ntpService", 1000, pdTRUE, NULL, sntp_sync_timer_callback);
}

void CC3100_WlanEvtHdlr(SlWlanEvent_t *pSlWlanEvent)
{
	if (pSlWlanEvent == NULL)
	{
		// Error due to NULL pointer
		return;
	}

	switch (pSlWlanEvent->Event)
	{
		case SL_WLAN_CONNECT_EVENT: {
			slWlanConnectAsyncResponse_t *pEventData = NULL;
			pEventData = &pSlWlanEvent->EventData.STAandP2PModeWlanConnected;

			wifi_status.connection = wifi_connected;

		}
			break;
		case SL_WLAN_DISCONNECT_EVENT: {
			slWlanConnectAsyncResponse_t *pEventData = NULL;
			pEventData = &pSlWlanEvent->EventData.STAandP2PModeDisconnected;

			/* If the user has initiated 'Disconnect' request, 'reason_code' is SL_USER_INITIATED_DISCONNECTION */
			if (SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
			{
				// Disconnected due to
			} else
			{
				//
			}

			wifi_status.connection = wifi_disconnected;
		}
			break;
		case SL_WLAN_SMART_CONFIG_COMPLETE_EVENT:
			break;
		case SL_WLAN_SMART_CONFIG_STOP_EVENT:
			// Smart Condig Stop response
			break;
		case SL_WLAN_STA_CONNECTED_EVENT: {
			slPeerInfoAsyncResponse_t *pEventData = NULL;
			pEventData = &pSlWlanEvent->EventData.APModeStaConnected;
			// Access Point Mode
		}
			break;
		case SL_WLAN_STA_DISCONNECTED_EVENT: {
			slPeerInfoAsyncResponse_t *pEventData = NULL;
			pEventData = &pSlWlanEvent->EventData.APModeStaConnected;

		}
			break;
		case SL_WLAN_P2P_DEV_FOUND_EVENT:
			/*
			 * Information about the remote P2P device (device name and MAC)
			 * will be available in 'slPeerInfoAsyncResponse_t' - Applications
			 * can use it if required
			 *
			 * slPeerInfoAsyncResponse_t *pEventData = NULL;
			 * pEventData = &pWlanEvent->EventData.P2PModeDevFound;
			 *
			 */
			break;
		case SL_WLAN_P2P_NEG_REQ_RECEIVED_EVENT:

			break;
		case SL_WLAN_CONNECTION_FAILED_EVENT: {
			slWlanConnFailureAsyncResponse_t *pEventData = NULL;
			pEventData = &pSlWlanEvent->EventData.P2PModewlanConnectionFailure;
			wifi_status.connection = wifi_disconnected;
		}
			break;
		default:
			break;
	}

	// set the bits
	xTaskNotify(wifi_task_handle, ((uint32_t )wifi_status.connection | WIFI_PENDING_STATE),
			eSetBits);

}
void CC3100_NetAppEvtHdlr(SlNetAppEvent_t *pSlNetApp)
{
	switch (pSlNetApp->Event)
	{

		case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
			//pSlNetApp->EventData.ipAcquiredV4;
			wifi_status.ip = wifi_ip_v4_acquired;
			break;
		case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
			//pSlNetApp->EventData.ipAcquiredV6;
			wifi_status.ip = wifi_ip_v6_acquired;
			break;
		case SL_NETAPP_IP_LEASED_EVENT:
			//pSlNetApp->EventData.ipLeased;
			break;
		case SL_NETAPP_IP_RELEASED_EVENT:
			//pSlNetApp->EventData.ipReleased;
			wifi_status.ip = wifi_ip_released;
			break;
		default:
			break;
	};

	xTaskNotify(wifi_task_handle, ((uint32_t)wifi_status.ip | WIFI_PENDING_STATE), eSetBits);

}

