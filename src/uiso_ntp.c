/*
 * uiso_ntp.c
 *
 *  Created on: 14 nov 2022
 *      Author: Francisco
 */

#include "uiso.h"

#include "uiso_ntp.h"

#include "simplelink.h"

static struct ntp_packet_s ntp_packet;


sntp_server_t sntp_server_list[] =
{
		{ .server_name = "0.de.pool.ntp.org", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "1.de.pool.ntp.org", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "2.de.pool.ntp.org", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "3.de.pool.ntp.org", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "time1.google.com", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "time2.google.com", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "time3.google.com", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = "time4.google.com", .next_time_stamp = 0,
				.last_time_stamp = 0, },
		{ .server_name = NULL, .next_time_stamp = 0, .last_time_stamp = 0, }, };

uint32_t sntp_sync_state = 0;

uint32_t sntp_is_synced(void)
{
	return sntp_sync_state;
}

sntp_server_t* select_server_from_list(void)
{
	return &sntp_server_list[0];
}

//static void update_server_from_list(sntp_server_t * server, uint32_t );

struct ntp_packet_s* create_sntp_client_request(struct ntp_packet_s *ntp_packet,
		uint32_t local_ntp_time)
{
	if (NULL != ntp_packet)
	{
		memset(ntp_packet, 0x00, sizeof(struct ntp_packet_s));

		ntp_packet->leap_version_mode = (uint8_t) sntp_client_request_v4;

		// If sntp_version_4
		*((uint32_t*) &(ntp_packet->transmit_timestamp_seconds[0])) = __REV(
				local_ntp_time);
	}
	return ntp_packet;
}

#include "sl_sleeptimer.h"

const char *const ntp_kiss_codes[] =
{ "ACTS", "AUTH", "AUTO", "BCSR", "CRYP", "DENY", "DROP", "RSTR", "INIT",
		"MCST", "NKEY", "RATE", "RMOT", "STEP" };

static uint32_t get_local_ntp_time(void)
{
	uint32_t local_time = (uint32_t) 0;

	if (SL_STATUS_OK
			!= sl_sleeptimer_convert_unix_time_to_ntp(sl_sleeptimer_get_time(),
					&local_time))
	{
		local_time = 0;
	}

	return local_time;
}

enum sntp_return_codes_e uiso_sntp_request(sntp_server_t* server, uint32_t * time_to_next_sync)
{
	sl_sleeptimer_date_t date =
	{ 0 };
	sl_sleeptimer_timestamp_t time_stamp = 0; // Unix timestamp
	sl_status_t sl_status = SL_STATUS_OK;
	_i16 dnsStatus = -1;
	_i16 sd = -1;
	_i16 res = -1;
	enum sntp_return_codes_e sntp_rcode = sntp_success;

	if(NULL == server)
	{
		return sntp_server_no_server;
	}
	else if(NULL == time_to_next_sync)
	{
		return sntp_server_no_server;
	}

	uint32_t local_origin_timestamp = 0;
	uint32_t local_destination_timestamp = 0;
	uint32_t local_timestamp_delta = 0;

	SlSockAddrIn_t ntp_srv_addr =
	{ .sin_family = SL_AF_INET, .sin_port = 0x7B00, .sin_addr =
	{ .s_addr = 0 } };
	SlSockAddrIn_t ntp_recv_addr =
	{ .sin_family = SL_AF_INET, .sin_port = 0, .sin_addr =
	{ .s_addr = 0 } };
	SlSocklen_t ntp_recv_addr_len = sizeof(ntp_recv_addr);

	dnsStatus = sl_NetAppDnsGetHostByName((_i8*) server->server_name,
			strlen(server->server_name), &(ntp_srv_addr.sin_addr.s_addr),
			SL_AF_INET);

	if (dnsStatus >= 0)
	{
		ntp_srv_addr.sin_addr.s_addr = __REV(ntp_srv_addr.sin_addr.s_addr);
		ntp_srv_addr.sin_port = __REV16(123);
	}
	else
	{
		sntp_rcode = sntp_server_ip_dns_error;
	}

	if (sntp_success == sntp_rcode)
	{
		sd = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, IPPROTO_UDP);
		if (sd >= 0)
		{
			res = sl_Bind(sd, (SlSockAddr_t*) &ntp_recv_addr,
					sizeof(SlSockAddr_t));
		}
		if (res >= 0)
		{
			local_origin_timestamp = get_local_ntp_time();
			(void) create_sntp_client_request(&ntp_packet,
					local_origin_timestamp);

			res = sl_SendTo(sd, &ntp_packet, sizeof(struct ntp_packet_s), 0,
					(SlSockAddr_t*) &ntp_srv_addr, sizeof(ntp_srv_addr));
		}
		if (res >= 0)
		{
			ntp_recv_addr.sin_family = SL_AF_INET;
			ntp_recv_addr.sin_port = 0;
			ntp_recv_addr.sin_addr.s_addr = 0;

			res = sl_RecvFrom(sd, &ntp_packet, sizeof(ntp_packet), 0,
					(SlSockAddr_t*) &ntp_recv_addr,
					(SlSocklen_t*) &ntp_recv_addr_len);
		}

		if (res >= 0)
		{
			local_destination_timestamp = get_local_ntp_time();
		}

		if (res >= 0)
		{
			res = sl_Close(sd);
		}
		else
		{
			(void) sl_Close(sd);
		}

		if (res < 0)
		{
			sntp_rcode = sntp_server_ip_error;
		}
	}

	/* Validate Server IP address*/
	if (sntp_success == sntp_rcode)
	{
		if ((ntp_recv_addr.sin_addr.s_addr != ntp_srv_addr.sin_addr.s_addr))
		{
			sntp_rcode = sntp_server_ip_mismatch;
		}
	}

	if (sntp_success == sntp_rcode)
	{
		if ((uint32_t) sntp_server_response_v4
				== ((uint32_t) ntp_packet.leap_version_mode
						& (uint32_t) sntp_server_response_v4))
		{
			// Expected server response
		}
		else
		{
			sntp_rcode = sntp_server_reply_header_error;
		}
	}

	if (sntp_success == sntp_rcode)
	{
		if ((uint32_t) ntp_stratum_kod == ntp_packet.stratum)
		{
			sntp_rcode = sntp_server_reply_kod;
		}
	}

	if (sntp_success == sntp_rcode)
	{
		uint32_t server_timestamp = __REV(
				*(uint32_t*) &(ntp_packet.recieve_timestamp_seconds[0]));
		uint32_t server_originate_timestamp = __REV(
				*(uint32_t*) &(ntp_packet.originate_timestamp_seconds[0]));
		uint32_t server_transmit_timestamp_seconds = __REV(
				*(uint32_t*) &(ntp_packet.transmit_timestamp_seconds[0]));
		uint32_t server_transmit_timestamp_fraction =
				(*(uint32_t*) &(ntp_packet.transmit_timestamp_fraction[0]));
		uint32_t server_poll_interval = ntp_packet.poll_interval;

		if ((uint32_t) 0 == (server_transmit_timestamp_seconds))
		{
			sntp_rcode = sntp_server_reply_invalid;
		}
		else if (server_originate_timestamp != local_origin_timestamp)
		{
			sntp_rcode = sntp_server_reply_invalid;
		}

		if (sntp_success == sntp_rcode)
		{
			if (SL_STATUS_OK
					!= sl_sleeptimer_convert_ntp_time_to_unix(server_timestamp,
							&time_stamp))
			{
				sntp_rcode = sntp_timestamp_format_error;
			}
		}

		if (sntp_success == sntp_rcode)
		{
			if (SL_STATUS_OK != sl_sleeptimer_set_time(time_stamp))
			{
				sntp_rcode = sntp_set_rtc_error;
			}
		}

		if (sntp_success == sntp_rcode)
		{
			if (SL_STATUS_OK == sl_sleeptimer_get_datetime(&date))
			{
				SlDateTime_t dateTime =
				{ 0 };

				/* Convert to SimpleLink format */
				dateTime.sl_tm_day = (_u32) date.month_day;
				dateTime.sl_tm_mon = (_u32) (date.month + 1);
				dateTime.sl_tm_year = (_u32) date.year + (_u32) 1900;
				dateTime.sl_tm_hour = (_u32) date.hour;
				dateTime.sl_tm_min = (_u32) date.min;
				dateTime.sl_tm_sec = (_u32) date.sec;

				res = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
				SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME, sizeof(SlDateTime_t),
						(_u8*) (&dateTime));
				if (res < 0)
				{
					sntp_rcode = sntp_set_simplelink_error;
				}
			}
			else
			{
				sntp_rcode = sntp_set_rtc_error;
			}
		}

		if (sntp_success == sntp_rcode)
		{
			if (server_poll_interval < (uint32_t) ntp_poll_interval_16s)
			{
				server_poll_interval = (uint32_t) ntp_poll_interval_16s;
			}
			else if (server_poll_interval
					> (uint32_t) ntp_poll_interval_131072s)
			{
				server_poll_interval = (uint32_t) ntp_poll_interval_131072s;
			}

			server->last_time_stamp = time_stamp;
			server->next_time_stamp = time_stamp
					+ (uint32_t) (1 << server_poll_interval);

			*time_to_next_sync = (1 << server_poll_interval) * (uint32_t)1000;
		}
		else
		{
			server->next_time_stamp = UINT32_MAX;
			*time_to_next_sync = 0;
		}
	}

	if(sntp_success == sntp_rcode)
	{
		sntp_sync_state = 1;
	}
	else
	{
		sntp_sync_state = 0;
	}

	return sntp_rcode;
}

