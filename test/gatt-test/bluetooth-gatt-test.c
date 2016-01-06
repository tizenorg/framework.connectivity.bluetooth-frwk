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
	{"Send alert to remote le device"		, 1},
	{"Set Link loss alert"		, 2},

	/* -----------------------------------------*/
	{"Finish"					, 0x00ff},
	{NULL					, 0x0000},

};

#define tc_result(success, tc_index) \
	TC_PRT("Test case [%d - %s] %s", tc_table[tc_index].tc_code, tc_table[tc_index].tc_name, ((success == TC_PASS)?"Success":"Failed"));

char *g_alert_char_handle = NULL;
guint8 g_alert_level = 0;

#define IMMEDIATE_ALERT_UUID	"00001802-0000-1000-8000-00805f9b34fb"
#define LINK_LOSS_UUID		"00001803-0000-1000-8000-00805f9b34fb"
#define ALERT_LEVEL_CHR_UUID	"2a06"

#define BD_ADDR_FILE "/opt/remote-bd"

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

static void convert_addr_string_to_addr_type(bluetooth_device_address_t *addr,
							const char *address)
{
	char *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;

	if (!address || !addr)
		return;

	addr->addr[0] = strtol(address, &ptr5, 16);
	addr->addr[1] = strtol(ptr5 + 1, &ptr4, 16);
	addr->addr[2] = strtol(ptr4 + 1, &ptr3, 16);
	addr->addr[3] = strtol(ptr3 + 1, &ptr2, 16);
	addr->addr[4] = strtol(ptr2 + 1, &ptr1, 16);
	addr->addr[5] = strtol(ptr1 + 1, NULL, 16);
}

char * get_bd_from_file(char *filename)
{
	int fd;
	char *buf;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror("Can't open file");
		return NULL;
	}

	buf = g_malloc0(20);
	/* Fix : NULL_RETURNS */
	if (buf == NULL) {
		close(fd);
		return NULL;
	}

	if (read(fd, buf, 17) < 17) {
		perror("Can't load firmware");
		g_free(buf);
		close(fd);
		return NULL;
	}

	close(fd);

	return buf;
}

static void __accept_bdaddress(bluetooth_device_address_t *device_address)
{
	char str_address[20] = {0};
	char *addr;

	addr = get_bd_from_file(BD_ADDR_FILE);
	if (addr) {
		TC_PRT("Remote bd adress from file: %s", addr);
		convert_addr_string_to_addr_type(device_address, addr);
		g_free(addr);
		return;
	}

	TC_PRT("Enter bd address: ");
	int ret = 0;
	ret = scanf("%s", str_address);
	if (ret < 0)
		TC_PRT("Some read error");
	TC_PRT("You have entered bd address %s\n", str_address);
	convert_addr_string_to_addr_type(device_address, str_address);
}

static void __accept_alert_level()
{
	TC_PRT("Enter alert level \n 0 - no alert 1 - mild alert 2 - High alert : ");
	int ret = 0;
	ret = scanf("%c", &g_alert_level);
	if (ret < 0)
		TC_PRT("Some read error");
	TC_PRT("You have selected alert level %hu ", g_alert_level);
}

int test_input_callback(void *data)
{
	int ret = 0;
	int test_id = (int)data;
	bluetooth_device_address_t device_address;
	bt_gatt_service_property_t service;

	switch (test_id) {
	case 0x00ff:
		TC_PRT("Finished");
		g_main_loop_quit(main_loop);
		break;
	case 1:
		TC_PRT("Immediate Alert");
		__accept_bdaddress(&device_address);

		__accept_alert_level();

		if (g_alert_char_handle) {
			if (bluetooth_gatt_set_characteristics_value(g_alert_char_handle,
						&g_alert_level, 1) != BLUETOOTH_ERROR_NONE)
				TC_PRT("Set char val failed");

			return 0;
		}

		ret = bluetooth_gatt_get_service_from_uuid(&device_address,
							IMMEDIATE_ALERT_UUID,
							&service);
		if (ret != BLUETOOTH_ERROR_NONE) {
			TC_PRT(" bluetooth_gatt_get_service_from_uuid FAILED");
			return 0;
		}

		ret = bluetooth_gatt_get_char_from_uuid(service.handle,
							ALERT_LEVEL_CHR_UUID);
		if (ret != BLUETOOTH_ERROR_NONE) {
			TC_PRT(" bluetooth_gatt_get_char_from_uuid FAILED");
			return 0;
		}

		break;
	case 2:
		TC_PRT("Proximity Link loss alert");
		__accept_bdaddress(&device_address);

		__accept_alert_level();

		/* TODO */
		break;
	default:
		break;
	}

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

static void __handle_alert_char(char *char_handle,
					bt_gatt_char_property_t *char_pty)
{
	if (char_pty->val == NULL)
		TC_PRT("Read only char");
	else
		TC_PRT("Current Alert level [%d]", char_pty->val[0]);

	g_alert_char_handle = g_strdup(char_handle);

	if (bluetooth_gatt_set_characteristics_value(char_handle,
				&g_alert_level, 1) != BLUETOOTH_ERROR_NONE)
		TC_PRT("Set char val failed");

}

static gboolean __handle_char(char *char_handle,
					bt_gatt_char_property_t *char_pty)
{
	TC_PRT("char uuid %s", char_pty->uuid);

	if (g_strstr_len(char_pty->uuid, -1, ALERT_LEVEL_CHR_UUID) != NULL) {
		TC_PRT("Alert char recieved");
		__handle_alert_char(char_handle, char_pty);
		return TRUE;
	} /* Add else if for other chars*/

	return FALSE;
}

void bt_event_callback(int event, bluetooth_event_param_t* param,
							void *user_data)
{
	TC_PRT(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	TC_PRT("bt event callback 0x%04x", event);
	switch(event) {
	case BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID:
	{
		TC_PRT("BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID");
		if (param->result != 0) {
			TC_PRT("Failed!!!");
			return;
		}
		bt_gatt_char_property_t *char_pty = param->param_data;

		__handle_char(char_pty->handle, char_pty);

	}
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

	if (g_io_channel_read_chars(chan, buf, sizeof(buf), 
			&len, NULL) ==  G_IO_STATUS_ERROR) {
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
