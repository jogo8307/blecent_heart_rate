/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example for using NimBLE as a BLE scanner
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timex.h"
#include "ztimer.h"
#include "shell.h"

#include "nimble_scanner.h"
#include "nimble_scanlist.h"

#include "nimble_riot.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"

/* Mandatory services. */
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

/* default scan interval */
#define DEFAULT_SCAN_INTERVAL_MS    30

/* default scan duration (1s) */
#define DEFAULT_DURATION_MS        (1 * MS_PER_SEC)

//static int blecent_gap_event(struct ble_gap_event *event, void *arg);

static uint16_t m_heart_rate_start_handle;
static uint16_t m_heart_rate_end_handle;
static uint16_t not_handle;

static bool notifying;

static uint8_t own_addr_type;

static struct ble_gap_conn_desc m_conn_out_desc;

static const ble_uuid16_t predef_uuid = BLE_UUID16_INIT(0x180D);
static const ble_uuid16_t heart_rate_uuid = BLE_UUID16_INIT(0x2A37);

//static int gatt_mtu_cb(uint16_t conn_handle,
//                            const struct ble_gatt_error *error,
//                            uint16_t mtu, void *arg);

static int gatt_discovery_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		const struct ble_gatt_svc *service,
		void *arg);

static int mtu_exchange_cb(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            uint16_t mtu, void *arg);

static int blecent_on_subscribe(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		struct ble_gatt_attr *attr,
		void *arg);

static int _cmd_notify(int argc, char **argv);

static int _cmd_discover(int argc, char **argv);

static int find_heart_rate_chr_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		const struct ble_gatt_chr *chr, void *arg);

static int heart_rate_service_dscr_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		uint16_t chr_val_handle,
		const struct ble_gatt_dsc *dsc,
		void *arg);

/* scan_event() calls scan(), so forward declaration is required */
static void scan(void);

/* connection has separate event handler from scan */
	static int
conn_event(struct ble_gap_event *event, void *arg)
{
	int rc;
	static uint8_t receiveCounter;
	puts("conn_event");
	(void)arg;
	switch (event->type) {
		case BLE_GAP_EVENT_CONNECT:
			if (event->connect.status == 0) {
				puts("Connection was established\n");
				//ble_gap_terminate(event->connect.conn_handle, 0x13);
				receiveCounter = 0;
				rc = ble_gap_conn_find(event->connect.conn_handle, &m_conn_out_desc);

				if (rc == 0)
				{
					ble_gattc_exchange_mtu(m_conn_out_desc.conn_handle,
							mtu_exchange_cb,NULL);

				} 
			}else {
				printf("Connection failed, error code: 0x%x\n",event->connect.status);
			}
			break;
		case BLE_GAP_EVENT_DISCONNECT:
			printf("Disconnected, reason code: %x\n", event->disconnect.reason);
			//scan();
			break;
		case BLE_GAP_EVENT_CONN_UPDATE_REQ:
			puts("Connection update request received\n");
			puts("peer: ");
			printf("itvl_max %d, itvl_min %d, latency %d, max_ce_len %d, min_ce_len %d, supervision_timeout %d\n",
					event->conn_update_req.peer_params->itvl_max,
					event->conn_update_req.peer_params->itvl_min,
					event->conn_update_req.peer_params->latency,
					event->conn_update_req.peer_params->max_ce_len,
					event->conn_update_req.peer_params->min_ce_len,
					event->conn_update_req.peer_params->supervision_timeout);

			*event->conn_update_req.self_params =
				*event->conn_update_req.peer_params;

//			event->conn_update_req.self_params->itvl_max = BLE_GAP_CONN_ITVL_MS(1000);
//			event->conn_update_req.self_params->itvl_min = BLE_GAP_CONN_ITVL_MS(800);
//			event->conn_update_req.self_params->latency = 2;

			break;
		case BLE_GAP_EVENT_CONN_UPDATE:


			if (event->conn_update.status == 0) {

				ble_gap_conn_find(event->conn_update.conn_handle, &m_conn_out_desc);

				(void)gatt_discovery_cb;
				//ble_gattc_disc_all_svcs(m_conn_out_desc.conn_handle, gatt_discovery_cb, NULL);
				//ble_gattc_disc_svc_by_uuid(m_conn_out_desc.conn_handle, &predef_uuid.u, gatt_discovery_cb, NULL);
				puts("Connection update successful\n");
			} else {
				printf("Connection update failed, reason: %x\n", event->conn_update.status);
			}
			break;
		case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
			puts("Connection l2cap update request received\n");
			puts("peer: ");
			printf("itvl_max %d, itvl_min %d, latency %d, max_ce_len %d, min_ce_len %d, supervision_timeout %d\n",
					event->conn_update_req.peer_params->itvl_max,
					event->conn_update_req.peer_params->itvl_min,
					event->conn_update_req.peer_params->latency,
					event->conn_update_req.peer_params->max_ce_len,
					event->conn_update_req.peer_params->min_ce_len,
					event->conn_update_req.peer_params->supervision_timeout);

			*event->conn_update_req.self_params =
				*event->conn_update_req.peer_params;

				//event->conn_update_req.self_params->itvl_max = BLE_GAP_CONN_ITVL_MS(1000);
				//event->conn_update_req.self_params->itvl_min = BLE_GAP_CONN_ITVL_MS(800);
				//event->conn_update_req.self_params->latency = 2;

	break;
		case BLE_GAP_EVENT_NOTIFY_RX:

			receiveCounter++;

			printf("receiveCounter: %u\n", receiveCounter);
			printf("notify rx, attr_handle: %u\n",event->notify_rx.attr_handle);
			printf("notify rx, event->notify_rx.om->om_flags %u\n",event->notify_rx.om->om_flags);
			uint8_t counter;
			for (counter = 0; counter < event->notify_rx.om->om_len; counter++)
			{
				printf("%x", event->notify_rx.om->om_data[counter]);
			}

			puts("\n");
			break;
		case BLE_GAP_EVENT_MTU:
			ble_gattc_disc_svc_by_uuid(m_conn_out_desc.conn_handle, &predef_uuid.u, gatt_discovery_cb, NULL);
			//ble_gattc_disc_all_svcs(m_conn_out_desc.conn_handle, gatt_discovery_cb, NULL);
			break;
		default:
			printf("Connection event type not supported, %d\n",
					event->type);
			break;
	}
	return 0;
}

static int scan_event(struct ble_gap_event *event, void *arg)
{
	(void)arg;
	struct ble_hs_adv_fields parsed_fields;
	int uuid_cmp_result;

	memset(&parsed_fields, 0, sizeof(parsed_fields));

	switch (event->type) {
		/* advertising report has been received during discovery procedure */
		case BLE_GAP_EVENT_DISC:
			puts("Advertising report received! Checking UUID...\n");
			ble_hs_adv_parse_fields(&parsed_fields, event->disc.data,
					event->disc.length_data);
			/* Predefined UUID is compared to recieved one;
			   if doesn't fit - end procedure and go back to scanning,
			   else - connect. */
			if (parsed_fields.num_uuids16 > 0)
			{
				uint8_t counter;
				for ( counter = 0; counter < parsed_fields.num_uuids16; counter++)
				{
					uuid_cmp_result = ble_uuid_cmp(&predef_uuid.u, &parsed_fields.uuids16[counter].u);
					if (!uuid_cmp_result) {
						puts("UUID fits, connecting...\n");
						ble_gap_disc_cancel();
						//
						ble_gap_connect(own_addr_type, &(event->disc.addr), 30000,
								NULL, conn_event, NULL);
						break;
					}
				}
			}
			break;
		case BLE_GAP_EVENT_DISC_COMPLETE:
			printf("Discovery completed, reason: %x\n",
					event->disc_complete.reason);
			break;
		default:
			puts("Discovery event not handled\n");
			break;
	}

	return 0;
}

	static void
scan(void)
{
	int rc;


	/* Figure out address to use while advertising (no privacy for now) */
	rc = ble_hs_id_infer_auto(0, &own_addr_type);

	//printf("ble_gattc_init: %d\n", ble_gattc_init());
	/* set scan parameters:
	   - scan interval in 0.625ms units
	   - scan window in 0.625ms units
	   - filter policy - 0 if whitelisting not used
	   - limited - should limited discovery be used
	   - passive - should passive scan be used
	   - filter duplicates - 1 enables filtering duplicated advertisements */
	const struct ble_gap_disc_params scan_params = {0, 0, 0, 0, 1, 1};

	/* performs discovery procedure */
	rc = ble_gap_disc(own_addr_type, 10000, &scan_params, scan_event, NULL);
	assert(rc == 0);
}

int _cmd_scan(int argc, char **argv)
{
	uint32_t timeout = DEFAULT_DURATION_MS;

	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: %s [timeout in ms]\n", argv[0]);

		return 0;
	}
	if (argc >= 2) {
		timeout = atoi(argv[1]);
	}

	nimble_scanlist_clear();
	printf("Scanning for %"PRIu32" ms now ...\n", timeout);
	nimble_scanner_start();
	ztimer_sleep(ZTIMER_MSEC, timeout);
	nimble_scanner_stop();
	puts("Done, results:");
	nimble_scanlist_print();
	puts("");

	return 0;
}

int _cmd_read(int argc, char **argv)
{

	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: %s [timeout in ms]\n", argv[0]);

		return 0;
	}

	puts("");

	return 0;
}

int _cmd_connect(int argc, char **argv)
{
	int rc;

	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: no arguments\n");

		return 0;
	}

	puts("Trying to connect");

	rc = ble_hs_is_enabled();

	if (rc != 0)
	{
		scan();
		puts("Scanning started");
	}
	else
	{
		puts("host not enabled");
	}
	puts("");

	return 0;
}

int _cmd_disconnect(int argc, char **argv)
{


	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: no arguments\n");

		return 0;
	}

	int rc = ble_gap_terminate(m_conn_out_desc.conn_handle, BLE_ERR_REM_USER_CONN_TERM);

	printf("Disconnected with Code: %x\n", rc);
	puts("");

	return 0;
}

static const shell_command_t _commands[] = {
	{ "scan", "trigger a BLE scan", _cmd_scan },
	{ "connect", "trigger a BLE scan", _cmd_connect },
	{ "notify", "notify heart rate", _cmd_notify },
	{ "disconnect", "disconnect from device", _cmd_disconnect },
	{ "read", "read from device", _cmd_read },
	{ "discover", "Discover services", _cmd_discover },
	{ NULL, NULL, NULL }
};

int main(void)
{
	puts("NimBLE Scanner Example Application");
	puts("Type `scan help` for more information");

	/* start shell */
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	shell_run(_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

	return 0;
}

static int find_heart_rate_chr_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		const struct ble_gatt_chr *chr, void *arg)
{
	puts("chr event");
	(void)conn_handle;
	(void)arg;

	printf("find_heart_rate_chr_cb error 0x%x, attr_handle: %x\n", error->status, error->att_handle);
	printf("find_heart_rate_chr_cb def_handle 0x%x, properties 0x%x\n", chr->def_handle, chr->properties);
	
		if (chr->uuid.u.type == BLE_UUID_TYPE_16)
		{
			printf("characteristic uuid typ 16, val: %x\n", chr->uuid.u16.value);
		}
		else if (chr->uuid.u.type == BLE_UUID_TYPE_32)
		{
			printf("characteristic uuid typ 32, val: %lx\n", chr->uuid.u32.value);
		}
		else if (chr->uuid.u.type == BLE_UUID_TYPE_128)
		{
			uint8_t counter;
			printf("characteristic uuid typ 128, val: ");
			for ( counter = 0; counter < 16; counter++)
			{
				printf("%x:", chr->uuid.u128.value[counter] );
			}
		}

		//rc = ble_uuid_cmp(&chr->uuid.u, &heart_rate_uuid.u);
		//if( ! rc )
		//{
		//	ble_gattc_disc_all_dscs(m_conn_out_desc.conn_handle, m_heart_rate_start_handle,
		//			m_heart_rate_end_handle,
		//			heart_rate_service_dscr_cb, NULL);
		//}
		if ( error->status == BLE_HS_EDONE )
		{

			ble_gattc_disc_all_dscs(m_conn_out_desc.conn_handle, m_heart_rate_start_handle,
					m_heart_rate_end_handle,
					heart_rate_service_dscr_cb, NULL);
		}

	return 0;

}

static int heart_rate_service_dscr_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		uint16_t chr_val_handle,
		const struct ble_gatt_dsc *dsc,
		void *arg)
{
	int rc;
	(void)chr_val_handle;	
	(void)conn_handle;	
	(void)arg;	

	if ( error->status == 0)
	{
		if (dsc->uuid.u.type == BLE_UUID_TYPE_16)
		{
			printf("characteristic uuid typ 16, val: %x\n", dsc->uuid.u16.value);
		}
		else if (dsc->uuid.u.type == BLE_UUID_TYPE_32)
		{
			printf("characteristic uuid typ 32, val: %lx\n", dsc->uuid.u32.value);
		}
		else if (dsc->uuid.u.type == BLE_UUID_TYPE_128)
		{
			uint8_t counter;
			printf("characteristic uuid typ 128, val: ");
			for ( counter = 0; counter < 16; counter++)
			{
				printf("%x:", dsc->uuid.u128.value[counter] );
			}
		}
		rc = ble_uuid_cmp(&dsc->uuid.u, &heart_rate_uuid.u);
		if( ! rc )
		{
			not_handle = dsc->handle + 1;
			printf("Indicate rc %x\n", rc);

		}
	}
	if ( error->status == BLE_HS_EDONE)
	{
		puts("All found");
	}

	printf("heart_rate_service_dscr_cb error = %x\n", error->status);

	return 0;
}

//static int gatt_mtu_cb(uint16_t conn_handle,
//                            const struct ble_gatt_error *error,
//                            uint16_t mtu, void *arg)
//{
//	(void)conn_handle;
//	(void)arg;
//	(void)error;
//	(void)mtu;
//
//	return 0;
//}

static int blecent_on_subscribe(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		struct ble_gatt_attr *attr,
		void *arg)
{
	(void)arg;
	printf("Subscribe complete; status=%d conn_handle=%d "
			"attr_handle=%d\n",
			error->status, conn_handle, attr->handle);

	return 0;
}

static int mtu_exchange_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		uint16_t mtu, void *arg)
{
	(void)arg;
	(void)conn_handle;

	if(error->att_handle == 0)
	{
		ble_att_set_preferred_mtu(mtu);
	printf("MTU exchange %d\n", mtu);

	}
	return 0;
}

static int _cmd_notify(int argc, char **argv)
{
	int rc;

	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: no arguments\n");

		return 0;
	}

	puts("Trying to connect");

	static uint8_t value[2];

	if ( notifying == true)
	{
		value[0] = 0;
		notifying = false;
	}
	else
	{
		value[0] = 1;
		notifying = false;
	}

	value[1] = 0;
	rc = ble_gattc_write_flat(m_conn_out_desc.conn_handle, not_handle,
			value, 2, blecent_on_subscribe, NULL);

	printf("Indicate rc %x\n", rc);
	puts("");

	return 0;
}

static int _cmd_discover(int argc, char **argv)
{
	//int rc;

	if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
		printf("usage: no arguments\n");

		return 0;
	}

	puts("Trying to connect");

	if (m_conn_out_desc.conn_handle != 0)
	{
		ble_gattc_disc_all_svcs(m_conn_out_desc.conn_handle, gatt_discovery_cb, NULL);
		puts("Discovery started\n");
	}
	else
	{
		puts("Discovery not started\n");
	}
	puts("");

	return 0;
}

static int gatt_discovery_cb(uint16_t conn_handle,
		const struct ble_gatt_error *error,
		const struct ble_gatt_svc *service,
		void *arg)
{
	(void)arg;
	(void)conn_handle;

		int uuid_cmp_result = ble_uuid_cmp(&predef_uuid.u, &service->uuid.u16.u);
		if( uuid_cmp_result == 0)
		{
			puts("service found\n");
			m_heart_rate_start_handle = service->start_handle;
			m_heart_rate_end_handle = service->end_handle;
			printf("heart rate start handle %u, end handle %u", m_heart_rate_start_handle, m_heart_rate_end_handle );
		}
		if ( error->status == BLE_HS_EDONE )
		{

			ble_gattc_disc_all_chrs(m_conn_out_desc.conn_handle,m_heart_rate_start_handle,
					m_heart_rate_end_handle, find_heart_rate_chr_cb, NULL);
		}

	printf("discovery event error 0x%x\n", error->status);

	return 0;
}
