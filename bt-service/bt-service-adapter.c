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
#include <gio/gio.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <vconf.h>
#include <vconf-internal-keys.h>
#include <syspopup_caller.h>
#include <aul.h>
#include <eventsystem.h>
#include <bundle_internal.h>

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
#include "bt-service-device.h"

typedef struct {
	guint event_id;
	int timeout;
	time_t start_time;
	gboolean alarm_init;
	int alarm_id;
} bt_adapter_timer_t;

bt_adapter_timer_t visible_timer = {0, };

static gboolean is_discovering;
static gboolean cancel_by_user;
static bt_status_t adapter_status = BT_DEACTIVATED;
static bt_le_status_t adapter_le_status = BT_LE_DEACTIVATED;
static void *adapter_agent = NULL;
static GDBusProxy *core_proxy = NULL;
static guint timer_id = 0;
static guint le_timer_id = 0;

static int status_reg_id;

#define BT_CORE_NAME "org.projectx.bt_core"
#define BT_CORE_PATH "/org/projectx/bt_core"
#define BT_CORE_INTERFACE "org.projectx.btcore"

#define BT_DISABLE_TIME 500 /* 500 ms */

GDBusProxy *_bt_init_core_proxy(void)
{
	GDBusProxy *proxy;
	GDBusConnection *conn;

	conn = _bt_get_system_gconn();
	if (!conn)
		return NULL;

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
					NULL,
					BT_CORE_NAME,
					BT_CORE_PATH,
					BT_CORE_INTERFACE,
					NULL, NULL);

	if (!proxy)
		return NULL;

	core_proxy = proxy;

	return proxy;
}

static GDBusProxy *__bt_get_core_proxy(void)
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
			g_variant_new("(in)", result, time_diff));

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
				g_variant_new("(in)", result, timeout));
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
		BT_ERR("Set vconf failed");
#endif

	if (timeout <= 0)
		return BLUETOOTH_ERROR_NONE;

	if (!visible_timer.alarm_init) {
		/* Set Alarm timer to switch off BT */
		result = alarmmgr_init("bt-service");
		if (result != 0)
			return BLUETOOTH_ERROR_INTERNAL;

		visible_timer.alarm_init = TRUE;
	}

	result = alarmmgr_set_cb(__bt_visibility_alarm_cb, NULL);
	if (result != 0)
		return BLUETOOTH_ERROR_INTERNAL;

	/* Take start time */
	time(&(visible_timer.start_time));
	visible_timer.event_id = g_timeout_add_seconds(1,
				__bt_timeout_handler, NULL);

	__bt_visibility_alarm_create();

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_get_service_list(GVariant *value, bluetooth_device_info_t *dev)
{
	int i = 0;
	char **parts;
	GVariantIter *iter;
	gchar *uuid = NULL;

	ret_if(value == NULL);
	ret_if(dev == NULL);

	dev->service_index = 0;

	g_variant_get(value, "as", &iter);
	while (g_variant_iter_loop(iter, "s", &uuid)) {
		g_strlcpy(dev->uuids[i], uuid, BLUETOOTH_UUID_STRING_MAX);
		parts = g_strsplit(uuid, "-", -1);

		if (parts == NULL || parts[0] == NULL) {
			g_free(uuid);
			break;
		}

		dev->service_list_array[i] = g_ascii_strtoull(parts[0], NULL, 16);
		g_strfreev(parts);

		dev->service_index++;
		i++;
	}
	g_variant_iter_free(iter);
}

static int __bt_get_bonded_device_info(gchar *device_path,
		bluetooth_device_info_t *dev_info)
{
	GError *error = NULL;
	GDBusProxy *device_proxy;
	const gchar *address = NULL;
	const gchar *name = NULL;
	unsigned int cod = 0;
	gint rssi = 0;
	gboolean trust = FALSE;
	gboolean paired = FALSE;
	guchar connected = 0;
	GByteArray *manufacturer_data = NULL;
	int ret;
	GDBusConnection *conn;
	GVariant *result;
	GVariantIter *property_iter;
	const gchar *key;
	GVariant *value;
	guint8 char_value;
	GVariantIter *char_value_iter;

	BT_CHECK_PARAMETER(device_path, return);
	BT_CHECK_PARAMETER(dev_info, return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
					NULL,
					BT_BLUEZ_NAME,
					device_path,
					BT_PROPERTIES_INTERFACE,
					NULL, NULL);

	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(device_proxy,
				"GetAll",
				g_variant_new("(s)", BT_DEVICE_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		BT_ERR("Error occured in Proxy call");
		if (error != NULL) {
			BT_ERR("Error occured in Proxy call (Error: %s)", error->message);
			g_clear_error(&error);
		}
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	g_variant_get(result, "(a{sv})", &property_iter);

	while (g_variant_iter_loop(property_iter, "{sv}", &key, &value)) {
		if (!g_strcmp0(key,"Paired")) {
			paired = g_variant_get_boolean(value);
		} else if(!g_strcmp0(key, "Address")) {
			address = g_variant_get_string(value, NULL);
		} else if (!g_strcmp0(key, "Alias")) {
			name = g_variant_get_string(value, NULL);
		} else if (!g_strcmp0(key, "Name")) {
			if (!name)
				name = g_variant_get_string(value, NULL);
		} else if (!g_strcmp0(key, "Class")) {
			cod = g_variant_get_uint32(value);
		} else if (!g_strcmp0(key, "Connected")) {
			connected = g_variant_get_byte(value);
		} else if (!g_strcmp0(key, "Trusted")) {
			trust = g_variant_get_boolean(value);
		} else if (!g_strcmp0(key, "RSSI")) {
			rssi = g_variant_get_int16(value);
		} else if (!g_strcmp0(key, "UUIDs")) {
			__bt_get_service_list(value, dev_info);
		} else if (!g_strcmp0(key, "ManufacturerDataLen")) {
			dev_info->manufacturer_data.data_len = g_variant_get_uint16(value);
		} else if (!g_strcmp0(key, "ManufacturerData")) {
			manufacturer_data = g_byte_array_new();
			g_variant_get(value, "ay", &char_value_iter);
			while(g_variant_iter_loop(char_value_iter, "y",  &char_value)) {
				g_byte_array_append(manufacturer_data, &char_value, 1);
			}
			if (manufacturer_data) {
				if (manufacturer_data->len > 0) {
					memcpy(dev_info->manufacturer_data.data, manufacturer_data->data,
						manufacturer_data->len);
				}
			}
		}
	}

	BT_DBG("trust: %d, paired: %d", trust, paired);

	g_variant_unref(result);

	if ((paired == FALSE) && (trust == FALSE)) {
		return BLUETOOTH_ERROR_NOT_PAIRED;
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
	ret = BLUETOOTH_ERROR_NONE;

	return ret;
}

void _bt_set_discovery_status(gboolean mode)
{
	is_discovering = mode;
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

#ifndef TIZEN_WEARABLE
static void __bt_set_visible_mode(void)
{
	int timeout = 0;

	if (vconf_get_int(BT_FILE_VISIBLE_TIME, &timeout) != 0)
                BT_ERR("Fail to get the timeout value");

	if (timeout == -1) {
		if (_bt_set_discoverable_mode(
			BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE,
			timeout) != BLUETOOTH_ERROR_NONE) {
			if (vconf_set_int(BT_FILE_VISIBLE_TIME, 0) != 0)
				BT_ERR("Set vconf failed");
		}
	} else {
		if (_bt_set_discoverable_mode(
			BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE,
			timeout) != BLUETOOTH_ERROR_NONE) {
				BT_ERR("Set connectable mode failed");
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

	/*
		1 critical issue: Even if HCI command is success, Iscan Scan value is false.
                  * Possible cause: As soon as send set connectable mode, send set discoverable mode
                                            Before stable bluez, send set discoverable mode.
               Scan enable: Inquiry Scan + Page Scan (0x03)
	*/
#ifndef TIZEN_WEARABLE
	__bt_set_visible_mode();
#endif

	__bt_set_local_name();

	/* Update Bluetooth Status to notify other modules */
	if (vconf_set_int(VCONFKEY_BT_STATUS, VCONFKEY_BT_STATUS_ON) != 0)
		BT_ERR("Set vconf failed\n");

	if (vconf_set_int(VCONFKEY_BT_DEVICE, VCONFKEY_BT_DEVICE_NONE) != 0)
		BT_ERR("Set vconf failed\n");

	if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_STATE,
						EVT_VAL_BT_ON) != ES_R_OK)
		BT_ERR("Fail to set value");

	/* Send enabled event to API */
	_bt_send_event(BT_ADAPTER_EVENT, BLUETOOTH_EVENT_ENABLED,
				g_variant_new("(i)", result));

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
			if (vconf_set_int(VCONFKEY_BT_OFF_DUE_TO_TIMEOUT, 1) != 0 )
				BT_ERR("Set vconf failed");

		/* Update Bluetooth Status to notify other modules */
		if (vconf_set_int(VCONFKEY_BT_STATUS, VCONFKEY_BT_STATUS_OFF) != 0)
			BT_ERR("Set vconf failed");

		if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_STATE,
							EVT_VAL_BT_OFF) != ES_R_OK)
			BT_ERR("Fail to set value");
	}

	if (vconf_set_int(VCONFKEY_BT_DEVICE, VCONFKEY_BT_DEVICE_NONE) != 0)
		BT_ERR("Set vconf failed\n");

	_bt_adapter_set_status(BT_DEACTIVATED);

	if (_bt_adapter_get_le_status() != BT_LE_DEACTIVATED) {
		/* Send disabled event */
		_bt_send_event(BT_ADAPTER_EVENT, BLUETOOTH_EVENT_DISABLED,
				g_variant_new("(i)", result));
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

	if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_LE_STATE,
						EVT_VAL_BT_LE_ON) != ES_R_OK)
		BT_ERR("Fail to set value");

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
				g_variant_new("(i)", result));

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

	if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_LE_STATE,
						EVT_VAL_BT_LE_OFF) != ES_R_OK)
		BT_ERR("Fail to set value");

	/* Send disabled event */
	_bt_send_event(BT_LE_ADAPTER_EVENT, BLUETOOTH_EVENT_LE_DISABLED,
			g_variant_new_int32(result));
}

void *_bt_get_adapter_agent(void)
{
	return adapter_agent;
}

int _bt_enable_core(void)
{
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

	proxy = __bt_get_core_proxy();
	retv_if(!proxy, BLUETOOTH_ERROR_INTERNAL);

	/* Clean up the process */
	result = g_dbus_proxy_call_sync(proxy,
				"EnableCore",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Bt core call failed(Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Bt core call failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

#if defined(TIZEN_BT_FLIGHTMODE_ENABLED) || !defined(TIZEN_WEARABLE)
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
#endif

void _bt_service_register_vconf_handler(void)
{
	BT_DBG("+");

#ifdef TIZEN_TELEPHONY_ENABLED
	if (vconf_notify_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb, NULL) < 0)
		BT_ERR("Unable to register key handler");
#else
	BT_DBG("Telephony is disabled");
#endif

#ifndef TIZEN_WEARABLE
	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb, NULL) < 0)
		BT_ERR("Unable to register key handler");
#endif
}

void _bt_service_unregister_vconf_handler(void)
{
	BT_DBG("+");

#ifdef TIZEN_TELEPHONY_ENABLED
	vconf_ignore_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb);
#endif

#ifndef TIZEN_WEARABLE
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_service_flight_ps_mode_cb);
#endif
}

static void __bt_state_event_handler(const char *event_name, bundle *data, void *user_data)
{
	const char *bt_status = NULL;
	const char *bt_le_status = NULL;
	const char *bt_transfering_status = NULL;
	BT_DBG("bt state set event(%s) received", event_name);

	bt_status = bundle_get_val(data, EVT_KEY_BT_STATE);
	BT_DBG("bt_state: (%s)", bt_status);

	bt_le_status = bundle_get_val(data, EVT_KEY_BT_LE_STATE);
	BT_DBG("bt_state: (%s)", bt_le_status);
}

void _bt_handle_adapter_added(void)
{
	BT_DBG("+");
	bt_status_t status;
	bt_le_status_t le_status;
	int ret;

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

#ifdef TIZEN_BT_PAN_NAP_ENABLE
	if (_bt_network_activate() != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to activate network");
#endif

	/* add the vconf noti handler */
	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_DEVICE_NAME_STR,
					__bt_phone_name_changed_cb, NULL);
	if (ret < 0)
		BT_ERR("Unable to register key handler");

	if (le_status == BT_LE_ACTIVATING ||
		 status == BT_ACTIVATING) {
		__bt_set_le_enabled();
		_bt_adapter_set_le_status(BT_LE_ACTIVATED);
	}

	if (status == BT_ACTIVATING) {
		__bt_set_enabled();
		_bt_adapter_set_status(BT_ACTIVATED);
	}

	_bt_service_register_vconf_handler();

	/* eventsystem */
	if (eventsystem_register_event(SYS_EVENT_BT_STATE, &status_reg_id,
			(eventsystem_handler)__bt_state_event_handler, NULL) != ES_R_OK) {
		BT_ERR("Fail to register system event");
	}
}

void _bt_handle_adapter_removed(void)
{
	int ret;

	_bt_adapter_set_status(BT_DEACTIVATED);

	__bt_visibility_alarm_remove();

	if (visible_timer.alarm_init) {
		alarmmgr_fini();
		visible_timer.alarm_init = FALSE;
	}

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_DEVICE_NAME_STR,
				(vconf_callback_fn)__bt_phone_name_changed_cb);
	if (0 != ret) {
		ERR("vconf_ignore_key_changed failed\n");
	}

	_bt_destroy_agent(adapter_agent);
	adapter_agent = NULL;

	_bt_reliable_terminate_service(NULL);

	if (eventsystem_unregister_event(status_reg_id) != ES_R_OK) {
		BT_ERR("Fail to unregister system event");
	}

}

static gboolean __bt_enable_timeout_cb(gpointer user_data)
{
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

	timer_id = 0;

	retv_if(_bt_adapter_get_status() == BT_ACTIVATED, FALSE);

	BT_ERR("EnableAdapter is failed");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return FALSE;

	/* Clean up the process */
	result = g_dbus_proxy_call_sync(proxy,
				"DisableAdapter",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Bt core call failed(Error: %s)", error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("Bt core call failed");
		}
		return FALSE;
	}

	g_variant_unref(result);
	_bt_set_disabled(BLUETOOTH_ERROR_TIMEOUT);

	_bt_terminate_service(NULL);

	return FALSE;
}

static gboolean __bt_enable_le_timeout_cb(gpointer user_data)
{
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

	le_timer_id = 0;

	retv_if(_bt_adapter_get_le_status() == BT_LE_ACTIVATED, FALSE);

	BT_ERR("EnableAdapterLE is failed");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return FALSE;

	/* Clean up the process */
	result = g_dbus_proxy_call_sync(proxy,
				"DisableAdapterLe",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Bt core call failed(Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Bt core call failed");
		return FALSE;
	}

	g_variant_unref(result);
	_bt_adapter_set_le_status(BT_LE_DEACTIVATED);

	_bt_set_le_disabled(BLUETOOTH_ERROR_TIMEOUT);

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
	GDBusProxy *proxy;
	GError *error = NULL;
	int ret;
	GVariant *result = NULL;
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

	result = g_dbus_proxy_call_sync(proxy, "EnableAdapter",
					 NULL,
					 G_DBUS_CALL_FLAGS_NONE, BT_ENABLE_TIMEOUT,
					 NULL, &error);
	 if (error) {
		 BT_ERR("EnableAdapterLe failed: %s", error->message);
 		_bt_adapter_set_status(BT_DEACTIVATED);
		g_clear_error(&error);
		error = NULL;
		result = g_dbus_proxy_call_sync(proxy,
				"DisableAdapter",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

		if (error != NULL) {
				BT_ERR("Bt core call failed(Error: %s)", error->message);
				g_clear_error(&error);
		}
		g_variant_unref(result);
		/* Terminate myself */
		g_idle_add((GSourceFunc)_bt_terminate_service, NULL);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(result);
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
	GDBusConnection *conn;
	GDBusProxy *dev_proxy;
	gboolean ret = FALSE;
	GVariant *result;
	GError *error = NULL;
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

		if (info.connected != BLUETOOTH_CONNECTED_LINK_NONE) {
			BT_DBG("Found Connected device");
			_bt_convert_addr_type_to_string(address, info.device_address.addr);
			device_path = _bt_get_device_object_path(address);
			if (device_path == NULL)
				continue;

			BT_DBG("Disconnecting : %s", device_path);

			dev_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
							NULL,
							BT_BLUEZ_NAME,
							device_path,
							BT_DEVICE_INTERFACE,
							NULL, NULL);

			if (dev_proxy == NULL)
				continue;

			result = g_dbus_proxy_call_sync(dev_proxy,
						"Disconnect",
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&error);

			if (!result) {
				if (error != NULL) {
					BT_ERR("Disconnect call failed(Error: %s)", error->message);
					g_clear_error(&error);
				} else
					BT_ERR("Disconnect call failed");
				g_object_unref(dev_proxy);
				return FALSE;
			}

			g_variant_unref(result);
			g_object_unref(dev_proxy);
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
	GDBusProxy *proxy;
	bt_le_status_t le_status;
	int ret;
	GVariant *result;
	GError *error = NULL;

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

	result = g_dbus_proxy_call_sync(proxy,
				"DisableAdapter",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to DisableAdapter (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to DisableAdapter");
		_bt_adapter_set_status(BT_ACTIVATED);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
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
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

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

	result = g_dbus_proxy_call_sync(proxy,
				"RecoverAdapter",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to RecoverAdapter (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to RecoverAdapter");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	__bt_disconnect_all();

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_reset_adapter(void)
{
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;

	BT_DBG("");

	proxy = __bt_get_core_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;

	result = g_dbus_proxy_call_sync(proxy,
				"ResetAdapter",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to ResetAdapter (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to ResetAdapter");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
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
	GDBusProxy *proxy;
	GError *error = NULL;
	bt_status_t status = _bt_adapter_get_status();
	bt_le_status_t le_status = _bt_adapter_get_le_status();
	GVariant *result;

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

	result = g_dbus_proxy_call_sync(proxy, "EnableAdapterLe",
					NULL,
					G_DBUS_CALL_FLAGS_NONE, BT_ENABLE_TIMEOUT,
					NULL, &error);
	if (error) {
		BT_ERR("EnableAdapterLe failed: %s", error->message);
		_bt_adapter_set_le_status(BT_DEACTIVATED);
		g_clear_error(&error);

		/* Clean up the process */
		result = g_dbus_proxy_call_sync(proxy,
					"DisableAdapterLe",
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

		if (!result) {
				BT_ERR("Bt core call failed");
				if (error) {
					BT_ERR("EnableAdapterLE Failed %s", error->message);
					g_clear_error(&error);
				}
		}
		g_variant_unref(result);
		/* Terminate myself */
		if (_bt_adapter_get_status() == BT_DEACTIVATED)
			g_idle_add((GSourceFunc)_bt_terminate_service, NULL);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (result)
		g_variant_unref(result);

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
	GDBusProxy *proxy;
	bt_le_status_t bt_le_state;
	GVariant *result;
	GError *error = NULL;

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

	result = g_dbus_proxy_call_sync(proxy,
				"DisableAdapterLe",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Bt core call failed (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Bt core call failed");
		_bt_adapter_set_le_status(BT_LE_ACTIVATED);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	_bt_set_le_disabled(BLUETOOTH_ERROR_NONE);
	BT_DBG("le status : %d", _bt_adapter_get_le_status());
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_local_address(bluetooth_device_address_t *local_address)
{

	GDBusProxy *proxy;
	GError *error = NULL;
	const char *address;
	GVariant *result;
	GVariant *temp;

	BT_CHECK_PARAMETER(local_address, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"Address"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		BT_ERR("Failed to get local address");
		if (error != NULL) {
			BT_ERR("Failed to get local address (Error: %s)", error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	address = g_variant_get_string(temp, NULL);
	BT_DBG("Address:%s", address);

	if (address) {
		_bt_convert_addr_string_to_type(local_address->addr, address);
	} else {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	g_variant_unref(temp);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_local_version(bluetooth_version_t *local_version)
{
	GDBusProxy *proxy;
	const char *ver = NULL;
	char *ptr = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	GVariant *result;
	GVariant *temp;

	BT_CHECK_PARAMETER(local_version, return);

	GError *error = NULL;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"Version"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get local version (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get local version");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	ver = g_variant_get_string(temp, NULL);
	BT_DBG("VERSION: %s", ver);

	if (ver && (strlen(ver) > 0)) {
		/* Check the utf8 valitation & Fill the NULL in the invalid location*/
		if (!g_utf8_validate(ver, -1, (const char **)&ptr))
			*ptr = '\0';

		g_strlcpy(local_version->version, ver,
				BLUETOOTH_VERSION_LENGTH_MAX + 1);

	} else {
		ret = BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	g_variant_unref(temp);
	return ret;
}

int _bt_get_local_name(bluetooth_device_name_t *local_name)
{
	GDBusProxy *proxy;
	const char *name = NULL;
	char *ptr = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	GVariant *result;
	GVariant *temp;
	GError *error = NULL;

	BT_CHECK_PARAMETER(local_name, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"Alias"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get local name (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get local name");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	name = g_variant_get_string(temp, NULL);
	BT_DBG("LOCAL NAME:%s", name);

	if (name && (strlen(name) > 0)) {
		/* Check the utf8 valitation & Fill the NULL in the invalid location*/
		if (!g_utf8_validate(name, -1, (const char **)&ptr))
			*ptr = '\0';

		g_strlcpy(local_name->name, name,
				BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1);
	} else {
		ret = BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(result);
	g_variant_unref(temp);
	return ret;
}

int _bt_set_local_name(char *local_name)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	char *ptr = NULL;
	GVariant *result;

	BT_CHECK_PARAMETER(local_name, return);

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!g_utf8_validate(local_name, -1, (const char **)&ptr))
		*ptr = '\0';

	result = g_dbus_proxy_call_sync(proxy,
				"Set",
				g_variant_new("(ssv)", BT_ADAPTER_INTERFACE,
					"Alias", g_variant_new("s", local_name)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to set Alias (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to set Alias");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_is_service_used(char *service_uuid, gboolean *used)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	GVariant *result;
	GVariant *value;
	GVariantIter *iter = NULL;
	gchar *uuid;

	BT_DBG("+");
	BT_CHECK_PARAMETER(service_uuid, return);
	BT_CHECK_PARAMETER(used, return);

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"UUIDs"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get UUIDs (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get UUIDs");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &value);
	g_variant_get(value, "as", &iter);
	if(iter == NULL) {
		BT_ERR("Failed to get UUIDs(%s)",service_uuid);
		*used = FALSE;
		g_variant_unref(result);
		g_variant_unref(value);
		return ret;
	}

	while (g_variant_iter_loop(iter, "s", &uuid)) {
		if (strcasecmp(uuid, service_uuid) == 0) {
			*used = TRUE;
			g_free(uuid);
			goto done;
		}
	}

	*used = FALSE;

done:
	g_variant_iter_free(iter);
	g_variant_unref(value);
	g_variant_unref(result);
	BT_DBG("Service Used? %d", *used);
	return ret;
}

static gboolean __bt_get_discoverable_property(void)
{
	GDBusProxy *proxy;
	gboolean discoverable_v;
	GError *error = NULL;
	GVariant *result;
	GVariant *temp;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, FALSE);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"Discoverable"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get Discoverable property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get Discoverable property");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	discoverable_v = g_variant_get_boolean(temp);
	BT_DBG("discoverable_v:%d", discoverable_v);

	g_variant_unref(result);
	g_variant_unref(temp);

	return discoverable_v;
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
	GDBusProxy *proxy;
	GVariant *result;

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

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

	BT_INFO("Req. discoverable_mode : %d, timeout : %d",
			discoverable_mode, timeout);

	result = g_dbus_proxy_call_sync(proxy,
				"Set",
				g_variant_new("(ssv)", BT_ADAPTER_INTERFACE,
					"Connectable", g_variant_new("b", pg_scan)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to set connectable property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to set connectable property");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(result);
	result = g_dbus_proxy_call_sync(proxy,
				"Set",
				g_variant_new("(ssv)", BT_ADAPTER_INTERFACE, "Discoverable",
						g_variant_new("b", inq_scan)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to set Discoverable property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to set Discoverable property");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_unref(result);
	result = g_dbus_proxy_call_sync(proxy,
				"Set",
				g_variant_new("(ssv)", BT_ADAPTER_INTERFACE,
					"DiscoverableTimeout", g_variant_new("u", timeout)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to set DiscoverableTimeout property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to set DiscoverableTimeout property");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (discoverable_mode == BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE)
		timeout = -1;

	ret = __bt_set_visible_time(timeout);

	g_variant_unref(result);

	return ret;
}

int _bt_start_discovery(void)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *result;

	if (_bt_is_discovering() == TRUE) {
		BT_ERR("BT is already in discovering");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	} else if (_bt_is_device_creating() == TRUE) {
		BT_ERR("Bonding device is going on");
		return BLUETOOTH_ERROR_DEVICE_BUSY;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"StartDiscovery",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("StartDiscovery failed (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("StartDiscovery failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_discovering = TRUE;
	cancel_by_user = FALSE;
	/* discovery status will be change in event */
	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_start_custom_discovery(bt_discovery_role_type_t role)
{
	GDBusProxy *proxy;
	GVariant *result;
	GError *error = NULL;
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

	result = g_dbus_proxy_call_sync(proxy,
				"StartCustomDiscovery",
				g_variant_new("s", disc_type),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("StartCustomDiscovery failed (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("StartCustomDiscovery failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	is_discovering = TRUE;
	cancel_by_user = FALSE;
	/* discovery status will be change in event */
	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_cancel_discovery(void)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *result;

	if (_bt_is_discovering() == FALSE) {
		BT_ERR("BT is not in discovering");
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"StopDiscovery",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("StopDiscovery failed (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("StopDiscovery failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cancel_by_user = TRUE;
	/* discovery status will be change in event */
	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_is_discovering(void)
{
	return is_discovering;
}

gboolean _bt_is_connectable(void)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	gboolean is_connectable = FALSE;
	GVariant *result;
	GVariant *temp;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"Connectable"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get connectable property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get connectable property");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	is_connectable = g_variant_get_boolean(temp);
	BT_DBG("discoverable_v:%d", is_connectable);

	g_variant_unref(result);
	g_variant_unref(temp);

	BT_INFO("Get connectable [%d]", is_connectable);
	return is_connectable;
}

int _bt_set_connectable(gboolean is_connectable)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *result;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to set connectable in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	proxy = _bt_get_adapter_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(proxy,
				"Set",
				g_variant_new("(ssv)", BT_ADAPTER_INTERFACE, "Connectable",
						g_variant_new("b", is_connectable)),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to set connectable property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to set connectable property");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("Set connectable [%d]", is_connectable);
	g_variant_unref(result);
	return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_get_discovering_property(bt_discovery_role_type_t discovery_type)
{
	GDBusProxy *proxy;
	gboolean discovering_v;
	GError *error = NULL;
	char *discovering_type =  NULL;
	GVariant *result;
	GVariant *temp;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (discovery_type == DISCOVERY_ROLE_BREDR)
		discovering_type = "Discovering";
	else if (discovery_type == DISCOVERY_ROLE_LE)
		discovering_type = "LEDiscovering";

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					discovering_type),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to get discovering property (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to get discovering property");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(v)", &temp);
	discovering_v = g_variant_get_boolean(temp);
	BT_DBG("discoverable_v:%d", discovering_v);

	g_variant_unref(result);
	g_variant_unref(temp);

	return discovering_v;
}

unsigned int _bt_get_discoverable_timeout_property(void)
{
	GDBusProxy *proxy;
	unsigned int timeout_v;
	GError *error = NULL;
	GVariant *result;
	GVariant *temp;

	proxy = _bt_get_adapter_properties_proxy();
	retv_if(proxy == NULL, 0);

	result = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", BT_ADAPTER_INTERFACE,
					"DiscoverableTimeout"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		BT_ERR("Fail to get discoverable timeout");
		if (error != NULL) {
			BT_ERR("Fail to get discoverable timeout (Error: %s)", error->message);
			g_clear_error(&error);
		}
		return 0;
	}

	g_variant_get(result, "(v)", &temp);
	timeout_v = g_variant_get_uint32(temp);
	BT_DBG("discoverable_v:%d", timeout_v);

	g_variant_unref(result);
	g_variant_unref(temp);

	return timeout_v;
}

static bluetooth_device_info_t *__bt_parse_device_info(GVariantIter *item_iter)
{
	bluetooth_device_info_t *dev_info;
	GVariant *value;
	const gchar *key;
	GByteArray *manufacturer_data = NULL;
	guint8 char_value;
	GVariantIter *char_value_iter;

	dev_info = g_malloc0(sizeof(bluetooth_device_info_t));

	while (g_variant_iter_loop(item_iter, "{sv}", &key, &value)) {

		if (key == NULL)
			continue;

		if (!g_strcmp0(key, "Address")) {
			const char *address = NULL;
			address = g_variant_get_string(value, NULL);
			_bt_convert_addr_string_to_type(dev_info->device_address.addr,
							address);
		} else if(!g_strcmp0(key, "Class")) {
			unsigned int cod;
			cod = g_variant_get_uint32(value);
			_bt_divide_device_class(&dev_info->device_class, cod);
		} else if(!g_strcmp0(key, "Name")) {
			const char *name = NULL;
			name = g_variant_get_string(value, NULL);
			/* If there is no Alias */
			if (strlen(dev_info->device_name.name) == 0) {
				g_strlcpy(dev_info->device_name.name, name,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
			}
		} else if(!g_strcmp0(key, "Alias")) {
			const char *alias = NULL;
			alias = g_variant_get_string(value, NULL);
			/* Overwrite the name */
			if (alias) {
				memset(dev_info->device_name.name, 0x00,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
				g_strlcpy(dev_info->device_name.name, alias,
						BLUETOOTH_DEVICE_NAME_LENGTH_MAX+1);
			}
		} else if (!g_strcmp0(key, "Connected")) {
			dev_info->connected = g_variant_get_byte(value);
		} else if (!g_strcmp0(key, "Paired")) {
			dev_info->paired = g_variant_get_boolean(value);
		} else if (!g_strcmp0(key, "Trusted")) {
			dev_info->trust = g_variant_get_boolean(value);
		} else if (!g_strcmp0(key, "RSSI")) {
			dev_info->rssi = g_variant_get_int16(value);
		} else if (!g_strcmp0(key, "UUIDs")) {
			GVariantIter *iter;
			gchar *uuid = NULL;
			char **parts;
			int i = 0;

			dev_info->service_index = 0;
			g_variant_get(value, "as", &iter);
			while (g_variant_iter_loop(iter, "s", &uuid)) {
				g_strlcpy(dev_info->uuids[i], uuid, BLUETOOTH_UUID_STRING_MAX);
				parts = g_strsplit(uuid, "-", -1);

				if (parts == NULL || parts[0] == NULL) {
					g_free(uuid);
					break;
				}

				dev_info->service_list_array[i] = g_ascii_strtoull(parts[0], NULL, 16);
				g_strfreev(parts);

				i++;
			}
			dev_info->service_index = i;
			g_variant_iter_free(iter);
		} else if (strcasecmp(key, "ManufacturerDataLen") == 0) {
			dev_info->manufacturer_data.data_len = g_variant_get_uint16(value);
		} else if (strcasecmp(key, "ManufacturerData") == 0) {
			manufacturer_data = g_byte_array_new();
			g_variant_get(value, "ay", &char_value_iter);
			while(g_variant_iter_loop(char_value_iter, "y",  &char_value)) {
				g_byte_array_append(manufacturer_data, &char_value, 1);
			}
			if (manufacturer_data) {
				if (manufacturer_data->len > 0) {
					memcpy(dev_info->manufacturer_data.data, manufacturer_data->data, manufacturer_data->len);
				}
			}
			g_variant_iter_free(char_value_iter);
			g_byte_array_free(manufacturer_data, TRUE);
		}
	}

	return dev_info;
}

static void __bt_extract_device_info(GVariantIter *iter,
							GArray **dev_list)
{
	bluetooth_device_info_t *dev_info = NULL;
	char *object_path = NULL;
	GVariantIter *interface_iter;
	GVariantIter *svc_iter;
	char *interface_str = NULL;

	/* Parse the signature:  oa{sa{sv}}} */
	while (g_variant_iter_loop(iter, "{&oa{sa{sv}}}", &object_path,
		&interface_iter)) {

		if (object_path == NULL)
			continue;

		while (g_variant_iter_loop(interface_iter, "{sa{sv}}",
				&interface_str, &svc_iter)) {
			if (g_strcmp0(interface_str, "org.bluez.Device1") == 0) {
				BT_DBG("Found a device: %s", object_path);
				dev_info = __bt_parse_device_info(svc_iter);
				if (dev_info) {
					if (dev_info->paired == TRUE) {
						g_array_append_vals(*dev_list, dev_info,
								sizeof(bluetooth_device_info_t));
					}
					g_free(dev_info);
				}
				g_free(interface_str);
				g_variant_iter_free(svc_iter);
				break;
			}
		}
	}
	BT_DBG("-");
}

int _bt_get_bonded_devices(GArray **dev_list)
{
	BT_DBG("+");
	GDBusConnection *conn;
	GDBusProxy *manager_proxy;
	GVariant *result = NULL;
	GVariantIter *iter = NULL;
	GError *error = NULL;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(manager_proxy, "GetManagedObjects",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				NULL);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to GetManagedObjects (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to Failed to GetManagedObjects");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* signature of GetManagedObjects:  a{oa{sa{sv}}} */
	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);

	__bt_extract_device_info(iter, dev_list);
	g_variant_iter_free(iter);
	g_variant_unref(result);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_bonded_device_info(bluetooth_device_address_t *device_address,
				bluetooth_device_info_t *dev_info)
{
	char *object_path = NULL;
	GDBusProxy *adapter_proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(dev_info, return);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	object_path = _bt_get_device_object_path(address);

	retv_if(object_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	ret = __bt_get_bonded_device_info(object_path, dev_info);
	g_free(object_path);

	return ret;
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

int _bt_set_le_privacy(gboolean set_privacy)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *result = NULL;

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

	result = g_dbus_proxy_call_sync(proxy,
				"SetLePrivacy",
				g_variant_new("(b)", set_privacy),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to SetLePrivacy (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Failed to SetLePrivacy");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(result);
	BT_INFO("SetLePrivacy as %d", set_privacy);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_manufacturer_data(bluetooth_manufacturer_data_t *m_data)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	int i;
	GVariant *val;
	GVariant *result;
	GVariantBuilder *builder;

	BT_CHECK_PARAMETER(m_data, return);

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < (m_data->data_len) + 2; i++) {
		g_variant_builder_add(builder, "y", m_data->data[i]);
	}

	val = g_variant_new("(ay)", builder);

	result = g_dbus_proxy_call_sync(proxy,
				"SetManufacturerData",
				val,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	g_variant_builder_unref(builder);
	if (!result) {
		if (error != NULL) {
			BT_ERR("Failed to SetManufacturerData (Error: %s)", error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("Failed to SetManufacturerData");
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}
	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < (m_data->data_len) + 2; i++) {
		g_variant_builder_add(builder, "y", m_data->data[i]);
	}

	val = g_variant_new("(ay)", builder);

	_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_MANUFACTURER_DATA_CHANGED,
			val);

	BT_INFO("Set manufacturer data");

	g_variant_builder_unref(builder);
	g_variant_unref(result);

	return BLUETOOTH_ERROR_NONE;
}
