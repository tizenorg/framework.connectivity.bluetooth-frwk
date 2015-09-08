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
#include "bt-service-device.h"
#include "bt-service-event.h"
#include "bt-service-util.h"

#include "bt-service-headset-connection.h"

#ifdef TIZEN_SUPPORT_DUAL_HF
#ifdef TIZEN_WEARABLE
#define VCONF_KEY_BT_HOST_BT_MAC_ADDR "db/wms/host_bt_mac"
#endif
#endif

typedef struct {
	unsigned int type;
	int device_state;
	char device_address[BT_ADDRESS_STRING_SIZE + 1];
} bt_connected_headset_data_t;

static GList *g_connected_list;

static bt_headset_wait_t *g_wait_data;

static bt_audio_function_data_t *pdata;

static void __bt_remove_device_from_wait_list();

static void __bt_free_wait_data();

static gboolean __bt_device_support_uuid(char *remote_address,
				bt_audio_type_t type);

static void __bt_hf_request_cb(DBusGProxy *proxy, DBusGProxyCall *call,
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

	BT_ERR("HFG request Dbus Call Error: %s\n", g_error->message);

	result = BLUETOOTH_ERROR_INTERNAL;

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

void _bt_audio_check_pending_connect()
{
	BT_DBG("+");
	bluetooth_device_address_t device_address;

	if (pdata == NULL)
		return;

	if (pdata->pending == BT_PENDING_CONNECT) {

		_bt_convert_addr_string_to_type(device_address.addr,
				pdata->address);
		_bt_audio_connect(pdata->req_id,
				BT_AUDIO_A2DP,
				&device_address,
				pdata->out_param);

		g_free(pdata->address);
		g_free(pdata);
		pdata = NULL;
	}

	BT_DBG("-");
	return;
}

static void __bt_audio_request_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				    gpointer user_data)
{
	GError *g_error = NULL;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	bt_audio_function_data_t *func_data;

	request_info_t *req_info;

	dbus_g_proxy_end_call(proxy, call, &g_error, G_TYPE_INVALID);

	g_object_unref(proxy);

	func_data = user_data;

	if (func_data == NULL) {
		/* Send reply */
		BT_ERR("func_data == NULL");
		goto done;
	}

	if (func_data->pending != BT_PENDING_NONE && g_error == NULL) {

		bluetooth_device_address_t device_address;
		_bt_convert_addr_string_to_type(device_address.addr,
					func_data->address);

		if (func_data->pending == BT_PENDING_CONNECT) {

			if (__bt_device_support_uuid(func_data->address,
							BT_AUDIO_A2DP)) {

				pdata = g_new0(bt_audio_function_data_t, 1);
				pdata->req_id = func_data->req_id;
				pdata->out_param = func_data->out_param;
				pdata->address = strdup(func_data->address);
				pdata->pending = func_data->pending;
			} else
				goto check_req_info;

		} else {

			if (_bt_is_service_connected(func_data->address
							, BT_AUDIO_A2DP)) {
				_bt_audio_disconnect(func_data->req_id,
					BT_AUDIO_A2DP,
					&device_address,
					func_data->out_param);
			} else
				goto check_req_info;
		}

		goto done;
	}

check_req_info:
	req_info = _bt_get_request_info(func_data->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (g_error == NULL)
		goto dbus_return;

	BT_ERR("Audio Connect/Disconnect Dbus Call Error: %s\n", g_error->message);

	result = BLUETOOTH_ERROR_INTERNAL;

	/* Remove the device from the list */
	_bt_remove_headset_from_list(func_data->type, func_data->address);

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
	if (req_info->context == NULL) {
		BT_DBG("req_info->context is NULL");
		goto done;
	}

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

		if (info.connected != BLUETOOTH_CONNECTED_LINK_NONE) {
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

#ifdef TIZEN_SUPPORT_DUAL_HF
gboolean __bt_is_companion_device(const char *addr)
{
#ifdef TIZEN_WEARABLE
	char *host_device_address = NULL;
	host_device_address = vconf_get_str(VCONF_KEY_BT_HOST_BT_MAC_ADDR);

	if (!host_device_address) {
		BT_INFO("Failed to get a companion device address");
		return FALSE;
	}

	if (g_strcmp0(host_device_address, addr) == 0) {
		BT_INFO("addr[%s] is companion device", addr);
		return TRUE;
	}

	return FALSE;
#else
	/* TODO : Need to add companion device check condition for Phone models */
	return FALSE;
#endif
}
#endif

static int __bt_is_headset_connected(int type, int req_id,
				const char *address, GArray **out_param1)
{
	gboolean connected = FALSE;
	char connected_address[BT_ADDRESS_STRING_SIZE + 1];
	bluetooth_device_address_t device_address;
	bt_connected_headset_data_t *connected_device = NULL;
#ifdef TIZEN_SUPPORT_DUAL_HF
	gboolean is_companion_device = FALSE;
#endif

	/* Check if any other headset is connected */
	GList *node = NULL;;

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		connected_device = node->data;
		if ((connected_device->type & type) == type) {
			g_strlcpy(connected_address, connected_device->device_address,
					BT_ADDRESS_STRING_SIZE + 1);
#ifdef TIZEN_SUPPORT_DUAL_HF
			is_companion_device = __bt_is_companion_device(connected_address);
			BT_INFO(" is_companion_device[%d]", is_companion_device);

			if (!is_companion_device) {
				connected = TRUE;
				break;
			}
#else
			connected = TRUE;
			break;
#endif
		}
		node = g_list_next(node);
	}

	if (!connected)
		return BLUETOOTH_ERROR_NOT_CONNECTED;

	BT_DBG("connected headset %s", connected_address);

	if (g_strcmp0(connected_address, address) == 0)
		return BLUETOOTH_ERROR_ALREADY_CONNECT;
#ifdef TIZEN_SUPPORT_DUAL_HF
	else if (TRUE == __bt_is_companion_device(address))
		return BLUETOOTH_ERROR_NOT_CONNECTED;
#endif

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
	_bt_audio_disconnect(0, connected_device->type & type, &device_address, NULL);
	return BLUETOOTH_ERROR_NONE;
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

void _bt_rel_wait_data(void)
{
	BT_DBG("_bt_rel_wait_data \n");
	__bt_free_wait_data();
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
			if (status == BT_STATE_CONNECTED)
				device->type |= type;
			device->device_state = status;
			return;
		}
		node = g_list_next(node);
	}

	connected_device = g_malloc0(sizeof(bt_connected_headset_data_t));
	/* Fix : NULL_RETURNS */
	if (connected_device == NULL) {
		BT_ERR("No memory allocated");
		return;
	}

	connected_device->device_state = status;
	if (status == BT_STATE_CONNECTED)
		connected_device->type |= type;
	g_strlcpy(connected_device->device_address, address,
			sizeof(connected_device->device_address));
	g_connected_list = g_list_append(g_connected_list, connected_device);
}

int _bt_get_device_state_from_list(int type, const char *address)
{
	GList *node;
	bt_connected_headset_data_t *device;

	BT_DBG("+");
	node = g_list_first(g_connected_list);
	while (node != NULL) {
		device = (bt_connected_headset_data_t *)node->data;
		if (g_strcmp0(device->device_address, address) == 0) {
			BT_DBG("Device found");
			return device->device_state;
		}
		node = g_list_next(node);
	}

	BT_DBG("Device not found");
	return BLUETOOTH_ERROR_INTERNAL;
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
		case BT_AVRCP:
			if (connected_device->type & BT_AVRCP)
				connected_device->type &= ~(BT_AVRCP);
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

static gboolean __bt_device_support_uuid(char *remote_address,
				bt_audio_type_t type)
{
	GArray *dev_list = NULL;
	int size,i,j;
	bluetooth_device_info_t info;
	char bond_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gboolean ret = FALSE;

	BT_DBG("+");

	dev_list = g_array_new (FALSE, FALSE, sizeof(gchar));

	_bt_get_bonded_devices(&dev_list);
	size = (dev_list->len) / sizeof(bluetooth_device_info_t);

	for (i=0; i < size; i++) {
		info = g_array_index(dev_list, bluetooth_device_info_t, i);
		_bt_convert_addr_type_to_string(bond_address,
				info.device_address.addr);
		if (strcmp(bond_address, remote_address) != 0)
			continue;

		BT_INFO("Device address Matched");
		j = 0;
		while (j != info.service_index) {
			if (type == BT_AUDIO_HSP) {
				if (strcmp(info.uuids[j], HFP_HS_UUID) == 0) {
					BT_INFO("HFP HS UUID exists");
					ret = TRUE;
					goto end;
				}
			} else if (type == BT_AUDIO_A2DP) {
				if (strcmp(info.uuids[j], A2DP_SINK_UUID) == 0) {
					BT_INFO("A2DP SINK UUID exists");
					ret = TRUE;
					goto end;
				}
			}
			j++;
		}
	}
end:
	g_array_free(dev_list, TRUE);
	BT_DBG("-");
	return ret;
}

gboolean _bt_is_service_connected(char* address, int type)
{
	GList *node;

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		bt_connected_headset_data_t *conn_device = node->data;

		if ((g_strcmp0(conn_device->device_address, address) == 0) &&
			(conn_device->type & type)) {
				BT_INFO("Service connected");
				return TRUE;
		}

		node = g_list_next(node);
	}
	BT_INFO("Service not connected");
	return FALSE;
}

int _bt_audio_connect(int request_id, int type,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	DBusGProxy *adapter_proxy;
	DBusGConnection *g_conn;
	int ret;
	char *uuid;
	int value = BLUETOOTH_ERROR_NONE;
	bt_audio_function_data_t *func_data;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	func_data = g_malloc0(sizeof(bt_audio_function_data_t));
	/* Fix : NULL_RETURNS */
	if (func_data == NULL) {
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto fail;
	}

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;
	func_data->type = type;
	func_data->pending = BT_PENDING_NONE;
	func_data->out_param = out_param1;

	switch (type) {
	case BT_AUDIO_HSP:
		uuid = HFP_HS_UUID;
		break;
	case BT_AUDIO_A2DP:
		uuid = A2DP_SINK_UUID;
		break;
	case BT_AVRCP:
		uuid = AVRCP_TARGET_UUID;
		break;
	case BT_AUDIO_A2DP_SOURCE:
		uuid = A2DP_SOURCE_UUID;
		break;
	case BT_AUDIO_ALL:
		if (__bt_device_support_uuid(address, BT_AUDIO_HSP)) {
			uuid = HFP_HS_UUID;
			func_data->pending = BT_PENDING_CONNECT;
		} else if (__bt_device_support_uuid(address, BT_AUDIO_A2DP)) {
			uuid = A2DP_SINK_UUID;
		} else {
			BT_ERR("No audio role supported");
			result = BLUETOOTH_ERROR_SERVICE_NOT_FOUND;
			goto fail;
		}
		break;
	default:
		BT_ERR("Unknown role");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}
	BT_INFO("Connecting to service %s", uuid);

	value = __bt_is_headset_connected(type, request_id, address, out_param1);

	if (value == BLUETOOTH_ERROR_ALREADY_CONNECT) {
		return BLUETOOTH_ERROR_ALREADY_CONNECT;
	} else if (value == BLUETOOTH_ERROR_NOT_CONNECTED) {
		_bt_headset_set_local_connection(TRUE);
		ret = _bt_connect_profile(address, uuid,
				__bt_audio_request_cb, func_data);

		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("_bt_connect_profile Error");
			_bt_headset_set_local_connection(FALSE);
			g_free(func_data->address);
			g_free(func_data);
			return ret;
		}

		/* Add data to the connected list */
		_bt_add_headset_to_list(type, BT_STATE_CONNECTING, address);
	}

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
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_audio_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGConnection *g_conn;
	GList *node;
	int ret;
	char *uuid;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	func_data = g_malloc0(sizeof(bt_audio_function_data_t));
	/* Fix : NULL_RETURNS */
	if (func_data == NULL) {
		BT_ERR("Memory allocation error");
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto fail;
	}
	func_data->address = g_strdup(address);
	func_data->req_id = request_id;
	func_data->pending = BT_PENDING_NONE;
	func_data->out_param = out_param1;
	func_data->type = type;

	switch (type) {
	case BT_AUDIO_HSP:
		uuid = HFP_HS_UUID;
		break;
	case BT_AUDIO_A2DP:
		uuid = A2DP_SINK_UUID;
		break;
	case BT_AVRCP:
		uuid = AVRCP_TARGET_UUID;
		break;
	case BT_AUDIO_A2DP_SOURCE:
		uuid = A2DP_SOURCE_UUID;
		break;
	case BT_AUDIO_ALL:
		if (_bt_is_service_connected(address, BT_AUDIO_HSP)) {
			uuid = HFP_HS_UUID;
			func_data->pending = BT_PENDING_DISCONNECT;
		} else if (_bt_is_service_connected(address, BT_AUDIO_A2DP)) {
			uuid = A2DP_SINK_UUID;
		} else {
			BT_ERR("No audio service connected");
			result = BLUETOOTH_ERROR_NOT_CONNECTED;
			goto fail;
		}
		break;
	default:
		BT_ERR("Unknown role");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	BT_INFO("Disconnecting service %s", uuid);
	ret = _bt_disconnect_profile(address, uuid,
			__bt_audio_request_cb, func_data);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_disconnect_profile Error");
		g_free(func_data->address);
		g_free(func_data);
		return ret;
	}

	/*
	 *	This logic is added for dual HF mode issue.
	 */
	node = g_list_first(g_connected_list);
	while (node != NULL) {
		bt_connected_headset_data_t *connected_device = node->data;

		if (g_strcmp0(connected_device->device_address, address) == 0) {
			BT_DBG("Connection type update");
			type = connected_device->type;
			break;
		}
		node = g_list_next(node);
	}
	_bt_add_headset_to_list(type, BT_STATE_DISCONNECTING, address);

	return BLUETOOTH_ERROR_NONE;
fail:
	if (out_param1 != NULL)
		g_array_append_vals(*out_param1, address,
				BT_ADDRESS_STR_LEN);

	return result;
}

void _bt_remove_from_connected_list(const char *address)
{
	bt_connected_headset_data_t *connected_device;
	GList *node;

	node = g_list_first(g_connected_list);
	while (node != NULL) {
		connected_device = node->data;
		if (connected_device != NULL &&
		g_strcmp0(connected_device->device_address, address) == 0) {
			BT_ERR("Device is removed from the list");
			g_connected_list = g_list_remove(g_connected_list, connected_device);
			g_free(connected_device);
			return;
		}
		node = g_list_next(node);
	}
}

int _bt_hf_connect(int request_id,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGConnection *g_conn;

	int ret;
	char *uuid;


	BT_CHECK_PARAMETER(device_address, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

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

	func_data = g_malloc0(sizeof(bt_function_data_t));
	/* Fix : NULL_RETURNS */
	if (func_data == NULL) {
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto fail;
	}

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;
	uuid = g_strdup(HFP_AG_UUID);

	BT_DBG("Connecting to service %s", uuid);

	ret = _bt_connect_profile(address, uuid,
			__bt_hf_request_cb, func_data);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_connect_profile Error");
		g_free(func_data->address);
		g_free(func_data);
		g_free(uuid);
		return ret;
	}
	g_free(uuid);
	return BLUETOOTH_ERROR_NONE;
fail:
	if (out_param1 != NULL)
		g_array_append_vals(*out_param1, address,
				BT_ADDRESS_STR_LEN);

	return result;
}

int _bt_hf_disconnect(int request_id,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGConnection *g_conn;

	int ret;
	char *uuid;

	BT_CHECK_PARAMETER(device_address, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

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

	func_data = g_malloc0(sizeof(bt_function_data_t));
	/* Fix : NULL_RETURNS */
	if (func_data == NULL) {
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto fail;
	}

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;
	uuid = g_strdup(HFP_AG_UUID);

	BT_DBG("Disconnecting service %s", uuid);
	ret = _bt_disconnect_profile(address, uuid,
			__bt_hf_request_cb, func_data);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_disconnect_profile Error");
		g_free(func_data->address);
		g_free(func_data);
		g_free(uuid);
		return ret;
	}
	g_free(uuid);
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

int _bt_audio_set_content_protect(gboolean status)
{
	DBusConnection *conn;
	DBusMessage *signal;

	BT_DBG("+\n");

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("Content Protection status = [%d] \n", status);

	/*Emit Content protection Status change signal with value*/
	signal = dbus_message_new_signal(BT_CONTENT_PROTECTION_PATH,
					BT_CONTENT_PROTECTION_INTERFACE,
					"ProtectionRequired");
	if (!signal)
		goto err;

	if (!dbus_message_append_args(signal,
				DBUS_TYPE_BOOLEAN, &status,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Signal appending failed\n");
		dbus_message_unref(signal);
		goto err;
	}

	dbus_connection_send(conn, signal, NULL);
	dbus_message_unref(signal);

	BT_DBG("-\n");
	return BLUETOOTH_ERROR_NONE;

err:
	return BLUETOOTH_ERROR_INTERNAL;
}
