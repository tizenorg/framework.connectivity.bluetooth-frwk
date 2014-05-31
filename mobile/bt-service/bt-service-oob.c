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

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-oob.h"
#include "bt-service-event.h"

int _bt_oob_read_local_data(bt_oob_data_t *local_oob_data)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	char *adapter_path;
	unsigned char *local_hash = NULL;
	unsigned char *local_randomizer = NULL;
	DBusConnection *conn;

	BT_CHECK_PARAMETER(local_oob_data, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
					BT_OOB_INTERFACE, "ReadLocalData");

	g_free(adapter_path);

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
					msg, -1, &err);

	dbus_message_unref(msg);
	if (!reply) {
		BT_ERR("Error in ReadLocalData \n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!dbus_message_get_args(reply, NULL,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
			&local_hash, &local_oob_data->hash_len,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
			&local_randomizer, &local_oob_data->randomizer_len,
			DBUS_TYPE_INVALID)) {
		BT_ERR("Error in reading arguments\n");
		dbus_message_unref(reply);
		return BLUETOOTH_ERROR_INVALID_DATA;
	}

	if (NULL != local_hash)
		memcpy(local_oob_data->hash, local_hash, local_oob_data->hash_len);

	if (NULL != local_randomizer)
		memcpy(local_oob_data->randomizer, local_randomizer,
					local_oob_data->randomizer_len);

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_oob_add_remote_data(
			bluetooth_device_address_t *remote_device_address,
			bt_oob_data_t *remote_oob_data)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	char *dev_addr;
	char *adapter_path;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	unsigned char *remote_hash;
	unsigned char *remote_randomizer;
	DBusConnection *conn;

	BT_CHECK_PARAMETER(remote_device_address, return);
	BT_CHECK_PARAMETER(remote_oob_data, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address,
		remote_device_address->addr);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
				BT_OOB_INTERFACE, "AddRemoteData");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("remote hash len = [%d] and remote random len = [%d]\n",
		remote_oob_data->hash_len, remote_oob_data->randomizer_len);

	remote_hash = remote_oob_data->hash;
	remote_randomizer = remote_oob_data->randomizer;

	dev_addr = g_strdup(address);

	dbus_message_append_args(msg,
		DBUS_TYPE_STRING, &dev_addr,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
		&remote_hash, remote_oob_data->hash_len,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
		&remote_randomizer, remote_oob_data->randomizer_len,
		DBUS_TYPE_INVALID);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
					msg, -1, &err);

	dbus_message_unref(msg);
	if (!reply) {
		BT_ERR("Error in AddRemoteData \n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
			g_free(dev_addr);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}

	g_free(dev_addr);
	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_oob_remove_remote_data(
			bluetooth_device_address_t *remote_device_address)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	char *dev_addr;
	char *adapter_path;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	DBusConnection *conn;

	BT_CHECK_PARAMETER(remote_device_address, return);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address,
		remote_device_address->addr);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
				BT_OOB_INTERFACE, "RemoveRemoteData");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dev_addr = g_strdup(address);

	dbus_message_append_args(msg, DBUS_TYPE_STRING,
		&dev_addr, DBUS_TYPE_INVALID);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
					msg, -1, &err);

	dbus_message_unref(msg);
	if (!reply) {
		BT_ERR("Error in RemoveRemoteData \n");
		if (dbus_error_is_set(&err)) {
			BT_DBG("%s", err.message);
			dbus_error_free(&err);
			g_free(dev_addr);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}

	g_free(dev_addr);
	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

