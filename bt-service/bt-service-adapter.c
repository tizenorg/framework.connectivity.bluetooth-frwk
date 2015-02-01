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

#include <stdio.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <vconf.h>
#include <syspopup_caller.h>
#include <aul.h>
#include <notification.h>
#include <journal/device.h>

#include "alarm.h"

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-adapter.h"
#include "bt-service-util.h"
#include "bt-service-network.h"
#include "bt-service-obex-server.h"
#include "bt-service-agent.h"
#include "bt-service-main.h"
#include "bt-service-avrcp.h"

typedef struct {
	guint event_id;
	int timeout;
	time_t start_time;
	int alarm_id;
} bt_adapter_timer_t;

bt_adapter_timer_t visible_timer;

static gboolean is_discovering;
static gboolean is_le_discovering;
static bt_le_discovery_type_t le_discovery_type = BT_LE_PASSIVE_SCAN;
static gboolean cancel_by_user;
static bt_status_t adapter_status = BT_DEACTIVATED;
static bt_le_status_t adapter_le_status = BT_LE_DEACTIVATED;
static void *adapter_agent = NULL;
static DBusGProxy *core_proxy = NULL;
static guint timer_id = 0;
static guint le_timer_id = 0;

#define BT_CORE_NAME "org.projectx.bt_core"
#define BT_CORE_PATH "/org/projectx/bt_core"
#define BT_CORE_INTERFACE "org.projectx.btcore"

#define BT_ADV_INTERVAL_MIN 20 /* msec */
#define BT_ADV_INTERVAL_MAX 10240
#define BT_ADV_INTERVAL_SPLIT 0.625
#define BT_DEFAULT_ADV_MIN_INTERVAL 500
#define BT_DEFAULT_ADV_MAX_INTERVAL 500
#define BT_LE_SCAN_INTERVAL_MIN 2.5
#define BT_LE_SCAN_INTERVAL_MAX 10240
#define BT_LE_SCAN_WINDOW_MIN 2.5
#define BT_LE_SCAN_WINDOW_MAX 10240

#define BT_DISABLE_TIME 500 /* 500 ms */
#define BT_DEFAULT_ADV_MIN_INTERVAL	500
#define BT_DEFAULT_ADV_MAX_INTERVAL	500
#define BT_ADV_FILTER_POLICY_DEFAULT	0x00
#define BT_ADV_TYPE_DEFAULT	0x00
#define BT_ADV_FILTER_POLICY_ALLOW_SCAN_CONN_WL_ONLY	0x03

static gboolean is_advertising;
static bluetooth_advertising_params_t adv_params = {
	BT_DEFAULT_ADV_MIN_INTERVAL,
	BT_DEFAULT_ADV_MAX_INTERVAL,
	BT_ADV_FILTER_POLICY_DEFAULT,
	BT_ADV_TYPE_DEFAULT};
static bluetooth_advertising_data_t adv_data = { {0} };
static int adv_data_len;
static bluetooth_scan_resp_data_t resp_data = { {0} };
static int resp_data_len;

DBusGProxy *_bt_init_core_proxy(void)
{
       DBusGProxy *proxy;
	DBusGConnection *conn;

	conn = _bt_get_system_gconn();
	if (!conn)
		return NULL;

       proxy = dbus_g_proxy_new_for_name(conn, BT_CORE_NAME,
                       BT_CORE_PATH, BT_CORE_INTERFACE);
	if (!proxy)
		return NULL;

       core_proxy = proxy;

       return proxy;
}

static DBusGProxy *__bt_get_core_proxy(void)
{
       return (core_proxy) ? core_proxy : _bt_init_core_proxy();
}

static gboolean __bt_is_factory_test_mode(void)
{
	int mode = 0;

	if (vconf_get_bool(VCONFKEY_BT_DUT_MODE, &mode)) {
		BT_ERR("Get the DUT Mode fail");
		return TRUE;
	}

	if (mode != FALSE) {
		BT_INFO("DUT Test Mode !!");
		return TRUE;
	}

	return FALSE;
}

static gboolean __bt_timeout_handler(gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	time_t current_time;
	int time_diff;

	/* Take current time */
	time(&current_time);
	time_diff = difftime(current_time, visible_timer.start_time);

	/* Send event to application */
	_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &time_diff,
			DBUS_TYPE_INVALID);

	if (visible_timer.timeout <= time_diff) {
		g_source_remove(visible_timer.event_id);
		visible_timer.event_id = 0;
		visible_timer.timeout = 0;

#ifndef TIZEN_WEARABLE
		if (vconf_set_int(BT_FILE_VISIBLE_TIME, 0) != 0)
			BT_ERR("Set vconf failed\n");
#endif
		return FALSE;
	}

	return TRUE;
}

static int __bt_visibility_alarm_cb(alarm_id_t alarm_id, void* user_param)
{
	BT_DBG("__bt_visibility_alarm_cb - alram id = [%d] \n", alarm_id);

	int result = BLUETOOTH_ERROR_NONE;
	int timeout = 0;

	if (alarm_id != visible_timer.alarm_id)
		return 0;

	if (visible_timer.event_id) {
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_INT16, &timeout,
				DBUS_TYPE_INVALID);
		g_source_remove(visible_timer.event_id);
		visible_timer.event_id = 0;
		visible_timer.timeout = 0;

#ifndef TIZEN_WEARABLE
		if (vconf_set_int(BT_FILE_VISIBLE_TIME, 0) != 0)
			BT_ERR("Set vconf failed\n");
#endif
	}
	/* Switch Off visibility in Bluez */
	_bt_set_discoverable_mode(BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE, 0);
	visible_timer.alarm_id = 0;
	alarmmgr_fini();
	return 0;
}

static void __bt_visibility_alarm_create()
{
	alarm_id_t alarm_id;
	int result;

	result = alarmmgr_add_alarm(ALARM_TYPE_VOLATILE, visible_timer.timeout,
						0, NULL, &alarm_id);
	if(result < 0) {
		BT_ERR("Failed to create alarm error = %d\n", result);
		alarmmgr_fini();
	} else {
		BT_DBG("Alarm created = %d\n", alarm_id);
		visible_timer.alarm_id = alarm_id;
	}
}

static void __bt_visibility_alarm_remove()
{
	if (visible_timer.event_id > 0) {
		g_source_remove(visible_timer.event_id);
		visible_timer.event_id = 0;
	}

	if (visible_timer.alarm_id > 0) {
		alarmmgr_remove_alarm(visible_timer.alarm_id);
		visible_timer.alarm_id = 0;
	}
}

int __bt_set_visible_time(int timeout)
{
	int result;

	__bt_visibility_alarm_remove();

	visible_timer.timeout = timeout;

#ifndef TIZEN_WEARABLE
	if (vconf_set_int(BT_FILE_VISIBLE_TIME, timeout) != 0)
		BT_ERR("Set vconf failed\n");
#endif

	if (timeout <= 0)
		return BLUETOOTH_ERROR_NONE;

	/* Take start time */
	time(&(visible_timer.start_time));
	visible_timer.event_id = g_timeout_add_seconds(1,
				__bt_timeout_handler, NULL);

	/* Set Alarm timer to switch off BT */
	result = alarmmgr_init("bt-service");
	if (result != 0)
		return BLUETOOTH_ERROR_INTERNAL;

	result = alarmmgr_set_cb(__bt_visibility_alarm_cb, NULL);
	if (result != 0)
		return BLUETOOTH_ERROR_INTERNAL;

	__bt_visibility_alarm_create();

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_get_service_list(GValue *value, bluetooth_device_info_t *dev)
{
	int i;
	char **uuids;
	char **parts;

	ret_if(value == NULL);
	ret_if(dev == NULL);

	uuids = g_value_get_boxed(value);
	ret_if(uuids == NULL);

	dev->service_index = 0;

	for (i = 0; uuids[i] != NULL; i++) {
		g_strlcpy(dev->uuids[i], uuids[i], BLUETOOTH_UUID_STRING_MAX);

		parts = g_strsplit(uuids[i], "-", -1);

		if (parts == NULL || parts[0] == NULL)
			break;

		dev->service_list_array[i] = g_ascii_strtoull(parts[0], NULL, 16);
		g_strfreev(parts);

		dev->service_index++;
	}
}

static int __bt_get_bonded_device_info(gchar *device_path,
		bluetooth_device_info_t *dev_info)
{
	GValue *value = { 0 };
	GError *err = NULL;
	DBusGProxy *device_proxy;
	const gchar *address;
	const gchar *name;
	unsigned int cod;
	gint rssi;
	gboolean trust;
	gboolean paired;
	gboolean connected;
	GByteArray *manufacturer_data = NULL;
	GHashTable *hash = NULL;
	int ret;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(device_path, return);
	BT_CHECK_PARAMETER(dev_info, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				device_path, BT_PROPERTIES_INTERFACE);

	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "GetAll", &err,
				G_TYPE_STRING, BT_DEVICE_INTERFACE,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
				G_TYPE_VALUE), &hash, G_TYPE_INVALID);

	g_object_unref(device_proxy);

	if (err != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);
		g_error_free(err);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Paired");
		paired = g_value_get_boolean(value);

		value = g_hash_table_lookup(hash, "Address");
		address = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Alias");
		name = value ? g_value_get_string(value) : NULL;

		if (name != NULL)
			BT_DBG("Alias Name [%s]", name);
		else {
			value = g_hash_table_lookup(hash, "Name");
			name = value ? g_value_get_string(value) : NULL;
		}

		value = g_hash_table_lookup(hash, "Class");
		cod = value ? g_value_get_uint(value) : 0;

		value = g_hash_table_lookup(hash, "Connected");
		connected = value ? g_value_get_boolean(value) : FALSE;

		value = g_hash_table_lookup(hash, "Trusted");
		trust = value ? g_value_get_boolean(value) : FALSE;

		BT_DBG("paired: %d", paired);
		BT_DBG("trust: %d", trust);

		if ((paired == FALSE) && (trust == FALSE)) {
			return BLUETOOTH_ERROR_NOT_PAIRED;
		}

		value = g_hash_table_lookup(hash, "RSSI");
		rssi = value ? g_value_get_int(value) : 0;

		value = g_hash_table_lookup(hash, "UUIDs");
		__bt_get_service_list(value, dev_info);

		value = g_hash_table_lookup(hash, "ManufacturerDataLen");
		dev_info->manufacturer_data.data_len = value ? g_value_get_uint(value) : 0;

		value = g_hash_table_lookup(hash, "ManufacturerData");
		manufacturer_data = value ? g_value_get_boxed(value) : NULL;
		if (manufacturer_data) {
			if (manufacturer_data->len > 0) {
				BT_DBG("manufacturer_data->len  = %d", manufacturer_data->len);
				memcpy(dev_info->manufacturer_data.data, manufacturer_data->data, manufacturer_data->len);
			}
		}

		_bt_convert_addr_string_to_type(dev_info->device_address.addr,
						address);

		_bt_divide_device_class(&dev_info->device_class, cod);

		g_strlcpy(dev_info->device_name.name, name,
				BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);

		dev_info->rssi = rssi;
		dev_info->trust = trust;
		dev_info->paired = paired;
		dev_info->connected = connected;
		g_hash_table_destroy(hash);
		ret = BLUETOOTH_ERROR_NONE;
	} else {
		BT_ERR("Hash is NULL\n");
		ret = BLUETOOTH_ERROR_INTERNAL;
	}

	return ret;
}

void _bt_set_discovery_status(gboolean mode)
{
	is_discovering = mode;
}

void _bt_set_le_discovery_status(gboolean mode)
{
	is_le_discovering = mode;
}

void _bt_set_le_discovery_type(bt_le_discovery_type_t type)
{
	le_discovery_type = type;
}

bt_le_discovery_type_t _bt_get_le_discovery_type(void)
{
	return le_discovery_type;
}
void _bt_set_advertising_status(gboolean mode)
{
	is_advertising = mode;
}

void _bt_set_cancel_by_user(gboolean value)
{
	cancel_by_user = value;
}

gboolean _bt_get_cancel_by_user(void)
{
	return cancel_by_user;
}

void _bt_adapter_set_status(bt_status_t status)
{
	BT_INFO("adapter_status changed [%d] -> [%d]", adapter_status, status);
	adapter_status = status;
}

bt_status_t _bt_adapter_get_status(void)
{
	return adapter_status;
}

void _bt_adapter_set_le_status(bt_le_status_t status)
{
	BT_INFO("adapter_le_status changed [%d] -> [%d]", adapter_le_status, status);
	adapter_le_status = status;
}

bt_le_status_t _bt_adapter_get_le_status(void)
{
	return adapter_le_status;
}

static void __bt_phone_name_changed_cb(keynode_t *node, void *data)
{
	char *phone_name = NULL;
	char *ptr = NULL;

	if (node == NULL)
		return;

	if (vconf_keynode_get_type(node) == VCONF_TYPE_STRING) {
		phone_name = vconf_keynode_get_str(node);

		if (phone_name && strlen(phone_name) != 0) {
                        if (!g_utf8_validate(phone_name, -1,
							(const char **)&ptr))
                                *ptr = '\0';

			_bt_set_local_name(phone_name);
		}
	}
}

#ifdef TIZEN_WEARABLE
static char * __bt_change_dev_name(const char *default_name)
{
	FILE *fp = NULL;
	char *buf = NULL;
	char *name = NULL;
	int result;

	if ((fp = fopen("/csa/bluetooth/.bd_addr", "r")) == NULL) {
		BT_ERR("Unable to open bd_addr");
		return NULL;
	}

	result = fseek(fp, -4, SEEK_END);
	if (result < 0) {
		BT_ERR("fseek is failed");
		fclose(fp);
		return NULL;
	}

	buf = (char *)malloc(sizeof(char) * 5);
	if (buf == NULL) {
		BT_ERR("malloc is failed");
		fclose(fp);
		return NULL;
	}

	result = fread(buf, 1, 4, fp);
	buf[4] = '\0';

	name = g_strdup_printf("%s (%s)", default_name, buf);

	BT_INFO("%s", name);

	free(buf);
	fclose(fp);

	return name ;
}
#else
static void __bt_set_visible_mode(void)
{
	int timeout = 0;

	if (vconf_get_int(BT_FILE_VISIBLE_TIME, &timeout) != 0)
                BT_ERR("Fail to get the timeout value");

	/* -1: Always on */
	if (timeout == -1) {
	        if (_bt_set_discoverable_mode(
			BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE,
			timeout) != BLUETOOTH_ERROR_NONE) {
	                if (vconf_set_int(BT_FILE_VISIBLE_TIME, 0) != 0)
	                        BT_ERR("Set vconf failed");
	        }
	}
}
#endif

static void __bt_set_local_name(void)
{
	char *phone_name = NULL;
	char *ptr = NULL;

	phone_name = vconf_get_str(VCONFKEY_SETAPPL_DEVICE_NAME_STR);

	if (!phone_name)
		return;

	if (strlen(phone_name) != 0) {
		if (!g_utf8_validate(phone_name, -1, (const char **)&ptr))
			*ptr = '\0';

#ifdef TIZEN_WEARABLE
		if (strstr(phone_name, "(") == NULL) {
			char *tmp = __bt_change_dev_name(phone_name);
			if (tmp != NULL) {
				free(phone_name);
				phone_name = tmp;
			}
		}
#endif
		_bt_set_local_name(phone_name);
	}
	free(phone_name);
}

static int __bt_set_enabled(void)
{
	int adapter_status = BT_ADAPTER_DISABLED;
	int result = BLUETOOTH_ERROR_NONE;

	_bt_check_adapter(&adapter_status);

	if (adapter_status == BT_ADAPTER_DISABLED) {
		BT_ERR("Bluetoothd is not running");
		return BLUETOOTH_ERROR_INTERNAL;
	}

#ifndef TIZEN_WEARABLE
	__bt_set_visible_mode();
#endif

	__bt_set_local_name();

	/* Update Bluetooth Status to notify other modules */
	if (vconf_set_int(VCONFKEY_BT_STATUS, VCONFKEY_BT_STATUS_ON) != 0)
		BT_ERR("Set vconf failed\n");

	if (vconf_set_int(VCONFKEY_BT_DEVICE, VCONFKEY_BT_DEVICE_NONE) != 0)
		BT_ERR("Set vconf failed\n");

	/* Send enabled event to API */
	_bt_send_event(BT_ADAPTER_EVENT, BLUETOOTH_EVENT_ENABLED,
				DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);

	return BLUETOOTH_ERROR_NONE;
}

void _bt_set_disabled(int result)
{
	int power_off_status = 0;
	int ret;
	int ret_pm_ignore;
	int pm_ignore_mode = 0;

	ret = vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &power_off_status);
	BT_DBG("ret : %d, power_off_status : %d", ret, power_off_status);

	ret_pm_ignore = vconf_get_int(VCONFKEY_PM_KEY_IGNORE, &pm_ignore_mode);

	/* Update the vconf BT status in normal Deactivation case only */
	if (ret == 0 && power_off_status == VCONFKEY_SYSMAN_POWER_OFF_NONE &&
		ret_pm_ignore == 0 && pm_ignore_mode != VCONFKEY_PM_KEY_LOCK) {

		BT_DBG("Update vconf for BT normal Deactivation");

		if (result == BLUETOOTH_ERROR_TIMEOUT)
			if (vconf_set_int(BT_OFF_DUE_TO_TIMEOUT, 1) != 0 )
				BT_ERR("Set vconf failed");

		/* Update Bluetooth Status to notify other modules */
		if (vconf_set_int(VCONFKEY_BT_STATUS, VCONFKEY_BT_STATUS_OFF) != 0)
			BT_ERR("Set vconf failed");
	}

	if (vconf_set_int(VCONFKEY_BT_DEVICE, VCONFKEY_BT_DEVICE_NONE) != 0)
		BT_ERR("Set vconf failed\n");

	_bt_adapter_set_status(BT_DEACTIVATED);

	if (_bt_adapter_get_le_status() != BT_LE_DEACTIVATED) {
		/* Send disabled event */
		_bt_send_event(BT_ADAPTER_EVENT, BLUETOOTH_EVENT_DISABLED,
				DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	}

	BT_INFO("Adapter disabled");
}

static int __bt_set_le_enabled(void)
{
	BT_DBG("+");
	int result = BLUETOOTH_ERROR_NONE;
	bt_status_t status;

	__bt_set_local_name();

	/* Update Bluetooth Status to notify other modules */
	if (vconf_set_int(VCONFKEY_BT_LE_STATUS, VCONFKEY_BT_LE_STATUS_ON) != 0)
		BT_ERR("Set vconf failed\n");

	/* Send enabled event to API */
	/*
	_bt_send_event(BT_ADAPTER_EVENT, BLUETOOTH_EVENT_ENABLED,
				DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	*/
	status = _bt_adapter_get_status();
	if (status == BT_DEACTIVATED) {
		BT_INFO("BREDR is off, turn off PSCAN");
		_bt_set_connectable(FALSE);
	}
	if (le_timer_id > 0) {
		g_source_remove(le_timer_id);
		le_timer_id = 0;
	}

	/* Send enabled event to API */
	_bt_send_event(BT_LE_ADAPTER_EVENT, BLUETOOTH_EVENT_LE_ENABLED,
				DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

void _bt_set_le_disabled(int result)
{
	int power_off_status;
	int ret;

	ret = vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &power_off_status);
	BT_DBG("ret : %d", ret);
	BT_DBG("power_off_status : %d", power_off_status);

	/* Update Bluetooth Status to notify other modules */
	BT_DBG("Update vconf for BT LE normal Deactivation");
	if (vconf_set_int(VCONFKEY_BT_LE_STATUS, VCONFKEY_BT_LE_STATUS_OFF) != 0)
		BT_ERR("Set vconf failed\n");
	_bt_adapter_set_le_status(BT_LE_DEACTIVATED);

	if (_bt_adapter_get_status() != BT_DEACTIVATED) {
		/* Send disabled event */
		_bt_send_event(BT_LE_ADAPTER_EVENT, BLUETOOTH_EVENT_LE_DISABLED,
				DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
	}
}

void *_bt_get_adapter_agent(void)
{
	return adapter_agent;
}

int _bt_enable_core(void)
{
	DBusGProxy *proxy;

	proxy = __bt_get_core_proxy();
	ret_if(!proxy);

	if (dbus_g_proxy_call(proxy, "EnableCore", NULL,
			G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
	}

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_service_flight_ps_mode_cb(keynode_t *node, void *data)
{
	gboolean flight_mode = FALSE;
	int power_saving_mode = 0;
	int type;

	DBG_SECURE("key=%s", vconf_keynode_get_name(node));
	type = vconf_keynode_get_type(node);
	if (type == VCONF_TYPE_BOOL) {
		flight_mode = vconf_keynode_get_bool(node);
		if (flight_mode != TRUE) {
			BT_ERR("Ignore the event");
			return;
		}
	} else if (type == VCONF_TYPE_INT) {
		power_saving_mode = vconf_keynode_get_int(node);
		if (power_saving_mode != 2) {
			BT_ERR("Ignore the event");
			return;
		}
	} else {
		BT_ERR("Invaild vconf key type : %d", type);
		return;
	}

	_bt_enable_core();
}

void _bt_service_register_vconf_handler(void)
{
	int ret;
	BT_DBG("+");

	ret = vconf_notify_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb, NULL);
	if (ret < 0)
		BT_ERR("Unable to register key handler");

#ifndef TIZEN_WEARABLE
	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb, NULL);
	if (ret < 0)
		BT_ERR("Unable to register key handler");
#endif
}

void _bt_service_unregister_vconf_handler(void)
{
	vconf_ignore_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb);

#ifndef TIZEN_WEARABLE
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb);
#endif
}

void _bt_handle_adapter_added(void)
{
	BT_DBG("+");
	bt_status_t status;
	bt_le_status_t le_status;

	if (timer_id > 0) {
		BT_DBG("g_source is removed");
		g_source_remove(timer_id);
		timer_id = 0;
	}

	status = _bt_adapter_get_status();
	le_status = _bt_adapter_get_le_status();
	BT_DBG("status : %d", status);
	BT_DBG("le_status : %d", le_status);

	adapter_agent = _bt_create_agent(BT_ADAPTER_AGENT_PATH, TRUE);
	if (!adapter_agent) {
		BT_ERR("Fail to register agent");
		return;
	}

	if (_bt_register_media_player() != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to register media player");

	if (_bt_register_obex_server() != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to init obex server");

#ifndef TIZEN_WEARABLE
	if (_bt_network_activate() != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to activate network");
#endif

	/* add the vconf noti handler */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_DEVICE_NAME_STR,
					__bt_phone_name_changed_cb, NULL);

	if (le_status == BT_LE_ACTIVATING) {
		__bt_set_le_enabled();
		_bt_adapter_set_le_status(BT_LE_ACTIVATED);
	}
	if (status == BT_ACTIVATING) {
		__bt_set_enabled();
		_bt_adapter_set_status(BT_ACTIVATED);
	}
	journal_bt_on();

	_bt_service_register_vconf_handler();
}

void _bt_handle_adapter_removed(void)
{
	int ret;

	_bt_adapter_set_status(BT_DEACTIVATED);
	journal_bt_off();

	__bt_visibility_alarm_remove();

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_DEVICE_NAME_STR,
				(vconf_callback_fn)__bt_phone_name_changed_cb);
	if (0 != ret) {
		ERR("vconf_ignore_key_changed failed\n");
	}

	_bt_destroy_agent(adapter_agent);
	adapter_agent = NULL;

	_bt_reliable_terminate_service(NULL);
}

static gboolean __bt_enable_timeout_cb(gpointer user_data)
{
	DBusGProxy *proxy;

	timer_id = 0;

	retv_if(_bt_adapter_get_status() == BT_ACTIVATED, FALSE);

	BT_ERR("EnableAdapter is failed");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return FALSE;

	/* Clean up the process */
	if (dbus_g_proxy_call(proxy, "DisableAdapter", NULL,
				G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
	}

	_bt_set_disabled(BLUETOOTH_ERROR_TIMEOUT);

	/* Display notification */
	notification_status_message_post(BT_STR_NOT_SUPPORT);

	_bt_terminate_service(NULL);

	return FALSE;
}

static gboolean __bt_enable_le_timeout_cb(gpointer user_data)
{
	DBusGProxy *proxy;

	le_timer_id = 0;

	retv_if(_bt_adapter_get_le_status() == BT_LE_ACTIVATED, FALSE);

	BT_ERR("EnableAdapterLE is failed");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return FALSE;

	/* Clean up the process */
	if (dbus_g_proxy_call(proxy, "DisableAdapterLe", NULL,
				G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
	}

	_bt_adapter_set_le_status(BT_LE_DEACTIVATED);

	_bt_set_le_disabled(BLUETOOTH_ERROR_TIMEOUT);

	/* Display notification */
	notification_status_message_post(BT_STR_NOT_SUPPORT);

	if (_bt_adapter_get_status() == BT_DEACTIVATED)
		_bt_terminate_service(NULL);

	return FALSE;
}

void _bt_adapter_start_le_enable_timer(void)
{
	if (le_timer_id > 0) {
		g_source_remove(le_timer_id);
		le_timer_id = 0;
	}

	le_timer_id = g_timeout_add(BT_ENABLE_TIMEOUT,
			__bt_enable_le_timeout_cb, NULL);

	return;
}

void _bt_adapter_start_enable_timer(void)
{
	if (timer_id > 0) {
		g_source_remove(timer_id);
		timer_id = 0;
	}

	timer_id = g_timeout_add(BT_ENABLE_TIMEOUT,
			__bt_enable_timeout_cb, NULL);

	return;
}

int _bt_enable_adapter(void)
{
	DBusGProxy *proxy;
	GError *err = NULL;
	int ret;
	bt_status_t status = _bt_adapter_get_status();
	bt_le_status_t le_status = _bt_adapter_get_le_status();
	BT_DBG("");

	if (status == BT_ACTIVATING) {
		BT_ERR("Enabling in progress");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	if (status == BT_ACTIVATED) {
		BT_ERR("Already enabled");
		return BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED;
	}

	if (status == BT_DEACTIVATING || le_status == BT_LE_DEACTIVATING) {
		BT_ERR("Disabling in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	_bt_adapter_set_status(BT_ACTIVATING);

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;

	if (le_status == BT_LE_ACTIVATED) {
		BT_INFO("LE Already enabled. Just turn on PSCAN");
		ret = _bt_set_connectable(TRUE);
		if (ret == BLUETOOTH_ERROR_NONE) {
			_bt_adapter_set_status(BT_ACTIVATED);
		} else {
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}

	 if (dbus_g_proxy_call_with_timeout(proxy, "EnableAdapter",
					BT_ENABLE_TIMEOUT, &err,
					G_TYPE_INVALID,
					G_TYPE_INVALID) == FALSE) {

		_bt_adapter_set_status(BT_DEACTIVATED);

		if (err != NULL) {
			BT_ERR("Bt core call failed: [%s]", err->message);
			g_error_free(err);
		}

		/* Clean up the process */
		if (dbus_g_proxy_call(proxy, "DisableAdapter", NULL,
				G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
				BT_ERR("Bt core call failed");
		}

		/* Display notification */
		notification_status_message_post(BT_STR_NOT_SUPPORT);

		/* Terminate myself */
		g_idle_add((GSourceFunc)_bt_terminate_service, NULL);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (le_status == BT_LE_ACTIVATED) {
		__bt_set_enabled();
	} else {
		_bt_adapter_start_enable_timer();
	}

	return BLUETOOTH_ERROR_NONE;
}

static gboolean __bt_disconnect_all(void)
{
	int i;
	DBusGConnection *conn;
	DBusGProxy *dev_proxy;
	gboolean ret = FALSE;

	GArray *device_list;
	bluetooth_device_info_t info;
	guint size;
	char *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };


	BT_DBG("");

	conn = _bt_get_system_gconn();

	device_list = g_array_new(FALSE, FALSE, sizeof(gchar));

	if (_bt_get_bonded_devices(&device_list)
					!= BLUETOOTH_ERROR_NONE) {
		g_array_free(device_list, TRUE);
		return FALSE;
	}

	size = (device_list->len) / sizeof(bluetooth_device_info_t);

	for (i = 0; i < size; i++) {

		info = g_array_index(device_list,
				bluetooth_device_info_t, i);

		if (info.connected == TRUE) {
			BT_DBG("Found Connected device");
			_bt_convert_addr_type_to_string(address, info.device_address.addr);
			device_path = _bt_get_device_object_path(address);
			if (device_path == NULL)
				continue;

			BT_DBG("Disconnecting : %s", device_path);

			dev_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
					device_path, BT_DEVICE_INTERFACE);
			if (dev_proxy == NULL)
				continue;

			if(!dbus_g_proxy_call(dev_proxy, "Disconnect",
						NULL, G_TYPE_INVALID, G_TYPE_INVALID)) {
				BT_ERR("Disconnect fail error.");
				g_object_unref(dev_proxy);
				return FALSE;
			}
		}
	}
	ret = TRUE;
	g_array_free(device_list, TRUE);

	return ret;
}

static gboolean __bt_set_disabled_timeout_cb(gpointer user_data)
{
	BT_DBG("");
	_bt_set_disabled(BLUETOOTH_ERROR_NONE);

	return FALSE;
}

int __bt_disable_cb(void)
{
	FN_START;
	DBusGProxy *proxy;
	bt_le_status_t le_status;
	int ret;

	_bt_adapter_set_status(BT_DEACTIVATING);
	le_status = _bt_adapter_get_le_status();
	BT_DBG("le_status : %d", le_status);
	if (le_status == BT_LE_ACTIVATED) {
		BT_INFO("LE is enabled. Just turn off PSCAN");

		if (_bt_is_discovering())
			_bt_cancel_discovery();

		if (_bt_is_connectable() == FALSE) {
			g_timeout_add(100, (GSourceFunc)__bt_set_disabled_timeout_cb, NULL);
		} else {
			ret = _bt_set_connectable(FALSE);
			if (ret != BLUETOOTH_ERROR_NONE) {
				BT_ERR("_bt_set_connectable fail!");
				_bt_adapter_set_status(BT_ACTIVATED);
				return BLUETOOTH_ERROR_INTERNAL;
			}
		}
	}

	proxy = __bt_get_core_proxy();
	retv_if(!proxy, BLUETOOTH_ERROR_INTERNAL);

	if (dbus_g_proxy_call(proxy, "DisableAdapter", NULL,
			G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
		_bt_adapter_set_status(BT_ACTIVATED);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_disable_adapter(void)
{
	BT_DBG("+");
	int ret;

	if (_bt_adapter_get_status() == BT_DEACTIVATING) {
		BT_DBG("Disabling in progress");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	if (_bt_adapter_get_status() == BT_DEACTIVATED) {
		BT_DBG("Already disabled");
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	if (timer_id > 0) {
		g_source_remove(timer_id);
		timer_id = 0;
	}

	__bt_disconnect_all();
	ret = __bt_disable_cb();

	BT_DBG("-");
	return ret;
}

int _bt_recover_adapter(void)
{
	BT_DBG("+");
	DBusGProxy *proxy;

	if (_bt_adapter_get_status() == BT_DEACTIVATING) {
		BT_DBG("Disabling in progress");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	if (_bt_adapter_get_status() == BT_DEACTIVATED) {
		BT_DBG("Already disabled");
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	_bt_adapter_set_status(BT_DEACTIVATING);

	proxy = __bt_get_core_proxy();
	retv_if(!proxy, BLUETOOTH_ERROR_INTERNAL);

	if (dbus_g_proxy_call(proxy, "RecoverAdapter", NULL,
	                               G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_disconnect_all();

	BT_ERR("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_reset_adapter(void)
{
	DBusGProxy *proxy;

	BT_DBG("");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;

	if (dbus_g_proxy_call(proxy, "ResetAdapter", NULL,
	                               G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* Terminate myself */
	if (_bt_adapter_get_status() == BT_DEACTIVATED) {
		g_idle_add((GSourceFunc)_bt_terminate_service, NULL);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_check_adapter(int *status)
{

	char *adapter_path = NULL;

	BT_CHECK_PARAMETER(status, return);

	*status = BT_ADAPTER_DISABLED;

	adapter_path = _bt_get_adapter_path();


	if (adapter_path != NULL)
		*status = BT_ADAPTER_ENABLED;

	g_free(adapter_path);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_enable_adapter_le(void)
{
	BT_DBG("+");
	DBusGProxy *proxy;
	GError *err = NULL;
	bt_status_t status = _bt_adapter_get_status();
	bt_le_status_t le_status = _bt_adapter_get_le_status();

	if (le_status == BT_LE_ACTIVATING) {
		BT_ERR("Enabling in progress");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	if (le_status == BT_LE_ACTIVATED) {
		BT_ERR("Already enabled");
		return BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED;
	}

	if (status == BT_DEACTIVATING || le_status == BT_LE_DEACTIVATING) {
		BT_ERR("Disabling in progress");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	_bt_adapter_set_le_status(BT_LE_ACTIVATING);

	proxy = __bt_get_core_proxy();
	retv_if(!proxy, BLUETOOTH_ERROR_INTERNAL);

	if (dbus_g_proxy_call_with_timeout(proxy, "EnableAdapterLe",
				BT_ENABLE_TIMEOUT, &err,
				G_TYPE_INVALID,
				G_TYPE_INVALID) == FALSE) {

		_bt_adapter_set_le_status(BT_DEACTIVATED);

		if (err != NULL) {
			BT_ERR("Bt core call failed: [%s]", err->message);
			g_error_free(err);
		}

		/* Clean up the process */
		if (dbus_g_proxy_call(proxy, "DisableAdapterLe", NULL,
					G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
			BT_ERR("Bt core call failed");
		}

		/* Display notification */
		notification_status_message_post(BT_STR_NOT_SUPPORT);

		/* Terminate myself */
		if (_bt_adapter_get_status() == BT_DEACTIVATED)
			g_idle_add((GSourceFunc)_bt_terminate_service, NULL);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	_bt_adapter_start_le_enable_timer();

	if (status == BT_ACTIVATED) {
		_bt_adapter_set_le_status(BT_LE_ACTIVATED);
		__bt_set_le_enabled();
	}
	BT_DBG("le status : %d", _bt_adapter_get_le_status());
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_disable_adapter_le(void)
{
	BT_DBG("+");
	DBusGProxy *proxy;
	bt_le_status_t bt_le_state;

	bt_le_state = _bt_adapter_get_le_status();
	if (bt_le_state == BT_LE_DEACTIVATING) {
		BT_DBG("Disabling in progress");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	if (bt_le_state == BT_LE_DEACTIVATED) {
		BT_DBG("Already disabled");
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	_bt_adapter_set_le_status(BT_LE_DEACTIVATING);

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;

	if (dbus_g_proxy_call(proxy, "DisableAdapterLe", NULL,
	                               G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		BT_ERR("Bt core call failed");
		_bt_adapter_set_le_status(BT_LE_ACTIVATED);
		return BLUETOOTH_ERROR_INTERNAL;
       }

	_bt_set_le_disabled(BLUETOOTH_ERROR_NONE);
	BT_DBG("le status : %d", _bt_adapter_get_le_status());
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_local_address(bluetooth_device_address_t *local_address)
{

	DBusGProxy *proxy;
	GError *err = NULL;
	char *address;
	GValue address_v = { 0 };

	BT_CHECK_PARAMETER(local_address, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Address",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &address_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	address = (char *)g_value_get_string(&address_v);

	if (address) {
		_bt_convert_addr_string_to_type(local_address->addr, address);
	} else {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_local_version(bluetooth_version_t *local_version)
{
	DBusGProxy *proxy;
	GHashTable *hash = NULL;
	char *ver = NULL;
	char *ptr = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(local_version, return);

	GError *err = NULL;
	GValue version_v = { 0 };

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Version",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &version_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	ver = (char *)g_value_get_string(&version_v);


	if (ver && (strlen(ver) > 0)) {
		/* Check the utf8 valitation & Fill the NULL in the invalid location*/
		if (!g_utf8_validate(ver, -1, (const char **)&ptr))
			*ptr = '\0';

		g_strlcpy(local_version->version, ver,
				BLUETOOTH_VERSION_LENGTH_MAX + 1);

	} else {
		ret = BLUETOOTH_ERROR_INTERNAL;
	}

	g_hash_table_destroy(hash);
	return ret;
}

int _bt_get_local_name(bluetooth_device_name_t *local_name)
{
	DBusGProxy *proxy;
	GHashTable *hash = NULL;
	char *name = NULL;
	char *ptr = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	GError *err = NULL;
	GValue name_v = { 0 };

	BT_CHECK_PARAMETER(local_name, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Alias",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &name_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	name = (char *)g_value_get_string(&name_v);

	if (name && (strlen(name) > 0)) {
		/* Check the utf8 valitation & Fill the NULL in the invalid location*/
		if (!g_utf8_validate(name, -1, (const char **)&ptr))
			*ptr = '\0';

		g_strlcpy(local_name->name, name,
				BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1);
	} else {
		ret = BLUETOOTH_ERROR_INTERNAL;
	}

	g_hash_table_destroy(hash);
	return ret;
}

int _bt_set_local_name(char *local_name)
{
	GValue name = { 0 };
	DBusGProxy *proxy;
	GError *error = NULL;
	char *ptr = NULL;

	BT_CHECK_PARAMETER(local_name, return);

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!g_utf8_validate(local_name, -1, (const char **)&ptr))
		*ptr = '\0';

	g_value_init(&name, G_TYPE_STRING);
	g_value_set_string(&name, local_name);

	dbus_g_proxy_call(proxy, "Set", &error,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Alias",
			G_TYPE_VALUE, &name,
			G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&name);

	if (error) {
		BT_ERR("SetProperty Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_is_service_used(char *service_uuid, gboolean *used)
{
	char **uuids;
	int i;
	DBusGProxy *proxy;
	GError *err = NULL;
	GValue uuids_v = { 0 };
	int ret = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");
	BT_CHECK_PARAMETER(service_uuid, return);
	BT_CHECK_PARAMETER(used, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "UUIDs",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &uuids_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	uuids = g_value_get_boxed(&uuids_v);

	if (uuids == NULL) {
		/* Normal case */
		*used = FALSE;
		goto done;
	}

	for (i = 0; uuids[i] != NULL; i++) {
		if (strcasecmp(uuids[i], service_uuid) == 0) {
			*used = TRUE;
			goto done;
		}
	}

	*used = FALSE;
done:
	BT_DBG("Service Used? %d", *used);
	return ret;
}


static gboolean __bt_get_discoverable_property(void)
{
	DBusGProxy *proxy;
	GValue discoverable_v = { 0 };
	GError *err = NULL;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, FALSE);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Discoverable",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &discoverable_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return FALSE;
	}

	return g_value_get_boolean(&discoverable_v);
}

int _bt_get_discoverable_mode(int *mode)
{
	gboolean discoverable;
	unsigned int timeout;

	BT_CHECK_PARAMETER(mode, return);

	discoverable = __bt_get_discoverable_property();
	timeout = _bt_get_discoverable_timeout_property();

	if (discoverable == TRUE) {
		if (timeout == 0)
			*mode = BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE;
		else
			*mode = BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE;
	} else {
		*mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;
	}
	return BLUETOOTH_ERROR_NONE;
}


int _bt_set_discoverable_mode(int discoverable_mode, int timeout)
{
	int ret = BLUETOOTH_ERROR_NONE;
	gboolean inq_scan;
	gboolean pg_scan;
	GError *error = NULL;
	GValue connectable = { 0 };
	GValue discoverable = { 0 };
	GValue val_timeout = { 0 };
	DBusGProxy *proxy;

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_value_init(&connectable, G_TYPE_BOOLEAN);
	g_value_init(&discoverable, G_TYPE_BOOLEAN);
	g_value_init(&val_timeout, G_TYPE_UINT);

	switch (discoverable_mode) {
	case BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE:
		pg_scan = TRUE;
		inq_scan = FALSE;
		timeout = 0;
		break;
	case BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE:
		pg_scan = TRUE;
		inq_scan = TRUE;
		timeout = 0;
		break;
	case BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE:
		inq_scan = TRUE;
		pg_scan = TRUE;
		break;
	default:
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	g_value_set_boolean(&connectable, pg_scan);
	g_value_set_boolean(&discoverable, inq_scan);
	g_value_set_uint(&val_timeout, timeout);

	dbus_g_proxy_call(proxy, "Set", &error,
				G_TYPE_STRING, BT_ADAPTER_INTERFACE,
				G_TYPE_STRING, "Powered",
				G_TYPE_VALUE, &connectable,
				G_TYPE_INVALID, G_TYPE_INVALID);

	if (error != NULL) {
		BT_ERR("Powered set err:[%s]", error->message);
		g_error_free(error);
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto done;
	}

	dbus_g_proxy_call(proxy, "Set", &error,
				G_TYPE_STRING, BT_ADAPTER_INTERFACE,
				G_TYPE_STRING, "Discoverable",
				G_TYPE_VALUE, &discoverable,
				G_TYPE_INVALID, G_TYPE_INVALID);


	if (error != NULL) {
		BT_ERR("Discoverable set err:[%s]", error->message);
		g_error_free(error);
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto done;
	}

	dbus_g_proxy_call(proxy, "Set", &error,
				G_TYPE_STRING, BT_ADAPTER_INTERFACE,
				G_TYPE_STRING, "DiscoverableTimeout",
				G_TYPE_VALUE, &val_timeout,
				G_TYPE_INVALID, G_TYPE_INVALID);

	if (error != NULL) {
		BT_ERR("Timeout set err:[%s]", error->message);
		g_error_free(error);
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto done;
	}

	if (discoverable_mode == BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE)
		timeout = -1;

	ret = __bt_set_visible_time(timeout);

done:
	g_value_unset(&val_timeout);
	g_value_unset(&connectable);
	g_value_unset(&discoverable);

	return ret;
}

int _bt_start_discovery(void)
{
	DBusGProxy *proxy;
	GError *err = NULL;

	if (_bt_is_discovering() == TRUE) {
		BT_ERR("BT is already in discovering");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "StartDiscovery", &err,
			       G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("StartDiscovery failed: [%s]\n", err->message);
			g_error_free(err);
		}
		BT_ERR("Discover start failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_discovering = TRUE;
	cancel_by_user = FALSE;
	/* discovery status will be change in event */

	return BLUETOOTH_ERROR_NONE;
}

int _bt_start_custom_discovery(bt_discovery_role_type_t role)
{
	DBusGProxy *proxy;

	const gchar *disc_type;

	if (_bt_is_discovering() == TRUE) {
		BT_ERR("BT is already in discovering");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (role == DISCOVERY_ROLE_BREDR)
		disc_type = "BREDR";
	else if (role == DISCOVERY_ROLE_LE)
		disc_type = "LE";
	else if (role == DISCOVERY_ROLE_LE_BREDR)
		disc_type = "LE_BREDR";
	else
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (!dbus_g_proxy_call(proxy, "StartCustomDiscovery", NULL,
			 G_TYPE_STRING, disc_type,
			       G_TYPE_INVALID, G_TYPE_INVALID)) {
		BT_ERR("StartCustomDiscovery failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_discovering = TRUE;
	cancel_by_user = FALSE;
	/* discovery status will be change in event */

	return BLUETOOTH_ERROR_NONE;
}

int _bt_cancel_discovery(void)
{
	DBusGProxy *proxy;
	GError *err = NULL;

	if (_bt_is_discovering() == FALSE) {
		BT_ERR("BT is not in discovering");
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "StopDiscovery", &err,
			       G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("StopDiscovery failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cancel_by_user = TRUE;
	/* discovery status will be change in event */

	return BLUETOOTH_ERROR_NONE;
}

int _bt_start_le_discovery(void)
{
   DBusGProxy *proxy;

   if (_bt_is_le_discovering() == TRUE) {
       BT_ERR("BT is already in LE discovering");
       return BLUETOOTH_ERROR_IN_PROGRESS;
   }

   proxy = _bt_get_adapter_proxy();
   retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

   if (!dbus_g_proxy_call(proxy, "StartLEDiscovery", NULL,
                  G_TYPE_INVALID, G_TYPE_INVALID)) {
       BT_ERR("LE Discover start failed");
       return BLUETOOTH_ERROR_INTERNAL;
   }

   is_le_discovering = TRUE;

   return BLUETOOTH_ERROR_NONE;
}

int _bt_stop_le_discovery(void)
{
   DBusGProxy *proxy;

   if (_bt_is_le_discovering() == FALSE) {
       BT_ERR("BT is not in LE discovering");
       return BLUETOOTH_ERROR_NOT_IN_OPERATION;
   }

   proxy = _bt_get_adapter_proxy();
   retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

   if (!dbus_g_proxy_call(proxy, "StopLEDiscovery", NULL,
                  G_TYPE_INVALID, G_TYPE_INVALID)) {
       BT_ERR("LE Discover stop failed");
       return BLUETOOTH_ERROR_INTERNAL;
   }

   return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_is_discovering(void)
{
	return is_discovering;
}

gboolean _bt_is_le_discovering(void)
{
   return is_le_discovering;
}

gboolean _bt_is_connectable(void)
{
	DBusGProxy *proxy;
	GValue connectable_v = { 0 };
	GError *err = NULL;
	gboolean is_connectable = FALSE;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "Connectable",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &connectable_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_connectable = g_value_get_boolean(&connectable_v);
	BT_INFO("Get connectable [%d]", is_connectable);

	return is_connectable;
}

int _bt_set_connectable(gboolean is_connectable)
{
	DBusGProxy *proxy;
	GValue connectable = { 0 };
	GError *error = NULL;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to set connectable in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_value_init(&connectable, G_TYPE_BOOLEAN);
	g_value_set_boolean(&connectable, is_connectable);

	dbus_g_proxy_call(proxy, "Set", &error,
				G_TYPE_STRING, BT_ADAPTER_INTERFACE,
				G_TYPE_STRING, "Connectable",
				G_TYPE_VALUE, &connectable,
				G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&connectable);
	if (error != NULL) {
		BT_ERR("Connectable set err:[%s]", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("Set connectable [%d]", is_connectable);
	return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_is_advertising(void)
{
	return is_advertising;
}

gboolean _bt_get_advertising_params(bluetooth_advertising_params_t *params)
{
	if (params == NULL)
		return FALSE;

	memcpy(params, &adv_params, sizeof(bluetooth_advertising_params_t));

	return TRUE;
}

gboolean _bt_get_discovering_property(bt_discovery_role_type_t discovery_type)
{

	DBusGProxy *proxy;
	GValue discovering_v = { 0 };
	GError *err = NULL;
	char *discovering_type =  NULL;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (discovery_type == DISCOVERY_ROLE_BREDR)
		discovering_type = "Discovering";
	else if (discovery_type == DISCOVERY_ROLE_LE)
		discovering_type = "LEDiscovering";

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, discovering_type,
			G_TYPE_INVALID,
			G_TYPE_VALUE, &discovering_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return g_value_get_boolean(&discovering_v);

}

unsigned int _bt_get_discoverable_timeout_property(void)
{
	DBusGProxy *proxy;
	GValue timeout_v = { 0 };
	GError *err = NULL;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, 0);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_ADAPTER_INTERFACE,
			G_TYPE_STRING, "DiscoverableTimeout",
			G_TYPE_INVALID,
			G_TYPE_VALUE, &timeout_v,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		return 0;
	}

	return g_value_get_uint(&timeout_v);
}

static bluetooth_device_info_t *__bt_parse_device_info(DBusMessageIter *item_iter)
{
	DBusMessageIter value_iter;
	bluetooth_device_info_t *dev_info;

	dbus_message_iter_recurse(item_iter, &value_iter);

	if (dbus_message_iter_get_arg_type(&value_iter) != DBUS_TYPE_DICT_ENTRY) {
		BT_DBG("No entry");
		return NULL;
	}

	dev_info = g_malloc0(sizeof(bluetooth_device_info_t));

	while (dbus_message_iter_get_arg_type(&value_iter) ==
						DBUS_TYPE_DICT_ENTRY) {
		char *value = NULL;
		char *key;
		DBusMessageIter dict_entry;
		DBusMessageIter iter_dict_val;

		dbus_message_iter_recurse(&value_iter, &dict_entry);

		dbus_message_iter_get_basic(&dict_entry, &key);

		if (key == NULL) {
			dbus_message_iter_next(&value_iter);
			continue;
		}

		if (!dbus_message_iter_next(&dict_entry)) {
			dbus_message_iter_next(&value_iter);
			continue;
		}
		dbus_message_iter_recurse(&dict_entry, &iter_dict_val);

		if (strcasecmp(key, "Address") == 0) {
			const char *address = NULL;
			dbus_message_iter_get_basic(&iter_dict_val, &address);
			_bt_convert_addr_string_to_type(dev_info->device_address.addr,
							address);

		} else if (strcasecmp(key, "Class") == 0) {
			unsigned int cod;
			dbus_message_iter_get_basic(&iter_dict_val, &cod);
			_bt_divide_device_class(&dev_info->device_class, cod);
		} else if (strcasecmp(key, "Name") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val, &value);

			/* If there is no Alias */
			if (strlen(dev_info->device_name.name) == 0) {
				g_strlcpy(dev_info->device_name.name, value,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
			}
		} else if (strcasecmp(key, "Alias") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val, &value);

			/* Overwrite the name */
			if (value) {
				memset(dev_info->device_name.name, 0x00,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
				g_strlcpy(dev_info->device_name.name, value,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
			}
		} else if (strcasecmp(key, "Connected") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->connected);
		} else if (strcasecmp(key, "Paired") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->paired);
		} else if (strcasecmp(key, "Trusted") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->trust);
		} else if (strcasecmp(key, "RSSI") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->rssi);
		} else if (strcasecmp(key, "UUIDs") == 0) {
			DBusMessageIter uuid_iter;
			char **parts;
			int i = 0;

			dbus_message_iter_recurse(&iter_dict_val, &uuid_iter);

			while (dbus_message_iter_get_arg_type(&uuid_iter) != DBUS_TYPE_INVALID) {
				dbus_message_iter_get_basic(&uuid_iter,
							&value);

				g_strlcpy(dev_info->uuids[i], value,
						BLUETOOTH_UUID_STRING_MAX);

				parts = g_strsplit(value, "-", -1);

				if (parts == NULL || parts[0] == NULL)
					break;

				dev_info->service_list_array[i] = g_ascii_strtoull(parts[0], NULL, 16);
				g_strfreev(parts);

				i++;
				if (!dbus_message_iter_next(&uuid_iter)) {
					break;
				}
			}

			dev_info->service_index = i;
		} else if (strcasecmp(key, "ManufacturerDataLen") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
								&dev_info->manufacturer_data.data_len);
		} else if (strcasecmp(key, "ManufacturerData") == 0) {
			DBusMessageIter manufacturer_iter;
			int i = 0;
			char byte = 0;

			dbus_message_iter_recurse(&iter_dict_val, &manufacturer_iter);

			while (dbus_message_iter_get_arg_type(&manufacturer_iter) == DBUS_TYPE_BYTE) {
				dbus_message_iter_get_basic(&manufacturer_iter, &byte);
				dev_info->manufacturer_data.data[i] = byte;
				i++;
				dbus_message_iter_next(&manufacturer_iter);
			}
		}

		dbus_message_iter_next(&value_iter);
	}

	return dev_info;
}

static void __bt_extract_device_info(DBusMessageIter *msg_iter,
							GArray **dev_list)
{
	bluetooth_device_info_t *dev_info = NULL;
	char *object_path = NULL;
	DBusMessageIter value_iter;

	/* Parse the signature:  oa{sa{sv}}} */
	ret_if(dbus_message_iter_get_arg_type(msg_iter) !=
				DBUS_TYPE_OBJECT_PATH);

	dbus_message_iter_get_basic(msg_iter, &object_path);
	ret_if(object_path == NULL);

	/* object array (oa) */
	ret_if(dbus_message_iter_next(msg_iter) == FALSE);
	ret_if(dbus_message_iter_get_arg_type(msg_iter) != DBUS_TYPE_ARRAY);

	dbus_message_iter_recurse(msg_iter, &value_iter);

	/* string array (sa) */
	while (dbus_message_iter_get_arg_type(&value_iter) ==
					DBUS_TYPE_DICT_ENTRY) {
		char *interface_name = NULL;
		DBusMessageIter interface_iter;

		dbus_message_iter_recurse(&value_iter, &interface_iter);

		ret_if(dbus_message_iter_get_arg_type(&interface_iter) !=
							DBUS_TYPE_STRING);

		dbus_message_iter_get_basic(&interface_iter, &interface_name);

		ret_if(dbus_message_iter_next(&interface_iter) == FALSE);

		ret_if(dbus_message_iter_get_arg_type(&interface_iter) !=
							DBUS_TYPE_ARRAY);

		if (g_strcmp0(interface_name, "org.bluez.Device1") == 0) {
			BT_DBG("Found a device: %s", object_path);
			dev_info = __bt_parse_device_info(&interface_iter);

			if (dev_info) {
				if (dev_info->paired == FALSE)
					goto not_paired;

				g_array_append_vals(*dev_list, dev_info,
						sizeof(bluetooth_device_info_t));

				g_free(dev_info);
			}

			return;
		}

		dbus_message_iter_next(&value_iter);
	}

	BT_DBG("There is no device interface");

not_paired:
	BT_DBG("Not paired");
	g_free(dev_info);
}

int _bt_get_bonded_devices(GArray **dev_list)
{
	BT_DBG("+");
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter reply_iter;
	DBusMessageIter value_iter;
	DBusError err;
	DBusConnection *conn;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, BT_MANAGER_PATH,
						BT_MANAGER_INTERFACE,
						"GetManagedObjects");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* Synchronous call */
	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(
					conn, msg,
					-1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Can't get managed objects");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (dbus_message_iter_init(reply, &reply_iter) == FALSE) {
		BT_ERR("Fail to iterate the reply");
		dbus_message_unref(reply);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_recurse(&reply_iter, &value_iter);

	/* signature of GetManagedObjects:  a{oa{sa{sv}}} */
	while (dbus_message_iter_get_arg_type(&value_iter) ==
						DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter msg_iter;

		dbus_message_iter_recurse(&value_iter, &msg_iter);

		__bt_extract_device_info(&msg_iter, dev_list);

		dbus_message_iter_next(&value_iter);
	}
	dbus_message_unref(reply);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_bonded_device_info(bluetooth_device_address_t *device_address,
				bluetooth_device_info_t *dev_info)
{
	char *object_path = NULL;
	DBusGProxy *adapter_proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(dev_info, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	object_path = _bt_get_device_object_path(address);

	retv_if(object_path == NULL, BLUETOOTH_ERROR_NOT_FOUND);

	if (__bt_get_bonded_device_info(object_path,
				dev_info) != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Can't get the paired device path \n");
		g_free(object_path);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_free(object_path);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_timeout_value(int *timeout)
{
	time_t current_time;
	int time_diff;

	/* Take current time */
	time(&current_time);
	time_diff = difftime(current_time, visible_timer.start_time);

	BT_DBG("Time diff = %d\n", time_diff);

	*timeout = visible_timer.timeout - time_diff;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_advertising(gboolean enable)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to start advertising in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	if (is_advertising == TRUE && enable == TRUE)
		return BLUETOOTH_ERROR_IN_PROGRESS;

	if (is_advertising == FALSE && enable == FALSE)
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(proxy, "SetAdvertising", &error,
			G_TYPE_BOOLEAN, enable,
			G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("SetAdvertising Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_advertising = enable;

	BT_INFO("Set advertising [%d]", enable);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_custom_advertising(gboolean enable, bluetooth_advertising_params_t *params)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	guint32 min = 0;
	guint32 max = 0;

	BT_CHECK_PARAMETER(params, return);

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to start advertising in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	if (is_advertising == TRUE && enable == TRUE)
		return BLUETOOTH_ERROR_IN_PROGRESS;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (params->interval_min > params->interval_max ||
			params->interval_min < BT_ADV_INTERVAL_MIN ||
			params->interval_max > BT_ADV_INTERVAL_MAX)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (params->filter_policy > BLUETOOTH_ALLOW_SCAN_CONN_WHITE_LIST)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (params->type  == BLUETOOTH_ADV_CONNECTABLE_DIRECT_HIGH ||
			params->type == BLUETOOTH_ADV_CONNECTABLE_DIRECT_LOW ||
			params->type == BLUETOOTH_ADV_NON_CONNECTABLE)
		return BLUETOOTH_ERROR_NOT_SUPPORT;

	min = params->interval_min / BT_ADV_INTERVAL_SPLIT;
	max = params->interval_max / BT_ADV_INTERVAL_SPLIT;

	dbus_g_proxy_call(proxy, "SetAdvertisingParameters", &error,
			G_TYPE_UINT, min,
			G_TYPE_UINT, max,
			G_TYPE_UINT, params->filter_policy,
			G_TYPE_UINT, params->type,
			G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("SetAdvertisingParameters Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	adv_params.interval_min = params->interval_min;
	adv_params.interval_max = params->interval_max;
	adv_params.filter_policy = params->filter_policy;
	adv_params.type= params->type;

	dbus_g_proxy_call(proxy, "SetAdvertising", &error,
			G_TYPE_BOOLEAN, enable,
			G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("SetAdvertising Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_advertising = enable;

	BT_INFO_C("Set advertising [%d]", enable);
	return BLUETOOTH_ERROR_NONE;
}

static int __bt_get_ad_data_by_type(char *in_data, int in_len,
		char in_type, char **data, int *data_len)
{
	if (in_data == NULL || data == NULL || data_len == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	if (in_len < 0)
		return BLUETOOTH_ERROR_INTERNAL;

	int i;
	int len = 0;
	int type = 0;

	for (i = 0; i < in_len; i++) {
		len = in_data[i];
		if (len <= 0 || i + 1 >= in_len) {
			BT_ERR("Invalid advertising data");
			return BLUETOOTH_ERROR_INTERNAL;
		}

		type = in_data[i + 1];
		if (type == in_type) {
			i = i + 2;
			len--;
			break;
		}

		i += len;
		len = 0;
	}

	if (i + len > in_len) {
		BT_ERR("Invalid advertising data");
		return BLUETOOTH_ERROR_INTERNAL;
	} else if (len == 0) {
		BT_DBG("AD Type 0x%02x data is not set", in_type);
		*data = NULL;
		*data_len = 0;
		return BLUETOOTH_ERROR_NONE;
	}

	*data = g_memdup(&in_data[i], len);
	if (*data == NULL)
		return BLUETOOTH_ERROR_OUT_OF_MEMORY;
	*data_len = len;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_advertising_data(bluetooth_advertising_data_t *adv, int *length)
{
	BT_CHECK_PARAMETER(adv, return);
	BT_CHECK_PARAMETER(length, return);

	memcpy(adv, &adv_data, sizeof(adv_data));
	*length = adv_data_len;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_advertising_data(bluetooth_advertising_data_t *adv, int length)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	GArray *arr;
	int i;
	char *old_mdata = NULL;
	char *new_mdata = NULL;
	int old_len = 0;
	int new_len = 0;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to set advertising data in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	BT_CHECK_PARAMETER(adv, return);

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	arr = g_array_new(TRUE, TRUE, sizeof(guint8));

	for (i = 0; i < length; i++)
		g_array_append_vals(arr, &(adv->data[i]), sizeof(guint8));

	dbus_g_proxy_call(proxy, "SetAdvertisingData", &error,
			DBUS_TYPE_G_UCHAR_ARRAY, arr,
			G_TYPE_INVALID, G_TYPE_INVALID);

	g_array_free(arr, TRUE);

	if (error) {
		BT_ERR("SetAdvertisingData Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_get_ad_data_by_type((char *)adv_data.data, adv_data_len, 0xff,
			&old_mdata, &old_len);
	__bt_get_ad_data_by_type((char *)adv->data, length, 0xff,
			&new_mdata, &new_len);
	if (old_len != new_len ||
			(old_mdata && new_mdata &&
			 memcmp(old_mdata, new_mdata, new_len))) {
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_ADVERTISING_MANUFACTURER_DATA_CHANGED,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
				&new_mdata, new_len,
				DBUS_TYPE_INVALID);
	}
	g_free(new_mdata);
	g_free(old_mdata);

	memset(&adv_data, 0x00, sizeof(bluetooth_advertising_data_t));
	memcpy(&adv_data, adv, length);
	adv_data_len = length;

	BT_INFO("Set advertising data");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_scan_parameters(bluetooth_le_scan_params_t *params)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	guint32 itv = 0;
	guint32 win = 0;

	BT_CHECK_PARAMETER(params, return);

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (params->interval < BT_LE_SCAN_INTERVAL_MIN || params->interval > BT_LE_SCAN_INTERVAL_MAX)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (params->window < BT_LE_SCAN_WINDOW_MIN || params->window > BT_LE_SCAN_WINDOW_MAX)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (params->window > params->interval)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	itv = params->interval / BT_ADV_INTERVAL_SPLIT;
	win = params->window / BT_ADV_INTERVAL_SPLIT;

	dbus_g_proxy_call(proxy, "SetScanParameters", &error,
			G_TYPE_UINT, params->type,
			G_TYPE_UINT, itv,
			G_TYPE_UINT, win,
			G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("SetScanParameters Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	_bt_set_le_discovery_type(params->type);

	BT_INFO("Set scan parameters");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_scan_response_data(bluetooth_scan_resp_data_t *response, int *length)
{
	BT_CHECK_PARAMETER(response, return);
	BT_CHECK_PARAMETER(length, return);

	memcpy(response, &resp_data, sizeof(resp_data));
	*length = resp_data_len;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_scan_response_data(bluetooth_scan_resp_data_t *response, int length)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	GArray *arr;
	int i;
	char *old_mdata = NULL;
	char *new_mdata = NULL;
	int old_len = 0;
	int new_len = 0;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to set scan response list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	BT_CHECK_PARAMETER(response, return);

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	arr = g_array_new(TRUE, TRUE, sizeof(guint8));

	for (i = 0; i < length; i++)
		g_array_append_vals(arr, &(response->data[i]), sizeof(guint8));

	dbus_g_proxy_call(proxy, "SetScanRespData", &error,
			DBUS_TYPE_G_UCHAR_ARRAY, arr,
			G_TYPE_INVALID, G_TYPE_INVALID);

	g_array_free(arr, TRUE);

	if (error) {
		BT_ERR("SetScanRespData Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* Compare with previous scan resp data */
	__bt_get_ad_data_by_type((char *)resp_data.data, resp_data_len, 0xff,
			&old_mdata, &old_len);
	__bt_get_ad_data_by_type((char *)response->data, length, 0xff,
			&new_mdata, &new_len);
	if (old_len != new_len ||
			(old_mdata && new_mdata &&
			 memcmp(old_mdata, new_mdata, new_len))) {
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
				&new_mdata, new_len,
				DBUS_TYPE_INVALID);
	}
	g_free(new_mdata);
	g_free(old_mdata);

	memset(&resp_data, 0x00, sizeof(bluetooth_scan_resp_data_t));
	memcpy(&resp_data, response, length);
	resp_data_len = length;

	BT_INFO("Set scan response data");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_add_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type)
{
	DBusGProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GError *error = NULL;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to add white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (is_advertising)
		return BLUETOOTH_ERROR_DEVICE_BUSY;

	BT_CHECK_PARAMETER(device_address, return);

	if (address_type != BLUETOOTH_DEVICE_PUBLIC_ADDRESS &&
		address_type != BLUETOOTH_DEVICE_RANDOM_ADDRESS)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_convert_addr_type_to_string(address, device_address->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(proxy, "AddDeviceWhiteList", &error,
		  G_TYPE_STRING, address,
		  G_TYPE_UINT, address_type,
		  G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("AddDeviceWhiteList Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("Add white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_remove_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type)
{
	DBusGProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GError *error = NULL;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to remove white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (is_advertising)
		return BLUETOOTH_ERROR_DEVICE_BUSY;

	BT_CHECK_PARAMETER(device_address, return);

	if (address_type != BLUETOOTH_DEVICE_PUBLIC_ADDRESS &&
		address_type != BLUETOOTH_DEVICE_RANDOM_ADDRESS)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_convert_addr_type_to_string(address, device_address->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(proxy, "RemoveDeviceWhiteList", &error,
		  G_TYPE_STRING, address,
		   G_TYPE_UINT, address_type,
		  G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("RemoveDeviceWhiteList Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("Remove white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_clear_white_list(void)
{
	DBusGProxy *proxy;
	GError *error = NULL;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to clear white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (is_advertising)
		return BLUETOOTH_ERROR_DEVICE_BUSY;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(proxy, "ClearDeviceWhiteList", &error,
		  G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("ClearDeviceWhiteList Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("Clear white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_le_privacy(gboolean set_privacy)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to set le privacy in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(proxy, "SetLePrivacy", &error,
				   G_TYPE_BOOLEAN, set_privacy,
				   G_TYPE_INVALID, G_TYPE_INVALID);

	if (error) {
		BT_ERR("SetLePrivacy Failed :[%s]", error->message);
		if (g_strrstr(error->message, BT_SERVICE_ERR_MSG_NOT_SUPPORTED))
			ret = BLUETOOTH_ERROR_NOT_SUPPORT;
		else
			ret = BLUETOOTH_ERROR_INTERNAL;
		g_error_free(error);
		return ret;
	}

	BT_INFO("SetLePrivacy as %d", set_privacy);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_manufacturer_data(bluetooth_manufacturer_data_t *m_data)
{
	DBusGProxy *proxy;
	GError *error = NULL;
	GArray *arr;
	int i;

	BT_CHECK_PARAMETER(m_data, return);

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	arr = g_array_new(TRUE, TRUE, sizeof(guint8));

	for (i = 0; i < (m_data->data_len) + 2; i++)
		g_array_append_vals(arr, &(m_data->data[i]), sizeof(guint8));

	dbus_g_proxy_call(proxy, "SetManufacturerData", &error,
			DBUS_TYPE_G_UCHAR_ARRAY, arr,
			G_TYPE_INVALID, G_TYPE_INVALID);

	g_array_free(arr, TRUE);

	if (error) {
		BT_ERR("SetManufacturerData Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_MANUFACTURER_DATA_CHANGED,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
			&m_data, m_data->data_len,
			DBUS_TYPE_INVALID);

	BT_INFO("Set manufacturer data");

	return BLUETOOTH_ERROR_NONE;
}
