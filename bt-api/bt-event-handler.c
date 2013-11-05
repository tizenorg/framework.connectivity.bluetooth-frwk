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

#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <vconf.h>

#include "bluetooth-api.h"
#include "bluetooth-audio-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-event-handler.h"
#include "bt-request-sender.h"

#define BT_RELIABLE_DISABLE_TIME 500 /* 500 ms */

typedef struct {
	int server_fd;
} bt_server_info_t;

typedef struct {
	int request_id;
} bt_sending_info_t;

static int obex_server_id;
static guint disable_timer_id;
static gboolean is_initialized;
static GSList *sending_list = NULL;
static GSList *server_list = NULL;
static GSList *event_list = NULL;
static int owner_sig_id = -1;

void _bt_add_push_request_id(int request_id)
{
	bt_sending_info_t *info;

	info = g_new0(bt_sending_info_t, 1);
	info->request_id = request_id;

	sending_list = g_slist_append(sending_list, info);
}

static gboolean __bt_is_request_id_exist(int request_id)
{
	GSList *l;
	bt_sending_info_t *info;

	for (l = sending_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->request_id == request_id)
			return TRUE;
	}

	return FALSE;
}

static void __bt_remove_push_request_id(int request_id)
{
	GSList *l;
	bt_sending_info_t *info;

	for (l = sending_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		BT_DBG("info->request_id = %d\n", info->request_id);
		BT_DBG("request_id = %d\n", request_id);
		if (info->request_id == request_id) {
			sending_list = g_slist_remove(sending_list, (void *)info);
			g_free(info);
			break;
		}
	}
}

static void __bt_remove_all_push_request_id(void)
{
	GSList *l;
	bt_sending_info_t *info;

	for (l = sending_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		g_free(info);
	}

	g_slist_free(sending_list);
	sending_list = NULL;
}

static void __bt_remove_all_server(void)
{
	GSList *l;
	bt_server_info_t *info;

	for (l = server_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		g_free(info);
	}

	g_slist_free(server_list);
	server_list = NULL;
}

static gboolean __bt_is_server_exist(int server_fd)
{
	GSList *l;
	bt_server_info_t *info;

	for (l = server_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		retv_if(info->server_fd == server_fd, TRUE);
	}

	return FALSE;
}

static void __bt_get_uuid_info(bluetooth_device_info_t *dev_info,
				char **uuids,
				int uuid_count)
{
	int i;
	char **parts;

	ret_if(dev_info == NULL);
	ret_if(uuids == NULL);
	ret_if(uuid_count <= 0);

	dev_info->service_index = uuid_count;

	for (i = 0; uuids[i] != NULL && i < uuid_count; i++) {
		g_strlcpy(dev_info->uuids[i], uuids[i], BLUETOOTH_UUID_STRING_MAX);

		parts = g_strsplit(uuids[i], "-", -1);

		if (parts == NULL || parts[0] == NULL) {
			g_strfreev(parts);
			continue;
		}

		dev_info->service_list_array[i] = g_ascii_strtoull(parts[0], NULL, 16);
		g_strfreev(parts);
	}
}
#ifdef __ENABLE_GDBUS__
static bluetooth_device_info_t *__bt_get_device_info_in_message(GVariant *parameters, int *ret)
{
	bluetooth_device_info_t *dev_info;
	const char *address = NULL;
	const char *name = NULL;
	gchar **uuids = NULL;
	unsigned int dev_class = 0;
	int rssi = 0;
	gboolean paired = FALSE;
	gboolean connected = FALSE;
	gboolean trust = FALSE;
	gsize uuid_count;
	int result = BLUETOOTH_ERROR_NONE;
	GVariant *string_var;
	unsigned char device_type;

	g_variant_get(parameters, "(i&sun&sbbby@as)", &result, &address,
			&dev_class, &rssi, &name, &paired,
			&connected, &trust, &device_type, &string_var);

	if (string_var == NULL) {
		BT_ERR("invalid parameters in signal");
		return NULL;
	}

	uuids = (gchar **)g_variant_get_strv(string_var, &uuid_count);
	dev_info = g_malloc0(sizeof(bluetooth_device_info_t));

	dev_info->rssi = rssi;
	dev_info->paired = paired;
	dev_info->connected = connected;
	dev_info->trust = trust;
	dev_info->device_type = device_type;

	g_strlcpy(dev_info->device_name.name, name,
		BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1);

	_bt_divide_device_class(&dev_info->device_class, dev_class);

	_bt_convert_addr_string_to_type(dev_info->device_address.addr,
					address);

	if (uuid_count > 0)
		__bt_get_uuid_info(dev_info, uuids, uuid_count);

	*ret = result;
	g_free(uuids);
	g_variant_unref(string_var);
	return dev_info;
}
#else
static bluetooth_device_info_t *__bt_get_device_info_in_message(DBusMessage *msg, int *ret)
{
	bluetooth_device_info_t *dev_info;
	char *address = NULL;
	char *name = NULL;
	char **uuids = NULL;
	unsigned int class = 0;
	int rssi = 0;
	gboolean paired = FALSE;
	gboolean connected = FALSE;
	gboolean trust = FALSE;
	int uuid_count = 0;
	int result = BLUETOOTH_ERROR_NONE;
	unsigned char device_type;

	if (!dbus_message_get_args(msg, NULL,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &address,
		DBUS_TYPE_UINT32, &class,
		DBUS_TYPE_INT16, &rssi,
		DBUS_TYPE_STRING, &name,
		DBUS_TYPE_BOOLEAN, &paired,
		DBUS_TYPE_BOOLEAN, &connected,
		DBUS_TYPE_BOOLEAN, &trust,
		DBUS_TYPE_BYTE, &device_type,
		DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
		&uuids, &uuid_count,
		DBUS_TYPE_INVALID)) {
		BT_ERR("Unexpected parameters in signal");
		return NULL;
	}

	dev_info = g_malloc0(sizeof(bluetooth_device_info_t));

	dev_info->rssi = rssi;
	dev_info->paired = paired;
	dev_info->connected = connected;
	dev_info->trust = trust;
	dev_info->device_type = device_type;

	g_strlcpy(dev_info->device_name.name, name, BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1);

	_bt_divide_device_class(&dev_info->device_class, class);

	_bt_convert_addr_string_to_type(dev_info->device_address.addr,
					address);

	if (uuid_count > 0)
		__bt_get_uuid_info(dev_info, uuids, uuid_count);

	*ret = result;

	return dev_info;
}
#endif

gboolean __bt_reliable_disable_cb(gpointer user_data)
{
	bt_event_info_t *event_info = user_data;

	if (is_initialized != FALSE) {
		_bt_common_event_cb(BLUETOOTH_EVENT_DISABLED,
				BLUETOOTH_ERROR_NONE, NULL,
				event_info->cb, event_info->user_data);
	}

	obex_server_id = BT_NO_SERVER;
	__bt_remove_all_server();
	__bt_remove_all_push_request_id();

	return FALSE;
}

#ifdef __ENABLE_GDBUS__
void __bt_adapter_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;

	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_ADAPTER_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	if (strcasecmp(signal_name, BT_ENABLED) == 0) {
		g_variant_get(parameters, "(i)", &result);
		if (result == BLUETOOTH_ERROR_NONE) {
			if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
				BT_ERR("Set vconf failed\n");

			if (vconf_set_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, 0) != 0)
				BT_ERR("Set vconf failed\n");

			if (vconf_set_int(BT_OFF_DUE_TO_TIMEOUT, 0) != 0)
				BT_ERR("Set vconf failed\n");
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_ENABLED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DISABLED) == 0) {
		int flight_mode_value = 0;
		int ps_mode_value = 0;

		if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
						&flight_mode_value) != 0)
			BT_ERR("Fail to get the flight_mode_deactivated value");

		if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
							&ps_mode_value) != 0)
			BT_ERR("Fail to get the ps_mode_deactivated value");

		if (flight_mode_value == 1 || ps_mode_value > 0) {
			BT_DBG("Flight mode deactivation");
			if (disable_timer_id > 0)
				g_source_remove(disable_timer_id);

			disable_timer_id = g_timeout_add(BT_RELIABLE_DISABLE_TIME,
					(GSourceFunc)__bt_reliable_disable_cb,
					event_info);
		}
	} else if (strcasecmp(signal_name, BT_DISCOVERABLE_MODE_CHANGED) == 0) {
		int mode = 0;

		g_variant_get(parameters, "(in)", &result, &mode);
		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
				result, &mode,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DISCOVERABLE_TIMEOUT_CHANGED) == 0) {
		int timeout = 0;

		g_variant_get(parameters, "(in)", &result, &timeout);
		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED,
				result, &timeout,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_ADAPTER_NAME_CHANGED) == 0) {
		char *adapter_name = NULL;

		g_variant_get(parameters, "(i&s)", &result, &adapter_name);
		_bt_common_event_cb(BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,
				result, adapter_name,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DISCOVERY_STARTED) == 0) {
		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERY_STARTED,
				BLUETOOTH_ERROR_NONE, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DISCOVERY_FINISHED) == 0) {
		g_variant_get(parameters, "(i)", &result);
		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERY_FINISHED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DEVICE_FOUND) == 0) {
		int event;
		bluetooth_device_info_t *device_info;

		device_info = __bt_get_device_info_in_message(parameters,
								&result);
		ret_if(device_info == NULL);

		if (strlen(device_info->device_name.name) > 0)
			event = BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED;
		else
			event = BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND;

		_bt_common_event_cb(event,
				result, device_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	} else if (strcasecmp(signal_name, BT_BOND_CREATED) == 0) {
		bluetooth_device_info_t *device_info;

		device_info = __bt_get_device_info_in_message(parameters,
								&result);
		ret_if(device_info == NULL);

		_bt_common_event_cb(BLUETOOTH_EVENT_BONDING_FINISHED,
				result, device_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	} else if (strcasecmp(signal_name, BT_BOND_DESTROYED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_SERVICE_SEARCHED) == 0) {
		bluetooth_device_info_t *device_info;
		bt_sdp_info_t sdp_info;

		device_info = __bt_get_device_info_in_message(parameters,
								&result);
		ret_if(device_info == NULL);

		memset(&sdp_info, 0x00, sizeof(bt_sdp_info_t));

		sdp_info.service_index = device_info->service_index;

		memcpy(&sdp_info.device_addr,
			&device_info->device_address,
			BLUETOOTH_ADDRESS_LENGTH);

		memcpy(sdp_info.service_list_array,
			device_info->service_list_array,
			BLUETOOTH_MAX_SERVICES_FOR_DEVICE);

		memcpy(sdp_info.uuids,
			device_info->uuids,
			BLUETOOTH_MAX_SERVICES_FOR_DEVICE * BLUETOOTH_UUID_STRING_MAX);

		_bt_common_event_cb(BLUETOOTH_EVENT_SERVICE_SEARCHED,
				result, &sdp_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	}
}

void __bt_device_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	short rssi;

	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_DEVICE_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_GATT_CONNECTED) == 0) {
		g_variant_get(parameters, "(i)", &result);
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_GATT_DISCONNECTED) == 0) {
		g_variant_get(parameters, "(i)", &result);
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_DISCONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_GATT_RSSI) == 0) {
                g_variant_get(parameters, "(in)", &result, &rssi);
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_RSSI,
				result, &rssi,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DEVICE_CONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DEVICE_DISCONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	}
}

void __bt_hid_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;

	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_HID_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_INPUT_CONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_input_event_cb(BLUETOOTH_HID_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_INPUT_DISCONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		BT_DBG("address: %s", address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_input_event_cb(BLUETOOTH_HID_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	}
}

void __bt_headset_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_HEADSET_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_STEREO_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_headset_event_cb(BLUETOOTH_EVENT_AV_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_STEREO_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_headset_event_cb(BLUETOOTH_EVENT_AV_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_SPEAKER_GAIN) == 0) {
		unsigned int gain;
		guint16 spkr_gain;
		char *address = NULL;

		g_variant_get(parameters, "(i&sq)", &result, &address,
								&spkr_gain);
		gain = (unsigned int)spkr_gain;

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_SPEAKER_GAIN,
				result, &gain,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MICROPHONE_GAIN) == 0) {
		unsigned int gain;
		guint16 mic_gain;
		char *address = NULL;

		g_variant_get(parameters, "(i&sq)", &result,
						&address, &mic_gain);
		gain = (unsigned int)mic_gain;

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_MIC_GAIN,
				result, &gain,
				event_info->cb, event_info->user_data);
	}
}

void __bt_network_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_NETWORK_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_NETWORK_CONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_NETWORK_DISCONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_NETWORK_SERVER_CONNECTED) == 0) {
		const char *device = NULL;
		const char *address = NULL;
		bluetooth_network_device_info_t network_info;

		g_variant_get(parameters, "(i&s&s)", &result,
							&device, &address);

		memset(&network_info, 0x00, sizeof(bluetooth_network_device_info_t));

		_bt_convert_addr_string_to_type(network_info.device_address.addr,
						address);

		_bt_print_device_address_t(&network_info.device_address);
		g_strlcpy(network_info.interface_name, device,
					sizeof(network_info.interface_name));

		DBG_SECURE("Interface: %s", network_info.interface_name);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED,
				result, &network_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_NETWORK_SERVER_DISCONNECTED) == 0) {
		const char *device = NULL;
		const char *address = NULL;
		bluetooth_network_device_info_t network_info;

		g_variant_get(parameters, "(i&s&s)", &result, &device, &address);

		memset(&network_info, 0x00, sizeof(bluetooth_network_device_info_t));

		_bt_convert_addr_string_to_type(network_info.device_address.addr,
						address);

		_bt_print_device_address_t(&network_info.device_address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED,
				result, &network_info,
				event_info->cb, event_info->user_data);
	}
}

void __bt_avrcp_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_AVRCP_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_STEREO_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_STEREO_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_SHUFFLE_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_EQUALIZER_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_REPEAT_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	}  else if (strcasecmp(signal_name, BT_MEDIA_SCAN_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	}
}

void __bt_opp_client_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_OPP_CLIENT_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_OPP_CONNECTED) == 0) {
		const char *address = NULL;
		int request_id = 0;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&si)", &result,
						&address, &request_id);

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);

		if (result != BLUETOOTH_ERROR_NONE) {
			__bt_remove_push_request_id(request_id);
		}
	} else if (strcasecmp(signal_name, BT_OPP_DISCONNECTED) == 0) {
		const char *address = NULL;
		int request_id = 0;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&si)", &result, &address,
							&request_id);

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);

		__bt_remove_push_request_id(request_id);
	} else if (strcasecmp(signal_name, BT_TRANSFER_STARTED) == 0) {
		const char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		bt_opc_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&sti)", &result, &file_name,
						&size, &request_id);

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	} else if (strcasecmp(signal_name, BT_TRANSFER_PROGRESS) == 0) {
		const char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		int progress = 0;
		bt_opc_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&stii)", &result,
			&file_name, &size, &progress, &request_id);

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;
		transfer_info.percentage = progress;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	} else if (strcasecmp(signal_name, BT_TRANSFER_COMPLETED) == 0) {
		const char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		bt_opc_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&sti)", &result,
					&file_name, &size, &request_id);

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	}
}

void __bt_opp_server_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_OPP_SERVER_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_TRANSFER_AUTHORIZED) == 0) {
		/* Native only event */
		const char *file_name = NULL;
		guint64 size = 0;
		bt_obex_server_authorize_into_t auth_info;

		g_variant_get(parameters, "(i&st)", &result, &file_name, &size);

		/* OSP server: Don't get this event */
		ret_if(obex_server_id == BT_CUSTOM_SERVER);

		memset(&auth_info, 0x00, sizeof(bt_obex_server_authorize_into_t));

		auth_info.filename = g_strdup(file_name);
		auth_info.length = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE,
				result, &auth_info,
				event_info->cb, event_info->user_data);

		g_free(auth_info.filename);
	} else if (strcasecmp(signal_name, BT_CONNECTION_AUTHORIZED) == 0) {
		/* OSP only event */
		const char *address = NULL;
		const char *name = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s&s)", &result, &address, &name);

		/* Native server: Don't get this event */
		ret_if(obex_server_id == BT_NATIVE_SERVER);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_TRANSFER_CONNECTED) == 0) {

		g_variant_get(parameters, "(i)", &result);

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED,
					result, NULL, event_info->cb,
					event_info->user_data);
	} else if (strcasecmp(signal_name, BT_TRANSFER_DISCONNECTED) == 0) {

		g_variant_get(parameters, "(i)", &result);

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_DISCONNECTED,
					result, NULL, event_info->cb,
					event_info->user_data);
	} else if (strcasecmp(signal_name, BT_TRANSFER_STARTED) == 0) {
		const char *file_name = NULL;
		const char *type = NULL;
		int transfer_id = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&s&stii)", &result, &file_name,
				&type, &size, &transfer_id, &server_type);

		/* Other server's event */
		ret_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
	} else if (strcasecmp(signal_name, BT_TRANSFER_PROGRESS) == 0) {
		const char *file_name = NULL;
		const char *type = NULL;
		int transfer_id = 0;
		int progress = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&s&stiii)", &result, &file_name,
						&type, &size, &transfer_id,
						&progress, &server_type);

		/* Other server's event */
		ret_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.percentage = progress;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
	} else if (strcasecmp(signal_name, BT_TRANSFER_COMPLETED) == 0) {
		const char *file_name = NULL;
		const char *device_name = NULL;
		const char *type = NULL;
		const char *file_path;
		int transfer_id = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		g_variant_get(parameters, "(i&s&s&s&stii)", &result, &file_name,
					&type, &device_name, &file_path, &size,
					&transfer_id, &server_type);

		/* Other server's event */
		ret_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.device_name = g_strdup(device_name);
		transfer_info.file_path = g_strdup(file_path);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
		g_free(transfer_info.device_name);
		g_free(transfer_info.file_path);
	}
}

void __bt_rfcomm_client_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_RFCOMM_CLIENT_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_RFCOMM_CONNECTED) == 0) {
		const char *address = NULL;
		const char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_connection_t conn_info;

		g_variant_get(parameters, "(i&s&sn)", &result, &address,
							&uuid, &socket_fd);

		memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
		conn_info.device_role = RFCOMM_ROLE_CLIENT;
		g_strlcpy(conn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		conn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RFCOMM_DISCONNECTED) == 0) {
		const char *address = NULL;
		const char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_disconnection_t disconn_info;

		g_variant_get(parameters, "(i&s&sn)", &result, &address,
								&uuid, &socket_fd);

		memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
		disconn_info.device_role = RFCOMM_ROLE_CLIENT;
		g_strlcpy(disconn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		disconn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
				result, &disconn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RFCOMM_DATA_RECEIVED) == 0) {
		char *buffer;
		int buffer_len = 0;
		int socket_fd = 0;
		bluetooth_rfcomm_received_data_t data_r;
		GVariant *byte_var;

		g_variant_get(parameters, "(in@ay)", &result, &socket_fd,
								&byte_var);

		buffer_len = g_variant_get_size( byte_var);
		buffer = (char *) g_variant_get_data(byte_var);

		data_r.socket_fd = socket_fd;
		data_r.buffer_size = buffer_len;
		data_r.buffer = buffer;

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
				result, &data_r,
				event_info->cb, event_info->user_data);
		g_variant_unref(byte_var);
	}
}

void __bt_rfcomm_server_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_RFCOMM_SERVER_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_RFCOMM_CONNECTED) == 0) {
		const char *address = NULL;
		const char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_connection_t conn_info;

		g_variant_get(parameters, "(i&s&sn)", &result, &address,
							&uuid, &socket_fd);

		memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
		conn_info.device_role = RFCOMM_ROLE_SERVER;
		g_strlcpy(conn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		conn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RFCOMM_DISCONNECTED) == 0) {
		const char *address = NULL;
		const char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_disconnection_t disconn_info;

		g_variant_get(parameters, "(i&s&sn)", &result, &address,
								&uuid, &socket_fd);

		memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
		disconn_info.device_role = RFCOMM_ROLE_SERVER;
		g_strlcpy(disconn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		disconn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
				result, &disconn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_CONNECTION_AUTHORIZED) == 0) {
		/* OSP only event */
		bluetooth_rfcomm_connection_request_t req_ind;
		const char *address = NULL;
		const char *uuid = NULL;
		const char *name = NULL;
		int socket_fd = 0;

		g_variant_get(parameters, "(i&s&s&sn)", &result, &address,
						&uuid, &name, &socket_fd);

		/* Don't send the authorized event to other server */
		ret_if(__bt_is_server_exist(socket_fd) == FALSE);

		memset(&req_ind, 0x00, sizeof(bluetooth_rfcomm_connection_request_t));
		_bt_convert_addr_string_to_type(req_ind.device_addr.addr,
						address);

		req_ind.socket_fd = socket_fd;

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,
				result, &req_ind,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RFCOMM_SERVER_REMOVED) == 0) {
		/* OSP only event */
		int socket_fd = 0;

		g_variant_get(parameters, "(in)", &result, &socket_fd);

		ret_if(__bt_is_server_exist(socket_fd) == FALSE);

		_bt_remove_server(socket_fd);
	} else if (strcasecmp(signal_name, BT_RFCOMM_DATA_RECEIVED) == 0) {
		char *buffer = NULL;
		int buffer_len = 0;
		int socket_fd = 0;
		bluetooth_rfcomm_received_data_t data_r;
		GVariant *byte_var;

		g_variant_get(parameters, "(in@ay)", &result,
						&socket_fd, &byte_var);

		buffer_len = g_variant_get_size( byte_var);
		buffer = (char *) g_variant_get_data(byte_var);

		data_r.socket_fd = socket_fd;
		data_r.buffer_size = buffer_len;
		data_r.buffer = buffer;

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
				result, &data_r,
				event_info->cb, event_info->user_data);
		g_variant_unref(byte_var);
	}
}
#else
static DBusHandlerResult __bt_adapter_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_ADAPTER_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_ENABLED) == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (result == BLUETOOTH_ERROR_NONE) {
			if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
				BT_ERR("Set vconf failed\n");

			if (vconf_set_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, 0) != 0)
				BT_ERR("Set vconf failed\n");

			if (vconf_set_int(BT_OFF_DUE_TO_TIMEOUT, 0) != 0)
				BT_ERR("Set vconf failed\n");
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_ENABLED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DISABLED) == 0) {
		int flight_mode_value = 0;
		int ps_mode_value = 0;

		if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
						&flight_mode_value) != 0)
			BT_ERR("Fail to get the flight_mode_deactivated value");

		if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
							&ps_mode_value) != 0)
			BT_ERR("Fail to get the ps_mode_deactivated value");

		if (flight_mode_value == 1 || ps_mode_value > 0) {
			BT_DBG("Flight mode deactivation");
			if (disable_timer_id > 0)
				g_source_remove(disable_timer_id);

			disable_timer_id = g_timeout_add(BT_RELIABLE_DISABLE_TIME,
					(GSourceFunc)__bt_reliable_disable_cb,
					event_info);
		}
	} else if (strcasecmp(member, BT_DISCOVERABLE_MODE_CHANGED) == 0) {
		int mode = 0;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &mode,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
				result, &mode,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DISCOVERABLE_TIMEOUT_CHANGED) == 0) {
		int timeout = 0;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &timeout,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED,
				result, &timeout,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_ADAPTER_NAME_CHANGED) == 0) {
		char *adapter_name = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &adapter_name,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,
				result, adapter_name,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DISCOVERY_STARTED) == 0) {
		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERY_STARTED,
				BLUETOOTH_ERROR_NONE, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DISCOVERY_FINISHED) == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_DISCOVERY_FINISHED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DEVICE_FOUND) == 0) {
		int event;
		bluetooth_device_info_t *device_info;

		device_info = __bt_get_device_info_in_message(msg, &result);
		retv_if(device_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		if (strlen(device_info->device_name.name) > 0) {
			event = BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED;
		} else {
			event = BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND;
		}

		_bt_common_event_cb(event,
				result, device_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	} else if (strcasecmp(member, BT_BOND_CREATED) == 0) {
		bluetooth_device_info_t *device_info;

		device_info = __bt_get_device_info_in_message(msg, &result);
		retv_if(device_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		_bt_common_event_cb(BLUETOOTH_EVENT_BONDING_FINISHED,
				result, device_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	} else if (strcasecmp(member, BT_BOND_DESTROYED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_SERVICE_SEARCHED) == 0) {
		bluetooth_device_info_t *device_info;
		bt_sdp_info_t sdp_info;

		device_info = __bt_get_device_info_in_message(msg, &result);
		retv_if(device_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&sdp_info, 0x00, sizeof(bt_sdp_info_t));

		sdp_info.service_index = device_info->service_index;

		memcpy(&sdp_info.device_addr,
			&device_info->device_address,
			BLUETOOTH_ADDRESS_LENGTH);

		memcpy(sdp_info.service_list_array,
			device_info->service_list_array,
			BLUETOOTH_MAX_SERVICES_FOR_DEVICE);

		memcpy(sdp_info.uuids,
			device_info->uuids,
			BLUETOOTH_MAX_SERVICES_FOR_DEVICE * BLUETOOTH_UUID_STRING_MAX);

		_bt_common_event_cb(BLUETOOTH_EVENT_SERVICE_SEARCHED,
				result, &sdp_info,
				event_info->cb, event_info->user_data);

		g_free(device_info);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_device_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);
	short rssi;

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_DEVICE_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;


	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_GATT_CONNECTED) == 0) {
                if (!dbus_message_get_args(msg, NULL,
                        DBUS_TYPE_INT32, &result,
                        DBUS_TYPE_INVALID)) {
                        BT_DBG("Unexpected parameters in signal");
                        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                }
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_GATT_DISCONNECTED) == 0) {
                if (!dbus_message_get_args(msg, NULL,
                        DBUS_TYPE_INT32, &result,
                        DBUS_TYPE_INVALID)) {
                        BT_DBG("Unexpected parameters in signal");
                        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
                }
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_DISCONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_GATT_RSSI) == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &rssi,
			DBUS_TYPE_INVALID)) {
			BT_DBG("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_RSSI,
				result, &rssi,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DEVICE_CONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_DBG("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_DEVICE_DISCONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_DBG("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_hid_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_HID_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_INPUT_CONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_DBG("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_input_event_cb(BLUETOOTH_HID_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_INPUT_DISCONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_DBG("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		BT_DBG("address: %s", address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_input_event_cb(BLUETOOTH_HID_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_headset_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_HEADSET_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_STEREO_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_headset_event_cb(BLUETOOTH_EVENT_AV_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_STEREO_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_headset_event_cb(BLUETOOTH_EVENT_AV_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_SPEAKER_GAIN) == 0) {
		unsigned int gain;
		guint16 spkr_gain;
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_UINT16, &spkr_gain,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		gain = (unsigned int)spkr_gain;

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_SPEAKER_GAIN,
				result, &gain,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_MICROPHONE_GAIN) == 0) {
		unsigned int gain;
		guint16 mic_gain;
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_UINT16, &mic_gain,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		gain = (unsigned int)mic_gain;

		_bt_headset_event_cb(BLUETOOTH_EVENT_AG_MIC_GAIN,
				result, &gain,
				event_info->cb, event_info->user_data);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_network_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_NETWORK_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_NETWORK_CONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_NETWORK_DISCONNECTED) == 0) {
		char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_NETWORK_SERVER_CONNECTED) == 0) {
		char *device = NULL;
		char *address = NULL;
		bluetooth_network_device_info_t network_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&network_info, 0x00, sizeof(bluetooth_network_device_info_t));

		_bt_convert_addr_string_to_type(network_info.device_address.addr,
						address);

		_bt_print_device_address_t(&network_info.device_address);
		g_strlcpy(network_info.interface_name, device, BLUETOOTH_INTERFACE_NAME_LENGTH);

		DBG_SECURE("Interface: %s", network_info.interface_name);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED,
				result, &network_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_NETWORK_SERVER_DISCONNECTED) == 0) {
		char *device = NULL;
		char *address = NULL;
		bluetooth_network_device_info_t network_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&network_info, 0x00, sizeof(bluetooth_network_device_info_t));

		_bt_convert_addr_string_to_type(network_info.device_address.addr,
						address);

		_bt_print_device_address_t(&network_info.device_address);

		_bt_common_event_cb(BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED,
				result, &network_info,
				event_info->cb, event_info->user_data);
	}


	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_avrcp_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_AVRCP_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_STEREO_HEADSET_CONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_STEREO_HEADSET_DISCONNECTED) == 0) {
		char *address = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_MEDIA_SHUFFLE_STATUS) == 0) {
		unsigned int status;
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_UINT32, &status,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_MEDIA_EQUALIZER_STATUS) == 0) {
		unsigned int status;
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_UINT32, &status,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_MEDIA_REPEAT_STATUS) == 0) {
		unsigned int status;
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_UINT32, &status,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	}  else if (strcasecmp(member, BT_MEDIA_SCAN_STATUS) == 0) {
		unsigned int status;
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_UINT32, &status,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_opp_client_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_OPP_CLIENT_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_OPP_CONNECTED) == 0) {
		char *address = NULL;
		int request_id = 0;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INT32, &request_id,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);

		if (result != BLUETOOTH_ERROR_NONE) {
			__bt_remove_push_request_id(request_id);
		}
	} else if (strcasecmp(member, BT_OPP_DISCONNECTED) == 0) {
		char *address = NULL;
		int request_id = 0;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INT32, &request_id,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);

		__bt_remove_push_request_id(request_id);
	} else if (strcasecmp(member, BT_TRANSFER_STARTED) == 0) {
		char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		bt_opc_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &request_id,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	} else if (strcasecmp(member, BT_TRANSFER_PROGRESS) == 0) {
		char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		int progress = 0;
		bt_opc_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &progress,
			DBUS_TYPE_INT32, &request_id,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;
		transfer_info.percentage = progress;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	} else if (strcasecmp(member, BT_TRANSFER_COMPLETED) == 0) {
		char *file_name = NULL;
		int request_id = 0;
		guint64 size = 0;
		bt_opc_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &request_id,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (__bt_is_request_id_exist(request_id) == FALSE) {
			BT_ERR("Different request id!");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&transfer_info, 0x00, sizeof(bt_opc_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.size = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_opp_server_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_OPP_SERVER_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_TRANSFER_AUTHORIZED) == 0) {
		/* Native only event */
		char *file_name = NULL;
		guint64 size = 0;
		bt_obex_server_authorize_into_t auth_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* OSP server: Don't get this event */
		retv_if(obex_server_id == BT_CUSTOM_SERVER,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&auth_info, 0x00, sizeof(bt_obex_server_authorize_into_t));

		auth_info.filename = g_strdup(file_name);
		auth_info.length = size;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE,
				result, &auth_info,
				event_info->cb, event_info->user_data);

		g_free(auth_info.filename);
	} else if (strcasecmp(member, BT_CONNECTION_AUTHORIZED) == 0) {
		/* OSP only event */
		char *address = NULL;
		char *name = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* Native server: Don't get this event */
		retv_if(obex_server_id == BT_NATIVE_SERVER,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_TRANSFER_CONNECTED) == 0) {

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED,
				result, NULL, event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_TRANSFER_DISCONNECTED) == 0) {

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_DISCONNECTED,
				result, NULL, event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_TRANSFER_STARTED) == 0) {
		char *file_name = NULL;
		char *type = NULL;
		int transfer_id = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_STRING, &type,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &transfer_id,
			DBUS_TYPE_INT32, &server_type,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* Other server's event */
		retv_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
	} else if (strcasecmp(member, BT_TRANSFER_PROGRESS) == 0) {
		char *file_name = NULL;
		char *type = NULL;
		int transfer_id = 0;
		int progress = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_STRING, &type,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &transfer_id,
			DBUS_TYPE_INT32, &progress,
			DBUS_TYPE_INT32, &server_type,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* Other server's event */
		retv_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.percentage = progress;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
	} else if (strcasecmp(member, BT_TRANSFER_COMPLETED) == 0) {
		char *file_name = NULL;
		char *device_name = NULL;
		char *type = NULL;
		char *file_path;
		int transfer_id = 0;
		int server_type = 0; /* bt_server_type_t */
		guint64 size = 0;
		bt_obex_server_transfer_info_t transfer_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &file_name,
			DBUS_TYPE_STRING, &type,
			DBUS_TYPE_STRING, &device_name,
			DBUS_TYPE_STRING, &file_path,
			DBUS_TYPE_UINT64, &size,
			DBUS_TYPE_INT32, &transfer_id,
			DBUS_TYPE_INT32, &server_type,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* Other server's event */
		retv_if(obex_server_id != server_type &&
			server_type != BT_FTP_SERVER,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&transfer_info, 0x00, sizeof(bt_obex_server_transfer_info_t));

		transfer_info.filename = g_strdup(file_name);
		transfer_info.type = g_strdup(type);
		transfer_info.device_name = g_strdup(device_name);
		transfer_info.file_path = g_strdup(file_path);
		transfer_info.file_size = size;
		transfer_info.transfer_id = transfer_id;
		transfer_info.server_type = (server_type == BT_FTP_SERVER) ?
						FTP_SERVER : OPP_SERVER;

		_bt_common_event_cb(BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,
				result, &transfer_info,
				event_info->cb, event_info->user_data);

		g_free(transfer_info.filename);
		g_free(transfer_info.type);
		g_free(transfer_info.device_name);
		g_free(transfer_info.file_path);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_rfcomm_client_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_RFCOMM_CLIENT_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_RFCOMM_CONNECTED) == 0) {
		char *address = NULL;
		char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_connection_t conn_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
		conn_info.device_role = RFCOMM_ROLE_CLIENT;
		g_strlcpy(conn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		conn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_RFCOMM_DISCONNECTED) == 0) {
		char *address = NULL;
		char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_disconnection_t disconn_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
		disconn_info.device_role = RFCOMM_ROLE_CLIENT;
		g_strlcpy(disconn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		disconn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
				result, &disconn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_RFCOMM_DATA_RECEIVED) == 0) {
		char *buffer = NULL;
		int buffer_len = 0;
		int socket_fd = 0;
		bluetooth_rfcomm_received_data_t data_r;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
			&buffer, &buffer_len,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		data_r.socket_fd = socket_fd;
		data_r.buffer_size = buffer_len;
		data_r.buffer = g_memdup(buffer, buffer_len);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
				result, &data_r,
				event_info->cb, event_info->user_data);

		g_free(data_r.buffer);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_rfcomm_server_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	const char *member = dbus_message_get_member(msg);

	event_info = (bt_event_info_t *)data;
	retv_if(event_info == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_interface(msg, BT_EVENT_SERVICE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_has_path(msg, BT_RFCOMM_SERVER_PATH))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, BT_RFCOMM_CONNECTED) == 0) {
		char *address = NULL;
		char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_connection_t conn_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
		conn_info.device_role = RFCOMM_ROLE_SERVER;
		g_strlcpy(conn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		conn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_RFCOMM_DISCONNECTED) == 0) {
		char *address = NULL;
		char *uuid = NULL;
		int socket_fd = 0;
		bluetooth_rfcomm_disconnection_t disconn_info;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
		disconn_info.device_role = RFCOMM_ROLE_SERVER;
		g_strlcpy(disconn_info.uuid, uuid, BLUETOOTH_UUID_STRING_MAX);
		disconn_info.socket_fd = socket_fd;
		_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
				result, &disconn_info,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_CONNECTION_AUTHORIZED) == 0) {
		/* OSP only event */
		bluetooth_rfcomm_connection_request_t req_ind;
		char *address = NULL;
		char *uuid = NULL;
		char *name = NULL;
		int socket_fd = 0;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		/* Don't send the authorized event to other server */
		retv_if(__bt_is_server_exist(socket_fd) == FALSE,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		memset(&req_ind, 0x00, sizeof(bluetooth_rfcomm_connection_request_t));
		_bt_convert_addr_string_to_type(req_ind.device_addr.addr,
						address);

		req_ind.socket_fd = socket_fd;

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,
				result, &req_ind,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(member, BT_RFCOMM_SERVER_REMOVED) == 0) {
		/* OSP only event */
		int socket_fd = 0;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		retv_if(__bt_is_server_exist(socket_fd) == FALSE,
				DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

		_bt_remove_server(socket_fd);
	} else if (strcasecmp(member, BT_RFCOMM_DATA_RECEIVED) == 0) {
		char *buffer = NULL;
		int buffer_len = 0;
		int socket_fd = 0;
		bluetooth_rfcomm_received_data_t data_r;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INT16, &socket_fd,
			DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
			&buffer, &buffer_len,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		data_r.socket_fd = socket_fd;
		data_r.buffer_size = buffer_len;
		data_r.buffer = g_memdup(buffer, buffer_len);

		_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
				result, &data_r,
				event_info->cb, event_info->user_data);

		g_free(data_r.buffer);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif

static void __bt_remove_all_events(void)
{
	GSList *l;
	bt_event_info_t *info;

	for (l = event_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;

		if (info)
			_bt_unregister_event(info->event_type);
	}

	g_slist_free(event_list);
	event_list = NULL;
}

static gboolean __bt_event_is_registered(int event_type)
{
	GSList *l;
	bt_event_info_t *info;

	for (l = event_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->event_type == event_type)
			return TRUE;
	}

	return FALSE;
}

bt_event_info_t* __bt_event_get_cb_data(int event_type)
{
	GSList *l;
	bt_event_info_t *info;

	for (l = event_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->event_type == event_type)
			return info;
	}

	return NULL;
}

void _bt_add_server(int server_fd)
{
	bt_server_info_t *info;

	info = g_new0(bt_server_info_t, 1);
	info->server_fd = server_fd;

	server_list = g_slist_append(server_list, info);
}

void _bt_remove_server(int server_fd)
{
	GSList *l;
	bt_server_info_t *info;

	for (l = server_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->server_fd == server_fd) {
			server_list = g_slist_remove(server_list, (void *)info);
		}

		g_free(info);
	}
}

void _bt_set_obex_server_id(int server_type)
{
	obex_server_id = server_type;
}

int _bt_get_obex_server_id(void)
{
	return obex_server_id;
}

int _bt_init_event_handler(void)
{
	if (is_initialized == TRUE) {
		BT_ERR("Connection already exist");
		return BLUETOOTH_ERROR_ALREADY_INITIALIZED;
	}

	__bt_remove_all_events();

	is_initialized = TRUE;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_deinit_event_handler(void)
{
	if (is_initialized == FALSE) {
		BT_ERR("Connection dose not exist");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_remove_all_events();

	if (disable_timer_id > 0) {
		g_source_remove(disable_timer_id);
		disable_timer_id = 0;
	}

	is_initialized = FALSE;

	return BLUETOOTH_ERROR_NONE;
}
#ifdef __ENABLE_GDBUS__
static void __bt_event_data_free(void *data)
{
	bt_event_info_t *cb_data = data;

	ret_if(cb_data == NULL);

	g_object_unref(cb_data->conn);
	g_free(cb_data);
}

int _bt_register_event(int event_type, void *event_cb, void *user_data)
{
	GError *error = NULL;
	GDBusConnection *connection_type;
	GDBusSignalCallback event_func;
	bt_event_info_t *cb_data;
	const char *path;

	if (is_initialized == FALSE)
		_bt_init_event_handler();

	if (__bt_event_is_registered(event_type) == TRUE) {
		BT_ERR("The event is already registed");
		return BLUETOOTH_ERROR_ALREADY_INITIALIZED;
	}

	switch (event_type) {
	case BT_ADAPTER_EVENT:
		event_func = __bt_adapter_event_filter;
		path = BT_ADAPTER_PATH;
		break;
	case BT_DEVICE_EVENT:
		event_func = __bt_device_event_filter;
		path = BT_DEVICE_PATH;
		break;
	case BT_HID_EVENT:
		event_func = __bt_hid_event_filter;
		path = BT_HID_PATH;
		break;
	case BT_HEADSET_EVENT:
		event_func = __bt_headset_event_filter;
		path = BT_HEADSET_PATH;
		break;
	case BT_NETWORK_EVENT:
		event_func = __bt_network_event_filter;
		path = BT_NETWORK_PATH;
		break;
	case BT_AVRCP_EVENT:
		event_func = __bt_avrcp_event_filter;
		path = BT_AVRCP_PATH;
		break;
	case BT_OPP_CLIENT_EVENT:
		event_func = __bt_opp_client_event_filter;
		path = BT_OPP_CLIENT_PATH;
		break;
	case BT_OPP_SERVER_EVENT:
		event_func = __bt_opp_server_event_filter;
		path = BT_OPP_SERVER_PATH;
		break;
	case BT_RFCOMM_CLIENT_EVENT:
		event_func = __bt_rfcomm_client_event_filter;
		path = BT_RFCOMM_CLIENT_PATH;
		break;
	case BT_RFCOMM_SERVER_EVENT:
		event_func = __bt_rfcomm_server_event_filter;
		path = BT_RFCOMM_SERVER_PATH;
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	connection_type = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection_type == NULL) {
		if (error) {
			BT_ERR("Unable to get the bus: %s", error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cb_data = g_new0(bt_event_info_t, 1);

	cb_data->event_type = event_type;
	cb_data->conn = connection_type;
	cb_data->cb = event_cb;
	cb_data->user_data = user_data;

	cb_data->id = g_dbus_connection_signal_subscribe(connection_type,
				NULL, BT_EVENT_SERVICE, NULL, path, NULL, 0,
				event_func, cb_data, __bt_event_data_free);

	event_list = g_slist_append(event_list, cb_data);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_event(int event_type)
{
	GDBusConnection *connection_type;
	bt_event_info_t *cb_data;

	if (is_initialized == FALSE) {
		BT_ERR("Event is not registered");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (__bt_event_is_registered(event_type) == FALSE) {
		BT_ERR("Not registered event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cb_data = __bt_event_get_cb_data(event_type);

	if (cb_data == NULL) {
		BT_ERR("No matched event data");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	connection_type = cb_data->conn;

	event_list = g_slist_remove(event_list, (void *)cb_data);

	retv_if(connection_type == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_connection_signal_unsubscribe(connection_type, cb_data->id);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_name_owner_changed(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	const char *name = NULL;
	const char *old_owner = NULL;
	const char *new_owner = NULL;
	bt_event_info_t *event_info;

	g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

	if (g_strcmp0(name, BT_DBUS_NAME) == 0 &&
			(new_owner != NULL && *new_owner == '\0')) {
		BT_DBG("bt-service is terminated abnormally");
		event_info = __bt_event_get_cb_data(BT_ADAPTER_EVENT);
		if (event_info == NULL)
			return;

		if (disable_timer_id > 0)
			g_source_remove(disable_timer_id);

		disable_timer_id = g_timeout_add(BT_RELIABLE_DISABLE_TIME,
				(GSourceFunc)__bt_reliable_disable_cb,
				event_info);
	}
}

void _bt_register_name_owner_changed(void)
{
	GDBusConnection *connection_type;

	connection_type = _bt_gdbus_get_system_gconn();
	if (connection_type == NULL) {
		BT_ERR("Unable to get the bus");
		return;
	}
	owner_sig_id = g_dbus_connection_signal_subscribe(connection_type,
				NULL, DBUS_INTERFACE_DBUS,
				BT_NAME_OWNER_CHANGED, NULL, NULL, 0,
				__bt_name_owner_changed, NULL, NULL);
}

void _bt_unregister_name_owner_changed(void)
{
	GDBusConnection *connection_type;

	connection_type = _bt_gdbus_get_system_gconn();
	if (connection_type != NULL && owner_sig_id != -1) {
		g_dbus_connection_signal_unsubscribe(connection_type,
							owner_sig_id);
		owner_sig_id = -1;
	}
}

#else
static void __bt_event_data_free(void *data)
{
	bt_event_info_t *cb_data = data;

	ret_if(cb_data == NULL);

	if (cb_data->conn)
		dbus_connection_unref(cb_data->conn);

	g_free(cb_data);
}

int _bt_register_event(int event_type, void *event_cb, void *user_data)
{
	DBusError dbus_error;
	char *match;
	DBusConnection *connection_type;
	DBusHandleMessageFunction event_func;
	bt_event_info_t *cb_data;

	if (is_initialized == FALSE)
		_bt_init_event_handler();

	if (__bt_event_is_registered(event_type) == TRUE) {
		BT_ERR("The event is already registed");
		return BLUETOOTH_ERROR_ALREADY_INITIALIZED;
	}

	switch (event_type) {
	case BT_ADAPTER_EVENT:
		event_func = __bt_adapter_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_ADAPTER_PATH);
		break;
	case BT_DEVICE_EVENT:
		event_func = __bt_device_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_DEVICE_PATH);
		break;
	case BT_HID_EVENT:
		event_func = __bt_hid_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_HID_PATH);
		break;
	case BT_HEADSET_EVENT:
		event_func = __bt_headset_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_HEADSET_PATH);
		break;
	case BT_NETWORK_EVENT:
		event_func = __bt_network_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_NETWORK_PATH);
		break;
	case BT_AVRCP_EVENT:
		event_func = __bt_avrcp_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_AVRCP_PATH);
		break;
	case BT_OPP_CLIENT_EVENT:
		event_func = __bt_opp_client_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_OPP_CLIENT_PATH);
		break;
	case BT_OPP_SERVER_EVENT:
		event_func = __bt_opp_server_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_OPP_SERVER_PATH);
		break;
	case BT_RFCOMM_CLIENT_EVENT:
		event_func = __bt_rfcomm_client_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_RFCOMM_CLIENT_PATH);
		break;
	case BT_RFCOMM_SERVER_EVENT:
		event_func = __bt_rfcomm_server_event_filter;
		match = g_strdup_printf(EVENT_MATCH_RULE, BT_EVENT_SERVICE,
					BT_RFCOMM_SERVER_PATH);
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	connection_type = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection_type == NULL) {
		g_free(match);
		BT_ERR("Unable to get the bus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cb_data = g_new0(bt_event_info_t, 1);

	cb_data->event_type = event_type;
	cb_data->conn = connection_type;
	cb_data->func = event_func;
	cb_data->cb = event_cb;
	cb_data->user_data = user_data;

	if (!dbus_connection_add_filter(connection_type, event_func,
				(void *)cb_data, __bt_event_data_free)) {
		BT_ERR("Fail to add filter");
		goto fail;
	}

	dbus_error_init(&dbus_error);

	if (match)
		dbus_bus_add_match(connection_type, match, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	g_free(match);

	event_list = g_slist_append(event_list, cb_data);

	return BLUETOOTH_ERROR_NONE;
fail:
	if (connection_type)
		dbus_connection_unref(connection_type);

	g_free(cb_data);
	g_free(match);
	return BLUETOOTH_ERROR_INTERNAL;
}

int _bt_unregister_event(int event_type)
{
	DBusConnection *connection_type;
	DBusHandleMessageFunction event_func;
	bt_event_info_t *cb_data;

	if (is_initialized == FALSE) {
		BT_ERR("Event is not registered");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (__bt_event_is_registered(event_type) == FALSE) {
		BT_ERR("Not registered event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cb_data = __bt_event_get_cb_data(event_type);

	if (cb_data == NULL) {
		BT_ERR("No matched event data");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	connection_type = cb_data->conn;
	event_func = cb_data->func;

	event_list = g_slist_remove(event_list, (void *)cb_data);

	retv_if(connection_type == NULL, BLUETOOTH_ERROR_INTERNAL);
	retv_if(event_func == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_connection_remove_filter(connection_type, event_func,
					(void *)cb_data);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_name_owner_changed(DBusConnection *bus, DBusMessage *m,
							void *user_data)
{
	const char *name = NULL;
	const char *old = NULL;
	const char *new = NULL;
	bt_event_info_t *event_info;

	if (!dbus_message_get_args(m, NULL,
				 DBUS_TYPE_STRING, &name,
				 DBUS_TYPE_STRING, &old,
				 DBUS_TYPE_STRING, &new,
				 DBUS_TYPE_INVALID)) {
		BT_ERR("Unexpected parameters in signal");
		return;
	}

	if (g_strcmp0(name, BT_DBUS_NAME) == 0 && *new == '\0') {
		BT_DBG("bt-service is terminated abnormally");
		event_info = __bt_event_get_cb_data(BT_ADAPTER_EVENT);
		if (event_info == NULL)
			return;

		if (disable_timer_id > 0)
			g_source_remove(disable_timer_id);

		disable_timer_id = g_timeout_add(BT_RELIABLE_DISABLE_TIME,
				(GSourceFunc)__bt_reliable_disable_cb,
				event_info);
	}
	return;
}

void _bt_register_name_owner_changed(void)
{
	DBusConnection *connection;
	DBusError dbus_error;
	char *match;

	match = g_strdup_printf("type='signal',interface=%s,member=%s",
				DBUS_INTERFACE_DBUS, BT_NAME_OWNER_CHANGED);

	connection = _bt_get_system_conn();
	if (connection == NULL) {
		BT_ERR("Unable to get the bus");
		goto fail;
	}

	if (!dbus_connection_add_filter(connection,
			(DBusHandleMessageFunction)__bt_name_owner_changed,
			NULL, NULL)) {
		BT_ERR("Fail to add filter");
		goto fail;
	}

	dbus_error_init(&dbus_error);

	dbus_bus_add_match(connection, match, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
	}

fail:
	g_free(match);
	return;

}

void _bt_unregister_name_owner_changed(void)
{
	DBusConnection *connection;

	connection = _bt_get_system_conn();
	if (connection != NULL) {
		dbus_connection_remove_filter(connection,
			(DBusHandleMessageFunction)__bt_name_owner_changed,
			NULL);
	}
	return;
}
#endif
