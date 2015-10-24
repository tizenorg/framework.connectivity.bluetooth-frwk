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
#include <dlog.h>
#include <string.h>
#include <gio/gio.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-oob.h"
#include "bt-service-event.h"

int _bt_oob_read_local_data(bt_oob_data_t *local_oob_data)
{
	GDBusProxy *proxy;
	GVariant *reply;
	GError *err = NULL;
	char *adapter_path;
	unsigned char *local_hash = NULL;
	unsigned char *local_randomizer = NULL;
	GDBusConnection *conn;
	GVariant *hash = NULL;
	GVariant *randomizer = NULL;

	BT_CHECK_PARAMETER(local_oob_data, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);


	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, adapter_path,
			BT_OOB_INTERFACE, NULL, &err);
	g_free(adapter_path);
	if (!proxy) {
		BT_ERR("Unable to create proxy");
		if (err) {
			BT_ERR("Error: %s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	reply = g_dbus_proxy_call_sync(proxy, "ReadLocalData",
			NULL,
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &err);
	g_object_unref(proxy);

	if (reply == NULL) {
		BT_ERR("ReadLocalData dBUS-RPC is failed");
		if (err != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					err->code, err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(reply ,"(@ay@ay)", &hash, &randomizer);
	g_variant_unref(reply);

	if (hash) {
		local_oob_data->hash_len =
			(unsigned int)g_variant_get_size(hash);
		local_hash =
			(unsigned char *)g_variant_get_data(hash);
		g_variant_unref(hash);
	}

	if (randomizer) {
		local_oob_data->randomizer_len =
			(unsigned int)g_variant_get_size(randomizer);
		local_randomizer =
			(unsigned char *)g_variant_get_data(randomizer);
		g_variant_unref(randomizer);
	}

	if (local_oob_data->hash_len > 0 && local_hash)
		memcpy(local_oob_data->hash, local_hash, local_oob_data->hash_len);

	if (local_oob_data->randomizer_len > 0 && local_randomizer)
		memcpy(local_oob_data->randomizer, local_randomizer,
				local_oob_data->randomizer_len);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_oob_add_remote_data(
			bluetooth_device_address_t *remote_device_address,
			bt_oob_data_t *remote_oob_data)
{
	GDBusProxy *proxy;
	GVariant *reply;
	GError *err = NULL;
	char *dev_addr;
	char *adapter_path;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	unsigned char *remote_hash;
	unsigned char *remote_randomizer;
	GDBusConnection *conn;
	GArray *in_param1 = NULL;
	GArray *in_param2 = NULL;
	GVariant *hash;
	GVariant *randomizer;

	BT_CHECK_PARAMETER(remote_device_address, return);
	BT_CHECK_PARAMETER(remote_oob_data, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address,
		remote_device_address->addr);

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, adapter_path,
			BT_OOB_INTERFACE, NULL, &err);
	g_free(adapter_path);
	if (!proxy) {
		BT_ERR("Unable to create proxy");
		if (err) {
			BT_ERR("Error: %s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	remote_hash = remote_oob_data->hash;
	remote_randomizer = remote_oob_data->randomizer;
	dev_addr = g_strdup(address);

	BT_DBG("remote hash len = [%d] and remote random len = [%d]\n",
		remote_oob_data->hash_len, remote_oob_data->randomizer_len);
	/*Create array of bytes variant*/
	in_param1 = g_array_new(TRUE, TRUE, sizeof(gchar));
	in_param2 = g_array_new(TRUE, TRUE, sizeof(gchar));

	g_array_append_vals(in_param1, remote_hash,
			remote_oob_data->hash_len);
	g_array_append_vals(in_param2, remote_randomizer,
			remote_oob_data->randomizer_len);

	hash = g_variant_new_from_data((const GVariantType *)"ay",
			in_param1->data, in_param1->len,
			TRUE, NULL, NULL);

	randomizer = g_variant_new_from_data((const GVariantType *)"ay",
			in_param2->data, in_param2->len,
			TRUE, NULL, NULL);

	g_array_free(in_param1, TRUE);
	g_array_free(in_param2, TRUE);

	/* Call AddRemoteData Method*/
	reply = g_dbus_proxy_call_sync(proxy, "AddRemoteData",
			g_variant_new("s@ay@ay", dev_addr, hash, randomizer),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &err);
	g_object_unref(proxy);
	g_free(dev_addr);

	/* Check the reply*/
	if (reply == NULL) {
		BT_ERR("AddRemoteData dBUS-RPC is failed");
		if (err != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					err->code, err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_oob_remove_remote_data(
			bluetooth_device_address_t *remote_device_address)
{
	GDBusProxy *proxy;
	GVariant *reply;
	GError *err = NULL;
	char *dev_addr;
	char *adapter_path;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GDBusConnection *conn;

	BT_CHECK_PARAMETER(remote_device_address, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address,
		remote_device_address->addr);

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, adapter_path,
			BT_OOB_INTERFACE, NULL, &err);
	g_free(adapter_path);
	if (!proxy) {
		BT_ERR("Unable to create proxy");
		if (err) {
			BT_ERR("Error: %s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dev_addr = g_strdup(address);

	/* Call RemoveRemoteData Method*/
	reply = g_dbus_proxy_call_sync(proxy, "RemoveRemoteData",
			g_variant_new("s", dev_addr),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &err);
	g_object_unref(proxy);
	g_free(dev_addr);

	/* Check the reply*/
	if (reply == NULL) {
		BT_ERR("RemoveRemoteData dBUS-RPC is failed");
		if (err != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					err->code, err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	return BLUETOOTH_ERROR_NONE;
}

