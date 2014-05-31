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

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-device.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-util.h"
#include "bt-service-agent.h"

#define BT_SYSPOPUP_IPC_RESPONSE_OBJECT "/org/projectx/bt_syspopup_res"
#define BT_SYSPOPUP_INTERFACE "User.Bluetooth.syspopup"
#define BT_SYSPOPUP_METHOD_RESPONSE "Response"

typedef struct {
	int req_id;
	int result;
	char *addr;
	gboolean is_autopair;
	DBusGProxy *device_proxy;
	DBusGProxy *adapter_proxy;
	void *agent;
} bt_funcion_data_t;

gboolean is_device_creating;
bt_funcion_data_t *bonding_info;
bt_funcion_data_t *searching_info;

/* This HID Mouse does not support pairing precedure. need to skip it. */
#define SMB_MOUSE_LAP_ADDR "00:12:A1"

static void __bt_bond_device_cb(DBusGProxy *proxy, DBusGProxyCall *call,
						     gpointer user_data);

static int __bt_retry_bond(void);


static void __bt_decline_pair_request()
{
	GArray *out_param1;
	GArray *out_param2;
	request_info_t *req_info;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;

	BT_DBG("+");

	req_info = _bt_get_request_info(bonding_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}


	remote_dev_info = _bt_get_remote_device_info(bonding_info->addr);

	/* Send the event to application */
	if (remote_dev_info != NULL) {
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_BONDING_FINISHED,
			DBUS_TYPE_INT32, &bonding_info->result,
			DBUS_TYPE_STRING, &bonding_info->addr,
			DBUS_TYPE_UINT32, &remote_dev_info->class,
			DBUS_TYPE_INT16, &remote_dev_info->rssi,
			DBUS_TYPE_STRING, &remote_dev_info->name,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->connected,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
			DBUS_TYPE_BYTE, &remote_dev_info->device_type,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&remote_dev_info->uuids, remote_dev_info->uuid_count,
			DBUS_TYPE_INVALID);

		_bt_free_device_info(remote_dev_info);
	}

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &bonding_info->result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);
done:
	_bt_destroy_agent(bonding_info->agent);

	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;

	BT_DBG("-");
}

static DBusHandlerResult __bt_popup_event_filter(
						DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	int response;

	if (msg == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_is_signal(msg, "User.Bluetooth.syspopup",
				"Response"))
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args(msg, NULL,
				   DBUS_TYPE_INT32, &response,
				   DBUS_TYPE_INVALID)) {

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	BT_DBG("response = %d\n", response);

	if (response == 0)
		__bt_retry_bond();
	else
		__bt_decline_pair_request();


	BT_DBG("-");
	return DBUS_HANDLER_RESULT_HANDLED;
}

static void  __bt_register_popup_event_signal(DBusGConnection *gconn)
{
	DBusError dbus_error;
	DBusConnection *conn;
	BT_DBG("+\n");

	conn = dbus_g_connection_get_connection(gconn);
	if (conn == NULL)
		return;

	dbus_error_init(&dbus_error);
	dbus_connection_add_filter(conn, __bt_popup_event_filter,
				NULL, NULL);

	dbus_bus_add_match(conn,
			"type='signal',interface='User.Bluetooth.syspopup'"
			",member='Response'", &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Error: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		return;
	}
	BT_DBG("-\n");
	return;
}

static gboolean __bt_syspopup_timer_cb(gpointer user_data)
{
	int ret;
	bundle *b;
	retv_if(user_data == NULL, FALSE);

	b = (bundle *)user_data;

	ret = syspopup_launch("bt-syspopup", b);
	if (ret < 0) {
		BT_ERR("Sorry!! Cannot launch popup return = %d, Retrying...", ret);
	} else {
		BT_DBG("Hurray!!! Finally Popup launched");
		bundle_free(b);
	}
	return (ret < 0) ? TRUE : FALSE;
}

static gboolean __bt_launch_syspopup(int result)
{
	BT_DBG("+");
	int ret = 0;
	bundle *b = NULL;
	DBusGConnection *conn;

	conn = _bt_get_system_gconn();
	if (conn == NULL)
		return FALSE;

	__bt_register_popup_event_signal(conn);

	b = bundle_create();
	if (b == NULL)
		return FALSE;

	bundle_add(b, "event-type", "pairing-retry-request");

	if (result == BLUETOOTH_ERROR_TIMEOUT)
		bundle_add(b, "error", "timeout");
	else if (result == BLUETOOTH_ERROR_AUTHENTICATION_FAILED)
		bundle_add(b, "error", "authfailed");
	else
		bundle_add(b, "error", "error");

	ret = syspopup_launch("bt-syspopup", b);
	if (0 > ret) {
		BT_ERR("Popup launch failed...retry %d \n", ret);
		g_timeout_add(200, (GSourceFunc) __bt_syspopup_timer_cb,
				b);
	} else {
		bundle_free(b);
	}

	BT_DBG("-");
	return TRUE;
}

gboolean _bt_is_device_creating(void)
{
	return is_device_creating;
}

gboolean _bt_is_bonding_device_address(const char *address)
{
	if (bonding_info == NULL || bonding_info->addr == NULL)
		return FALSE;

	if (g_strcmp0(bonding_info->addr, address) == 0) {
		BT_DBG("[%s]  is bonding device", address);
		return TRUE;
	}

	BT_DBG("[%s]  is NOT bonding device", address);
	return FALSE;
}

void _bt_set_autopair_status_in_bonding_info(gboolean is_autopair)
{
	ret_if(bonding_info == NULL);
	bonding_info->is_autopair = is_autopair;
}

void _bt_device_path_to_address(const char *device_path,
					char *device_address)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *dev_addr;
	char *pos;

	ret_if(device_path == NULL);
	ret_if(device_address == NULL);

	dev_addr = strstr(device_path, "dev_");
	ret_if(dev_addr == NULL);

	dev_addr += 4;
	g_strlcpy(address, dev_addr, sizeof(address));

	while ((pos = strchr(address, '_')) != NULL) {
		*pos = ':';
	}

	g_strlcpy(device_address, address, BT_ADDRESS_STRING_SIZE);
}

void __bt_cancel_search_service_done(void)
{
	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;
	request_info_t *req_info;
	bluetooth_device_info_t dev_info;
	GArray *out_param1;
	GArray *out_param2;

	ret_if(searching_info == NULL);

	req_info = _bt_get_request_info(searching_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					searching_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);

done:
	if (searching_info->device_proxy)
		g_object_unref(searching_info->device_proxy);

	if (searching_info->adapter_proxy)
		g_object_unref(searching_info->adapter_proxy);

	g_free(searching_info->addr);
	g_free(searching_info);
	searching_info = NULL;
}

static void __bt_get_uuids(GValue *value, bt_remote_dev_info_t *info)
{
	int i = 0;
	char **uuid_value;

	ret_if(value == NULL);
	ret_if(info == NULL);

	info->uuid_count = 0;

	uuid_value = g_value_get_boxed(value);
	ret_if(uuid_value == NULL);

	while (uuid_value[i]) {
		i++;
	}
	ret_if(i == 0);

	info->uuid_count = i;

	info->uuids = g_new0(char *, info->uuid_count + 1);

	for (i = 0; uuid_value[i] != NULL; i++) {
		info->uuids[i] = g_strdup(uuid_value[i]);
	}
}

bt_remote_dev_info_t *_bt_get_remote_device_info(char *address)
{
	bt_remote_dev_info_t *dev_info;
	char *object_path = NULL;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *name;
	DBusGConnection *conn;

	retv_if(address == NULL, NULL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, NULL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &object_path,
			  G_TYPE_INVALID);

	retv_if(object_path == NULL, NULL);

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("conn == NULL");
		g_free(object_path);
		return NULL;
	}

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				object_path, BT_DEVICE_INTERFACE);
	g_free(object_path);
	retv_if(device_proxy == NULL, NULL);

	dbus_g_proxy_call(device_proxy, "GetProperties", NULL,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
				G_TYPE_VALUE), &hash, G_TYPE_INVALID);

	g_object_unref(device_proxy);

	dev_info = g_malloc0(sizeof(bt_remote_dev_info_t));

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Alias");
		name = value ? g_value_get_string(value) : NULL;

		if (name != NULL)
			DBG_SECURE("Alias Name [%s]", name);
		else {
			value = g_hash_table_lookup(hash, "Name");
			name = value ? g_value_get_string(value) : NULL;
		}

		value = g_hash_table_lookup(hash, "Class");
		dev_info->class = value ? g_value_get_uint(value) : 0;

		value = g_hash_table_lookup(hash, "Connected");
		dev_info->connected = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Trusted");
		dev_info->trust = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "DeviceType");
		dev_info->device_type = value ? g_value_get_char(value) : 0;

		value = g_hash_table_lookup(hash, "RSSI");
		dev_info->rssi = value ? g_value_get_int(value) : 0;

		value = g_hash_table_lookup(hash, "UUIDs");
		__bt_get_uuids(value, dev_info);

		dev_info->address = g_strdup(address);
		dev_info->name = g_strdup(name);
		dev_info->paired = TRUE;
		g_hash_table_destroy(hash);
	} else {
		BT_ERR("Hash is NULL\n");
		g_free(dev_info);
		dev_info = NULL;
	}

	return dev_info;
}

static gboolean __ignore_auto_pairing_request(const char *address)
{
	gchar *buffer;
	char **lines;
	int i;
	char lap_address[BT_LOWER_ADDRESS_LENGTH + 1] = {0,};
	char *temp_buffer;
	FILE *fp;
	long size;
	size_t result;

	BT_DBG("+\n");

	if (address == NULL)
		return FALSE;

	/* Get the LAP(Lower Address part) */
	/* User BT_LOWER_ADDRESS_LENGTH+1 for lap_address to accomodate
	     a "," */
	snprintf(lap_address, sizeof(lap_address), ",%s", address);

	fp = fopen(BT_AGENT_AUTO_PAIR_BLACKLIST_FILE, "r");

	if (fp == NULL) {
		BT_DBG("fopen failed \n");
		return FALSE;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	if (size < 0) {
		BT_DBG("Get file size failed \n");
		fclose(fp);
		return FALSE;
	}

	buffer = g_malloc0(sizeof(char) * size);
	result = fread((char *)buffer, 1, size, fp);
	fclose(fp);
	if (result != size) {
		BT_DBG("Read Error\n");
		g_free(buffer);
		return FALSE;
	}

	BT_DBG("Buffer = %s\n", buffer);

	lines = g_strsplit_set(buffer, BT_AGENT_NEW_LINE, 0);
	g_free(buffer);

	if (lines == NULL)
		return FALSE;

	/* Write the data and insert new device data */
	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix(lines[i], "AddressBlacklist")) {
			temp_buffer = g_strconcat(lines[i], lap_address, NULL);
			g_free(lines[i]);
			lines[i] = temp_buffer;
		}
	}
	buffer = g_strjoinv(BT_AGENT_NEW_LINE, lines);
	g_strfreev(lines);

	fp = fopen(BT_AGENT_AUTO_PAIR_BLACKLIST_FILE, "w");

	if (fp == NULL) {
		BT_DBG("fopen failed \n");
		g_free(buffer);
		return FALSE;
	}

	BT_DBG("Buffer = %s\n", buffer);
	fwrite(buffer, 1, strlen(buffer), fp);
	fclose(fp);

	g_free(buffer);

	BT_DBG("-\n");

	return FALSE;
}

static int __bt_retry_bond(void)
{
	DBusGProxy *adapter_proxy;

	BT_CHECK_PARAMETER(bonding_info, return);
	BT_CHECK_PARAMETER(bonding_info->addr, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	is_device_creating = TRUE;

	if (!dbus_g_proxy_begin_call_with_timeout(adapter_proxy, "CreatePairedDevice",
				(DBusGProxyCallNotify) __bt_bond_device_cb,
				NULL, NULL, BT_MAX_DBUS_TIMEOUT,
				G_TYPE_STRING, bonding_info->addr,
				DBUS_TYPE_G_OBJECT_PATH, BT_DEVICE_AGENT_PATH,
				G_TYPE_STRING, "DisplayYesNo",
				G_TYPE_INVALID)) {
		BT_ERR("CreatePairedDevice call fail");
		is_device_creating = FALSE;
		return BLUETOOTH_ERROR_NO_RESOURCES;
	}

	return BLUETOOTH_ERROR_NONE;
}


static int __bt_remove_and_bond(void)
{
	DBusGProxy *adapter_proxy;
	GError *err = NULL;
	char *device_path = NULL;

	BT_CHECK_PARAMETER(bonding_info, return);
	BT_CHECK_PARAMETER(bonding_info->addr, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, bonding_info->addr,
			  G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
			  &device_path,
			  G_TYPE_INVALID);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "RemoveDevice",
			  &err, DBUS_TYPE_G_OBJECT_PATH, device_path,
			  G_TYPE_INVALID, G_TYPE_INVALID);
	g_free(device_path);
	if (err != NULL) {
		BT_ERR("RemoveDevice Fail: %s", err->message);
		g_error_free(err);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return __bt_retry_bond();
}

static int __bt_cancel_and_bond(void)
{
	DBusGProxy *adapter_proxy;
	GError *err = NULL;

	BT_CHECK_PARAMETER(bonding_info, return);
	BT_CHECK_PARAMETER(bonding_info->addr, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "CancelDeviceCreation", &err,
				   G_TYPE_STRING, bonding_info->addr,
				   G_TYPE_INVALID, G_TYPE_INVALID);

	if (err != NULL) {
		BT_ERR("CancelCreation Fail: %s", err->message);
		g_error_free(err);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return __bt_retry_bond();
}


static void __bt_bond_device_cb(DBusGProxy *proxy, DBusGProxyCall *call,
						     gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	char *device_path = NULL;
	GError *err = NULL;
	GArray *out_param1;
	GArray *out_param2;
	request_info_t *req_info;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;

	/* Terminate ALL system popup */
	syspopup_destroy_all();

	dbus_g_proxy_end_call(proxy, call, &err,
			      DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	g_free(device_path);
	is_device_creating = FALSE;

	if (bonding_info == NULL) {
		/* Send reply */
		BT_ERR("bonding_info == NULL");
		if (err)
			g_error_free(err);
		return;
	}

	req_info = _bt_get_request_info(bonding_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in CreateBonding [%s]", err->message);

		if (!strcmp(err->message, "Already Exists")) {
			BT_DBG("Existing Bond, remove and retry");
			ret_if(__bt_remove_and_bond() == BLUETOOTH_ERROR_NONE);

			result = BLUETOOTH_ERROR_PARING_FAILED;
		} else if (_bt_agent_is_canceled(bonding_info->agent) ||
			!strcmp(err->message, "Authentication Canceled")) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else if (!strcmp(err->message, "Authentication Rejected")) {
			result = BLUETOOTH_ERROR_ACCESS_DENIED;
		} else if (!strcmp(err->message, "In Progress")) {
			BT_DBG("Bond in progress, cancel and retry");
			ret_if(__bt_cancel_and_bond() == BLUETOOTH_ERROR_NONE);

			result = BLUETOOTH_ERROR_PARING_FAILED;
		} else if (!strcmp(err->message, "Authentication Failed")) {
			if (bonding_info->is_autopair == TRUE) {
				_bt_set_autopair_status_in_bonding_info(FALSE);
				__ignore_auto_pairing_request(bonding_info->addr);
			}
			result = BLUETOOTH_ERROR_AUTHENTICATION_FAILED;
		} else if (!strcmp(err->message, "Page Timeout")) {
			/* This is the special case
			     As soon as call bluetooth_bond_device, try to cancel bonding.
			     In this case, before completing to call 'CreatePairedDevice' method
			     the procedure is stopped. So 'Cancle' error is not return.
			*/
			result = BLUETOOTH_ERROR_HOST_DOWN;
		} else if (!strcmp(err->message, BT_TIMEOUT_MESSAGE)) {
			dbus_g_proxy_call(proxy, "CancelDeviceCreation", NULL,
					   G_TYPE_STRING, bonding_info->addr,
					   G_TYPE_INVALID, G_TYPE_INVALID);

			result = BLUETOOTH_ERROR_INTERNAL;
		} else if (!strcmp(err->message, "Connection Timeout")) {
			/* Pairing request timeout */
			result = BLUETOOTH_ERROR_TIMEOUT;
		} else {
			BT_DBG("Default case");
			result = BLUETOOTH_ERROR_PARING_FAILED;
		}
	}


	if (result == BLUETOOTH_ERROR_PARING_FAILED ||
			result == BLUETOOTH_ERROR_AUTHENTICATION_FAILED ||
			result == BLUETOOTH_ERROR_TIMEOUT) {

		bonding_info->result = result;
		if (TRUE == __bt_launch_syspopup(result)) {
			BT_DBG("Retry pairing");
			return;
		}
	}

	if (result != BLUETOOTH_ERROR_NONE)
		goto dbus_return;

	remote_dev_info = _bt_get_remote_device_info(bonding_info->addr);

	/* Send the event to application */
	if (remote_dev_info != NULL) {
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_BONDING_FINISHED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &bonding_info->addr,
			DBUS_TYPE_UINT32, &remote_dev_info->class,
			DBUS_TYPE_INT16, &remote_dev_info->rssi,
			DBUS_TYPE_STRING, &remote_dev_info->name,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->connected,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
			DBUS_TYPE_BYTE, &remote_dev_info->device_type,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&remote_dev_info->uuids, remote_dev_info->uuid_count,
			DBUS_TYPE_INVALID);

		_bt_free_device_info(remote_dev_info);
	}

dbus_return:
	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	_bt_destroy_agent(bonding_info->agent);

	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;

}

int _bt_bond_device(int request_id,
		bluetooth_device_address_t *device_address,
		GArray **out_param1)
{
	DBusGProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	void *agent;
	bluetooth_device_info_t dev_info;

	BT_CHECK_PARAMETER(device_address, return);

	if (bonding_info) {
		BT_ERR("Bonding in progress");

		memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
		memcpy(dev_info.device_address.addr, device_address->addr,
				BLUETOOTH_ADDRESS_LENGTH);

		g_array_append_vals(*out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));

		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	agent = _bt_create_agent(BT_DEVICE_AGENT_PATH, FALSE);
	retv_if(agent == NULL, BLUETOOTH_ERROR_INTERNAL);

	bonding_info = g_malloc0(sizeof(bt_funcion_data_t));
	bonding_info->addr = g_strdup(address);
	bonding_info->req_id = request_id;
	bonding_info->agent = agent;

	is_device_creating = TRUE;

	if (!strncmp(address, SMB_MOUSE_LAP_ADDR, strlen(SMB_MOUSE_LAP_ADDR))) {
		bluetooth_device_address_t device_addr = { {0} };
		BT_ERR("This device don't support pairing. So skip pairing.");
		if (!dbus_g_proxy_begin_call(proxy, "CreateDevice",
				(DBusGProxyCallNotify)__bt_bond_device_cb,
				NULL, NULL,
				G_TYPE_STRING, address,
				G_TYPE_INVALID)) {
			BT_ERR("CreateDevice failed");
			goto fail;
		}

		_bt_convert_addr_string_to_type(device_addr.addr, address);
		if (_bt_set_authorization(&device_addr, TRUE))
			BT_ERR("_bt_set_authorization failed [%s]", address);

	} else {
		if (!dbus_g_proxy_begin_call_with_timeout(proxy, "CreatePairedDevice",
					(DBusGProxyCallNotify) __bt_bond_device_cb,
					NULL, NULL, BT_MAX_DBUS_TIMEOUT,
					G_TYPE_STRING, address,
					DBUS_TYPE_G_OBJECT_PATH, BT_DEVICE_AGENT_PATH,
					G_TYPE_STRING, "DisplayYesNo",
					G_TYPE_INVALID)) {
			BT_ERR("CreatePairedDevice call fail");
			goto fail;
		}
	}

	return BLUETOOTH_ERROR_NONE;
fail:
	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	g_array_append_vals(*out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));

	is_device_creating = FALSE;

	_bt_destroy_agent(agent);

	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;

	return BLUETOOTH_ERROR_INTERNAL;
}

int _bt_cancel_bonding(void)
{
	DBusGProxy *adapter_proxy;

	retv_if(bonding_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	_bt_agent_set_canceled(bonding_info->agent, TRUE);
	dbus_g_proxy_call_no_reply(adapter_proxy, "CancelDeviceCreation",
				   G_TYPE_STRING, bonding_info->addr,
				   G_TYPE_INVALID);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_unbond_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				gpointer user_data)
{
	GError *err = NULL;
	GArray *out_param1;
	GArray *out_param2;
	int result = BLUETOOTH_ERROR_NONE;
	bt_funcion_data_t *unbonding_info;
	bluetooth_device_info_t dev_info;
	request_info_t *req_info;

	dbus_g_proxy_end_call(proxy, call, &err, G_TYPE_INVALID);

	unbonding_info = user_data;

	if (unbonding_info == NULL) {
		/* Send reply */
		BT_ERR("unbonding_info == NULL");
		goto done;
	}

	req_info = _bt_get_request_info(unbonding_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in RemoveBonding [%s]\n", err->message);
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					unbonding_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	_bt_delete_request_list(req_info->req_id);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

done:
	if (err)
		g_error_free(err);

	if (unbonding_info) {
		g_free(unbonding_info->addr);
		g_free(unbonding_info);
	}
}

int _bt_unbond_device(int request_id,
			bluetooth_device_address_t *device_address,
			GArray **out_param1)
{
	char *device_path = NULL;
	bt_funcion_data_t *unbonding_info;
	DBusGProxy *adapter_proxy;
	int result = BLUETOOTH_ERROR_INTERNAL;
	bluetooth_device_info_t dev_info;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* allocate user data so that it can be retrieved in callback */
	unbonding_info = g_malloc0(sizeof(bt_funcion_data_t));
	unbonding_info->addr = g_malloc0(BT_ADDRESS_STRING_SIZE);
	unbonding_info->req_id = request_id;

	_bt_convert_addr_type_to_string(unbonding_info->addr,
					device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
				  G_TYPE_STRING, unbonding_info->addr,
				  G_TYPE_INVALID,
				  DBUS_TYPE_G_OBJECT_PATH, &device_path,
				  G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		result = BLUETOOTH_ERROR_NOT_PAIRED;
		goto fail;
	}

	if (!dbus_g_proxy_begin_call(adapter_proxy, "RemoveDevice",
				(DBusGProxyCallNotify) __bt_unbond_cb,
				(gpointer)unbonding_info, NULL,
				DBUS_TYPE_G_OBJECT_PATH, device_path,
				G_TYPE_INVALID)) {
		BT_ERR("RemoveBonding begin failed\n");
		goto fail;
	}
	g_free(device_path);
	return BLUETOOTH_ERROR_NONE;

fail:
	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					unbonding_info->addr);

	g_array_append_vals(*out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_free(device_path);
	g_free(unbonding_info->addr);
	g_free(unbonding_info);
	return result;
}

static void __bt_discover_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				gpointer user_data)
{
	GError *err = NULL;
	GHashTable *hash = NULL;
	GArray *out_param1;
	GArray *out_param2;
	int result = BLUETOOTH_ERROR_NONE;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;
	request_info_t *req_info;

	dbus_g_proxy_end_call(proxy, call, &err,
			      dbus_g_type_get_map("GHashTable", G_TYPE_UINT, G_TYPE_STRING), &hash,
			      G_TYPE_INVALID);

	g_object_unref(proxy);

	if (searching_info == NULL) {
		/* Send reply */
		BT_ERR("unbonding_info == NULL");
		goto done;
	}

	req_info = _bt_get_request_info(searching_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);

		if (!strcmp("Operation canceled", err->message)) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else if (!strcmp("In Progress", err->message)) {
			result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (!strcmp("Host is down", err->message)) {
			result = BLUETOOTH_ERROR_HOST_DOWN;
		} else {
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		}

		if (result == BLUETOOTH_ERROR_HOST_DOWN ||
		     result == BLUETOOTH_ERROR_CONNECTION_ERROR) {
			remote_dev_info = _bt_get_remote_device_info(searching_info->addr);
			if (remote_dev_info && remote_dev_info->uuids != NULL &&
			     remote_dev_info->uuid_count > 0) {
				result = BLUETOOTH_ERROR_NONE;
				goto event;
			}
			_bt_free_device_info(remote_dev_info);
		}
		goto dbus_return;
	}

	remote_dev_info = _bt_get_remote_device_info(searching_info->addr);

event:
	/* Send the event to application */
	if (remote_dev_info != NULL) {
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_SERVICE_SEARCHED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &searching_info->addr,
			DBUS_TYPE_UINT32, &remote_dev_info->class,
			DBUS_TYPE_INT16, &remote_dev_info->rssi,
			DBUS_TYPE_STRING, &remote_dev_info->name,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->connected,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
			DBUS_TYPE_BYTE, &remote_dev_info->device_type,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&remote_dev_info->uuids, remote_dev_info->uuid_count,
			DBUS_TYPE_INVALID);

		_bt_free_device_info(remote_dev_info);
	}

dbus_return:
	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					searching_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	g_hash_table_destroy(hash);

	if (searching_info) {
		g_free(searching_info->addr);
		g_free(searching_info);
		searching_info = NULL;
	}
}

static void __bt_create_device_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				gpointer user_data)
{
	GError *err = NULL;
	char *device_path = NULL;
	GArray *out_param1;
	GArray *out_param2;
	int result = BLUETOOTH_ERROR_NONE;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;
	request_info_t *req_info;

	is_device_creating = FALSE;

	dbus_g_proxy_end_call(proxy, call, &err,
			DBUS_TYPE_G_OBJECT_PATH, &device_path,
			G_TYPE_INVALID);
	g_free(device_path);
	if (searching_info == NULL) {
		/* Send reply */
		BT_ERR("searching_info == NULL");
		goto done;
	}

	req_info = _bt_get_request_info(searching_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);
		result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		goto dbus_return;
	}

	remote_dev_info = _bt_get_remote_device_info(searching_info->addr);

	/* Send the event to application */
	if (remote_dev_info != NULL) {
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_SERVICE_SEARCHED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &searching_info->addr,
			DBUS_TYPE_UINT32, &remote_dev_info->class,
			DBUS_TYPE_INT16, &remote_dev_info->rssi,
			DBUS_TYPE_STRING, &remote_dev_info->name,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->connected,
			DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
			DBUS_TYPE_BYTE, &remote_dev_info->device_type,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&remote_dev_info->uuids, remote_dev_info->uuid_count,
			DBUS_TYPE_INVALID);

		_bt_free_device_info(remote_dev_info);
	}

dbus_return:
	if (req_info->context == NULL)
		goto done;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					searching_info->addr);

	g_array_append_vals(out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	if (searching_info) {
		g_free(searching_info->addr);
		g_free(searching_info);
		searching_info = NULL;
	}
}

int _bt_search_device(int request_id,
			bluetooth_device_address_t *device_address)
{
	char *device_path = NULL;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy = NULL;
	DBusGConnection *conn;
	int result = BLUETOOTH_ERROR_INTERNAL;

	BT_CHECK_PARAMETER(device_address, return);

	if (searching_info) {
		BT_ERR("Service searching in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* allocate user data so that it can be retrieved in callback */
	searching_info = g_malloc0(sizeof(bt_funcion_data_t));
	searching_info->addr = g_malloc0(BT_ADDRESS_STRING_SIZE);
	searching_info->req_id = request_id;

	_bt_convert_addr_type_to_string(searching_info->addr,
					device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
				  G_TYPE_STRING, searching_info->addr,
				  G_TYPE_INVALID,
				  DBUS_TYPE_G_OBJECT_PATH, &device_path,
				  G_TYPE_INVALID);

	if (device_path == NULL) {
		BT_ERR("No paired device");

		is_device_creating = TRUE;

		if (!dbus_g_proxy_begin_call(adapter_proxy,
				"CreateDevice",
				(DBusGProxyCallNotify)__bt_create_device_cb,
				(gpointer)searching_info, NULL,
				G_TYPE_STRING, searching_info->addr,
				G_TYPE_INVALID)) {
			BT_ERR("CreateDevice failed");
			result = BLUETOOTH_ERROR_INTERNAL;
			is_device_creating = FALSE;
			goto fail;
		}

		searching_info->adapter_proxy = device_proxy;

		return BLUETOOTH_ERROR_NONE;
	}

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	if (device_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}
	if (!dbus_g_proxy_begin_call(device_proxy, "DiscoverServices",
				(DBusGProxyCallNotify)__bt_discover_cb,
				(gpointer)searching_info, NULL,
				G_TYPE_STRING, "",
				G_TYPE_INVALID)) {
		BT_ERR("DiscoverServices failed");
		goto fail;
	}

	searching_info->device_proxy = device_proxy;

	return BLUETOOTH_ERROR_NONE;
fail:
	if (device_proxy)
		g_object_unref(device_proxy);

	g_free(searching_info->addr);
	g_free(searching_info);
	searching_info = NULL;
	return result;
}

int _bt_cancel_search_device(void)
{
	GError *err = NULL;

	retv_if(searching_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	if (searching_info->device_proxy) {
		dbus_g_proxy_call(searching_info->device_proxy,
				"CancelDiscovery",
				&err,
				G_TYPE_INVALID, G_TYPE_INVALID);
	} else if (searching_info->adapter_proxy) {
		dbus_g_proxy_call(searching_info->adapter_proxy,
				"CancelDeviceCreation",
				&err,
				G_TYPE_STRING, searching_info->addr,
				G_TYPE_INVALID, G_TYPE_INVALID);
	} else {
		BT_ERR("No proxy info");
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;
	}

	if (err != NULL) {
		BT_ERR("Error occured [%s]\n", err->message);
		g_error_free(err);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_cancel_search_service_done();

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_alias(bluetooth_device_address_t *device_address,
				      const char *alias)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy;
	GError *error = NULL;
	GValue name = { 0 };
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(alias, return);

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

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				device_path, BT_DEVICE_INTERFACE);

	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	g_value_init(&name, G_TYPE_STRING);
	g_value_set_string(&name, alias);

	dbus_g_proxy_call(device_proxy, "SetProperty", &error,
			   G_TYPE_STRING, "Alias", G_TYPE_VALUE,
			   &name, G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref(device_proxy);

	g_value_unset(&name);

	if (error) {
		 BT_ERR("SetProperty error: [%s]", error->message);
		 g_error_free(error);
		 return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_authorization(bluetooth_device_address_t *device_address,
				      gboolean authorize)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy;
	gboolean previous_value;
	GError *error = NULL;
	GValue trusted = { 0 };
	GValue *value;
	GHashTable *hash = NULL;
	DBusGConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

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

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
				G_TYPE_VALUE), &hash, G_TYPE_INVALID);

	if (error != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", error->message);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (hash == NULL) {
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	value = g_hash_table_lookup(hash, "Trusted");
	previous_value = value ? g_value_get_boolean(value) : FALSE;

	/* If the input is same with previous value, return error. */
	if (previous_value == authorize) {
		BT_ERR("Same value: %d", previous_value);
		g_object_unref(device_proxy);
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto done;
	}

	g_value_init(&trusted, G_TYPE_BOOLEAN);
	g_value_set_boolean(&trusted, authorize);

	dbus_g_proxy_call(device_proxy, "SetProperty", &error,
			   G_TYPE_STRING, "Trusted", G_TYPE_VALUE,
			   &trusted, G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref(device_proxy);

	g_value_unset(&trusted);

	if (error) {
		 BT_ERR("SetProperty error: [%s]", error->message);
		 g_error_free(error);
		 ret = BLUETOOTH_ERROR_INTERNAL;
	}
done:
	g_hash_table_destroy(hash);

	return ret;
}

int _bt_is_device_connected(bluetooth_device_address_t *device_address,
			int connection_type, gboolean *is_connected)
{
	char *object_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	DBusGProxy *device_proxy = NULL;
	DBusGProxy *adapter_proxy = NULL;
	DBusGConnection *conn;
	GError *error = NULL;
	GHashTable *hash = NULL;
	GValue *value = NULL;
	char *interface = NULL;

	retv_if(device_address == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(is_connected == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	if (connection_type == BLUETOOTH_RFCOMM_SERVICE)
		return _bt_rfcomm_is_device_connected(device_address,
						is_connected);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice",
			  &error, G_TYPE_STRING, address,
			  G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
			  &object_path, G_TYPE_INVALID);

	if (error != NULL) {
		BT_ERR("Failed to Find device: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_NOT_PAIRED;
	}

	retv_if(object_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_ERR("Confirm connection type : %x", connection_type);

	switch (connection_type) {
	case BLUETOOTH_HSP_SERVICE:
		interface = BT_HEADSET_INTERFACE;
		break;
	case BLUETOOTH_A2DP_SERVICE:
		interface = BT_SINK_INTERFACE;
		break;
	case BLUETOOTH_HID_SERVICE:
		interface = BT_INPUT_INTERFACE;
		break;
	case BLUETOOTH_NAP_SERVICE:
		interface = BT_NETWORK_CLIENT_INTERFACE;
		break;
	default:
		BT_DBG("Unknown type!");
		g_free(object_path);
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	BT_DBG("Interface: %s", interface);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME, object_path, interface);
	g_free(object_path);
	if (device_proxy == NULL) {
		BT_DBG("Device don't have this service");
		is_connected = FALSE;
		return BLUETOOTH_ERROR_NONE;
	}
	dbus_g_proxy_call(device_proxy, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);

	if (error != NULL) {
		BT_DBG("Failed to get properties: %s\n", error->message);
		g_error_free(error);
		g_object_unref(device_proxy);
		is_connected = FALSE;
		return BLUETOOTH_ERROR_NONE;
	}

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Connected");
		*is_connected = value ? g_value_get_boolean(value) : FALSE;
		g_hash_table_destroy(hash);
	} else {
		*is_connected = FALSE;
	}

	g_object_unref(device_proxy);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_connect_le_device(const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	DBusGProxy *device_proxy = NULL;
	DBusGProxy *adapter_proxy;
	DBusGConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
				(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", &error,
		G_TYPE_STRING, device_address, G_TYPE_INVALID,
		DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(adapter_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(adapter_proxy);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
					  device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "ConnectLE", &error, G_TYPE_INVALID, G_TYPE_INVALID);
	if (error) {
		BT_ERR("ConnectLE Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	return ret;
}

int _bt_disconnect_le_device(const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	DBusGProxy *device_proxy = NULL;
	DBusGProxy *adapter_proxy;
	DBusGConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
				(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", &error,
		G_TYPE_STRING, device_address, G_TYPE_INVALID,
		DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(adapter_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(adapter_proxy);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
					  device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "DisconnectLE", &error, G_TYPE_INVALID, G_TYPE_INVALID);
	if (error) {
		BT_ERR("DisconnectLE Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	return ret;
}


int _bt_read_rssi_value(const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	DBusGProxy *device_proxy = NULL;
	DBusGProxy *adapter_proxy;
	DBusGConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
				(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", &error,
		G_TYPE_STRING, device_address, G_TYPE_INVALID,
		DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(adapter_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(adapter_proxy);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
					  device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "ReadRSSI", &error, G_TYPE_INVALID, G_TYPE_INVALID);
	if (error) {
		BT_ERR("ReadRSSI Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	return ret;
}
