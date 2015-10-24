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

#include <glib.h>
#include <gio/gio.h>
#include <dlog.h>
#include <string.h>
#include <syspopup_caller.h>
#include "bluetooth-api.h"

#include "bt-service-common.h"
#include "bt-service-device.h"
#include "bt-service-hid.h"
#include "bt-service-event.h"
#include "bt-service-util.h"

static void __bt_hid_connect_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	GError *g_error = NULL;
	GVariant *out_param1 = NULL;
	GVariant *reply = NULL;
	bluetooth_device_address_t device_addr = { {0} };
	int result = BLUETOOTH_ERROR_NONE;
	bt_function_data_t *func_data;
	request_info_t *req_info;

	BT_DBG("+");
	func_data = user_data;

	reply = g_dbus_proxy_call_finish(proxy, res, &g_error);
	g_object_unref(proxy);
	if (reply == NULL) {
		BT_ERR("Hid Connect Dbus Call Error");
		if (g_error) {
			BT_ERR("Error: %s\n", g_error->message);
			g_clear_error(&g_error);
		}
		result = BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(reply);


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

	if (req_info->context == NULL)
		goto done;
	BT_DBG("Address: %s", func_data->address);
	_bt_convert_addr_string_to_type(device_addr.addr,
					func_data->address);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
			&device_addr, sizeof(bluetooth_device_address_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("iv", result, out_param1));

	_bt_delete_request_list(req_info->req_id);
	BT_DBG("HID Connected..");

done:
	if (func_data) {
		g_free(func_data->address);
		g_free(func_data);
	}
}

static void __bt_hid_disconnect_cb(GDBusProxy *proxy,GAsyncResult *res,
					gpointer user_data)
{
	GError *g_error = NULL;
	GVariant *out_param1 = NULL;
	GVariant *reply;
	bluetooth_device_address_t device_addr = { {0} };
	int result = BLUETOOTH_ERROR_NONE;
	bt_function_data_t *func_data;
	request_info_t *req_info;

	reply = g_dbus_proxy_call_finish(proxy, res, &g_error);
	g_object_unref(proxy);

	if (reply == NULL) {
		BT_ERR("Hid Disconnect Dbus Call Error");
		if (g_error) {
			BT_ERR("Error: %s\n", g_error->message);
			g_clear_error(&g_error);
		}
		result = BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(reply);

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

	if (req_info->context == NULL)
		goto done;
	BT_DBG("Address: %s", func_data->address);
	_bt_convert_addr_string_to_type(device_addr.addr,
					func_data->address);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
							&device_addr, sizeof(bluetooth_device_address_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("iv", result, out_param1));

	_bt_delete_request_list(req_info->req_id);
	BT_DBG("HID Disconnected..");

done:
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
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	/* GDBusProxy *adapter_proxy; */
	GDBusConnection *conn;

	int ret;
	char *uuid;

	BT_CHECK_PARAMETER(device_address, return);

	/* Unused adapter proxy
	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	*/
	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	func_data = g_malloc0(sizeof(bt_function_data_t));
	/* Fix : NULL_RETURNS */
	retv_if(func_data == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;
	uuid = HID_UUID;

	ret = _bt_connect_profile(address, uuid,
			__bt_hid_connect_cb, func_data);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_connect_profile Error");
		return ret;
	}
	return BLUETOOTH_ERROR_NONE;
}

int _bt_hid_disconnect(int request_id,
		bluetooth_device_address_t *device_address)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	/* GDBusProxy *adapter_proxy; */
	GDBusConnection *conn;

	int ret;

	BT_CHECK_PARAMETER(device_address, return);

	/* Unused adapter proxy
	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	*/
	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	func_data = g_malloc0(sizeof(bt_function_data_t));
	/* Fix : NULL_RETURNS */
	retv_if(func_data == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	ret = _bt_disconnect_profile(address, HID_UUID,
			__bt_hid_disconnect_cb, func_data);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_disconnect_profile Error");
		return ret;
	}

	return BLUETOOTH_ERROR_NONE;
}
