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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <string.h>
#include <dlog.h>
#include <vconf.h>
#include <vconf-internal-bt-keys.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-adapter.h"
#include "bt-service-device.h"
#include "bt-service-audio.h"
#include "bt-service-headset-connection.h"
#include "bt-service-opp-client.h"



static GList *p_connection_list = NULL;
typedef enum {
	BLUETOOTH_NONE_CONNECTED = 0x00,
	BLUETOOTH_HFP_CONNECTED ,
	BLUETOOTH_A2DP_CONNECTED,
	BLUETOOTH_ALL_CONNECTED,
} bluetooth_state_type_t;

typedef struct {
	bluetooth_state_type_t state;
	bluetooth_device_info_t dev_info;
	int connection_type;
} bt_connection_node_info_t;

gboolean connection_local = FALSE;


void _bt_headset_set_local_connection(gboolean value)
{
	BT_INFO("setting connection_local to %d", value);
	connection_local = value;
}

gboolean _bt_headset_get_local_connection()
{
	return connection_local;
}

//gint compare_state(GList *data, bluetooth_state_type_t state)
gint compare_state(gconstpointer list_data, gconstpointer conn_state)
{
	GList *data = (GList *) list_data;
	bluetooth_state_type_t *state = (bluetooth_state_type_t *)conn_state;
	bt_connection_node_info_t *p_data = (bt_connection_node_info_t *) data;
	if (p_data->state == *state) {
		BT_INFO("State Already Added");
		return 0;
	}
	return 1;
}

gboolean connect_remote_media_audio(gpointer user_data)
{
	bt_connection_node_info_t *conn_info =
			(bt_connection_node_info_t *) user_data;
	GList *list = NULL;
	bluetooth_state_type_t state;

	char remote_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	_bt_convert_addr_type_to_string(remote_address, conn_info->dev_info.device_address.addr);
	if (p_connection_list == NULL) {
		BT_INFO("None of device connected and this hasbeen triggered");
		return FALSE;
	}
	if (conn_info->connection_type == BT_AUDIO_A2DP) {
		state = BLUETOOTH_A2DP_CONNECTED;
		list = g_list_find_custom(p_connection_list,
				&state, compare_state);
		if (list == NULL) {
			BT_INFO("Head Set didn't initiated a2dp connection");
			BT_INFO("local device initiating A2DP connection");
			_bt_audio_connect(0, BT_AUDIO_A2DP,
					&conn_info->dev_info.device_address, NULL);
		} else {
			BT_INFO("A2DP Connection Already exists");
		}
		g_free(conn_info);
	} else {
		state = BLUETOOTH_HFP_CONNECTED;
		list = g_list_find_custom(p_connection_list,
				&state, compare_state);
		if (list == NULL) {
			BT_INFO("Headset didn't initiated HFP connection");
			BT_INFO("local device intiating HFP Connection");
			_bt_audio_connect(0, BT_AUDIO_HSP,
					&conn_info->dev_info.device_address, NULL);
		} else {
			BT_INFO("HFP Connection Already exists");
		}
		g_free(conn_info);
	}
	return FALSE;
}

void _bt_get_bluetooth_device_info(char *remote_address, bluetooth_device_info_t *device)
{
	GArray *dev_list = NULL;
	int size,i=0;
	bluetooth_device_info_t info;
	char bond_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	dev_list = g_array_new (FALSE, FALSE, sizeof(gchar));
	if (device == NULL)
		return;
	_bt_get_bonded_devices(&dev_list);
	size = (dev_list->len) / sizeof(bluetooth_device_info_t);
	for (i=0; i < size; i++) {
		info = g_array_index(dev_list, bluetooth_device_info_t, i);
		_bt_convert_addr_type_to_string(bond_address, info.device_address.addr);
		if (strcmp(bond_address, remote_address) == 0) {
			BT_INFO("Match found");
			memcpy(device, &info, sizeof(bluetooth_device_info_t));
			g_array_free(dev_list, TRUE);
			return;
		}
	}
	g_array_free(dev_list, TRUE);
	return;
}

void _bt_headset_add_timer_function(int connection_type, bluetooth_device_info_t info)
{
	bt_connection_node_info_t *pass_conn_info = NULL;
	pass_conn_info = g_new0(bt_connection_node_info_t, 1);
	pass_conn_info->connection_type = connection_type;
	memcpy(&pass_conn_info->dev_info, &info, sizeof(bluetooth_device_info_t));
	/* This need to be freed in timer function */
	g_timeout_add(CONNECT_TIMEOUT, connect_remote_media_audio,
		pass_conn_info);
	return;
}

void _bt_start_timer_for_connection(char *remote_address, int connection_type)
{
	GArray *dev_list = NULL;
	int size,i=0,j;
	bluetooth_device_info_t info;
	char bond_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	dev_list = g_array_new (FALSE, FALSE, sizeof(gchar));
	_bt_get_bonded_devices(&dev_list);
	size = (dev_list->len) / sizeof(bluetooth_device_info_t);

	for (i=0; i < size; i++) {
		info = g_array_index(dev_list, bluetooth_device_info_t, i);
		j = 0;
		_bt_convert_addr_type_to_string(bond_address,
				info.device_address.addr);
		if (strcmp(bond_address, remote_address) != 0)
			continue;
		BT_INFO("Device address Matched");

		while (j != info.service_index) {
			BT_INFO("UUID %s", info.uuids[j]);
			if (connection_type == BT_AUDIO_A2DP) {
				if (strcmp(info.uuids[j], A2DP_SINK_UUID) == 0) {
					BT_INFO("Remote Device has A2DP Sink Support start timer");
					_bt_headset_add_timer_function(BT_AUDIO_A2DP, info);
					goto end;
				}
			} else {
				if (strcmp(info.uuids[j], HFP_HS_UUID) == 0) {
					BT_INFO("Remote Device has HFP Sink Support start timer");
					_bt_headset_add_timer_function(BT_AUDIO_HSP, info);
					goto end;
				}
			}
			j++;
		}
	}
end:
	g_array_free(dev_list, TRUE);
}

void __bt_connection_manager_set_state(char *remote_address, int event)
{
	bt_connection_node_info_t *info = g_new0(bt_connection_node_info_t, 1);

	char bond_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	if (event == BLUETOOTH_EVENT_AG_CONNECTED) {
		info->state = BLUETOOTH_HFP_CONNECTED;
		_bt_get_bluetooth_device_info(remote_address, &info->dev_info);
		_bt_convert_addr_type_to_string(bond_address,
				info->dev_info.device_address.addr);
		BT_INFO("Adding HFP Connected device to list");
		p_connection_list = g_list_append(p_connection_list, info);
	}
	else if (event == BLUETOOTH_EVENT_AG_DISCONNECTED) {
		/* Delete coresponding node */
		BT_INFO("Deleting HFP Connected device from list");
		GList *list = NULL;
		bluetooth_state_type_t state;
		bt_connection_node_info_t *h_conn;
		state = BLUETOOTH_HFP_CONNECTED;
		list = g_list_find_custom(p_connection_list,
				&state, compare_state);
		if (list == NULL) {
			BT_INFO("Didn't found any device with HFP State");
			return;
		}
		h_conn = list->data;
		p_connection_list = g_list_remove(p_connection_list, h_conn);
		g_free(h_conn);
	} else if (event == BLUETOOTH_EVENT_AV_CONNECTED) {
		info->state = BLUETOOTH_A2DP_CONNECTED;
		_bt_get_bluetooth_device_info(remote_address, &info->dev_info);
		_bt_convert_addr_type_to_string(bond_address,
				info->dev_info.device_address.addr);
		BT_INFO("Adding A2DP Connected device to list");
		p_connection_list = g_list_append(p_connection_list, info);
	} else if (event == BLUETOOTH_EVENT_AV_DISCONNECTED) {
		BT_INFO("Deleting A2DP Connected device from list");
		bt_connection_node_info_t *a_conn;
		GList *list = NULL;
		bluetooth_state_type_t state;
		state = BLUETOOTH_A2DP_CONNECTED;
		list = g_list_find_custom(p_connection_list,
				&state, compare_state);
		if (list == NULL) {
			BT_INFO("Didn't found any device with A2DP State");
			return;
		}
		a_conn = list->data;
		p_connection_list = g_list_remove(p_connection_list, a_conn);
		g_free(a_conn);
	}
}

