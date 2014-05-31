/*
 * bluetooth-frwk
 *
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <syspopup_caller.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-audio.h"
#include "bt-service-adapter.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"

typedef struct {
	unsigned int type;
	int device_state;
	char device_address[BT_ADDRESS_STRING_SIZE + 1];
} bt_connected_headset_data_t;

static GList *g_connected_list;

static bt_headset_wait_t *g_wait_data;

static void __bt_remove_device_from_wait_list();

static void __bt_free_wait_data();

static void __bt_audio_request_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				    gpointer user_data)
{
	GError *g_error = NULL;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	int result = BLUETOOTH_ERROR_NONE;
	bt_function_data_t *func_data;
	request_info_t *req_info;

	dbus_g_proxy_end_call(proxy, call, &g_error, G_TYPE_INVALID);

	g_object_unref(proxy);

	func_data = user_data;

	if (func_data == NULL) {
		/* Send reply */
		BT_ERR("func_data == NULL");
		goto done;
	}

	req_info = _bt_get_request_info(func_data->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (g_error == NULL)
		goto dbus_return;

	BT_ERR("Audio Connect Dbus Call Error: %s\n", g_error->message);

	result = BLUETOOTH_ERROR_INTERNAL;

	/* Remove the device from the list */
	_bt_remove_headset_from_list(BT_AUDIO_ALL, func_data->address);

	/* Error, check if any waiting device is there */
	if (g_wait_data == NULL)
		goto dbus_return;

	if (g_strcmp0(g_wait_data->address, func_data->address) != 0) {
		bluetooth_device_address_t device_address;
		_bt_convert_addr_string_to_type(device_address.addr,
							g_wait_data->address);
		_bt_audio_connect(g_wait_data->req_id, g_wait_data->type,
				&device_address, g_wait_data->out_param1);
	}

	/* Event will be sent by the event reciever */
dbus_return:
	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	g_array_append_vals(out_param1, func_data->address,
				BT_ADDRESS_STR_LEN);
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);
done:
	if (g_error)
		g_error_free(g_error);

	if (func_data) {
		g_free(func_data->address);
		g_free(func_data);
	}
}

static char *__bt_get_audio_path(bluetooth_device_address_t *address)
{

	char *object_path = NULL;
	char addr_str[BT_ADDRESS_STRING_SIZE + 1] = { 0 };
	DBusGProxy *audio_proxy;
	DBusGProxy *adapter_proxy;
	DBusGConnection *g_conn;
	GError *error = NULL;

	retv_if(address == NULL, NULL);

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, NULL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, NULL);

	_bt_convert_addr_type_to_string(addr_str, address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice",
			  &error, G_TYPE_STRING, addr_str,
			  G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
			  &object_path, G_TYPE_INVALID);

	if (error != NULL) {
		BT_ERR("Failed to Find device: %s\n", error->message);
		g_error_free(error);
		return NULL;
	}

	retv_if(object_path == NULL, NULL);

	audio_proxy = dbus_g_proxy_new_for_name(g_conn,
					BT_BLUEZ_NAME,
					object_path,
					BT_HEADSET_INTERFACE);

	retv_if(audio_proxy == NULL, NULL);

	g_object_unref(audio_proxy);

	return object_path;
}

static char *__bt_get_connected_audio_path(void)
{
	int i;
	guint size;
	char *audio_path = NULL;
	GArray *device_list;
	bluetooth_device_info_t info;

	/* allocate the g_pointer_array */
	device_list = g_array_new(FALSE, FALSE, sizeof(gchar));

	if (_bt_get_bonded_devices(&device_list)
					!= BLUETOOTH_ERROR_NONE) {
		g_array_free(device_list, TRUE);
		return NULL;
	}

	size = device_list->len;
	size = (device_list->len) / sizeof(bluetooth_device_info_t);

	for (i = 0; i < size; i++) {

		info = g_array_index(device_list,
				bluetooth_device_info_t, i);

		if (info.connected == TRUE) {
			audio_path = __bt_get_audio_path(&info.device_address);
			if (audio_path)
				break;
		}
	}

	g_array_free(device_list, TRUE);

	return audio_path;
}

static void __bt_free_wait_data()
{
	if (g_wait_data != NULL) {
		g_free(g_wait_data->address);
		g_free(g_wait_data);
		g_wait_data = NULL;
	}
}

static void __bt_remove_device_from_wait_list()
{
	/* Before deleting the request update the UI */
	GArray *out_param_1 = NULL;
	GArray *out_param_2 = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL;
	request_info_t *req_info;

	req_info = _bt_get_request_info(g_wait_data->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		return;
	}

	out_param_1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param_2 = g_array_new(FALSE, FALSE, sizeof(gchar));
	g_array_append_vals(out_param_1, g_wait_data->address,
				BT_ADDRESS_STR_LEN);
	g_array_append_vals(out_param_2, &result, sizeof(int));
	dbus_g_method_return(req_info->context,
				out_param_1, out_param_2);
	g_array_free(out_param_1, TRUE);
	g_array_free(out_param_2, TRUE);
	_bt_delete_request_list(g_wait_data->req_id);
}

static void __bt_set_headset_disconnection_type(const char *address)
{
	bt_connected_headset_data_t *connected_device;
	GList *node;

	BT_DBG("__bt_set_headset_disconnection_type \n");

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		connected_device = node->data;
		if (g_strcmp0(connected_device->device_address, address) == 0) {
			g_wait_data->disconnection_type = connected_device->type;
			return;
		}
		node = g_list_next(node);
	}
}

gboolean _bt_is_headset_type_connected(int type, char *address)
{
	GList *node;

	BT_DBG("_bt_is_headset_type_connected \n");

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		bt_connected_headset_data_t *connected_device = node->data;

		if (connected_device->type & type) {
			if (address != NULL)
				g_strlcpy(address, connected_device->device_address,
						BT_ADDRESS_STRING_SIZE + 1);
			return TRUE;
		}

		node = g_list_next(node);
	}
	return FALSE;
}

static gboolean __bt_is_headset_connected(int type, int req_id,
				const char *address, GArray **out_param1)
{
	gboolean connected;
	char connected_address[BT_ADDRESS_STRING_SIZE + 1];
	bluetooth_device_address_t device_address;

	BT_DBG("__bt_is_headset_connected \n");

	/* Check if any other headset is connected */
	connected = _bt_is_headset_type_connected(type, connected_address);

	if (!connected)
		return FALSE;

	BT_DBG("connected headset %s", connected_address);

	if (g_strcmp0(connected_address, address) == 0)
		return TRUE;

	/* If already one device is waiting, remove current waiting device and add new */
	if (g_wait_data != NULL) {
		if (g_strcmp0(g_wait_data->address, address) != 0) {
			__bt_remove_device_from_wait_list();
			__bt_free_wait_data();
		}
	}

	if (g_wait_data == NULL) {
		g_wait_data = g_malloc0(sizeof(bt_headset_wait_t));
		g_wait_data->address = g_strdup(address);
		g_wait_data->req_id = req_id;
		g_wait_data->type = type;
		g_wait_data->ag_flag = FALSE;
		g_wait_data->out_param1 = out_param1;

		/* Set disconnection type */
		__bt_set_headset_disconnection_type(connected_address);
	}

	/* Convert BD adress from string type */
	_bt_convert_addr_string_to_type(device_address.addr, connected_address);
	_bt_audio_disconnect(0, type, &device_address, NULL);
	return TRUE;
}

void _bt_set_audio_wait_data_flag(gboolean flag)
{
	BT_DBG("_bt_set_audio_wait_data_flag \n");
	g_wait_data->ag_flag = flag;
}

bt_headset_wait_t *_bt_get_audio_wait_data(void)
{
	BT_DBG("_bt_get_audio_wait_data \n");
	return g_wait_data;
}

void _bt_add_headset_to_list(int type, int status, const char *address)
{
	bt_connected_headset_data_t *connected_device;
	bt_connected_headset_data_t *device;
	GList *node;

	BT_DBG("_bt_add_headset_to_list \n");

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		device = (bt_connected_headset_data_t *)node->data;

		if (g_strcmp0(device->device_address, address) == 0) {
			BT_DBG("Address match, update connection type \n");
			device->type |= type;
			device->device_state = status;
			return;
		}
		node = g_list_next(node);
	}

	connected_device = g_malloc0(sizeof(bt_connected_headset_data_t));
	connected_device->type |= type;
	connected_device->device_state = status;
	g_strlcpy(connected_device->device_address, address,
				sizeof(connected_device->device_address));
	g_connected_list = g_list_append(g_connected_list, connected_device);
}

void _bt_remove_headset_from_list(int type, const char *address)
{
	GList *node;

	BT_DBG("_bt_remove_headset_from_list \n");

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		bt_connected_headset_data_t *connected_device = node->data;

		if (g_strcmp0(connected_device->device_address, address) != 0) {
			node = g_list_next(node);
			continue;
		}

		BT_DBG("Address match \n");

		BT_DBG("Connection type = %x\n", connected_device->type);

		switch (type) {
		case BT_AUDIO_A2DP:
			if (connected_device->type & BT_AUDIO_A2DP)
				connected_device->type &= ~(BT_AUDIO_A2DP);
			break;
		case BT_AUDIO_HSP:
			if (connected_device->type & BT_AUDIO_HSP)
				connected_device->type &= ~(BT_AUDIO_HSP);
			break;
		case BT_AUDIO_ALL:
			if (connected_device->type & BT_AUDIO_ALL)
				connected_device->type &= ~(BT_AUDIO_ALL);
			break;
		}

		BT_DBG("Connection type = %x\n", connected_device->type);

		if (connected_device->type == 0x00) {
			g_connected_list = g_list_remove(g_connected_list, connected_device);
			g_free(connected_device);
		}

		node = g_list_next(node);
	}
}

int _bt_audio_connect(int request_id, int type,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	gchar *device_path = NULL;
	char *interface;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *profile_proxy;
	DBusGConnection *g_conn;

	BT_CHECK_PARAMETER(device_address, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	switch (type) {
	case BT_AUDIO_HSP:
		interface = BT_HEADSET_INTERFACE;
		break;
	case BT_AUDIO_A2DP:
		interface = BT_SINK_INTERFACE;
		break;
	case BT_AUDIO_ALL:
		interface = BT_AUDIO_INTERFACE;
		break;
	default:
		BT_ERR("Unknown role");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	if (__bt_is_headset_connected(type, request_id, address, out_param1))
		return BLUETOOTH_ERROR_NONE;

	adapter_proxy = _bt_get_adapter_proxy();
	if (adapter_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	g_conn = _bt_get_system_gconn();
	if (g_conn == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		result = BLUETOOTH_ERROR_NOT_PAIRED;
		goto fail;
	}

	profile_proxy = dbus_g_proxy_new_for_name(g_conn, BT_BLUEZ_NAME,
				      device_path, interface);

	g_free(device_path);

	if (profile_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	func_data = g_malloc0(sizeof(bt_function_data_t));
	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(profile_proxy, "Connect",
			(DBusGProxyCallNotify)__bt_audio_request_cb,
			func_data, NULL,
			G_TYPE_INVALID)) {
		BT_ERR("Audio connect Dbus Call Error");
		g_object_unref(profile_proxy);

		g_free(func_data->address);
		g_free(func_data);

		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}
	/* Add data to the connected list */
	_bt_add_headset_to_list(type, BT_STATE_CONNECTING, address);
	__bt_free_wait_data();

	return BLUETOOTH_ERROR_NONE;
fail:
	g_array_append_vals(*out_param1, address,
				BT_ADDRESS_STR_LEN);

	return result;
}

int _bt_audio_disconnect(int request_id, int type,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	gchar *device_path = NULL;
	char *interface;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *profile_proxy;
	DBusGConnection *g_conn;

	BT_CHECK_PARAMETER(device_address, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	switch (type) {
	case BT_AUDIO_HSP:
		interface = BT_HEADSET_INTERFACE;
		break;
	case BT_AUDIO_A2DP:
		interface = BT_SINK_INTERFACE;
		break;
	case BT_AUDIO_ALL:
		interface = BT_AUDIO_INTERFACE;
		break;
	default:
		BT_ERR("Unknown role");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	adapter_proxy = _bt_get_adapter_proxy();
	if (adapter_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	g_conn = _bt_get_system_gconn();
	if (g_conn == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		result = BLUETOOTH_ERROR_NOT_PAIRED;
		goto fail;
	}

	profile_proxy = dbus_g_proxy_new_for_name(g_conn, BT_BLUEZ_NAME,
				      device_path, interface);

	g_free(device_path);

	if (profile_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	if (g_wait_data != NULL) {
		if (!dbus_g_proxy_begin_call(profile_proxy, "Disconnect",
				NULL, NULL, NULL, G_TYPE_INVALID)) {
			BT_ERR("Audio disconnect Dbus Call Error");
			g_object_unref(profile_proxy);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	} else {
		func_data = g_malloc0(sizeof(bt_function_data_t));
		func_data->address = g_strdup(address);
		func_data->req_id = request_id;
		if (!dbus_g_proxy_begin_call(profile_proxy, "Disconnect",
				(DBusGProxyCallNotify)__bt_audio_request_cb,
				func_data, NULL,
				G_TYPE_INVALID)) {
			BT_ERR("Audio disconnect Dbus Call Error");
			g_object_unref(profile_proxy);

			g_free(func_data->address);
			g_free(func_data);

			result = BLUETOOTH_ERROR_INTERNAL;
			goto fail;
		}
	}

	return BLUETOOTH_ERROR_NONE;
fail:
	if (out_param1 != NULL)
		g_array_append_vals(*out_param1, address,
				BT_ADDRESS_STR_LEN);

	return result;
}

int _bt_audio_get_speaker_gain(unsigned int *gain)
{
	char *device_path = NULL;
	DBusGProxy *adapter_proxy;
	DBusGProxy *profile_proxy;
	DBusGConnection *g_conn;
	GHashTable *hash = NULL;
	GValue *value;

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = __bt_get_connected_audio_path();
	retv_if(device_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);

	profile_proxy = dbus_g_proxy_new_for_name(g_conn, BT_BLUEZ_NAME,
				      device_path, BT_HEADSET_INTERFACE);

	g_free(device_path);

	retv_if(profile_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(profile_proxy, "GetProperties", NULL,
			G_TYPE_INVALID,
			dbus_g_type_get_map("GHashTable",
			G_TYPE_STRING, G_TYPE_VALUE),
			&hash, G_TYPE_INVALID);

	g_object_unref(profile_proxy);

	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	value = g_hash_table_lookup(hash, "SpeakerGain");
	*gain = value ? g_value_get_uint(value) : 0;
	g_hash_table_destroy(hash);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_audio_set_speaker_gain(unsigned int gain)
{
	char *device_path = NULL;
	char *gain_str = "SpeakerGain";
	char sig[2] = {DBUS_TYPE_UINT16, '\0'};
	int ret = BLUETOOTH_ERROR_NONE;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessageIter value;
	DBusConnection *conn;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = __bt_get_connected_audio_path();
	retv_if(device_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME,
			device_path, BT_HEADSET_INTERFACE,
			"SetProperty");

	g_free(device_path);

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING,
			&gain_str);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
			sig, &value);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_UINT16,
			&gain);
	dbus_message_iter_close_container(&iter, &value);

	if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL)
		dbus_message_set_no_reply(msg, TRUE);

	if (!dbus_connection_send(conn, msg, NULL)) {
		BT_ERR("Dbus sending failed\n");
		ret = BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_message_unref(msg);

	return ret;
}
