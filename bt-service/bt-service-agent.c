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

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stacktrim.h>
#include <syspopup_caller.h>
#include <vconf.h>
#include <tethering.h>

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

#define BT_AGENT_OBJECT "/org/bluez/agent/frwk_agent"
#define BT_AGENT_INTERFACE "org.bluez.Agent"
#define BT_AGENT_SIGNAL_RFCOMM_AUTHORIZE "RfcommAuthorize"
#define BT_AGENT_SIGNAL_OBEX_AUTHORIZE "ObexAuthorize"

#define BT_PIN_MAX_LENGTH 16
#define BT_PASSKEY_MAX_LENGTH 4

#define BT_AGENT_SYSPOPUP_TIMEOUT_FOR_MULTIPLE_POPUPS 200
#define BT_AGENT_SYSPOPUP_MAX_ATTEMPT 3

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

static int __launch_system_popup(bt_agent_event_type_t event_type,
							const char *device_name,
							char *passkey,
							const char *filename,
							const char *agent_path)
{
	int ret;
	bundle *b;
	char event_str[BT_MAX_EVENT_STR_LENGTH + 1];

	BT_DBG("_bt_agent_launch_system_popup +");

	b = bundle_create();
	if (!b)
		return -1;

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

	default:
		bundle_free(b);
		return -1;

	}

	bundle_add(b, "event-type", event_str);

	ret = syspopup_launch("bt-syspopup", b);
	if (0 > ret) {
		BT_DBG("Popup launch failed...retry %d\n", ret);

		g_timeout_add(BT_AGENT_SYSPOPUP_TIMEOUT_FOR_MULTIPLE_POPUPS,
			      (GSourceFunc)__bt_agent_system_popup_timer_cb, b);
	} else {
		bundle_free(b);
	}

	BT_DBG("_bt_agent_launch_system_popup -%d", ret);
	return 0;
}

static gboolean __pincode_request(GapAgent *agent, DBusGProxy *device)
{
	uint32_t device_class;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address;
	const gchar *name;
	GError *error = NULL;

	BT_DBG("+\n");

	dbus_g_proxy_call(device, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable",
						G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_DBG("error in GetBasicProperties [%s]\n", error->message);
		g_error_free(error);
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Class");
	device_class = value ? g_value_get_uint(value) : 0;

	value = g_hash_table_lookup(hash, "Address");
	address = value ? g_value_get_string(value) : NULL;
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Name");
	name = value ? g_value_get_string(value) : NULL;
	if (!name)
		name = address;

	if (_bt_is_device_creating() == TRUE &&
		_bt_is_bonding_device_address(address) == TRUE &&
		__bt_agent_is_auto_response(device_class, address, name)) {
		/* Use Fixed PIN "0000" for basic pairing*/
		_bt_set_autopair_status_in_bonding_info(TRUE);
		gap_agent_reply_pin_code(agent, GAP_AGENT_ACCEPT, "0000",
									NULL);
	} else if (__bt_agent_is_hid_keyboard(device_class)) {
		char str_passkey[BT_PASSKEY_MAX_LENGTH + 1] = { 0 };

		if (__bt_agent_generate_passkey(str_passkey,
					BT_PASSKEY_MAX_LENGTH) != 0) {
			gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT,
						"", NULL);
			goto done;
		}

		gap_agent_reply_pin_code(agent, GAP_AGENT_ACCEPT,
							str_passkey, NULL);

		__launch_system_popup(BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST,
						name, str_passkey, NULL,
						_gap_agent_get_path(agent));
	} else {
		__launch_system_popup(BT_AGENT_EVENT_PIN_REQUEST, name, NULL,
					NULL, _gap_agent_get_path(agent));
	}

done:
	g_hash_table_destroy(hash);
	__bt_agent_release_memory();

	BT_DBG("-\n");

	return TRUE;
}

static gboolean __passkey_request(GapAgent *agent, DBusGProxy *device)
{
	uint32_t device_class;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address;
	const gchar *name;
	GError *error = NULL;

	BT_DBG("+\n");

	dbus_g_proxy_call(device, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable",
						G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_DBG("error in GetBasicProperties [%s]\n", error->message);
		g_error_free(error);
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Class");
	device_class = value ? g_value_get_uint(value) : 0;

	value = g_hash_table_lookup(hash, "Address");
	address = value ? g_value_get_string(value) : NULL;
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Name");
	name = value ? g_value_get_string(value) : NULL;
	if (!name)
		name = address;

	__launch_system_popup(BT_AGENT_EVENT_PASSKEY_REQUEST, name, NULL, NULL,
						_gap_agent_get_path(agent));

done:
	__bt_agent_release_memory();
	g_hash_table_destroy(hash);
	BT_DBG("-\n");

	return TRUE;
}

static gboolean __display_request(GapAgent *agent, DBusGProxy *device,
								guint passkey)
{
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address;
	const gchar *name;
	GError *error = NULL;
	char *str_passkey;

	BT_DBG("+\n");

	dbus_g_proxy_call(device, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
								 G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_DBG("error in GetBasicProperties [%s]\n", error->message);
		g_error_free(error);
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Address");
	address = value ? g_value_get_string(value) : NULL;
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Name");
	name = value ? g_value_get_string(value) : NULL;
	if (!name)
		name = address;

	str_passkey = g_strdup_printf("%d", passkey);

	__launch_system_popup(BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST, name,
						str_passkey, NULL,
						_gap_agent_get_path(agent));

	g_free(str_passkey);

done:
	__bt_agent_release_memory();
	g_hash_table_destroy(hash);
	BT_DBG("-\n");

	return TRUE;
}

static gboolean __confirm_request(GapAgent *agent, DBusGProxy *device,
								guint passkey)
{
	uint32_t device_class;
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address;
	const gchar *name;
	GError *error = NULL;
	char str_passkey[7];

	BT_DBG("+ passkey[%.6d]\n", passkey);

	snprintf(str_passkey, sizeof(str_passkey), "%.6d", passkey);

	dbus_g_proxy_call(device, "GetProperties", &error,
				G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
								 G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_DBG("error in GetBasicProperties [%s]\n", error->message);
		g_error_free(error);
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Class");
	device_class = value ? g_value_get_uint(value) : 0;

	value = g_hash_table_lookup(hash, "Address");
	address = value ? g_value_get_string(value) : NULL;
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Name");
	name = value ? g_value_get_string(value) : NULL;
	if (!name)
		name = address;

	__launch_system_popup(BT_AGENT_EVENT_PASSKEY_CONFIRM_REQUEST, name,
						str_passkey, NULL,
						_gap_agent_get_path(agent));

done:
	__bt_agent_release_memory();
	g_hash_table_destroy(hash);

	BT_DBG("-\n");

	return TRUE;
}

static gboolean __pairing_cancel_request(GapAgent *agent, const char *address)
{
	BT_DBG("On Going Pairing is cancelled by remote\n");

	gap_agent_reply_pin_code(agent, GAP_AGENT_CANCEL, "", NULL);

	syspopup_destroy_all();

	__bt_agent_release_memory();

	return TRUE;
}

static gboolean __a2dp_authorize_request_check(void)
{
	/* Check for existing Media device to disconnect */
	return _bt_is_headset_type_connected(BT_AUDIO_A2DP, NULL);
}

static gboolean __authorize_request(GapAgent *agent, DBusGProxy *device,
							const char *uuid)
{
	GHashTable *hash = NULL;
	GValue *value;
	const gchar *address;
	const gchar *name;
	bool enabled;
	gboolean trust;
	gboolean paired;
	tethering_h tethering = NULL;
	GError *error = NULL;
	int ret;
	int result = BLUETOOTH_ERROR_NONE;
	int request_type = BT_AGENT_EVENT_AUTHORIZE_REQUEST;

	BT_DBG("+\n");

	BT_DBG("+\n");

	/* Check if already Media connection exsist */
	if (!strcasecmp(uuid, A2DP_UUID)) {
		gboolean ret = FALSE;

		ret = __a2dp_authorize_request_check();

		if (ret) {
			BT_DBG("Already one A2DP device connected \n");
			gap_agent_reply_authorize(agent, GAP_AGENT_REJECT,
					      NULL);
			goto done;
		}
	}
	/* Check completed */

	if (!strcasecmp(uuid, HFP_AUDIO_GATEWAY_UUID) ||
	     !strcasecmp(uuid, HSP_AUDIO_GATEWAY_UUID) ||
	     !strcasecmp(uuid, A2DP_UUID) ||
	     !strcasecmp(uuid, HID_UUID) ||
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

		gap_agent_reply_authorize(agent, GAP_AGENT_ACCEPT,
					      NULL);
		goto done;
fail:
		gap_agent_reply_authorize(agent, GAP_AGENT_REJECT,
		      NULL);

		goto done;
	}


	dbus_g_proxy_call(device, "GetProperties", &error, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
								 G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_DBG("error in GetBasicProperties [%s]\n", error->message);
		g_error_free(error);
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "",
					     NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Address");
	address = value ? g_value_get_string(value) : NULL;
	if (!address) {
		gap_agent_reply_pin_code(agent, GAP_AGENT_REJECT, "", NULL);
		goto done;
	}

	value = g_hash_table_lookup(hash, "Alias");
	name = value ? g_value_get_string(value) : NULL;
	if (!name)
		name = address;

	value = g_hash_table_lookup(hash, "Trusted");
	trust = value ? g_value_get_boolean(value) : 0;

	value = g_hash_table_lookup(hash, "Paired");
	paired = value ? g_value_get_boolean(value) : 0;
	if ((paired == FALSE) && (trust == FALSE)) {
		BT_DBG("No paired & No trusted device");
		gap_agent_reply_authorize(agent,
					      GAP_AGENT_REJECT, NULL);
		goto done;
	}

	BT_DBG("Authorization request for device [%s] Service:[%s]\n", address,
									uuid);

	if (strcasecmp(uuid, OPP_UUID) == 0 &&
	     _gap_agent_exist_osp_server(agent, BT_OBEX_SERVER,
					NULL) == TRUE) {
		_bt_send_event(BT_OPP_SERVER_EVENT,
			BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INVALID);

		goto done;
	}

	if (_gap_agent_exist_osp_server(agent, BT_RFCOMM_SERVER,
					(char *)uuid) == TRUE) {
		bt_rfcomm_server_info_t *server_info;

		server_info = _bt_rfcomm_get_server_info_using_uuid((char *)uuid);
		retv_if(server_info == NULL, TRUE);
		retv_if(server_info->server_type != BT_CUSTOM_SERVER, TRUE);

		_bt_send_event(BT_RFCOMM_SERVER_EVENT,
			BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INT16, &server_info->control_fd,
			DBUS_TYPE_INVALID);

		goto done;
	}

	if (!strcasecmp(uuid, OPP_UUID))
		request_type = BT_AGENT_EVENT_EXCHANGE_REQUEST;
	else if (!strcasecmp(uuid, PBAP_UUID))
		request_type = BT_AGENT_EVENT_PBAP_REQUEST;
	else if (!strcasecmp(uuid, MAP_UUID))
		request_type = BT_AGENT_EVENT_MAP_REQUEST;

	if (trust) {
		BT_DBG("Trusted device, so authorize\n");
		gap_agent_reply_authorize(agent,
					      GAP_AGENT_ACCEPT, NULL);
	} else {
		__launch_system_popup(request_type, name, NULL, NULL,
						_gap_agent_get_path(agent));
	}

done:
	__bt_agent_release_memory();
	g_hash_table_destroy(hash);

	BT_DBG("-\n");

	return TRUE;
}

static gboolean __authorization_cancel_request(GapAgent *agent,
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

	_gap_agent_reset_dbus(agent);

	g_object_unref(agent);
}

void* _bt_create_agent(const char *path, gboolean adapter)
{
	GAP_AGENT_FUNC_CB func_cb;
	DBusGProxy *adapter_proxy;
	GapAgent* agent;

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

	agent = _gap_agent_new();

	_gap_agent_setup_dbus(agent, &func_cb, path, adapter_proxy);

	if (adapter) {
		if (!_gap_agent_register(agent)) {
			_bt_destroy_agent(agent);
			return NULL;
		}
	}

	return agent;
}

gboolean _bt_agent_register_osp_server(const gint type, const char *uuid)
{
	void *agent = _bt_get_adapter_agent();
	if (!agent)
		return FALSE;

	return _gap_agent_register_osp_server(agent, type, uuid);

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

gboolean _bt_agent_is_canceled(void *agent)
{
	return _gap_agent_is_canceled(agent);
}

void _bt_agent_set_canceled(void *agent, gboolean value)
{
	return _gap_agent_set_canceled(agent, value);
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

	BT_DBG("+ \n");

	fp = fopen(BT_AGENT_AUTO_PAIR_BLACKLIST_FILE, "r");

	if (fp == NULL) {
		BT_DBG("fopen failed \n");
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

	if (lines == NULL) {
		BT_DBG("No lines in the file \n");
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
	}
	g_strfreev(lines);
	BT_DBG("- \n");
	return FALSE;
done:
	BT_DBG("Found the device\n");
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
		if ((dev_class & 0xff) == 0x80)  /* 0x80: Pointing device(Mouse) */
			is_mouse = TRUE;
		break;
	}

	if ((!is_headset) && (!is_mouse))
		return FALSE;

	/* Get the LAP(Lower Address part) */
	g_strlcpy(lap_address, address, sizeof(lap_address));

	BT_DBG("Device address = %s\n", address);
	BT_DBG("Address 3 byte = %s\n", lap_address);

	if (__bt_agent_is_device_blacklist(lap_address, name)) {
		BT_DBG("Device is black listed\n");
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
		passkey[i] = '0' + (value % 10);
	}

	close(random_fd);

	BT_DBG("passkey: %s", passkey);

	return 0;
}
