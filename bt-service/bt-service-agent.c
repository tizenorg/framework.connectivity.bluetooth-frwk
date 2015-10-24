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

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stacktrim.h>
#include <syspopup_caller.h>
#include <vconf.h>
#include <bundle_internal.h>

#ifdef TIZEN_NETWORK_TETHERING_ENABLE
#include <tethering.h>
#endif

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-agent.h"
#include "bt-service-gap-agent.h"
#include "bt-service-adapter.h"
#include "bt-service-event.h"
#include "bt-service-rfcomm-server.h"
#include "bt-service-device.h"
#include "bt-service-audio.h"

#define BT_APP_AUTHENTICATION_TIMEOUT		35
#define BT_APP_AUTHORIZATION_TIMEOUT		15

#define HFP_AUDIO_GATEWAY_UUID "0000111f-0000-1000-8000-00805f9b34fb"
#define HSP_AUDIO_GATEWAY_UUID "00001112-0000-1000-8000-00805f9b34fb"
#define A2DP_UUID "0000110D-0000-1000-8000-00805F9B34FB"
#define AVRCP_TARGET_UUID "0000110c-0000-1000-8000-00805f9b34fb"
#define OPP_UUID "00001105-0000-1000-8000-00805f9b34fb"
#define FTP_UUID "00001106-0000-1000-8000-00805f9b34fb"
#define SPP_UUID "00001101-0000-1000-8000-00805f9b34fb"
#define PBAP_UUID "0000112f-0000-1000-8000-00805f9b34fb"
#define MAP_UUID "00001132-0000-1000-8000-00805f9b34fb"
#define NAP_UUID "00001116-0000-1000-8000-00805f9b34fb"
#define GN_UUID "00001117-0000-1000-8000-00805f9b34fb"
#define BNEP_UUID "0000000f-0000-1000-8000-00805f9b34fb"
#define HID_UUID "00001124-0000-1000-8000-00805f9b34fb"
#define SAP_UUID_OLD "a49eb41e-cb06-495c-9f4f-bb80a90cdf00"
#define SAP_UUID_NEW "a49eb41e-cb06-495c-9f4f-aa80a90cdf4a"

#define BT_AGENT_OBJECT "/org/bluez/agent/frwk_agent"

#define BT_AGENT_INTERFACE "org.bluez.Agent1"

#define BT_AGENT_SIGNAL_RFCOMM_AUTHORIZE "RfcommAuthorize"
#define BT_AGENT_SIGNAL_OBEX_AUTHORIZE "ObexAuthorize"

#define BT_PIN_MAX_LENGTH 16
#define BT_PASSKEY_MAX_LENGTH 4

#define BT_AGENT_SYSPOPUP_TIMEOUT_FOR_MULTIPLE_POPUPS 200
#define BT_AGENT_SYSPOPUP_MAX_ATTEMPT 3
#define BT_PAN_MAX_CONNECTION 4
extern guint nap_connected_device_count;

#define G_VARIANT_UNREF(variant) \
	g_variant_unref(variant); \
	variant = NULL

static gboolean syspopup_mode = TRUE;

static int __bt_agent_is_auto_response(uint32_t dev_class, const gchar *address,
							const gchar *name);
static gboolean __bt_agent_is_hid_keyboard(uint32_t dev_class);
static int __bt_agent_generate_passkey(char *passkey, int size);

static void __bt_agent_release_memory(void)
{
	/* Release Malloc Memory*/
	malloc_trim(0);

	/* Release Stack Memory*/
	stack_trim();
}

static gboolean __bt_agent_system_popup_timer_cb(gpointer user_data)
{
	int ret;
	static int retry_count;
	bundle *b = (bundle *)user_data;
	retv_if(user_data == NULL, FALSE);

	++retry_count;

	ret = syspopup_launch("bt-syspopup", b);
	if (ret < 0) {
		BT_ERR("Sorry! Can't launch popup, ret=%d, Re-try[%d] time..",
							ret, retry_count);
		if (retry_count >= BT_AGENT_SYSPOPUP_MAX_ATTEMPT) {
			BT_ERR("Sorry!! Max retry %d reached", retry_count);
			bundle_free(b);
			retry_count = 0;
			return FALSE;
		}
	} else {
		BT_DBG("Hurray!! Finally Popup launched");
		retry_count = 0;
		bundle_free(b);
	}

	return (ret < 0) ? TRUE : FALSE;
}

#ifdef TIZEN_WEARABLE
static void __bt_unbond_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	GError *err = NULL;
	GVariant *value;

	value = g_dbus_proxy_call_finish(proxy, res, &err);
	if (value == NULL) {
		BT_ERR("Error: Unbond Failed");
		if (err) {
			BT_ERR("errCode[%x], message[%s]\n", err->code, err->message);
			g_clear_error(&err);
		}
		return;
	}
	g_variant_unref(value);
	BT_INFO("Unbonding is done");
	return;
}

static gboolean __bt_unpair_device(void)
{
	GArray *device_list;
	int no_of_device;
	int i;

	device_list = g_array_new(FALSE, FALSE, sizeof(gchar));
	if (device_list == NULL) {
		BT_ERR("g_array_new is failed");
		return FALSE;
	}

	if (_bt_get_bonded_devices(&device_list) != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_get_bonded_devices is failed");
		g_array_free(device_list, TRUE);
		return FALSE;
	}

	no_of_device = device_list->len / sizeof(bluetooth_device_info_t);
	for (i = 0; i < no_of_device; i++) {
		GDBusProxy *adapter_proxy;
		bluetooth_device_info_t info;
		char addr[BT_ADDRESS_STRING_SIZE] = { 0 };
		char *device_path = NULL;

		info = g_array_index(device_list, bluetooth_device_info_t, i);
		if (info.device_class.major_class ==
				BLUETOOTH_DEVICE_MAJOR_CLASS_AUDIO)
			continue;

		adapter_proxy = _bt_get_adapter_proxy();
		if (!adapter_proxy) {
			BT_ERR("adapter_proxy is NULL");
			g_array_free(device_list, TRUE);
			return FALSE;
		}

		_bt_convert_addr_type_to_string(addr, info.device_address.addr);
		device_path = _bt_get_device_object_path(addr);
		if (device_path == NULL) {
			BT_ERR("device_path is NULL");
			g_array_free(device_list, TRUE);
			return FALSE;
		}

		g_dbus_proxy_call(adapter_proxy,
				"UnpairDevice", g_variant_new("o", device_path),
				G_DBUS_CALL_FLAGS_NONE, -1, NULL,
				(GAsyncReadyCallback)__bt_unbond_cb, NULL);

		BT_INFO("unbonding %s is requested", addr);

		g_array_free(device_list, TRUE);
		return TRUE;
	}

	g_array_free(device_list, TRUE);
	return FALSE;
}

static void __bt_popup_event_filter(GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{
	BT_DBG("Sender Name[%s] Object Path[%s] Interface[%s] Signal[%s]",
			sender_name, object_path, interface_name, signal_name);

	if (g_strcmp0(interface_name, "User.Bluetooth.syspopup") == 0 &&
			g_strcmp0(signal_name, "ResetResponse") == 0) {
		int response;

		g_variant_get(parameters, "(i)", &response);
		BT_DBG("response = %d", response);
	}
}

int __bt_service_subscribe_popup(GDBusConnection *conn,
		gboolean subscribe)
{
	static guint subs_interface_added_id = 0;

	if (conn == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (subscribe) {
		if (subs_interface_added_id == 0) {
			subs_interface_added_id = g_dbus_connection_signal_subscribe(conn,
					NULL, "User.Bluetooth.syspopup", "ResetResponse", NULL, NULL, 0,
					__bt_popup_event_filter, NULL, NULL);
		}
	} else {
		if (subs_interface_added_id > 0) {
			g_dbus_connection_signal_unsubscribe(conn,
					subs_interface_added_id);
			subs_interface_added_id = 0;
		}
	}
	return BLUETOOTH_ERROR_NONE;
}

static void  __bt_register_popup_event_signal(void)
{
	GDBusConnection *conn;

	BT_DBG("+\n");

	conn = _bt_get_system_gconn();
	if (conn == NULL)
		return;

	__bt_service_subscribe_popup(conn, TRUE);

	BT_DBG("-\n");
	return;
}

static gboolean __is_reset_required(const gchar *address)
{
	GArray *device_list;
	uint32_t no_of_device;
	uint32_t i;
	bluetooth_device_info_t info;
	gboolean is_required = FALSE;

	device_list = g_array_new(FALSE, FALSE, sizeof(gchar));
	if (device_list == NULL) {
		BT_ERR("g_array_new is failed");
		return FALSE;
	}

	if (_bt_get_bonded_devices(&device_list) != BLUETOOTH_ERROR_NONE) {
		BT_ERR("_bt_get_bonded_devices is failed");
		g_array_free(device_list, TRUE);
		return FALSE;
	}

	no_of_device = device_list->len / sizeof(bluetooth_device_info_t);
	for (i = 0; i < no_of_device; i++) {
		char addr[BT_ADDRESS_STRING_SIZE] = { 0 };

		info = g_array_index(device_list, bluetooth_device_info_t, i);

		_bt_convert_addr_type_to_string(addr, info.device_address.addr);
		if (g_strcmp0(address, addr) == 0) {
			BT_DBG("This device is already in paired list");
			is_required = FALSE;
			break;
		}

		if (info.device_class.major_class != BLUETOOTH_DEVICE_MAJOR_CLASS_AUDIO) {
			is_required = TRUE;
			break;
		}
	}
	g_array_free(device_list, TRUE);

	return is_required;
}
#endif

int _bt_launch_system_popup(bt_agent_event_type_t event_type,
							const char *device_name,
							char *passkey,
							const char *filename,
							const char *agent_path)
{
	int ret;
	bundle *b;
	char event_str[BT_MAX_EVENT_STR_LENGTH + 1];

	b = bundle_create();
	if (!b) {
		BT_ERR("Launching system popup failed");
		return -1;
	}

	bundle_add(b, "device-name", device_name);
	bundle_add(b, "passkey", passkey);
	bundle_add(b, "file", filename);
	bundle_add(b, "agent-path", agent_path);

	switch (event_type) {
	case BT_AGENT_EVENT_PIN_REQUEST:
		g_strlcpy(event_str, "pin-request", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_PASSKEY_CONFIRM_REQUEST:
		g_strlcpy(event_str, "passkey-confirm-request",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_PASSKEY_AUTO_ACCEPTED:
		g_strlcpy(event_str, "passkey-auto-accepted",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_PASSKEY_REQUEST:
		g_strlcpy(event_str, "passkey-request", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_PASSKEY_DISPLAY_REQUEST:
		g_strlcpy(event_str, "passkey-display-request",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_AUTHORIZE_REQUEST:
		g_strlcpy(event_str, "authorize-request",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_CONFIRM_MODE_REQUEST:
		g_strlcpy(event_str, "confirm-mode-request",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_FILE_RECEIVED:
		g_strlcpy(event_str, "file-received", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST:
		g_strlcpy(event_str, "keyboard-passkey-request",
						sizeof(event_str));
		break;

	case BT_AGENT_EVENT_TERMINATE:
		g_strlcpy(event_str, "terminate", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_EXCHANGE_REQUEST:
		g_strlcpy(event_str, "exchange-request", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_PBAP_REQUEST:
		g_strlcpy(event_str, "phonebook-request", sizeof(event_str));
		break;

	case BT_AGENT_EVENT_MAP_REQUEST:
		g_strlcpy(event_str, "message-request", sizeof(event_str));
		break;

#ifdef TIZEN_WEARABLE
	case BT_AGENT_EVENT_SYSTEM_RESET_REQUEST:
		__bt_register_popup_event_signal();
		g_strlcpy(event_str, "system-reset-request", sizeof(event_str));
		break;
#endif

	case BT_AGENT_EVENT_LEGACY_PAIR_FAILED_FROM_REMOTE:
		g_strlcpy(event_str, "remote-legacy-pair-failed", sizeof(event_str));
		break;

	default:
		BT_ERR("Invalid event type");
		bundle_free(b);
		return -1;

	}

	bundle_add(b, "event-type", event_str);

	ret = syspopup_launch("bt-syspopup", b);
	if (0 > ret) {
		BT_ERR("Popup launch failed...retry %d", ret);

		g_timeout_add(BT_AGENT_SYSPOPUP_TIMEOUT_FOR_MULTIPLE_POPUPS,
			      (GSourceFunc)__bt_agent_system_popup_timer_cb, b);
	} else {
		bundle_free(b);
	}

	BT_INFO("_bt_agent_launch_system_popup");
	return 0;
}

static GVariant *__bt_service_getall(GDBusProxy *device, const char *interface)
{
	GError *error = NULL;
	GVariant *reply;

	reply = g_dbus_proxy_call_sync(device,
			"GetAll", g_variant_new("(s)", interface),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &error);
	if (reply == NULL) {
		ERR("GetAll dBUS-RPC failed");
		if (error) {
			ERR("D-Bus API failure: errCode[%x], message[%s]",
				error->code, error->message);
			g_clear_error(&error);
		}
		return NULL;
	}

	return reply;
}

static gboolean __pincode_request(GapAgentPrivate *agent, GDBusProxy *device)
{
	uint32_t device_class;
	const gchar *address;
	const gchar *name;
	GVariant *reply = NULL;
	GVariant *reply_temp = NULL;
	GVariant *tmp_value;
	GVariant *param;
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");

	reply_temp = __bt_service_getall(device, BT_DEVICE_INTERFACE);

	if (reply_temp == NULL) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
				NULL);
		goto done;
	}

	g_variant_get(reply_temp,"(@a{sv})", &reply); /* Format of reply a{sv}*/

	tmp_value = g_variant_lookup_value(reply, "Class", G_VARIANT_TYPE_UINT32);
	g_variant_get(tmp_value, "u", &device_class);
	G_VARIANT_UNREF(tmp_value);

	tmp_value = g_variant_lookup_value(reply, "Address", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &address);
	G_VARIANT_UNREF(tmp_value);
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	tmp_value = g_variant_lookup_value(reply, "Name", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &name);
	G_VARIANT_UNREF(tmp_value);
	if (!name)
		name = address;

	if (_bt_is_device_creating() == TRUE &&
		_bt_is_bonding_device_address(address) == TRUE &&
		__bt_agent_is_auto_response(device_class, address, name)) {
		BT_DBG("0000 Auto Pair");
		/* Use Fixed PIN "0000" for basic pairing */
		_bt_set_autopair_status_in_bonding_info(TRUE);
		gap_agent_reply_pin_code(agent, GAP_AGENT_ACCEPT, "0000",
									NULL);
	} else if (__bt_agent_is_hid_keyboard(device_class)) {
		BT_DBG("HID Keyboard");
		char str_passkey[BT_PASSKEY_MAX_LENGTH + 1] = { 0 };

		if (__bt_agent_generate_passkey(str_passkey,
					BT_PASSKEY_MAX_LENGTH) != 0) {
			gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT,
						"", NULL);
			goto done;
		}

		gap_agent_reply_pin_code(agent, GAP_AGENT_ACCEPT,
							str_passkey, NULL);

		if (syspopup_mode) {
			BT_DBG("LAUNCH SYSPOPUP");
			_bt_launch_system_popup(BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST,
					name, str_passkey, NULL,
					_gap_agent_get_path(agent));
		} else {
			BT_DBG("Send BLUETOOTH_EVENT_KEYBOARD_PASSKEY_DISPLAY");
			param = g_variant_new("(isss)", result, address, name, str_passkey);
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_KEYBOARD_PASSKEY_DISPLAY, param);
		}
	} else {
		BT_DBG("Show Pin entry");

		if (syspopup_mode) {
			BT_DBG("LAUNCH SYSPOPUP");
			_bt_launch_system_popup(BT_AGENT_EVENT_PIN_REQUEST, name, NULL,
					NULL, _gap_agent_get_path(agent));
		} else {
			BT_DBG("Send BLUETOOTH_EVENT_PIN_REQUEST");
			param = g_variant_new("(iss)", result, address, name);
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_PIN_REQUEST, param);
		}
	}

done:
	g_variant_unref(reply);
	g_variant_unref(reply_temp);
	__bt_agent_release_memory();
	BT_DBG("-");

	return TRUE;
}

static gboolean __passkey_request(GapAgentPrivate *agent, GDBusProxy *device)
{
	const gchar *address;
	const gchar *name;
	GVariant *reply = NULL;
	GVariant *reply_temp = NULL;
	GVariant *tmp_value;
	BT_DBG("+");

	reply_temp = __bt_service_getall(device, BT_DEVICE_INTERFACE);

	if (reply_temp == NULL) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	g_variant_get(reply_temp,"(@a{sv})", &reply); /* Format of reply a{sv}*/

	tmp_value = g_variant_lookup_value (reply, "Address", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &address);
	G_VARIANT_UNREF(tmp_value);
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	tmp_value = g_variant_lookup_value(reply, "Name", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &name);
	G_VARIANT_UNREF(tmp_value);
	if (!name)
		name = address;

	if (syspopup_mode) {
		BT_DBG("LAUNCH SYSPOPUP");
		_bt_launch_system_popup(BT_AGENT_EVENT_PASSKEY_REQUEST, name, NULL, NULL,
				_gap_agent_get_path(agent));
	} else {
		int result = BLUETOOTH_ERROR_NONE;
		GVariant *param;

		BT_DBG("Send BLUETOOTH_EVENT_PASSKEY_REQUEST");
		param = g_variant_new("(iss)", result, address, name);
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_PASSKEY_REQUEST, param);
	}

done:
	g_variant_unref(reply);
	g_variant_unref(reply_temp);
	__bt_agent_release_memory();

	BT_DBG("-");
	return TRUE;
}

static gboolean __display_request(GapAgentPrivate *agent, GDBusProxy *device,
								guint passkey)
{
	const gchar *address;
	const gchar *name;
	char *str_passkey;
	GVariant *reply = NULL;
	GVariant *reply_temp = NULL;
	GVariant *tmp_value = NULL;

	BT_DBG("+");

	reply_temp = __bt_service_getall(device, BT_DEVICE_INTERFACE);
	if (reply_temp == NULL) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	g_variant_get(reply_temp,"(@a{sv})", &reply); /* Format of reply a{sv}*/

	tmp_value = g_variant_lookup_value (reply, "Address", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &address);
	G_VARIANT_UNREF(tmp_value);
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	tmp_value = g_variant_lookup_value(reply, "Name", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &name);
	G_VARIANT_UNREF(tmp_value);
	if (!name)
		name = address;

	str_passkey = g_strdup_printf("%d", passkey);

	if (syspopup_mode) {
		BT_DBG("LAUNCH SYSPOPUP");
		_bt_launch_system_popup(BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST, name,
				str_passkey, NULL,
				_gap_agent_get_path(agent));
	} else {
		int result = BLUETOOTH_ERROR_NONE;
		GVariant *param;

		BT_DBG("Send BLUETOOTH_EVENT_KEYBOARD_PASSKEY_DISPLAY");
		param = g_variant_new("(isss)", result, address, name, str_passkey);
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_KEYBOARD_PASSKEY_DISPLAY, param);
	}

	g_free(str_passkey);

done:
	g_variant_unref(reply);
	g_variant_unref(reply_temp);
	__bt_agent_release_memory();

	BT_DBG("-");
	return TRUE;
}

static gboolean __confirm_request(GapAgentPrivate *agent, GDBusProxy *device,
								guint passkey)
{
	const gchar *address;
	const gchar *name;
	char str_passkey[7];
	GVariant *reply_temp = NULL;
	GVariant *reply = NULL;
	GVariant *tmp_value;
	BT_DBG("+ passkey[%.6d]", passkey);

	snprintf(str_passkey, sizeof(str_passkey), "%.6d", passkey);

	reply_temp = __bt_service_getall(device, BT_DEVICE_INTERFACE);

	if (reply_temp == NULL) {
		BT_ERR("Device doesn't exist");
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	g_variant_get(reply_temp,"(@a{sv})", &reply); /* Format of reply a{sv}*/

	tmp_value = g_variant_lookup_value (reply, "Address", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &address);
	G_VARIANT_UNREF(tmp_value);
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	tmp_value = g_variant_lookup_value (reply, "Name", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &name);
	G_VARIANT_UNREF(tmp_value);
	if (!name)
		name = address;

#ifdef TIZEN_WEARABLE
	uint32_t device_class = 0x00;
	uint32_t major_class;

	tmp_value = g_variant_lookup_value(reply, "Class", G_VARIANT_TYPE_UINT32);
	g_variant_get(tmp_value, "u", &device_class);
	G_VARIANT_UNREF(tmp_value);

	major_class = (device_class & 0x1f00) >> 8;

	if (major_class == BLUETOOTH_DEVICE_MAJOR_CLASS_AUDIO) {
		BT_DBG("Audio device. Launch passkey pop-up");
		_bt_launch_system_popup(BT_AGENT_EVENT_PASSKEY_CONFIRM_REQUEST, name,
				str_passkey, NULL, _gap_agent_get_path(agent));
		goto done;
	}

	if (__is_reset_required(address)) {
		BT_INFO("Launch system reset pop-up");
		_bt_launch_system_popup(BT_AGENT_EVENT_SYSTEM_RESET_REQUEST, name,
				NULL, NULL, _gap_agent_get_path(agent));
	} else {
		BT_INFO("Launch passkey pop-up");
		_bt_launch_system_popup(BT_AGENT_EVENT_PASSKEY_AUTO_ACCEPTED, name,
				str_passkey, NULL, _gap_agent_get_path(agent));

		gap_agent_reply_confirmation(agent, GAP_AGENT_ACCEPT, NULL);
	}
#else
	if (syspopup_mode) {
		BT_DBG("LAUNCH SYSPOPUP");
		_bt_launch_system_popup(BT_AGENT_EVENT_PASSKEY_CONFIRM_REQUEST, name,
				str_passkey, NULL,
				_gap_agent_get_path(agent));
	} else {
		int result = BLUETOOTH_ERROR_NONE;
		GVariant *param;

		BT_DBG("Send BLUETOOTH_EVENT_PASSKEY_CONFIRM_REQUEST");
		param = g_variant_new("(isss)", result, address, name, str_passkey);
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_PASSKEY_CONFIRM_REQUEST, param);
	}
#endif

done:
	g_variant_unref(reply);
	g_variant_unref(reply_temp);
	__bt_agent_release_memory();
	BT_DBG("-");

	return TRUE;
}

static gboolean __pairing_cancel_request(GapAgentPrivate *agent, const char *address)
{
	BT_DBG("On Going Pairing is cancelled by remote\n");

	syspopup_destroy_all();

	__bt_agent_release_memory();

	return TRUE;
}

static gboolean __a2dp_authorize_request_check(void)
{
	/* Check for existing Media device to disconnect */
	return _bt_is_headset_type_connected(BT_AUDIO_A2DP, NULL);
}

static gboolean __authorize_request(GapAgentPrivate *agent, GDBusProxy *device,
							const char *uuid)
{
	const gchar *address;
	const gchar *name;
	gboolean trust;
	gboolean paired;
	GVariant *reply = NULL;
	GVariant *reply_temp = NULL;
	GVariant *tmp_value;
#ifdef TIZEN_NETWORK_TETHERING_ENABLE
	bool enabled;
	tethering_h tethering = NULL;
#endif
	int result = BLUETOOTH_ERROR_NONE;
	int request_type = BT_AGENT_EVENT_AUTHORIZE_REQUEST;

	BT_DBG("+");

	/* Check if already Media connection exsist */
	if (!strcasecmp(uuid, A2DP_UUID)) {
		gboolean ret = FALSE;

		ret = __a2dp_authorize_request_check();

		if (ret) {
			BT_ERR("Already one A2DP device connected \n");
			gap_agent_reply_authorize(agent, GAP_AGENT_REJECT,
					      NULL);
			goto done;
		}
	}
	/* Check completed */

	if (!strcasecmp(uuid, HFP_AUDIO_GATEWAY_UUID) ||
	     !strcasecmp(uuid, HSP_AUDIO_GATEWAY_UUID) ||
	     !strcasecmp(uuid, HFP_HS_UUID) ||
	     !strcasecmp(uuid, HSP_HS_UUID) ||
	     !strcasecmp(uuid, A2DP_UUID) ||
	     !strcasecmp(uuid, HID_UUID) ||
	     !strcasecmp(uuid, SAP_UUID_OLD) ||
	     !strcasecmp(uuid, SAP_UUID_NEW) ||
	     !strcasecmp(uuid, AVRCP_TARGET_UUID)) {
		BT_DBG("Auto accept authorization for audio device (HFP, A2DP, AVRCP) [%s]", uuid);
		gap_agent_reply_authorize(agent, GAP_AGENT_ACCEPT,
					      NULL);

		goto done;
	}

	if (!strcasecmp(uuid, NAP_UUID) ||
	     !strcasecmp(uuid, GN_UUID) ||
	      !strcasecmp(uuid, BNEP_UUID)) {

		BT_DBG("Network connection request: %s", uuid);
#ifdef TIZEN_NETWORK_TETHERING_ENABLE
		if (nap_connected_device_count >=
					BT_PAN_MAX_CONNECTION) {
			BT_ERR("Max connection exceeded");
			goto fail;
		}
		int ret;
		ret = tethering_create(&tethering);

		if (ret != TETHERING_ERROR_NONE) {
			BT_ERR("Fail to create tethering: %d", ret);
			goto fail;
		}

		enabled = tethering_is_enabled(tethering, TETHERING_TYPE_BT);

		ret = tethering_destroy(tethering);

		if (ret != TETHERING_ERROR_NONE) {
			BT_ERR("Fail to create tethering: %d", ret);
		}

		if (enabled != true) {
			BT_ERR("BT tethering is not enabled");
			goto fail;
		}
#endif

		gap_agent_reply_authorize(agent, GAP_AGENT_ACCEPT,
					      NULL);
		goto done;
#ifdef TIZEN_NETWORK_TETHERING_ENABLE
fail:
		gap_agent_reply_authorize(agent, GAP_AGENT_REJECT,
		      NULL);

		goto done;
#endif
	}

	reply_temp = __bt_service_getall(device, BT_DEVICE_INTERFACE);
	if (reply_temp == NULL) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	g_variant_get(reply_temp,"(@a{sv})", &reply); /* Format of reply a{sv}*/

	tmp_value = g_variant_lookup_value (reply, "Address", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &address);
	G_VARIANT_UNREF(tmp_value);
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	tmp_value = g_variant_lookup_value(reply, "Alias", G_VARIANT_TYPE_STRING);
	g_variant_get(tmp_value, "s", &name);
	G_VARIANT_UNREF(tmp_value);
	if (!name)
		name = address;

	tmp_value = g_variant_lookup_value(reply, "Trusted", G_VARIANT_TYPE_BOOLEAN);
	g_variant_get(tmp_value, "b", &trust);
	G_VARIANT_UNREF(tmp_value);

	tmp_value = g_variant_lookup_value(reply, "Paired", G_VARIANT_TYPE_BOOLEAN);
	g_variant_get(tmp_value, "b", &paired);
	G_VARIANT_UNREF(tmp_value);
	if ((paired == FALSE) && (trust == FALSE)) {
		BT_ERR("No paired & No trusted device");
		gap_agent_reply_authorize(agent,
				GAP_AGENT_REJECT, NULL);
		goto done;
	}

	BT_INFO("Authorization request for device [%s] Service:[%s]\n", address, uuid);

	if (strcasecmp(uuid, OPP_UUID) == 0 &&
	     _gap_agent_exist_osp_server(agent, BT_OBEX_SERVER,
					NULL) == TRUE) {
		_bt_send_event(BT_OPP_SERVER_EVENT,
				BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
				g_variant_new("(iss)", result, address, name));

		goto done;
	}

	if (_gap_agent_exist_osp_server(agent, BT_RFCOMM_SERVER,
					(char *)uuid) == TRUE) {
		bt_agent_osp_server_t *osp_serv;
		osp_serv = _gap_agent_get_osp_server(agent,
						BT_RFCOMM_SERVER, (char *)uuid);

		if (osp_serv) {
			_bt_send_event(BT_RFCOMM_SERVER_EVENT,
				BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,
				g_variant_new("(issssn)", result, address, uuid,
						name, osp_serv->path, osp_serv->fd));
		}

		goto done;
	}

	if (!strcasecmp(uuid, OPP_UUID))
		request_type = BT_AGENT_EVENT_EXCHANGE_REQUEST;
	else if (!strcasecmp(uuid, PBAP_UUID))
		request_type = BT_AGENT_EVENT_PBAP_REQUEST;
	else if (!strcasecmp(uuid, MAP_UUID))
		request_type = BT_AGENT_EVENT_MAP_REQUEST;

	if (trust) {
		BT_INFO("Trusted device, so authorize\n");
		gap_agent_reply_authorize(agent,
					      GAP_AGENT_ACCEPT, NULL);
	} else {
		_bt_launch_system_popup(request_type, name, NULL, NULL,
						_gap_agent_get_path(agent));
	}

done:
	g_variant_unref(reply);
	g_variant_unref(reply_temp);
	__bt_agent_release_memory();
	BT_DBG("-");

	return TRUE;
}

static gboolean __authorization_cancel_request(GapAgentPrivate *agent,
							const char *address)
{
	BT_DBG("On Going Authorization is cancelled by remote\n");

	gap_agent_reply_authorize(agent, GAP_AGENT_CANCEL, NULL);

	syspopup_destroy_all();

	__bt_agent_release_memory();

	return TRUE;
}

void _bt_destroy_agent(void *agent)
{
	if (!agent)
		return;

	_gap_agent_reset_dbus((GapAgentPrivate *)agent);

	g_free(agent);
}

void* _bt_create_agent(const char *path, gboolean adapter)
{
	GAP_AGENT_FUNC_CB func_cb;
	GDBusProxy *adapter_proxy;
	GapAgentPrivate *agent;

	adapter_proxy = _bt_get_adapter_proxy();
	if (!adapter_proxy)
		return NULL;

	func_cb.pincode_func = __pincode_request;
	func_cb.display_func = __display_request;
	func_cb.passkey_func = __passkey_request;
	func_cb.confirm_func = __confirm_request;
	func_cb.authorize_func = __authorize_request;
	func_cb.pairing_cancel_func = __pairing_cancel_request;
	func_cb.authorization_cancel_func = __authorization_cancel_request;

	/* Allocate memory*/
	agent = g_new0(GapAgentPrivate, 1);

	_gap_agent_setup_dbus(agent, &func_cb, path, adapter_proxy);

	if (adapter) {
		if (!_gap_agent_register(agent)) {
			_bt_destroy_agent(agent);
			agent = NULL;
		}
	}

	return agent;
}

gboolean _bt_agent_register_osp_server(const gint type,
		const char *uuid, char *path, int fd)
{
	void *agent = _bt_get_adapter_agent();
	if (!agent)
		return FALSE;

	return _gap_agent_register_osp_server(agent, type, uuid, path, fd);

}

gboolean _bt_agent_unregister_osp_server(const gint type, const char *uuid)
{
	void *agent = _bt_get_adapter_agent();

	if (!agent)
		return FALSE;

	return _gap_agent_unregister_osp_server(agent, type, uuid);
}

gboolean _bt_agent_reply_authorize(gboolean accept)
{
	guint accept_val;

	void *agent = _bt_get_adapter_agent();
	if (!agent)
		return FALSE;

	accept_val = accept ? GAP_AGENT_ACCEPT : GAP_AGENT_REJECT;

	return gap_agent_reply_authorize(agent, accept_val, NULL);
}

gboolean _bt_agent_is_canceled(void)
{
	void *agent = _bt_get_adapter_agent();
	if (!agent)
		return FALSE;

	return _gap_agent_is_canceled(agent);
}

void _bt_agent_set_canceled(gboolean value)
{
	void *agent = _bt_get_adapter_agent();
	if (!agent)
		return;

	return _gap_agent_set_canceled(agent, value);
}

int _bt_agent_reply_cancellation(void)
{
	void *agent = _bt_get_adapter_agent();

	if (!agent)
		return BLUETOOTH_ERROR_INTERNAL;

	if (gap_agent_reply_confirmation(agent, GAP_AGENT_CANCEL, NULL) != TRUE) {
		BT_ERR("Fail to reply agent");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

static gboolean __bt_agent_is_hid_keyboard(uint32_t dev_class)
{
	switch ((dev_class & 0x1f00) >> 8) {
	case 0x05:
		switch ((dev_class & 0xc0) >> 6) {
		case 0x01:
			/* input-keyboard" */
			return TRUE;
		}
		break;
	}

	return FALSE;
}

static gboolean __bt_agent_find_device_by_address_exactname(char *buffer,
							const char *address)
{
	char *pch;
	char *last;

	pch = strtok_r(buffer, "= ,", &last);

	if (pch == NULL)
		return FALSE;

	while ((pch = strtok_r(NULL, ",", &last))) {
		if (0 == g_strcmp0(pch, address)) {
			BT_DBG("Match found\n");
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean __bt_agent_find_device_by_partial_name(char *buffer,
						const char *partial_name)
{
	char *pch;
	char *last;

	pch = strtok_r(buffer, "= ,", &last);

	if (pch == NULL)
		return FALSE;

	while ((pch = strtok_r(NULL, ",", &last))) {
		if (g_str_has_prefix(partial_name, pch)) {
			BT_DBG("Match found\n");
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean __bt_agent_is_device_blacklist(const char *address,
							const char *name)
{
	char *buffer;
	char **lines;
	int i;
	FILE *fp;
	long size;
	size_t result;

	BT_DBG("+");

	fp = fopen(BT_AGENT_AUTO_PAIR_BLACKLIST_FILE, "r");

	if (fp == NULL) {
		BT_ERR("Unable to open blacklist file");
		return FALSE;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	if (size <= 0) {
		BT_DBG("size is not a positive number");
		fclose(fp);
		return FALSE;
	}

	rewind(fp);

	buffer = g_malloc0(sizeof(char) * size);
	/* Fix : NULL_RETURNS */
	if (buffer == NULL) {
		BT_ERR("Fail to allocate memory");
		fclose(fp);
		return FALSE;
	}
	result = fread((char *)buffer, 1, size, fp);
	fclose(fp);
	if (result != size) {
		BT_ERR("Read Error");
		g_free(buffer);
		return FALSE;
	}

	BT_DBG("Buffer = %s", buffer);

	lines = g_strsplit_set(buffer, BT_AGENT_NEW_LINE, 0);
	g_free(buffer);

	if (lines == NULL) {
		BT_ERR("No lines in the file");
		return FALSE;
	}

	for (i = 0; lines[i] != NULL; i++) {
		if (g_str_has_prefix(lines[i], "AddressBlacklist"))
			if (__bt_agent_find_device_by_address_exactname(
						lines[i], address))
				goto done;
		if (g_str_has_prefix(lines[i], "ExactNameBlacklist"))
			if (__bt_agent_find_device_by_address_exactname(
						lines[i], name))
				goto done;
		if (g_str_has_prefix(lines[i], "PartialNameBlacklist"))
			if (__bt_agent_find_device_by_partial_name(lines[i],
								name))
				goto done;
		if (g_str_has_prefix(lines[i], "KeyboardAutoPair"))
			if (__bt_agent_find_device_by_address_exactname(
						lines[i], address))
				goto done;
	}
	g_strfreev(lines);
	BT_DBG("-");
	return FALSE;
done:
	BT_DBG("Found the device");
	g_strfreev(lines);
	return TRUE;
}

static gboolean __bt_agent_is_auto_response(uint32_t dev_class,
				const gchar *address, const gchar *name)
{
	gboolean is_headset = FALSE;
	gboolean is_mouse = FALSE;
	char lap_address[BT_LOWER_ADDRESS_LENGTH];

	BT_DBG("bt_agent_is_headset_class, %d +", dev_class);

	if (address == NULL)
		return FALSE;

	switch ((dev_class & 0x1f00) >> 8) {
	case 0x04:
		switch ((dev_class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			/* Headset */
			is_headset = TRUE;
			break;
		case 0x06:
			/* Headphone */
			is_headset = TRUE;
			break;
		case 0x0b:	/* VCR */
		case 0x0c:	/* Video Camera */
		case 0x0d:	/* Camcorder */
			break;
		default:
			/* Other audio device */
			is_headset = TRUE;
			break;
		}
		break;
	case 0x05:
		switch (dev_class & 0xff) {
		case 0x80:  /* 0x80: Pointing device(Mouse) */
			is_mouse = TRUE;
			break;

		case 0x40: /* 0x40: input device (BT keyboard) */

			/* Get the LAP(Lower Address part) */
			g_strlcpy(lap_address, address, sizeof(lap_address));

			/* Need to Auto pair the blacklisted Keyboard */
			if (__bt_agent_is_device_blacklist(lap_address, name) != TRUE) {
				BT_DBG("Device is not black listed\n");
				return FALSE;
			} else {
				BT_ERR("Device is black listed\n");
				return TRUE;
			}
		}
	}

	if ((!is_headset) && (!is_mouse))
		return FALSE;

	/* Get the LAP(Lower Address part) */
	g_strlcpy(lap_address, address, sizeof(lap_address));

	BT_DBG("Device address = %s\n", address);
	BT_DBG("Address 3 byte = %s\n", lap_address);

	if (__bt_agent_is_device_blacklist(lap_address, name)) {
		BT_ERR("Device is black listed\n");
		return FALSE;
	}

	return TRUE;
}

static int __bt_agent_generate_passkey(char *passkey, int size)
{
	int i;
	ssize_t len;
	int random_fd;
	unsigned int value = 0;

	if (passkey == NULL)
		return -1;

	if (size <= 0)
		return -1;

	random_fd = open("/dev/urandom", O_RDONLY);

	if (random_fd < 0)
		return -1;

	for (i = 0; i < size; i++) {
		len = read(random_fd, &value, sizeof(value));
		if (len > 0)
			passkey[i] = '0' + (value % 10);
	}

	close(random_fd);

	BT_DBG("passkey: %s", passkey);

	return 0;
}
