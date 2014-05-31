/*
 * Bluetooth-frwk
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Hocheol Seo <hocheol.seo@samsung.com>
 *		 Girishashok Joshi <girish.joshi@samsung.com>
 *		 Chanyeol Park <chanyeol.park@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/**
 * @file       bluetooth-gatt-test.c
 * @brief      This is the source file for bluetooth framework test suite.
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "bluetooth-api.h"


#define PRT(format, args...) printf("%s:%d() "format, __FUNCTION__, __LINE__, ##args)
#define TC_PRT(format, args...) PRT(format"\n", ##args)

#define TC_PASS 1
#define TC_FAIL 0

GMainLoop *main_loop = NULL;

typedef struct
{
	const char *tc_name;
	int tc_code;
} tc_table_t;

tc_table_t tc_table[] =
{
	{"SetAdvertising ON"		, 1},
	{"SetAdvertising OFF"		, 2},
	{"SetCustomAdvertising ON, filter 0x03"		, 3},
	{"SetCustomAdvertising ON, filter 0x00"		, 4},
	{"SetAdvertisingData"		, 5},
	{"SetScanParameters"		,6},
	{"SetScanResponseData"	,7},
	{"Start LE Scan"		,8},
	{"Stop LE Scan"		,9},
	{"IsAdvertising"	,10},
	{"Add White List"	,11},
	{"Remove White List"	,12},
	{"Clear White List"	,13},
	{"Set Connectable ON"	,14},
	{"Set Connectable OFF"	,15},

	/* -----------------------------------------*/
	{"Finish"					, 0x00ff},
	{NULL					, 0x0000},

};

#define tc_result(success, tc_index) \
	TC_PRT("Test case [%d - %s] %s", tc_table[tc_index].tc_code, tc_table[tc_index].tc_name, ((success == TC_PASS)?"Success":"Failed"));


void tc_usage_print(void)
{
	int i = 0;

	while (tc_table[i].tc_name) {
		if (tc_table[i].tc_code != 0x00ff) {
			TC_PRT("Key %d : usage %s", tc_table[i].tc_code,
							tc_table[i].tc_name);
		} else {
			TC_PRT("Key %d : usage %s\n\n", 0x00ff,
							tc_table[i].tc_name);
		}

		i++;
	}
}

int test_input_callback(void *data)
{
	int ret = 0;
	int test_id = (int)data;

	switch (test_id) {
	case 0x00ff:
		TC_PRT("Finished");
		g_main_loop_quit(main_loop);
		break;
	case 1:
		TC_PRT("SetAdvertising ON");
		ret = bluetooth_set_advertising(TRUE);
		break;
	case 2:
		TC_PRT("SetAdvertising OFF");
		ret = bluetooth_set_advertising(FALSE);
		break;
	case 3:
		TC_PRT("SetCustomAdvertising ON, Filter 0x03");
		ret = bluetooth_set_custom_advertising(TRUE, 1280, 1280, 0x03);
		break;
	case 4:
		TC_PRT("SetCustomAdvertising ON, Filter 0x00");
		ret = bluetooth_set_custom_advertising(TRUE, 1280, 1280, 0x00);
		break;
	case 5: {
		TC_PRT("SetAdvertisingData");
		bluetooth_advertising_data_t adv;
		guint8 data[6]  = {0x05, 0xFF, 0x02, 0x03, 0x04, 0x05};

		TC_PRT("%x %x %x %x %x %x", data[0], data[1], data[2], data[3],
				data[4], data[5]);
		memcpy(adv.data, data, sizeof(data));
		ret = bluetooth_set_advertising_data(&adv, sizeof(data));
		break;
	}
	case 6:
		TC_PRT("SetScanParameters");
	//	ret = bluetooth_set_scan_parameters(1280, 160 /* 80 */);
		break;
	case 7: {
		TC_PRT("SetScanResponseData");
		bluetooth_scan_resp_data_t rsp;
		guint8 data[7]  = {0x06, 0xFF, 0x02, 0x03, 0x04, 0x05, 0x06};

		TC_PRT("%x %x %x %x %x %x %x", data[0], data[1], data[2],
				data[3], data[4], data[5], data[6]);
		memcpy(rsp.data, data, sizeof(data));
		ret = bluetooth_set_scan_response_data(&rsp, sizeof(data));
		break;
	}
	case 8:
		TC_PRT("Start LE Scan");
		ret = bluetooth_start_custom_discovery(DISCOVERY_ROLE_LE, 0, 0, 0);
		break;
	case 9:
		TC_PRT("Stop LE Scan");
		ret = bluetooth_cancel_discovery();
		break;
	case 10: {
		TC_PRT("IsAdvertising");
		gboolean advertising = FALSE;
		int ret;

		ret = bluetooth_is_advertising(&advertising);
		if (ret == BLUETOOTH_ERROR_NONE)
			TC_PRT("Advertising is %s", advertising ? "started" : "stopped");
		else
			TC_PRT("bluetooth_is_advertising failed with [%d]", ret);
		break;
	}
	case 11: {
		bluetooth_device_address_t device_address={{0x00,0x19,0x0E,0x11,0x56,0x06}};

		TC_PRT("Add White List");

		ret = bluetooth_add_white_list(&device_address);
		if (ret != BLUETOOTH_ERROR_NONE)
			TC_PRT("bluetooth_add_white_list failed with [%d]", ret);

		break;
	}
	case 12: {
		bluetooth_device_address_t device_address={{0x00,0x19,0x0E,0x11,0x56,0x06}};

		TC_PRT("Remove White List");

		ret = bluetooth_remove_white_list(&device_address);
		if (ret != BLUETOOTH_ERROR_NONE)
			TC_PRT("bluetooth_remove_white_list failed with [%d]", ret);

		break;
	}
	case 13: {
		TC_PRT("Clear White List");

		ret = bluetooth_clear_white_list();
		if (ret != BLUETOOTH_ERROR_NONE)
			TC_PRT("bluetooth_remove_white_list failed with [%d]", ret);

		break;
	}
	case 14: {
		TC_PRT("Set Connectable ON");

		ret = bluetooth_set_connectable(TRUE);
		if (ret != BLUETOOTH_ERROR_NONE)
			TC_PRT("bt_adapter_set_connectable failed with [%d]", ret);

		break;
	}
	case 15: {
		TC_PRT("Set Connectable OFF");

		ret = bluetooth_set_connectable(FALSE);
		if (ret != BLUETOOTH_ERROR_NONE)
			TC_PRT("bt_adapter_set_connectable failed with [%d]", ret);

		break;
	}

	default:
		break;
	}

	TC_PRT("Result : %d", ret);
	return 0;
}

void startup()
{
	TC_PRT("bluetooth framework TC startup");

	if (!g_thread_supported())
		g_thread_init(NULL);

	dbus_g_thread_init();

	g_type_init();
	main_loop = g_main_loop_new(NULL, FALSE);
}

void cleanup()
{
	TC_PRT("bluetooth framework TC cleanup");
	if ( main_loop != NULL)
		g_main_loop_unref(main_loop);
}

void bt_event_callback(int event, bluetooth_event_param_t* param,
							void *user_data)
{
	TC_PRT(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	TC_PRT("bt event callback 0x%04x", event);
	switch(event) {
	case BLUETOOTH_EVENT_DISCOVERY_STARTED:
		TC_PRT("BLUETOOTH_EVENT_DISCOVERY_STARTED, result [0x%04x]", param->result);
		break;

	case BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND:
	{
		bluetooth_device_info_t *device_info = NULL;
		TC_PRT("BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND, result [0x%04x]", param->result);
		device_info  = (bluetooth_device_info_t *)param->param_data;
		TC_PRT("dev [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", \
			device_info->device_address.addr[0], device_info->device_address.addr[1], device_info->device_address.addr[2], \
			device_info->device_address.addr[3], device_info->device_address.addr[4], device_info->device_address.addr[5]);
		break;
	}

	case BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED:
	{
		bluetooth_device_info_t *device_info = NULL;
		TC_PRT("BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED, result [0x%04x]", param->result);
		device_info  = (bluetooth_device_info_t *)param->param_data;
		TC_PRT("dev [%s] [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", device_info->device_name.name, \
			device_info->device_address.addr[0], device_info->device_address.addr[1], device_info->device_address.addr[2], \
			device_info->device_address.addr[3], device_info->device_address.addr[4], device_info->device_address.addr[5]);
		break;
	}

	case BLUETOOTH_EVENT_DISCOVERY_FINISHED:
		TC_PRT("BLUETOOTH_EVENT_DISCOVERY_FINISHED, result [0x%04x]", param->result);
		break;

	case BLUETOOTH_EVENT_ADVERTISING_STARTED:
		TC_PRT("BLUETOOTH_EVENT_ADVERTISING_STARTED, result [0x%04x], "
				"interval_min [%d ms], interval_max [%d ms]",
				param->result,
				((bluetooth_advertising_params_t *)param->param_data)->interval_min,
				((bluetooth_advertising_params_t *)param->param_data)->interval_max);
		break;

	case BLUETOOTH_EVENT_ADVERTISING_STOPPED:
		TC_PRT("BLUETOOTH_EVENT_ADVERTISING_STOPPED, result [0x%04x]", param->result);
		break;

	default:
		TC_PRT("received event [0x%04x]", event);
		break;
	}
	TC_PRT("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

static gboolean key_event_cb(GIOChannel *chan, GIOCondition cond, gpointer data)
{
	char buf[10] = {0};
	unsigned int len = 0;
	int test_id;

	if (g_io_channel_read(chan, buf, sizeof(buf), &len) !=
							G_IO_ERROR_NONE) {
		printf("IO Channel read error");
		return FALSE;
	}
	printf("%s\n",buf);
	tc_usage_print();

	test_id = atoi(buf);

	if (test_id)
		g_idle_add(test_input_callback, (void*)test_id);

	return TRUE;
}

int main()
{
	int ret_val;
	GIOChannel *key_io;

	startup();

	/* Register callback function */
	TC_PRT("TC : %s", tc_table[0].tc_name);
	ret_val = bluetooth_register_callback(bt_event_callback, NULL);
	if (ret_val >= BLUETOOTH_ERROR_NONE) {
		TC_PRT("bluetooth_register_callback returned Success");
		tc_result(TC_PASS, 0);
	} else {
		TC_PRT("bluetooth_register_callback returned failiure [0x%04x]", ret_val);
		tc_result(TC_FAIL, 0);
		return 0;
	}

	ret_val = bluetooth_check_adapter();
	if (ret_val < BLUETOOTH_ERROR_NONE) {
		TC_PRT("bluetooth_check_adapter returned failiure [0x%04x]", ret_val);
		tc_result(TC_FAIL, 3);
	} else {
		TC_PRT("BT state [0x%04x]", ret_val);
		tc_result(TC_PASS, 3);
	}

	key_io = g_io_channel_unix_new(fileno(stdin));

	g_io_add_watch(key_io, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			key_event_cb, NULL);
	g_io_channel_unref(key_io);

	g_main_loop_run(main_loop);

	cleanup();
	return 0;
}
