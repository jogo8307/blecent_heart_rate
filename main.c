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

static uint16_t m_connection_handle;

static const ble_uuid16_t predef_uuid = BLE_UUID16_INIT(0x180D);
static const ble_uuid16_t heart_rate_uuid = BLE_UUID16_INIT(0x2a37);

static int gatt_discovery_cb(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *service,
                                 void *arg);

static int heart_rate_attr_cb(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr,
                             void *arg);

static int find_heart_rate_chr_cb(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            const struct ble_gatt_chr *chr, void *arg);

/* scan_event() calls scan(), so forward declaration is required */
static void scan(void);

static int gatt_discovery_cb(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *service,
                                 void *arg)
{
	(void)arg;
	(void)conn_handle;

		if (error->status == 0)
		{
			int uuid_cmp_result = ble_uuid_cmp(&predef_uuid.u, &service->uuid.u16.u);
			if( uuid_cmp_result == 0)
			{
				puts("service found\n");
				ble_gattc_disc_chrs_by_uuid(m_connection_handle,service->start_handle,
						service->end_handle , &heart_rate_uuid.u,
						find_heart_rate_chr_cb, NULL);
			}
		}
		else
		{
			printf("discovery event error 0x%x\n", error->status);
		}

		return 0;
}
/* connection has separate event handler from scan */
static int
conn_event(struct ble_gap_event *event, void *arg)
{
	puts("conn_event");
	(void)arg;
	switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            puts("Connection was established\n");
            //ble_gap_terminate(event->connect.conn_handle, 0x13);
	    m_connection_handle = event->connect.conn_handle;

	    ble_gattc_disc_svc_by_uuid(m_connection_handle, &predef_uuid.u, gatt_discovery_cb, NULL);

        } else {
		printf("Connection failed, error code: 0x%x\n",event->connect.status);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("Disconnected, reason code: %x\n", event->disconnect.reason);
        break;
    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        puts("Connection update request received\n");
        break;
    case BLE_GAP_EVENT_CONN_UPDATE:
        if (event->conn_update.status == 0) {
            puts("Connection update successful\n");
        } else {
            printf("Connection update failed; reson: %d\n", event->conn_update.status);
        }
        break;
    default:
        printf("Connection event type not supported, %d\n",
                    event->type);
        break;
    }
    return 0;
}

static int
scan_event(struct ble_gap_event *event, void *arg)
{
	puts("Hello scan event");

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
		printf("\n uuids16.%d %x",counter, parsed_fields.uuids16[counter].value);
		if (uuid_cmp_result) {
			puts("UUID doesn't fit\n");
			//            printf("UUID: ");
			//		    uint8_t counter;
			//		    for (counter = 5; counter != 0; counter--)
			//		    {
			//			    printf( "%x:",event->disc.addr.val[counter]);
			//		    }
			//		    puts("\n");
			//
			//		    for (counter = 15; counter != 0; counter--)
			//		    {
			//			    printf("%x",parsed_fields.uuids128->value[counter]);
			//		    }
			//		    if (parsed_fields.num_uuids16 > 0)
			//		    {
			//			    for (counter = 0; counter < parsed_fields.num_uuids16; counter++)
			//			    {
			//				    printf("\n uuids16.%d %x",counter, parsed_fields.uuids16[counter].value);
			//			    }
			//		    }
			//		    puts("\n");
		} else {
			puts("UUID fits, connecting...\n");
			ble_gap_disc_cancel();
			ble_gap_connect(nimble_riot_own_addr_type, &(event->disc.addr), 10000,
					NULL, conn_event, NULL);
			break;
		}
		}
	}
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        printf("Discovery completed, reason: %d\n",
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

    //printf("ble_gattc_init: %d\n", ble_gattc_init());
    /* set scan parameters:
        - scan interval in 0.625ms units
        - scan window in 0.625ms units
        - filter policy - 0 if whitelisting not used
        - limited - should limited discovery be used
        - passive - should passive scan be used
        - filter duplicates - 1 enables filtering duplicated advertisements */
    const struct ble_gap_disc_params scan_params = {10000, 500, 0, 0, 0, 0};

    /* performs discovery procedure */
    rc = ble_gap_disc(nimble_riot_own_addr_type, 10000, &scan_params,scan_event, NULL);
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

int _cmd_connect(int argc, char **argv)
{

    uint32_t timeout = DEFAULT_DURATION_MS;

    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: no arguments\n");

        return 0;
    }
    if (argc >= 2) {
        timeout = atoi(argv[1]);
    }
    (void) timeout;

    puts("Trying to connect");
    scan();
    puts("Scanning started");
    puts("");

    return 0;
}

int _cmd_disconnect(int argc, char **argv)
{


    if ((argc == 2) && (memcmp(argv[1], "help", 4) == 0)) {
        printf("usage: no arguments\n");

        return 0;
    }

    int rc = ble_gap_terminate(m_connection_handle, BLE_ERR_REM_USER_CONN_TERM);

    printf("Disconnected with Code: %x\n", rc);
    puts("");

    return 0;
}

static const shell_command_t _commands[] = {
    { "scan", "trigger a BLE scan", _cmd_scan },
    { "connect", "trigger a BLE scan", _cmd_connect },
    { "disconnect", "disconnect from device", _cmd_disconnect },
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

static int heart_rate_attr_cb(uint16_t conn_handle,
                             const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr,
                             void *arg)
{
	puts("gattc event");
	(void)conn_handle;
	(void)arg;
	uint16_t counter;
	puts("\nData:\n");
	if (attr != NULL && attr->om != NULL)
	{
		struct os_mbuf *databuf = attr->om;
		if ( databuf->om_len > 0 && databuf->om_data != NULL)
		{
			for (counter = 0; counter < databuf->om_len; counter++ )
			{
				printf("%u", databuf->om_data[counter]);
			}
		}
	}
	puts("\nEnd:\n");
	printf("error 0x%x\n", error->status);
return 0;
}

static int find_heart_rate_chr_cb(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            const struct ble_gatt_chr *chr, void *arg)
{
	puts("chr event");
	(void)conn_handle;
	(void)arg;
	if ( error->status != 0 )
	{
		printf("error 0x%x\n", error->status);
	}
	else
	{
		
		ble_gattc_read(m_connection_handle, chr->def_handle,
				heart_rate_attr_cb, NULL);

	}
	//uint16_t counter;
	//

//int ble_gatts_notify(uint16_t conn_handle, uint16_t chr_val_handle);
return 0;

}
