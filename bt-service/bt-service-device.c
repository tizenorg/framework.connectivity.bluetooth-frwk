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
#include <bundle_internal.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-device.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-util.h"
#include "bt-service-agent.h"
#include "bt-service-network.h"
#include "bt-service-adapter.h"
#include "bt-service-gap-agent.h"

#define BT_SYSPOPUP_IPC_RESPONSE_OBJECT "/org/projectx/bt_syspopup_res"
#define BT_SYSPOPUP_INTERFACE "User.Bluetooth.syspopup"
#define BT_SYSPOPUP_METHOD_RESPONSE "Response"

#define BT_LE_CONN_INTERVAL_MIN 7.5 /* msec */
#define BT_LE_CONN_INTERVAL_MAX 4000 /* msec */
#define BT_LE_CONN_SUPER_TO_MIN 100 /* msec */
#define BT_LE_CONN_SUPER_TO_MAX 32000 /* msec */
#define BT_LE_CONN_SLAVE_LATENCY_MAX 499
#define BT_LE_CONN_INTERVAL_SPLIT 1.25 /* msec */
#define BT_LE_CONN_TO_SPLIT 10 /* msec */
#define BT_DEVICE_PIN_CODE_SLOT_MAX 10

#define BT_LE_CONN_PARAM_DEFAULT_SUPERVISION_TIMEOUT	20000	/* msec */

#define BT_LE_CONN_PARAM_BALANCED_MIN_INTERVAL	30	/* msec */
#define BT_LE_CONN_PARAM_BALANCED_MAX_INTERVAL	50	/* msec */
#define BT_LE_CONN_PARAM_BALANCED_SLAVE_LATENCY	0	/* event */

#define BT_LE_CONN_PARAM_LOW_LATENCY_MIN_INTERVAL	7.5	/* msec */
#define BT_LE_CONN_PARAM_LOW_LATENCY_MAX_INTERVAL	10	/* msec */
#define BT_LE_CONN_PARAM_LOW_LATENCY_SLAVE_LATENCY	0	/* event */

#define BT_LE_CONN_PARAM_LOW_POWER_MIN_INTERVAL		80	/* msec */
#define BT_LE_CONN_PARAM_LOW_POWER_MAX_INTERVAL		100	/* msec */
#define BT_LE_CONN_PARAM_LOW_POWER_SLAVE_LATENCY	2	/* event */

typedef struct {
	int req_id;
	int result;
	char *addr;
	gboolean is_autopair;
	GDBusProxy *device_proxy;
	GDBusProxy *adapter_proxy;
	void *agent;
	unsigned short conn_type;
} bt_funcion_data_t;

typedef struct {
	char *address;
	char *pin_code;
} bt_pin_code_info_t;

gboolean is_device_creating;
bt_funcion_data_t *bonding_info;
bt_funcion_data_t *searching_info;
bt_funcion_data_t *le_connection_info;

static GSList *pin_info_list = NULL;

/* This HID Mouse does not support pairing precedure. need to skip it. */
#define SMB_MOUSE_LAP_ADDR "00:12:A1"

static void __bt_bond_device_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data);

static int __bt_retry_bond(void);


/*static void __bt_decline_pair_request()
{
	GVariant *out_param1;
	GVariant *out_param2;
	request_info_t *req_info;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;
	GVariant *uuid_list, *manufacture_data;
	GVariant *param;

	BT_DBG("+");
	if (bonding_info) {
		req_info = _bt_get_request_info(bonding_info->req_id);
		if (req_info == NULL) {
			BT_ERR("req_info == NULL");
			goto done;
		}
		remote_dev_info = _bt_get_remote_device_info(bonding_info->addr);
	} else {
		BT_DBG("bonding_info is NULL");
		BT_DBG("-");
		return;
	}

	uuid_list =  g_variant_new_from_data((const GVariantType *)"as",
						remote_dev_info->uuids, remote_dev_info->uuid_count,
						TRUE, NULL, NULL);

	manufacture_data = g_variant_new_from_data((const GVariantType *)"ay",
						remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
						TRUE, NULL, NULL);

	param = g_variant_new("isunsbuba{s}na{y})",
			bonding_info->result,
			bonding_info->addr,
			remote_dev_info->class,
			remote_dev_info->rssi,
			remote_dev_info->name,
			remote_dev_info->paired,
			remote_dev_info->connected,
			remote_dev_info->trust,
			uuid_list,
			remote_dev_info->manufacturer_data_len,
			manufacture_data);


	//Send the event to application
	if (remote_dev_info != NULL) {
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_BONDING_FINISHED,
			param);

		_bt_free_device_info(remote_dev_info);
	}

	if (req_info->context == NULL)
		goto done;

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&dev_info, sizeof(bluetooth_device_info_t), TRUE, NULL, NULL);

	out_param2 = g_variant_new_from_data((const GVariantType *)"i",
		&bonding_info->result, sizeof(int), TRUE, NULL, NULL);


	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("@ayi", out_param1, out_param2));


	_bt_delete_request_list(req_info->req_id);
done:

	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;

	BT_DBG("-");
} */

#ifdef TIZEN_WEARABLE
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

static gboolean __bt_launch_unable_to_pairing_syspopup(int result)
{
	BT_DBG("+");
	int ret = 0;
	bundle *b = NULL;
	GDBusConnection *conn;

	conn = _bt_get_system_gconn();
	if (conn == NULL)
		return FALSE;

	b = bundle_create();
	if (b == NULL)
		return FALSE;

	bundle_add(b, "event-type", "unable-to-pairing");

	if (result == BLUETOOTH_ERROR_TIMEOUT )
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
#endif

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
	GVariant *out_param1;

	ret_if(searching_info == NULL);

	req_info = _bt_get_request_info(searching_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		goto done;
	}

	if (req_info->context == NULL)
		goto done;

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					searching_info->addr);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&dev_info, sizeof(bluetooth_device_info_t), TRUE, NULL, NULL);


	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));

	_bt_delete_request_list(req_info->req_id);

done:

	g_free(searching_info->addr);
	g_free(searching_info);
	searching_info = NULL;
}

static void __bt_get_uuids(GVariant *value, bt_remote_dev_info_t *info)
{
	ret_if(value == NULL);
	ret_if(info == NULL);

	info->uuid_count = g_variant_get_size(value);
	info->uuids = g_variant_dup_strv(value, &info->uuid_count);
}

bt_remote_dev_info_t *_bt_get_remote_device_info(char *address)
{
	bt_remote_dev_info_t *dev_info;
	char *object_path = NULL;
	GDBusProxy *adapter_proxy;
	GDBusProxy *device_proxy;
	GVariant *value;
	GVariant *tmp_value;
	gchar *name;
	gchar *manufacturer_data = NULL;
	GDBusConnection *conn;
	GError *error = NULL;
	GVariant *result = NULL;

	retv_if(address == NULL, NULL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, NULL);


	object_path = _bt_get_device_object_path(address);

	retv_if(object_path == NULL, NULL);

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("conn == NULL");
		g_free(object_path);
		return NULL;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								object_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);

	g_free(object_path);
	retv_if(device_proxy == NULL, NULL);

	result = g_dbus_proxy_call_sync(device_proxy, "GetAll",
				g_variant_new("(s)", BT_DEVICE_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	g_object_unref(device_proxy);

	dev_info = g_malloc0(sizeof(bt_remote_dev_info_t));

	if (result != NULL) {
		g_variant_get(result , "(@a{sv})", &value);
		g_variant_unref(result);

		tmp_value = g_variant_lookup_value (value, "Alias", G_VARIANT_TYPE_STRING);

		g_variant_get(tmp_value, "s", &name);
		g_variant_unref(tmp_value);
		if (name != NULL)
			DBG_SECURE("Alias Name [%s]", name);
		else {
			tmp_value = g_variant_lookup_value(value, "Name", G_VARIANT_TYPE_STRING);
			g_variant_get(tmp_value, "s", &name);
			g_variant_unref(tmp_value);
		}

		tmp_value = g_variant_lookup_value(value, "Class", G_VARIANT_TYPE_UINT32);
		dev_info->class = tmp_value ? g_variant_get_uint32(tmp_value) : 0;
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "Connected",  G_VARIANT_TYPE_BOOLEAN);
		dev_info->connected = tmp_value ? g_variant_get_boolean(tmp_value) : BLUETOOTH_CONNECTED_LINK_NONE;
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "Trusted",  G_VARIANT_TYPE_BOOLEAN);
		dev_info->trust = tmp_value ? g_variant_get_boolean(tmp_value) : FALSE;
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "Paired",  G_VARIANT_TYPE_BOOLEAN);
		dev_info->paired = tmp_value ? g_variant_get_boolean(tmp_value) : FALSE;
		g_variant_unref(tmp_value);

		BT_DBG("Paired %d", dev_info->paired );

		tmp_value = g_variant_lookup_value(value, "RSSI", G_VARIANT_TYPE_INT32);
		dev_info->rssi = tmp_value ? g_variant_get_int32(tmp_value) : 0;
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "LastAddrType", G_VARIANT_TYPE_UINT32);
		dev_info->addr_type = tmp_value ? g_variant_get_uint32(tmp_value) : 0;
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "UUIDs", G_VARIANT_TYPE_STRING_ARRAY);
		__bt_get_uuids(tmp_value, dev_info);
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "ManufacturerDataLen", G_VARIANT_TYPE_UINT32);
		dev_info->manufacturer_data_len = tmp_value ? g_variant_get_uint32(tmp_value) : 0;
		if (dev_info->manufacturer_data_len > BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX) {
			BT_ERR("manufacturer_data_len is too long(len = %d)", dev_info->manufacturer_data_len);
			dev_info->manufacturer_data_len = BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX;
		}
		g_variant_unref(tmp_value);

		tmp_value = g_variant_lookup_value(value, "ManufacturerData", G_VARIANT_TYPE_BYTESTRING);
		manufacturer_data = value ? (gchar *)g_variant_get_bytestring(tmp_value) : NULL;
		if (manufacturer_data) {
			if (dev_info->manufacturer_data_len > 0) {
				dev_info->manufacturer_data = g_malloc0(dev_info->manufacturer_data_len);
				memcpy(dev_info->manufacturer_data, manufacturer_data, dev_info->manufacturer_data_len);
			}
		}
		g_variant_unref(tmp_value);

		dev_info->address = g_strdup(address);
		dev_info->name = g_strdup(name);
		g_free(name);
		g_variant_unref(value);
	} else {
		BT_ERR("result  is NULL\n");
		g_free(dev_info);
		dev_info = NULL;
	}

	return dev_info;
}

char *_bt_get_bonded_device_name(char *address)
{
	bluetooth_device_address_t device_address = { {0} };
	bluetooth_device_info_t dev_info;

	retv_if(address == NULL, strdup(""));

	_bt_convert_addr_string_to_type(device_address.addr, address);

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));

	_bt_get_bonded_device_info(&device_address, &dev_info);

	return g_strdup(dev_info.device_name.name);
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
		BT_ERR("fopen failed \n");
		return FALSE;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	if (size < 0) {
		BT_ERR("Get file size failed \n");
		fclose(fp);
		return FALSE;
	}

	buffer = g_malloc0(sizeof(char) * size);
	/* Fix : NULL_RETURNS */
	if (buffer == NULL) {
		BT_ERR("Memory allocation error\n");
		fclose(fp);
		return FALSE;
	}
	result = fread((char *)buffer, 1, size, fp);
	fclose(fp);
	if (result != size) {
		BT_ERR("Read Error\n");
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
	/* Fix : NULL_RETURNS */
	retv_if(buffer == NULL, FALSE);

	fp = fopen(BT_AGENT_AUTO_PAIR_BLACKLIST_FILE, "w");

	if (fp == NULL) {
		BT_ERR("fopen failed \n");
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
	BT_CHECK_PARAMETER(bonding_info, return);
	BT_CHECK_PARAMETER(bonding_info->addr, return);

	g_dbus_proxy_call(bonding_info->device_proxy, "Pair",
				g_variant_new("(y)", bonding_info->conn_type),
				G_DBUS_CALL_FLAGS_NONE,
				BT_MAX_DBUS_TIMEOUT,
				NULL,
				(GAsyncReadyCallback)__bt_bond_device_cb,
				NULL);

	return BLUETOOTH_ERROR_NONE;
}


static int __bt_remove_and_bond(char *device_path)
{
	GDBusProxy *adapter_proxy;
	GVariant *result = NULL;
	GError *err = NULL;

	BT_CHECK_PARAMETER(bonding_info, return);
	BT_CHECK_PARAMETER(bonding_info->addr, return);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(adapter_proxy, "UnpairDevice",
				g_variant_new("(o)", device_path),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&err);

	if (err != NULL) {
		BT_ERR("UnpairDevice Fail: %s", err->message);
		g_error_free(err);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return __bt_retry_bond();
}

static int __bt_cancel_and_bond(void)
{
	int ret = BLUETOOTH_ERROR_NONE;

	ret = _bt_agent_reply_cancellation();
	if (ret != BLUETOOTH_ERROR_NONE){
		BT_ERR("Fail to call reply cancellation");
		return ret;
	}

	return __bt_retry_bond();
}


static void __bt_bond_device_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	GError *err = NULL;
	GVariant *out_param1;
	request_info_t *req_info;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;
	GVariant *manufacture_data;
	GVariant *param;
	const char *device_path;

	/* Terminate ALL system popup */
	syspopup_destroy_all();

	 g_dbus_proxy_call_finish(proxy, res, &err);

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

	device_path = g_dbus_proxy_get_object_path(proxy);

	BT_DBG("device path: %s", device_path);

	if (err != NULL) {
		g_dbus_error_strip_remote_error(err);
		BT_ERR("Error occured in CreateBonding [%s]", err->message);

		if (g_strrstr(err->message, "Already Exists")) {
			BT_INFO("Existing Bond, remove and retry");
			ret_if(__bt_remove_and_bond(device_path) == BLUETOOTH_ERROR_NONE);

			BT_INFO("Fail to retry bonding");

			result = BLUETOOTH_ERROR_PARING_FAILED;
		} else if (_bt_agent_is_canceled() ||
			g_strrstr(err->message, "Authentication Canceled")) {
			BT_INFO("Cancelled by USER");
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else if (g_strrstr(err->message, "Authentication Rejected")) {
			BT_INFO("REJECTED");
			result = BLUETOOTH_ERROR_ACCESS_DENIED;
		} else if (g_strrstr(err->message, "In Progress")) {
			BT_INFO("Bond in progress, cancel and retry");
			ret_if(__bt_cancel_and_bond() == BLUETOOTH_ERROR_NONE);

			result = BLUETOOTH_ERROR_PARING_FAILED;
		} else if (g_strrstr(err->message, "Authentication Failed")) {
			BT_INFO("Authentication Failed");
			if (bonding_info->is_autopair == TRUE) {
				_bt_set_autopair_status_in_bonding_info(FALSE);
				__ignore_auto_pairing_request(bonding_info->addr);
			}
			result = BLUETOOTH_ERROR_AUTHENTICATION_FAILED;
		} else if (g_strrstr(err->message, "Page Timeout")) {
			BT_INFO("Page Timeout");
			/* This is the special case
			     As soon as call bluetooth_bond_device, try to cancel bonding.
			     In this case, before completing to call 'CreatePairedDevice' method
			     the procedure is stopped. So 'Cancle' error is not return.
			*/
			result = BLUETOOTH_ERROR_HOST_DOWN;
		} else if (g_strrstr(err->message, BT_TIMEOUT_MESSAGE)) {
			g_dbus_proxy_call_sync(proxy, "CancelDeviceCreation",
						g_variant_new("(s)", bonding_info->addr),
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						NULL);

			result = BLUETOOTH_ERROR_INTERNAL;
		} else if (g_strrstr(err->message, "Connection Timeout")) {
			/* Pairing request timeout */
			result = BLUETOOTH_ERROR_TIMEOUT;
		} else if (g_strrstr(err->message, "Authentication Timeout")) {
			/* Pairing request timeout */
			result = BLUETOOTH_ERROR_TIMEOUT;
		} else {
			BT_DBG("Default case");
			result = BLUETOOTH_ERROR_PARING_FAILED;
		}
	}

	if (result == BLUETOOTH_ERROR_PARING_FAILED ||
			result == BLUETOOTH_ERROR_AUTHENTICATION_FAILED ||
			result == BLUETOOTH_ERROR_TIMEOUT ||
			result == BLUETOOTH_ERROR_HOST_DOWN) {
		bonding_info->result = result;
#ifdef TIZEN_WEARABLE
		__bt_launch_unable_to_pairing_syspopup(result);
#endif
	}

	g_object_unref(proxy);
	bonding_info->device_proxy = NULL;

	if (result != BLUETOOTH_ERROR_NONE)
		goto dbus_return;

	remote_dev_info = _bt_get_remote_device_info(bonding_info->addr);
	if (remote_dev_info == NULL)
		goto dbus_return;

	GVariant *uuids = NULL;
	GVariantBuilder *builder = NULL;
	int i = 0;
	builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
	for (i=0; i < remote_dev_info->uuid_count; i++) {
		g_variant_builder_add(builder, "s",
			remote_dev_info->uuids[i]);
	}
	uuids = g_variant_new("as", builder);
	g_variant_builder_unref(builder);
	manufacture_data = g_variant_new_from_data((const GVariantType *)"ay",
						remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
						TRUE, NULL, NULL);

	param = g_variant_new("(isunsbub@asn@ay)",
			result,
			bonding_info->addr,
			remote_dev_info->class,
			remote_dev_info->rssi,
			remote_dev_info->name,
			remote_dev_info->paired,
			remote_dev_info->connected,
			remote_dev_info->trust,
			uuids,
			remote_dev_info->manufacturer_data_len,
			manufacture_data);


	/* Send the event to application */
	_bt_send_event(BT_ADAPTER_EVENT,
		BLUETOOTH_EVENT_BONDING_FINISHED,
		param);

	_bt_free_device_info(remote_dev_info);

dbus_return:
	if (req_info->context == NULL)
		goto done;

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	if (_bt_adapter_get_status() != BT_ACTIVATED)
		result = BLUETOOTH_ERROR_NOT_IN_OPERATION;

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&dev_info, sizeof(bluetooth_device_info_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));

	g_variant_unref(out_param1);
//	g_variant_unref(out_param2);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	_bt_agent_set_canceled(FALSE);


	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;
}

int _bt_bond_device(int request_id,
		bluetooth_device_address_t *device_address,
		unsigned short conn_type, GArray **out_param1)
{
	GDBusProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bluetooth_device_info_t dev_info;

	GDBusConnection *conn;
	char *device_path = NULL;
	GDBusProxy *adapter_proxy;
	GError *error = NULL;

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

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	device_path = _bt_get_device_object_path(address);

	if (device_path == NULL) {
		BT_ERR("No searched device");
		GVariant *ret = NULL;
		adapter_proxy = _bt_get_adapter_proxy();
		retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		ret = g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
					g_variant_new("(s)", address),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

		if (error != NULL) {
			BT_ERR("CreateDevice Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);
		device_path = _bt_get_device_object_path(address);
		if (device_path == NULL) {
			memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
			memcpy(dev_info.device_address.addr, device_address->addr,
					BLUETOOTH_ADDRESS_LENGTH);

			g_array_append_vals(*out_param1, &dev_info,
					sizeof(bluetooth_device_info_t));

			return BLUETOOTH_ERROR_NOT_PAIRED;
		} else {
			BT_INFO("device_path is created[%s]", device_path);
		}
	}

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);

	g_free(device_path);
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	bonding_info = g_malloc0(sizeof(bt_funcion_data_t));
	bonding_info->addr = g_strdup(address);
	bonding_info->req_id = request_id;

	bonding_info->device_proxy = proxy;
	bonding_info->conn_type = conn_type;

	is_device_creating = TRUE;

	g_dbus_proxy_call(proxy, "Pair",
				g_variant_new("(y)", conn_type),
				G_DBUS_CALL_FLAGS_NONE,
				BT_MAX_DBUS_TIMEOUT,
				NULL,
				(GAsyncReadyCallback)__bt_bond_device_cb,
				NULL);

/* TODO: We need to check if we can pair the specific device using 'pair' API of bluez 5.x */

	return BLUETOOTH_ERROR_NONE;
/*fail:
	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					bonding_info->addr);

	g_array_append_vals(*out_param1, &dev_info,
				sizeof(bluetooth_device_info_t));

	is_device_creating = FALSE;

	g_free(bonding_info->addr);
	g_free(bonding_info);
	bonding_info = NULL;

	return BLUETOOTH_ERROR_INTERNAL;*/
}

int _bt_cancel_bonding(void)
{
	int ret = BLUETOOTH_ERROR_NONE;

	retv_if(bonding_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	ret = _bt_agent_reply_cancellation();
	if (ret != BLUETOOTH_ERROR_NONE){
		BT_ERR("Fail to call reply cancellation");
		return ret;
	}

	_bt_agent_set_canceled(TRUE);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_unbond_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	GError *err = NULL;
	GVariant *out_param1;
	int result = BLUETOOTH_ERROR_NONE;
	bt_funcion_data_t *unbonding_info;
	bluetooth_device_info_t dev_info;
	request_info_t *req_info;

	g_dbus_proxy_call_finish(proxy, res, &err);

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

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					unbonding_info->addr);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&dev_info, sizeof(bluetooth_device_info_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));


	_bt_delete_request_list(req_info->req_id);

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
	GDBusProxy *adapter_proxy = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusConnection *conn;
	int result = BLUETOOTH_ERROR_INTERNAL;
	bluetooth_device_info_t dev_info;
	GError *error = NULL;
	GVariant *ret = NULL;

	BT_CHECK_PARAMETER(device_address, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* allocate user data so that it can be retrieved in callback */
	unbonding_info = g_malloc0(sizeof(bt_funcion_data_t));
	/* Fix : NULL_RETURNS */
	if (unbonding_info == NULL) {
		BT_ERR("Memory not allocated !");
		return BLUETOOTH_ERROR_MEMORY_ALLOCATION;
	}

	unbonding_info->addr = g_malloc0(BT_ADDRESS_STRING_SIZE);
	unbonding_info->req_id = request_id;

	_bt_convert_addr_type_to_string(unbonding_info->addr,
					device_address->addr);

	device_path = _bt_get_device_object_path(unbonding_info->addr);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		result = BLUETOOTH_ERROR_NOT_PAIRED;
		goto fail;
	}

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("conn is NULL");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);

	if (device_proxy != NULL) {

		ret = g_dbus_proxy_call_sync(device_proxy, "Get",
					g_variant_new("(ss)", BT_DEVICE_INTERFACE, "Paired"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
		if (error) {
			BT_ERR("Getting property failed: [%s]\n", error->message);
			g_error_free(error);
			result = BLUETOOTH_ERROR_NOT_PAIRED;
			goto fail;
		} else {
			if (!ret) {
				BT_ERR("No paired device");
				g_object_unref(device_proxy);
				result = BLUETOOTH_ERROR_NOT_PAIRED;
				goto fail;
			}
			g_variant_unref(ret);
		}
		g_object_unref(device_proxy);
	}

	g_dbus_proxy_call(adapter_proxy, "UnpairDevice",
				g_variant_new("(o)", device_path),
				G_DBUS_CALL_FLAGS_NONE,
				BT_MAX_DBUS_TIMEOUT,
				NULL,
				(GAsyncReadyCallback)__bt_unbond_cb,
				NULL);

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

static void __bt_discover_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	GError *err = NULL;
	GVariant *out_param1;
	int result = BLUETOOTH_ERROR_NONE;
	bluetooth_device_info_t dev_info;
	bt_remote_dev_info_t *remote_dev_info;
	request_info_t *req_info;
	GVariant *uuid_list, *manufacture_data;
	GVariant *param;
	GVariantBuilder *builder = NULL;
	int i = 0;

	g_dbus_proxy_call_finish(proxy, res, &err);

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
		g_dbus_error_strip_remote_error(err);
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);

		if (g_strrstr("Operation canceled", err->message)) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else if (g_strrstr("In Progress", err->message)) {
			result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (g_strrstr("Host is down", err->message)) {
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
	if (remote_dev_info == NULL)
		goto dbus_return;

event:
	builder = g_variant_builder_new(G_VARIANT_TYPE("as"));
	for (i=0; i < remote_dev_info->uuid_count; i++) {
		g_variant_builder_add(builder, "s",
			remote_dev_info->uuids[i]);
	}
	uuid_list = g_variant_new("as", builder);
	g_variant_builder_unref(builder);
	manufacture_data = g_variant_new_from_data((const GVariantType *)"ay",
						remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
						TRUE, NULL, NULL);

	param = g_variant_new("(isunsbub@asn@ay)",
			result,
			searching_info->addr,
			remote_dev_info->class,
			remote_dev_info->rssi,
			remote_dev_info->name,
			remote_dev_info->paired,
			remote_dev_info->connected,
			remote_dev_info->trust,
			uuid_list,
			remote_dev_info->manufacturer_data_len,
			manufacture_data);

	/* Send the event to application */
	_bt_send_event(BT_ADAPTER_EVENT,
		BLUETOOTH_EVENT_SERVICE_SEARCHED,
		param);

	_bt_free_device_info(remote_dev_info);

dbus_return:
	if (req_info->context == NULL)
		goto done;

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
	_bt_convert_addr_string_to_type(dev_info.device_address.addr,
					searching_info->addr);

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&dev_info, sizeof(bluetooth_device_info_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));


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
	GDBusProxy *device_proxy = NULL;
	GDBusConnection *conn;

	GDBusProxy *adapter_proxy;
	int result = BLUETOOTH_ERROR_INTERNAL;

	BT_CHECK_PARAMETER(device_address, return);

	if (bonding_info) {
		BT_ERR("Bonding in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	if (searching_info) {
		BT_ERR("Service searching in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* allocate user data so that it can be retrieved in callback */
	searching_info = g_malloc0(sizeof(bt_funcion_data_t));
	searching_info->addr = g_malloc0(BT_ADDRESS_STRING_SIZE);
	searching_info->req_id = request_id;

	_bt_convert_addr_type_to_string(searching_info->addr,
					device_address->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);


	device_path = _bt_get_device_object_path(searching_info->addr);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		result = BLUETOOTH_ERROR_NOT_PAIRED;
		goto fail;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(device_path);
	if (device_proxy == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	g_dbus_proxy_call(device_proxy, "DiscoverServices",
			g_variant_new("(s)", ""),
			G_DBUS_CALL_FLAGS_NONE,
			BT_MAX_DBUS_TIMEOUT,
			NULL,
			(GAsyncReadyCallback)__bt_discover_cb,
			searching_info);

	searching_info->device_proxy = device_proxy;

	return BLUETOOTH_ERROR_NONE;
fail:
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
		g_dbus_proxy_call_sync(searching_info->device_proxy, "CancelDiscovery",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&err);
	}
	__bt_cancel_search_service_done();

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_alias(bluetooth_device_address_t *device_address,
				      const char *alias)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GDBusProxy *adapter_proxy;
	GDBusProxy *device_proxy;
	GError *error = NULL;
	GDBusConnection *conn;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(alias, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	device_path = _bt_get_device_object_path(address);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		return BLUETOOTH_ERROR_NOT_PAIRED;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);

	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(device_proxy, "Set",
				g_variant_new("(ssv)", BT_DEVICE_INTERFACE,  "Alias", g_variant_new("s", alias)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	g_object_unref(device_proxy);

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
	GDBusProxy *device_proxy;
	gboolean previous_value;
	GError *error = NULL;
	GDBusConnection *conn;
	GVariant *result = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	device_path = _bt_get_device_object_path(address);

	if (device_path == NULL) {
		BT_ERR("No paired device");
		return BLUETOOTH_ERROR_NOT_PAIRED;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);

	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(device_proxy, "Get",
				g_variant_new("(ss)", BT_DEVICE_INTERFACE, "Trusted" ),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error != NULL) {
		BT_ERR("Getting property failed: [%s]\n", error->message);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	previous_value = g_variant_get_boolean(result);
	g_variant_unref(result);
	/* If the input is same with previous value, return error. */
	if (previous_value == authorize) {
		BT_ERR("Same value: %d", previous_value);
		g_object_unref(device_proxy);
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto done;
	}

	g_dbus_proxy_call_sync(device_proxy, "Set",
				 g_variant_new("(ssv)", BT_DEVICE_INTERFACE, "Trusted", g_variant_new("b",authorize)),
				 G_DBUS_CALL_FLAGS_NONE,
				 -1,
				 NULL,
				 &error);

	g_object_unref(device_proxy);
	if (error) {
		 BT_ERR("SetProperty error: [%s]", error->message);
		 g_error_free(error);
		 ret = BLUETOOTH_ERROR_INTERNAL;
	}
done:
	return ret;
}

int _bt_is_gatt_connected(bluetooth_device_address_t *device_address,
			gboolean *is_connected)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *object_path = NULL;

	GDBusProxy *device_proxy;
	GError *error = NULL;
	GVariant *value;
	GVariant *tmp_value;
	GDBusConnection *conn;
	GVariant *result = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	object_path = _bt_get_device_object_path(address);
	retv_if(object_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								object_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);
	g_free(object_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(device_proxy, "GetAll",
			  g_variant_new("(s)", BT_DEVICE_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error != NULL || result == NULL) {
		if (error != NULL) {
			BT_ERR("Error occured in Proxy call [%s]\n", error->message);
			g_error_free(error);
		}
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result , "(@a{sv})", &value);
	g_variant_unref(result);

	tmp_value = g_variant_lookup_value (value, "GattConnected", G_VARIANT_TYPE_BOOLEAN);

	if (tmp_value == NULL) {
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	*is_connected = g_variant_get_boolean (tmp_value);

	BT_DBG("gatt is connected : %d", *is_connected);
	g_variant_unref(tmp_value);
	g_variant_unref(value);
	g_object_unref(device_proxy);

	return ret;
}

int _bt_is_device_connected(bluetooth_device_address_t *device_address,
			int connection_type, gboolean *is_connected)
{
	char *object_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GDBusProxy *device_proxy = NULL;
	GDBusProxy *adapter_proxy = NULL;
	GDBusConnection *conn;
	GError *error = NULL;
	GVariant *tmp_value = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	char *uuid = NULL;

	retv_if(device_address == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(is_connected == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	*is_connected = FALSE;
	BT_DBG("connection_type: %d", connection_type);
	if (connection_type == BLUETOOTH_RFCOMM_SERVICE)
		return _bt_rfcomm_is_device_connected(device_address,
						is_connected);
	else if (connection_type == BLUETOOTH_GATT_SERVICE)
		return _bt_is_gatt_connected(device_address, is_connected);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	if(connection_type == BLUETOOTH_NAP_SERVER_SERVICE)	{
		object_path = _bt_get_adapter_path();
		device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
									NULL, BT_BLUEZ_NAME,
									object_path, BT_NETWORK_SERVER_INTERFACE,  NULL, NULL);
		g_free(object_path);
		if (device_proxy == NULL) {
			BT_DBG("Device don't have this service");
			return BLUETOOTH_ERROR_INTERNAL;
		}

		result = g_dbus_proxy_call_sync(device_proxy, "GetProperties",
					g_variant_new("(s)", address),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

		if (result == NULL) {
			BT_ERR("Error occured in Proxy call");
			if (error) {
				BT_ERR("Error occured in Proxy call [%s]\n", error->message);
				g_error_free(error);
			}
			g_object_unref(device_proxy);
			return BLUETOOTH_ERROR_INTERNAL;
		}
		g_variant_get(result , "(@a{sv})", &value);
		g_variant_unref(result);

		if (value) {
			tmp_value = g_variant_lookup_value(value,
							"Connected",
							G_VARIANT_TYPE_BOOLEAN);
			if (tmp_value) {
				*is_connected = g_variant_get_boolean(tmp_value);
				g_variant_unref(tmp_value);
			}
			g_variant_unref(value);
		}
	} else if(connection_type == BLUETOOTH_NAP_SERVICE) {
		return _bt_is_network_connected(_bt_get_net_conn(),
						device_address->addr, is_connected);
	} else {
		uuid = _bt_get_profile_uuid128(connection_type);
		if (uuid == NULL) {
			BT_ERR("uuid is NULL");
			return BLUETOOTH_ERROR_INTERNAL;
		}

		BT_DBG("uuid: %s", uuid);

		object_path = _bt_get_device_object_path(address);
		retv_if(object_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);
		BT_DBG("object_path: %s", object_path);
		device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
									NULL, BT_BLUEZ_NAME,
									object_path, BT_DEVICE_INTERFACE,  NULL, NULL);
		g_free(object_path);
		if (device_proxy == NULL) {
			BT_DBG("Device don't have this service");
			g_free(uuid);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		result = g_dbus_proxy_call_sync(device_proxy, "IsConnectedProfile",
					g_variant_new("(s)", uuid),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

		if (result == NULL) {
			BT_ERR("Error occured in Proxy call");
			if (error) {
				BT_ERR("Error occured in Proxy call [%s]\n", error->message);
				g_error_free(error);
			}
			g_object_unref(device_proxy);
			return BLUETOOTH_ERROR_INTERNAL;
		}
		g_variant_get(result, "(b)", is_connected);
		g_free(uuid);
		g_variant_unref(result);
	}

	g_object_unref(device_proxy);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_connected_link(bluetooth_device_address_t *device_address,
			bluetooth_connected_link_t *connected)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *object_path = NULL;

	GDBusProxy *device_proxy;
	GError *error = NULL;
	GDBusConnection *conn;
	GVariant *tmp_value = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;

	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	object_path = _bt_get_device_object_path(address);
	retv_if(object_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								object_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);
	g_free(object_path);
	if (device_proxy == NULL) {
		*connected = BLUETOOTH_CONNECTED_LINK_NONE;
		return BLUETOOTH_ERROR_NONE;
	}

	result = g_dbus_proxy_call_sync(device_proxy, "GetAll",
					g_variant_new("(s)", BT_DEVICE_INTERFACE),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

	if (error != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", error->message);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result , "(@a{sv})", &value);
	g_variant_unref(result);

	tmp_value = g_variant_lookup_value (value, "Connected", G_VARIANT_TYPE_BOOLEAN);
	if (tmp_value != NULL) {
		g_object_unref(device_proxy);
		g_variant_unref(value);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	*connected = g_variant_get_boolean (tmp_value);
	g_variant_unref(tmp_value);
	g_variant_unref(value);
	g_object_unref(device_proxy);

	return ret;
}

static void __bt_connect_le_device_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	GError *err = NULL;
	GVariant *out_param1;
	request_info_t *req_info;
	bluetooth_device_address_t device_addr = { {0} };
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");
	g_dbus_proxy_call_finish(proxy, res, &err);

	req_info = _bt_get_request_info(le_connection_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		g_object_unref(proxy);
		le_connection_info->device_proxy = NULL;
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in ConnectLE [%s]", err->message);

		if (g_strrstr(err->message, "NotSupported")) {
			BT_INFO("Connection Not Supported");
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		} else if (g_strrstr(err->message, "AlreadyConnected")) {
			BT_INFO("Connection already exists");
			result = BLUETOOTH_ERROR_ALREADY_CONNECT;
		} else if (g_strrstr(err->message, "Connection Timeout")) {
			BT_INFO("Connection Timeout");
			result = BLUETOOTH_ERROR_TIMEOUT;
		} else {
			BT_DBG("Default case");
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		}
	}

	g_object_unref(proxy);
	le_connection_info->device_proxy = NULL;

	_bt_convert_addr_string_to_type(device_addr.addr,
					le_connection_info->addr);

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
			&device_addr, sizeof(bluetooth_device_address_t), TRUE, NULL, NULL);
	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));

	g_variant_unref(out_param1);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	g_free(le_connection_info->addr);
	g_free(le_connection_info);
	le_connection_info = NULL;

	BT_DBG("-");
}

int _bt_connect_le_device(int request_id,
		const bluetooth_device_address_t *bd_addr,
		gboolean auto_connect)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusProxy *adapter_proxy;
	GDBusConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
			(unsigned char *)bd_addr->addr);

	if (le_connection_info) {
		BT_ERR("LE Connection in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

/* When auto_connect is true, Web TCT fail. It need to be checked */
	auto_connect = FALSE;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = _bt_get_device_object_path(device_address);
	if (device_path == NULL) {
		BT_DBG("device_path NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		return ret;
	}

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);


	le_connection_info = g_malloc0(sizeof(bt_funcion_data_t));
	le_connection_info->addr = g_strdup(device_address);
	le_connection_info->req_id = request_id;
	le_connection_info->device_proxy = device_proxy;

	g_dbus_proxy_call(device_proxy, "ConnectLE",
				 g_variant_new("(b)", auto_connect),
				 G_DBUS_CALL_FLAGS_NONE,
				 BT_MAX_DBUS_TIMEOUT,
				 NULL,
				 (GAsyncReadyCallback)__bt_connect_le_device_cb,
				 NULL);


	return ret;
}

static void __bt_disconnect_le_device_cb(GDBusProxy *proxy, GAsyncResult *res,
					gpointer user_data)
{
	GError *err = NULL;
	GVariant *out_param1;
	request_info_t *req_info;
	bt_funcion_data_t *le_disconnection_info = user_data;
	bluetooth_device_address_t device_addr = { {0} };
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");
	g_dbus_proxy_call_finish(proxy, res, &err);

	req_info = _bt_get_request_info(le_disconnection_info->req_id);
	if (req_info == NULL) {
		BT_ERR("req_info == NULL");
		g_object_unref(proxy);
		le_disconnection_info->device_proxy = NULL;
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in DisconnectLE [%s]", err->message);

		if (g_strrstr(err->message, "NotSupported")) {
			BT_INFO("Connection Not Supported");
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		} else if (g_strrstr(err->message, "NotConnected")) {
			BT_INFO("Connection does not exists");
			result = BLUETOOTH_ERROR_NOT_CONNECTED;
		} else if (g_strrstr(err->message, "Cancelled")) {
			BT_INFO("User Cancelled");
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else {
			BT_DBG("Default case");
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		}
	}

	g_object_unref(proxy);
	le_disconnection_info->device_proxy = NULL;

	_bt_convert_addr_string_to_type(device_addr.addr,
					le_disconnection_info->addr);

	if (req_info->context == NULL)
		goto done;

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
		&device_addr, sizeof(bluetooth_device_address_t), TRUE, NULL, NULL);

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));

	g_variant_unref(out_param1);

	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	g_free(le_disconnection_info->addr);
	g_free(le_disconnection_info);
	le_disconnection_info = NULL;

	BT_DBG("-");
}

int _bt_disconnect_le_device(int request_id,
		const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusProxy *adapter_proxy;
	GDBusConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;
	bt_funcion_data_t *le_disconnection_info = NULL;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
			(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = _bt_get_device_object_path(device_address);
	if (device_path == NULL) {
		BT_DBG("device_path NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		return ret;
	}

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	le_disconnection_info = g_malloc0(sizeof(bt_funcion_data_t));
	le_disconnection_info->addr = g_strdup(device_address);
	le_disconnection_info->req_id = request_id;
	le_disconnection_info->device_proxy = device_proxy;

	g_dbus_proxy_call(device_proxy, "DisconnectLE",
				 NULL,
				 G_DBUS_CALL_FLAGS_NONE,
				 BT_MAX_DBUS_TIMEOUT,
				 NULL,
				 (GAsyncReadyCallback)__bt_disconnect_le_device_cb,
				 le_disconnection_info);

	return ret;
}

int _bt_connect_le_ipsp_device(const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusProxy *adapter_proxy;
	GDBusConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
			(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = _bt_get_device_object_path(device_address);
	if (device_path == NULL) {
		BT_DBG("device_path NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		return ret;
	}

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
							NULL, BT_BLUEZ_NAME,
							device_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(device_proxy, "ConnectIpsp",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error) {
		BT_ERR("ConnectIpsp Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	return ret;
}

int _bt_disconnect_le_ipsp_device(const bluetooth_device_address_t *bd_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusProxy *adapter_proxy;
	GDBusConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(bd_addr, return);

	_bt_convert_addr_type_to_string(device_address,
			(unsigned char *)bd_addr->addr);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_path = _bt_get_device_object_path(device_address);
	if (device_path == NULL) {
		BT_DBG("device_path NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		return ret;
	}

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
							NULL, BT_BLUEZ_NAME,
							device_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(device_proxy, "DisconnectIpsp",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error) {
		BT_ERR("DisconnectIpsp Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	return ret;
}

int _bt_connect_profile(char *address, char *uuid,
						void *cb, gpointer func_data)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusConnection *conn;
	GDBusProxy *adapter_proxy;
	GError *error = NULL;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	object_path = _bt_get_device_object_path(address);
	if (object_path == NULL) {
		BT_ERR("No searched device");

		adapter_proxy = _bt_get_adapter_proxy();
		retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
					 g_variant_new("(s)", address),
					 G_DBUS_CALL_FLAGS_NONE,
					 -1,
					 NULL,
					 &error);

		if (error != NULL) {
			BT_ERR("CreateDevice Fail: %s", error->message);
			g_error_free(error);
		}

		object_path = _bt_get_device_object_path(address);
	}
	retv_if(object_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								object_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(object_path);
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call(proxy, "ConnectProfile",
				g_variant_new("(s)", uuid),
				G_DBUS_CALL_FLAGS_NONE,
				BT_MAX_DBUS_TIMEOUT,
				NULL,
				(GAsyncReadyCallback)cb,
				func_data);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_disconnect_profile(char *address, char *uuid,
						void *cb, gpointer func_data)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusConnection *conn;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	object_path = _bt_get_device_object_path(address);
	retv_if(object_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								object_path, BT_DEVICE_INTERFACE,  NULL, NULL);
	g_free(object_path);
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call(proxy, "DisconnectProfile",
					g_variant_new("(s)", uuid),
					G_DBUS_CALL_FLAGS_NONE,
					BT_MAX_DBUS_TIMEOUT,
					NULL,
					(GAsyncReadyCallback)cb,
					func_data);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_enable_rssi(bluetooth_device_address_t *bd_addr, int link_type,
		int low_threshold, int in_range_threshold, int high_threshold)
{
	int ret = BLUETOOTH_ERROR_NONE;
	GDBusProxy *proxy;
	GError *error = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(bd_addr, return);
	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X] Link Type[%d]",
			bd_addr->addr[0], bd_addr->addr[1],
			bd_addr->addr[2], bd_addr->addr[3],
			bd_addr->addr[4], bd_addr->addr[5],
			link_type);
	BT_DBG("Enable RSSI: [Threshold %d %d %d]", low_threshold,
			in_range_threshold, high_threshold);

	_bt_convert_addr_type_to_string(address, bd_addr->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(proxy, "EnableRssi",
				g_variant_new("(siiii)", address, link_type, low_threshold, in_range_threshold, high_threshold),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error != NULL) {
			BT_ERR("Dbus Call Error:[%s]", error->message);
			g_error_free(error);
			ret = BLUETOOTH_ERROR_INTERNAL;
	}
	return ret;
}

int _bt_get_rssi_strength(bluetooth_device_address_t *bd_addr,
					int link_type)
{
	int ret = BLUETOOTH_ERROR_NONE;
	GDBusProxy *proxy;
	GError *error = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(bd_addr, return);
	BT_DBG("BD Address [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X] Link Type[%d]",
			bd_addr->addr[0], bd_addr->addr[1],
			bd_addr->addr[2], bd_addr->addr[3],
			bd_addr->addr[4], bd_addr->addr[5],
			link_type);

	_bt_convert_addr_type_to_string(address, bd_addr->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(proxy, "GetRssiStrength",
				g_variant_new("(si)", address, link_type),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
			BT_ERR("Dbus Call Error:[%s]", error->message);
			g_error_free(error);
			ret = BLUETOOTH_ERROR_INTERNAL;
	}
	return ret;
}

int _bt_le_conn_update(unsigned char *device_address,
				float interval_min, float interval_max,
				guint16 latency, guint16 time_out)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	GDBusProxy *device_proxy = NULL;
	GDBusConnection *conn;
	GVariant *reply;
	guint32 min, max, to;
	guint32 min_supervision_to;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");

	BT_CHECK_PARAMETER(device_address, return);

	BT_DBG("Min interval: %f, Max interval: %f, Latency: %u, Supervision timeout: %u",
			interval_min, interval_max, latency, time_out);

	if (interval_min > interval_max ||
			interval_min < BT_LE_CONN_INTERVAL_MIN ||
			interval_max > BT_LE_CONN_INTERVAL_MAX) {
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto fail;
	}

	if (time_out < BT_LE_CONN_SUPER_TO_MIN ||
			time_out > BT_LE_CONN_SUPER_TO_MAX) {
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto fail;
	}

	if (latency > BT_LE_CONN_SLAVE_LATENCY_MAX) {
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto fail;
	}

	/*
	 * The Supervision_Timeout in milliseconds shall be larger than
	 * (1 + Conn_Latency) * Conn_Interval_Max * 2,
	 * where Conn_Interval_Max is given in milliseconds.
	 */
	min_supervision_to = (1 + latency) * interval_max * 2;
	if (time_out <= min_supervision_to) {
		ret = BLUETOOTH_ERROR_INVALID_PARAM;
		goto fail;
	}

	_bt_convert_addr_type_to_string(address, device_address);

	BT_DBG("Remote device address: %s", address);

	device_path = _bt_get_device_object_path(address);

	if (device_path == NULL) {
		BT_DBG("device_path NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_DBG("conn NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);

	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	min = interval_min / BT_LE_CONN_INTERVAL_SPLIT;
	max = interval_max / BT_LE_CONN_INTERVAL_SPLIT;
	to = time_out / BT_LE_CONN_TO_SPLIT;

	reply = g_dbus_proxy_call_sync(device_proxy, "LeConnUpdate",
				g_variant_new("(uuuu)", min, max, latency, to),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	g_object_unref(device_proxy);
	BT_ERR("LeConnUpdate Call Error for %s", address);
	if (reply == NULL) {
		if (error) {
			BT_ERR("Error %s[%s]", error->message, address);
			g_error_free(error);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}
	g_variant_unref(reply);
	BT_DBG("-");

fail:
	return ret;
}

int _bt_set_pin_code(bluetooth_device_address_t *device_address,
				bluetooth_device_pin_code_t *pin_code)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GSList *l = NULL;
	bt_pin_code_info_t *pin_info = NULL;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(pin_code, return);
	retv_if(g_slist_length(pin_info_list) >= BT_DEVICE_PIN_CODE_SLOT_MAX,
			BLUETOOTH_ERROR_NO_RESOURCES);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	for (l = pin_info_list; l != NULL; l = l->next) {
		pin_info = l->data;

		if (g_strcmp0(pin_info->address, address) == 0) {
			g_free(pin_info->pin_code);
			pin_info->pin_code = g_strdup(pin_code->pin_code);
			return BLUETOOTH_ERROR_NONE;
		}
	}

	pin_info = g_malloc0(sizeof(bt_pin_code_info_t));
	if (pin_info != NULL) {
		pin_info->address = g_strdup(address);
		pin_info->pin_code = g_strdup(pin_code->pin_code);
		pin_info_list = g_slist_append(pin_info_list, pin_info);
		return BLUETOOTH_ERROR_NONE;
	}

	return BLUETOOTH_ERROR_INTERNAL;
}

gint __bt_compare_address(gpointer *a, gpointer *b)
{
	bt_pin_code_info_t *pin_info = (bt_pin_code_info_t *)a;
	char *address = (char *)b;
	return g_strcmp0(pin_info->address, address);
}

int _bt_unset_pin_code(bluetooth_device_address_t *device_address)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GSList *l = NULL;
	bt_pin_code_info_t *pin_info = NULL;

	BT_CHECK_PARAMETER(device_address, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	l = g_slist_find_custom(pin_info_list, address,
		(GCompareFunc)__bt_compare_address);
	if (l)
		pin_info = l->data;
	if (pin_info) {
		pin_info_list = g_slist_remove(pin_info_list, pin_info);
		g_free(pin_info->address);
		g_free(pin_info->pin_code);
		g_free(pin_info);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_device_pin_code(const char *address, char *pin_code)
{
	GSList *l = NULL;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(pin_code, return);

	for (l = pin_info_list; l != NULL; l = l->next) {
		bt_pin_code_info_t *pin_info = l->data;

		if (g_strcmp0(pin_info->address, address) == 0) {
			g_strlcpy(pin_code, pin_info->pin_code,
					BLUETOOTH_PIN_CODE_MAX_LENGTH + 1);

			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NOT_FOUND;
}

int _bt_get_le_connection_parameter(bluetooth_le_connection_mode_t mode,
		bluetooth_le_connection_param_t *param)
{
	if (param == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (mode < BLUETOOTH_LE_CONNECTION_MODE_BALANCED ||
	    mode > BLUETOOTH_LE_CONNECTION_MODE_LOW_POWER)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	memset(param, 0x00, sizeof(bluetooth_le_connection_param_t));

	switch (mode) {
	case BLUETOOTH_LE_CONNECTION_MODE_BALANCED:
		param->interval_min = BT_LE_CONN_PARAM_BALANCED_MIN_INTERVAL;
		param->interval_max = BT_LE_CONN_PARAM_BALANCED_MAX_INTERVAL;
		param->latency = BT_LE_CONN_PARAM_BALANCED_SLAVE_LATENCY;
		param->timeout = BT_LE_CONN_PARAM_DEFAULT_SUPERVISION_TIMEOUT;
		break;

	case BLUETOOTH_LE_CONNECTION_MODE_LOW_LATENCY:
		param->interval_min = BT_LE_CONN_PARAM_LOW_LATENCY_MIN_INTERVAL;
		param->interval_max = BT_LE_CONN_PARAM_LOW_LATENCY_MAX_INTERVAL;
		param->latency = BT_LE_CONN_PARAM_LOW_LATENCY_SLAVE_LATENCY;
		param->timeout = BT_LE_CONN_PARAM_DEFAULT_SUPERVISION_TIMEOUT;
		break;

	case BLUETOOTH_LE_CONNECTION_MODE_LOW_POWER:
		param->interval_min = BT_LE_CONN_PARAM_LOW_POWER_MIN_INTERVAL;
		param->interval_max = BT_LE_CONN_PARAM_LOW_POWER_MAX_INTERVAL;
		param->latency = BT_LE_CONN_PARAM_LOW_POWER_SLAVE_LATENCY;
		param->timeout = BT_LE_CONN_PARAM_DEFAULT_SUPERVISION_TIMEOUT;
		break;

	default:
		BT_ERR("Unhandled mode : %d", mode);
		break;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_passkey_reply(const char *passkey, gboolean authentication_reply)
{
	GapAgentPrivate *agent = _bt_get_adapter_agent();
	retv_if(!agent, BLUETOOTH_ERROR_INTERNAL);

	if (authentication_reply)
		gap_agent_reply_pin_code(agent, GAP_AGENT_ACCEPT, passkey, NULL);
	else
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, passkey, NULL);

	BT_DBG("BT_PASSKEY_REPLY");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_passkey_confirmation_reply(gboolean confirmation_reply)
{
	GapAgentPrivate *agent = _bt_get_adapter_agent();
	retv_if(!agent, BLUETOOTH_ERROR_INTERNAL);

	if (confirmation_reply)
		gap_agent_reply_confirmation(agent, GAP_AGENT_ACCEPT, NULL);
	else
		gap_agent_reply_confirmation(agent, GAP_AGENT_REJECT, NULL);

	BT_DBG("BT_PASSKEY_CONFIRMATION_REPLY");
	return BLUETOOTH_ERROR_NONE;
}
