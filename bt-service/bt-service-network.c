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
#include <stdio.h>
#include <syspopup_caller.h>
#include <net_connection.h>

#include "bluetooth-api.h"
#include "bt-service-network.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-internal-types.h"

void _bt_util_addr_type_to_addr_net_string(char *address,
					unsigned char *addr)
{
	ret_if(address == NULL);
	ret_if(addr == NULL);

	snprintf(address, BT_ADDRESS_STR_LEN, "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X", addr[0],
			addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static connection_profile_h __bt_get_net_profile(void *connection,
						connection_iterator_type_e type,
						unsigned char *address)
{
	int result;
	gchar **split_string;
	char net_address[BT_ADDRESS_STR_LEN + 1] = { 0 };
	char *profile_name = NULL;
	connection_profile_iterator_h profile_iter;
	connection_profile_h profile_h;
	connection_profile_type_e profile_type;

	retv_if(connection == NULL, NULL);
	retv_if(address == NULL, NULL);

	BT_DBG("net_conn: %x", connection);

	_bt_util_addr_type_to_addr_net_string(net_address, address);

	result = connection_get_profile_iterator(connection,
							type,
							&profile_iter);
	if (result != CONNECTION_ERROR_NONE) {
		BT_ERR("Fail to get profile iterator [%d]", result);
		return NULL;
	}

	while (connection_profile_iterator_has_next(profile_iter)) {
			profile_name = NULL;
			profile_h = NULL;
			split_string = NULL;

			if (connection_profile_iterator_next(profile_iter,
						&profile_h) != CONNECTION_ERROR_NONE) {
				BT_ERR("Fail to get profile handle");
				return NULL;
			}

			if (connection_profile_get_type(profile_h,
						&profile_type) != CONNECTION_ERROR_NONE) {
				BT_ERR("Fail to get profile type");
				continue;
			}

			if (profile_type != CONNECTION_PROFILE_TYPE_BT)
				continue;

			if (connection_profile_get_name(profile_h,
						&profile_name) != CONNECTION_ERROR_NONE) {
				BT_ERR("Fail to get profile name");
				return NULL;
			}

			split_string = g_strsplit(profile_name, "_", 3);

			g_free(profile_name);

			if (g_strv_length(split_string) < 3)
				continue;

			if (g_ascii_strcasecmp(split_string[2], net_address) == 0) {
				BT_DBG("matched profile");
				g_strfreev(split_string);
				return profile_h;
			}

			g_strfreev(split_string);
	}

	return NULL;
}

int _bt_is_network_connected(void *connection, unsigned char *address,
					gboolean *is_connected)
{
	void *handle = NULL;
	handle = __bt_get_net_profile(connection,
				CONNECTION_ITERATOR_TYPE_CONNECTED,
				address);
	if(handle)
		*is_connected = TRUE;
	else
		*is_connected = FALSE;

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_network_connect_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				    gpointer user_data)
{
	GError *g_error = NULL;
	char *device = NULL;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	bluetooth_device_address_t device_addr = { {0} };
	int result = BLUETOOTH_ERROR_NONE;
	bt_function_data_t *func_data;
	request_info_t *req_info;

	dbus_g_proxy_end_call(proxy, call, &g_error, G_TYPE_STRING, &device, G_TYPE_INVALID);

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
		BT_ERR("Network Connect Dbus Call Error: %s\n", g_error->message);
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

static void __bt_network_disconnect_cb(DBusGProxy *proxy, DBusGProxyCall *call,
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
		BT_ERR("Network Connect Dbus Call Error: %s\n", g_error->message);
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

int _bt_network_activate(void)
{
	int ret = BLUETOOTH_ERROR_NONE;
	char *adapter_path;
	GError *err = NULL;
	DBusGConnection *conn;
	DBusGProxy *server_proxy;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	server_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
			     adapter_path, BT_NETWORK_SERVER_INTERFACE);

	g_free(adapter_path);

	if (server_proxy == NULL) {
		BT_ERR("Failed to get the network server proxy\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!dbus_g_proxy_call(server_proxy, "Register", &err,
			G_TYPE_STRING, NAP_UUID_NAME,
			G_TYPE_STRING, NET_BRIDGE_INTERFACE,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Network server register Error: %s\n", err->message);
			if (g_strcmp0(err->message, "Already Exists") == 0) {
				ret = BLUETOOTH_ERROR_ALREADY_INITIALIZED;
			} else {
				ret = BLUETOOTH_ERROR_INTERNAL;
			}
			g_error_free(err);
		}
	}

	g_object_unref(server_proxy);

	return ret;
}

int _bt_network_deactivate(void)
{
	char *adapter_path;
	GError *err = NULL;
	DBusGConnection *conn;
	DBusGProxy *server_proxy;
	int ret = BLUETOOTH_ERROR_NONE;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	server_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
			     adapter_path, BT_NETWORK_SERVER_INTERFACE);

	g_free(adapter_path);

	if (server_proxy == NULL) {
		BT_ERR("Failed to get the network server proxy\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!dbus_g_proxy_call(server_proxy, "Unregister", &err,
			G_TYPE_STRING, NAP_UUID_NAME,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Network server unregister Error: %s\n", err->message);
			if (g_strcmp0(err->message,
					"Operation currently not available") == 0) {
				ret = BLUETOOTH_ERROR_ALREADY_DEACTIVATED;
			} else {
				ret = BLUETOOTH_ERROR_INTERNAL;
			}
			g_error_free(err);
		}
		g_object_unref(server_proxy);
	}

	g_object_unref(server_proxy);

	return ret;
}

int _bt_network_connect(int request_id, int role,
		bluetooth_device_address_t *device_address)
{
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char remote_role[BLUETOOTH_UUID_STRING_MAX] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *profile_proxy;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);

	switch (role) {
	case BLUETOOTH_NETWORK_PANU_ROLE:
		g_strlcpy(remote_role, PANU_UUID_NAME, BLUETOOTH_UUID_STRING_MAX);
		break;

	case BLUETOOTH_NETWORK_NAP_ROLE:
		g_strlcpy(remote_role, NAP_UUID_NAME, BLUETOOTH_UUID_STRING_MAX);
		break;

	case BLUETOOTH_NETWORK_GN_ROLE:
		g_strlcpy(remote_role, GN_UUID_NAME, BLUETOOTH_UUID_STRING_MAX);
		break;
	default:
		BT_ERR("Unknown role");
		return BLUETOOTH_ERROR_INTERNAL;
	}

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

	profile_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_NETWORK_CLIENT_INTERFACE);
	g_free(device_path);
	retv_if(profile_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	func_data = g_malloc0(sizeof(bt_function_data_t));

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(profile_proxy, "Connect",
			(DBusGProxyCallNotify)__bt_network_connect_cb,
			func_data, NULL,
			G_TYPE_STRING, remote_role,
			G_TYPE_INVALID)) {
		BT_ERR("network connect Dbus Call Error");
		g_object_unref(profile_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_network_disconnect(int request_id,
		bluetooth_device_address_t *device_address)
{
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *adapter_proxy;
	DBusGProxy *profile_proxy;
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

	profile_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_NETWORK_CLIENT_INTERFACE);
	g_free(device_path);
	retv_if(profile_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	func_data = g_malloc0(sizeof(bt_function_data_t));

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(profile_proxy, "Disconnect",
			(DBusGProxyCallNotify)__bt_network_disconnect_cb,
			func_data, NULL, G_TYPE_INVALID)) {
		BT_ERR("network disconnect Dbus Call Error");
		g_object_unref(profile_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_network_server_disconnect(int request_id,
		bluetooth_device_address_t *device_address)
{
	gchar *adapter_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_function_data_t *func_data;
	DBusGProxy *profile_proxy;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	if (adapter_path == NULL) {
		BT_ERR("No adapter found");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	_bt_convert_addr_type_to_string(address, device_address->addr);

	profile_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      adapter_path, BT_NETWORK_SERVER_INTERFACE);
	g_free(adapter_path);
	retv_if(profile_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	func_data = g_malloc0(sizeof(bt_function_data_t));

	func_data->address = g_strdup(address);
	func_data->req_id = request_id;

	if (!dbus_g_proxy_begin_call(profile_proxy, "Disconnect",
			(DBusGProxyCallNotify)__bt_network_disconnect_cb,
			func_data, NULL, G_TYPE_STRING, address,
			G_TYPE_INVALID)) {
		BT_ERR("network server disconnect Dbus Call Error");
		g_free(func_data->address);
		g_free(func_data);
		g_object_unref(profile_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}
