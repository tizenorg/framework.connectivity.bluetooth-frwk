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
#include "bt-service-common.h"
#include "bt-service-hid.h"
#include "bt-service-event.h"
#include "bt-service-util.h"

static void __bt_hid_connect_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				    gpointer user_data)
{
	GError *g_error = NULL;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	bluetooth_device_address_t device_addr = { {0} };
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

	if (g_error != NULL) {
		BT_ERR("Hidh Connect Dbus Call Error: %s\n", g_error->message);
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	_bt_convert_addr_string_to_type(device_addr.addr,
					func_data->address);

	g_array_append_vals(out_param1, &device_addr,
				sizeof(bluetooth_device_address_t));
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

static void __bt_hid_disconnect_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				    gpointer user_data)
{
	GError *g_error = NULL;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	bluetooth_device_address_t device_addr = { {0} };
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

	if (g_error != NULL) {
		BT_ERR("Hidh Connect Dbus Call Error: %s\n", g_error->message);
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	_bt_convert_addr_string_to_type(device_addr.addr,
					func_data->address);

	g_array_append_vals(out_param1, &device_addr,
				sizeof(bluetooth_device_address_t));
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


/**********************************************************************
*                               HID APIs                              *
***********************************************************************/

int _bt_hid_connect(int request_id,
		bluetooth_device_address_t *device_address)
{
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *hid_proxy;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		return BLUETOOTH_ERROR_NOT_PAIRED;
	}

	hid_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_INPUT_INTERFACE);
	g_free(device_path);
	retv_if(hid_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	func_data = g_malloc0(sizeof(bt_function_data_t));

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(hid_proxy, "Connect",
			(DBusGProxyCallNotify)__bt_hid_connect_cb,
			func_data, NULL, G_TYPE_INVALID)) {
		BT_ERR("Hidh connect Dbus Call Error");
		g_object_unref(hid_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_hid_disconnect(int request_id,
		bluetooth_device_address_t *device_address)
{
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *hid_proxy;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		return BLUETOOTH_ERROR_NOT_PAIRED;
	}

	hid_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_INPUT_INTERFACE);
	g_free(device_path);
	retv_if(hid_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	func_data = g_malloc0(sizeof(bt_function_data_t));

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(hid_proxy, "Disconnect",
			(DBusGProxyCallNotify)__bt_hid_disconnect_cb,
			func_data, NULL, G_TYPE_INVALID)) {
		BT_ERR("Hidh disconnect Dbus Call Error");
		g_object_unref(hid_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}
