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
#include "bluetooth-media-control.h"

#include "bt-common.h"
#include "bt-event-handler.h"
#include "bt-request-sender.h"

#define BT_RELIABLE_DISABLE_TIME 300 /* 300 ms */

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
static gboolean is_adapter_enabled = TRUE;

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

	for (i = 0; i < uuid_count && uuids[i] != NULL; i++) {
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
static int __bt_call_list_create(bt_hf_call_list_s **list)
{
	bt_hf_call_list_s *handle;

	if (*list != NULL) {
		BT_DBG("Already Initialized");
		return BLUETOOTH_ERROR_NONE;
	}
	handle = g_malloc0(sizeof(bt_hf_call_list_s));
	*list = handle;
	return BLUETOOTH_ERROR_NONE;
}

static int __bt_call_list_reset(bt_hf_call_list_s *list)
{
	bt_hf_call_list_s *handle;
	bt_hf_call_status_info_t *call_status;

	if (list == NULL) {
		BT_ERR("invalid parameter");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	handle = (bt_hf_call_list_s *)list;
	do  {
		call_status = (bt_hf_call_status_info_t *)g_list_nth_data(handle->list, 0);
		if (call_status == NULL)
			break;
		handle->list = g_list_remove(handle->list, call_status);
		g_free(call_status->number);
		g_free(call_status);
	} while (1);
	return BLUETOOTH_ERROR_NONE;
}

static int __bt_call_list_destroy(bt_hf_call_list_s *list)
{
	int result;
	bt_hf_call_list_s *handle;

	if (list == NULL) {
		BT_ERR("invalid parameter");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	handle = (bt_hf_call_list_s *)list;
	result = __bt_call_list_reset(list);
	g_free(handle);
	return result;
}
static int __bt_call_list_add(bt_hf_call_list_s *list, char * number,
								int dir, int status, int mpart, int idx)
{
	bt_hf_call_list_s *handle;
	bt_hf_call_status_info_t *call_status;

	if (list == NULL) {
		BT_ERR("invalid parameter");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	handle = (bt_hf_call_list_s *)list;
	call_status = g_malloc0(sizeof(bt_hf_call_status_info_t));
	/* Fix : NULL_RETURNS */
	retv_if(call_status == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	call_status->number = g_strdup(number);
	call_status->direction= dir;
	call_status->status = status;
	call_status->mpart = mpart;
	call_status->idx = idx;
	handle->list = g_list_append(handle->list, (gpointer)call_status);
	return BLUETOOTH_ERROR_NONE;
}

static bluetooth_device_info_t *__bt_get_device_info_in_message(GVariant *parameters, int *ret)
{
	bluetooth_device_info_t *dev_info;
	const char *address = NULL;
	const char *name = NULL;
	gchar **uuids = NULL;
	unsigned int dev_class = 0;
	short rssi = 0;
	gboolean paired = FALSE;
	guint connected = 0;
	gboolean trust = FALSE;
	gsize uuid_count;
	int result = BLUETOOTH_ERROR_NONE;
	GVariant *string_var;
	int i = 0, len = 0;
	int manufacturer_data_len = 0;
	GVariant *manufacturer_var = NULL;
	const char *manufacturer_data = NULL;

	g_variant_get(parameters, "(i&sun&sbub@asn@ay)", &result, &address,
			&dev_class, &rssi, &name, &paired,
			&connected, &trust,  &string_var, &manufacturer_data_len, &manufacturer_var);

	if (string_var == NULL) {
		BT_ERR("invalid parameters in signal");
		return NULL;
	}

	uuids = (gchar **)g_variant_get_strv(string_var, &uuid_count);

	len = g_variant_get_size(manufacturer_var);
	if (len > 0)
		manufacturer_data = (char *)g_variant_get_data(manufacturer_var);

	dev_info = g_malloc0(sizeof(bluetooth_device_info_t));
	/* Fix : NULL_RETURNS */
	if (dev_info == NULL) {
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto done;
	}

	dev_info->rssi = rssi;
	dev_info->paired = paired;
	dev_info->connected = connected;
	dev_info->trust = trust;

	g_strlcpy(dev_info->device_name.name, name,
		BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1);

	_bt_divide_device_class(&dev_info->device_class, dev_class);

	_bt_convert_addr_string_to_type(dev_info->device_address.addr,
					address);

	if (uuid_count > 0)
		__bt_get_uuid_info(dev_info, uuids, uuid_count);

	if (manufacturer_data_len > BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX) {
		BT_ERR("manufacturer_data_len is too long(len = %d)", manufacturer_data_len);
		manufacturer_data_len = BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX;
	}
	dev_info->manufacturer_data.data_len = manufacturer_data_len;
	if (manufacturer_data)
		for (i = 0; i < manufacturer_data_len; i++)
			dev_info->manufacturer_data.data[i] = manufacturer_data[i];
done:
	*ret = result;
	g_free(uuids);
	g_variant_unref(string_var);
	g_variant_unref(manufacturer_var);
	return dev_info;
}

static bluetooth_le_device_info_t *__bt_get_le_device_info_in_message(GVariant *parameters, int *ret)
{
	bluetooth_le_device_info_t *le_dev_info;
	const char *address = NULL;
	int i;
	short addr_type = 0;
	short rssi = 0;
	int len = 0;
	int adv_data_len = 0;
	GVariant *adv_var = NULL;
	const char *adv_data = NULL;
	int scan_data_len = 0;
	GVariant *scan_var = NULL;
	const char *scan_data = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	g_variant_get(parameters, "(i&snnn@ayn@ay)", &result, &address,
			&addr_type, &rssi, &adv_data_len, &adv_var, &scan_data_len, &scan_var);

	len = g_variant_get_size(adv_var);
	if (len > 0)
		adv_data = (char *)g_variant_get_data(adv_var);

	len = g_variant_get_size(scan_var);
	if (len > 0)
		scan_data = (char *)g_variant_get_data(scan_var);

	le_dev_info = g_malloc0(sizeof(bluetooth_le_device_info_t));
	/* Fix : NULL_RETURNS */
	if (le_dev_info == NULL) {
		result = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto done;
	}
	_bt_convert_addr_string_to_type(le_dev_info->device_address.addr, address);
	le_dev_info->addr_type = addr_type;
	le_dev_info->rssi = rssi;
	le_dev_info->adv_ind_data.data_len = adv_data_len;

	for (i = 0; i < adv_data_len; i++)
		if (adv_data)
			le_dev_info->adv_ind_data.data.data[i] = adv_data[i];

	le_dev_info->scan_resp_data.data_len = scan_data_len;

	for (i = 0; i < scan_data_len; i++)
		if (scan_data)
			le_dev_info->scan_resp_data.data.data[i] = scan_data[i];

done:
	*ret = result;

	g_variant_unref(adv_var);
	g_variant_unref(scan_var);
	return le_dev_info;
}

gboolean __bt_reliable_disable_cb(gpointer user_data)
{
	BT_DBG("+");
	bt_event_info_t *event_info = user_data;

	_bt_set_le_scan_status(FALSE);

	if (is_initialized != FALSE) {
		if (is_adapter_enabled == TRUE) {
			is_adapter_enabled = FALSE;
			_bt_common_event_cb(BLUETOOTH_EVENT_DISABLED,
					BLUETOOTH_ERROR_NONE, NULL,
					event_info->cb, event_info->user_data);
			_bt_common_event_cb(BLUETOOTH_EVENT_LE_DISABLED,
					BLUETOOTH_ERROR_NONE, NULL,
					event_info->cb, event_info->user_data);
		}
	}

	obex_server_id = BT_NO_SERVER;
	__bt_remove_all_server();
	__bt_remove_all_push_request_id();
#ifdef RFCOMM_DIRECT
	 _bt_rfcomm_server_free_all();
#endif
	BT_DBG("-");
	return FALSE;
}

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
		BT_INFO("BT_ENABLED");
		g_variant_get(parameters, "(i)", &result);
		if (result == BLUETOOTH_ERROR_NONE) {
			if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
				BT_ERR("Set vconf failed");

			if (vconf_set_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, 0) != 0)
				BT_ERR("Set vconf failed");

			if (vconf_set_int(BT_OFF_DUE_TO_TIMEOUT, 0) != 0)
				BT_ERR("Set vconf failed");
		}

		is_adapter_enabled = TRUE;

		_bt_common_event_cb(BLUETOOTH_EVENT_ENABLED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DISABLED) == 0) {
		BT_INFO("BT_DISABLED");
		int flight_mode_value = 0;
		int ps_mode_value = 0;

		if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
						&flight_mode_value) != 0)
			BT_ERR("Fail to get the flight_mode_deactivated value");

		if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
							&ps_mode_value) != 0)
			BT_ERR("Fail to get the ps_mode_deactivated value");

		if (flight_mode_value == 1 || ps_mode_value > 0) {
			BT_INFO("Flight mode deactivation");
			if (disable_timer_id > 0)
				g_source_remove(disable_timer_id);

			disable_timer_id = g_timeout_add(BT_RELIABLE_DISABLE_TIME,
					(GSourceFunc)__bt_reliable_disable_cb,
					event_info);
		} else {
			is_adapter_enabled = FALSE;

			_bt_common_event_cb(BLUETOOTH_EVENT_DISABLED,
					result, NULL,
					event_info->cb, event_info->user_data);
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
	} else if (strcasecmp(signal_name, BT_CONNECTABLE_CHANGED) == 0) {
		gboolean connectable = FALSE;

		g_variant_get(parameters, "(b)", &connectable);
		BT_DBG("Connectable is changed : %d", connectable);

		_bt_common_event_cb(BLUETOOTH_EVENT_CONNECTABLE_CHANGED,
				result, &connectable,
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
	} else if (strcasecmp(signal_name, BT_ADVERTISING_STARTED) == 0) {
		int adv_handle;

		g_variant_get(parameters, "(ii)", &result, &adv_handle);
		_bt_common_event_cb(BLUETOOTH_EVENT_ADVERTISING_STARTED,
				result, &adv_handle,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_ADVERTISING_STOPPED) == 0) {
		int adv_handle;

		g_variant_get(parameters, "(ii)", &result, &adv_handle);
		_bt_common_event_cb(BLUETOOTH_EVENT_ADVERTISING_STOPPED,
				result, &adv_handle,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_ADVERTISING_MANUFACTURER_DATA_CHANGED) == 0) {
		GVariant *var = NULL;
		char *data;
		int len;

		g_variant_get(parameters, "(@ay)", &var);
		len = g_variant_get_size(var);
		data = (char *)g_variant_get_data(var);

		_bt_common_event_cb(BLUETOOTH_EVENT_ADVERTISING_MANUFACTURER_DATA_CHANGED,
				len, data,
				event_info->cb, event_info->user_data);

		g_variant_unref(var);
	}  else if (strcasecmp(signal_name, BT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED) == 0) {
		GVariant *var = NULL;
		char *data;
		int len;

		g_variant_get(parameters, "(@ay)", &var);
		len = g_variant_get_size(var);
		data = (char *)g_variant_get_data(var);

		_bt_common_event_cb(BLUETOOTH_EVENT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED,
				len, data,
				event_info->cb, event_info->user_data);

		g_variant_unref(var);
	} else if (strcasecmp(signal_name, BT_MANUFACTURER_DATA_CHANGED) == 0) {
		GVariant *var = NULL;
		char *data;
		int len;

		g_variant_get(parameters, "(@ay)", &var);
		len = g_variant_get_size(var);
		data = (char *)g_variant_get_data(var);

		_bt_common_event_cb(BLUETOOTH_EVENT_MANUFACTURER_DATA_CHANGED,
				len, data,
				event_info->cb, event_info->user_data);

		g_variant_unref(var);
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

void __bt_adapter_le_event_filter(GDBusConnection *connection,
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

	if (strcasecmp(object_path, BT_LE_ADAPTER_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	if (strcasecmp(signal_name, BT_LE_ENABLED) == 0) {
		BT_INFO("BT_LE_ENABLED");
		g_variant_get(parameters, "(i)", &result);
		_bt_common_event_cb(BLUETOOTH_EVENT_LE_ENABLED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_LE_DISABLED) == 0) {
		BT_INFO("BT_LE_DISABLED");
		_bt_common_event_cb(BLUETOOTH_EVENT_LE_DISABLED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_LE_DISCOVERY_STARTED) == 0) {
		_bt_common_event_cb(BLUETOOTH_EVENT_LE_DISCOVERY_STARTED,
				BLUETOOTH_ERROR_NONE, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_LE_DISCOVERY_FINISHED) == 0) {
		g_variant_get(parameters, "(i)", &result);
		_bt_common_event_cb(BLUETOOTH_EVENT_LE_DISCOVERY_FINISHED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_LE_DEVICE_FOUND) == 0) {
		bluetooth_le_device_info_t *le_device_info;

		le_device_info = __bt_get_le_device_info_in_message(parameters,
								&result);
		ret_if(le_device_info == NULL);

		_bt_common_event_cb(BLUETOOTH_EVENT_REMOTE_LE_DEVICE_FOUND,
				result, le_device_info,
				event_info->cb, event_info->user_data);

		g_free(le_device_info);
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

	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	if (strcasecmp(object_path, BT_DEVICE_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_GATT_CONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };
		BT_DBG("BT_GATT_CONNECTED");
		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr, address);
		BT_DBG("Sending Event to Framework");
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_GATT_DISCONNECTED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };
		BT_DBG("BT_GATT_DISCONNECTED");
		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr, address);
		BT_DBG("Sending Event to Framework");
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_DISCONNECTED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_GATT_CHAR_VAL_CHANGED) == 0) {
		const char *char_handle = NULL;
		int len = 0;
		const char * value = NULL;
		GVariant *char_value_var = NULL;
		bt_gatt_char_value_t char_val = { 0, };
		BT_DBG("BT_GATT_CHAR_VAL_CHANGED");

		g_variant_get(parameters, "(i&s@ay)", &result, &char_handle, &char_value_var);

		len = g_variant_get_size(char_value_var);
		if (len > 0)
			value = (char *)g_variant_get_data(char_value_var);

		char_val.char_handle = g_strdup(char_handle);
		char_val.val_len = len;
		/* Fix : FORWARD_NULL : g_variant_get_data can return NULL */
		if (char_val.val_len > 0 && value != NULL) {
			char_val.char_value = (unsigned char*) g_malloc0(char_val.val_len);
			/* Fix : NULL_RETURNS */
			if (char_val.char_value == NULL) {
				BT_ERR("BLUETOOTH_ERROR_OUT_OF_MEMORY");
				g_free(char_val.char_handle);
				return;
			}
			memcpy(char_val.char_value, value, len);
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
					result, &char_val,
					event_info->cb, event_info->user_data);
			g_free(char_val.char_value);
		}
		g_free(char_val.char_handle);
	} else if (strcasecmp(signal_name, BT_DEVICE_CONNECTED) == 0) {
		const char *address = NULL;
		unsigned char addr_type;
		bt_connection_info_t conn_info;
		bluetooth_device_address_t dev_address = { {0} };
		BT_DBG("BT_DEVICE_CONNECTED");
		g_variant_get(parameters, "(i&sy)", &result, &address, &addr_type);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		memset(&conn_info, 0x00, sizeof(bt_connection_info_t));

		memcpy(conn_info.device_addr.addr,
					dev_address.addr,
					BLUETOOTH_ADDRESS_LENGTH);

		conn_info.addr_type = addr_type;
		conn_info.disc_reason = 0;
		BT_DBG("Sending Event to Framework");
		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_CONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);

	} else if (strcasecmp(signal_name, BT_DEVICE_DISCONNECTED) == 0) {
		const char *address = NULL;
		unsigned char addr_type;
		bt_connection_info_t conn_info;
		bluetooth_device_address_t dev_address = { {0} };
		BT_DBG("BT_DEVICE_DISCONNECTED");
		g_variant_get(parameters, "(i&sy)", &result, &address, &addr_type);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		memset(&conn_info, 0x00, sizeof(bt_connection_info_t));

		memcpy(conn_info.device_addr.addr,
					dev_address.addr,
					BLUETOOTH_ADDRESS_LENGTH);

		conn_info.addr_type = addr_type;
		conn_info.disc_reason = result;
		BT_DBG("Sending Event to Framework");
		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_DISCONNECTED,
				result, &conn_info,
				event_info->cb, event_info->user_data);

	} else if (strcasecmp(signal_name, BT_RSSI_MONITORING_ENABLED) == 0) {
		bt_rssi_enabled_t enabled = { 0, };
		char *address;
		int link_type;
		gboolean rssi_enabled = FALSE;

		g_variant_get(parameters, "(isib)", &result, &address,
					&link_type, &rssi_enabled);

		BT_DBG("RSSI Enabled[Address:%s LinkType:%d RSSI_dbm:%d]",
				address, link_type, rssi_enabled);
		enabled.address = address;
		enabled.link_type = link_type;
		enabled.rssi_enabled = rssi_enabled;

		_bt_common_event_cb(BLUETOOTH_EVENT_RSSI_ENABLED,
				result, &enabled,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RSSI_ALERT) == 0) {
		int alert_type;
		int rssi_dbm;
		char *address;
		int link_type;
		bt_rssi_alert_t alert = { 0, };

		g_variant_get(parameters, "(isiii)", &result, &address,
					&link_type, &alert_type, &rssi_dbm);

		alert.alert_type = alert_type;
		alert.rssi_dbm = rssi_dbm;
		alert.address = address;
		alert.link_type = link_type;
		BT_DBG("Address [%s] LinkType[%d] AlertType[%d] RSSI dBm[%d]",
				address, link_type, alert_type, rssi_dbm);
		_bt_common_event_cb(BLUETOOTH_EVENT_RSSI_ALERT,
				result, &alert,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_RAW_RSSI_EVENT) == 0) {
		int rssi_dbm;
		int link_type;
		char *address;
		bt_raw_rssi_t raw_rssi = { 0, };

		g_variant_get(parameters, "(isii)", &result,
					&address, &link_type, &rssi_dbm);

		BT_DBG("Address [%s] Link Type[%d] dBm[%d]",
				address, link_type, rssi_dbm);

		raw_rssi.rssi_dbm = rssi_dbm;
		raw_rssi.address = address;
		raw_rssi.link_type = link_type;

		_bt_common_event_cb(BLUETOOTH_EVENT_RAW_RSSI,
				result, &raw_rssi,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DEVICE_AUTHORIZED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_AUTHORIZED,
				result, &dev_address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_DEVICE_UNAUTHORIZED) == 0) {
		const char *address = NULL;
		bluetooth_device_address_t dev_address = { {0} };

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_convert_addr_string_to_type(dev_address.addr,
						address);

		_bt_common_event_cb(BLUETOOTH_EVENT_DEVICE_UNAUTHORIZED,
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
	} else if (strcasecmp(signal_name, BT_A2DP_SOURCE_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);
		_bt_a2dp_source_event_cb(BLUETOOTH_EVENT_AV_SOURCE_CONNECTED,
						result, address,
						event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_A2DP_SOURCE_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_a2dp_source_event_cb(BLUETOOTH_EVENT_AV_SOURCE_DISCONNECTED,
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

void __bt_a2dp_source_event_filter(GDBusConnection *connection,
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

	if (strcasecmp(object_path, BT_A2DP_SOURCE_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_A2DP_SOURCE_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);
		_bt_a2dp_source_event_cb(BLUETOOTH_EVENT_AV_SOURCE_CONNECTED,
						result, address,
						event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_A2DP_SOURCE_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_a2dp_source_event_cb(BLUETOOTH_EVENT_AV_SOURCE_DISCONNECTED,
						result, address,
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

	if (strcasecmp(signal_name, BT_AVRCP_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_AVRCP_DISCONNECTED) == 0) {
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

void __bt_avrcp_control_event_filter(GDBusConnection *connection,
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

	if (strcasecmp(object_path, BT_AVRCP_CONTROL_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_AVRCP_CONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_AVRCP_DISCONNECTED) == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(i&s)", &result, &address);

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_SHUFFLE_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_SHUFFLE_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_EQUALIZER_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_EQUALIZER_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_REPEAT_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_REPEAT_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	}  else if (strcasecmp(signal_name, BT_MEDIA_SCAN_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_CONTROL_SCAN_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_PLAY_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_PLAY_STATUS_CHANGED,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_POSITION_STATUS) == 0) {
		unsigned int status;

		g_variant_get(parameters, "(u)", &status);
		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_SONG_POSITION_STATUS,
				result, &status,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_MEDIA_TRACK_CHANGE) == 0) {
		media_metadata_attributes_t metadata;
		const char *title;
		const char *artist;
		const char *album;
		const char *genre;
		unsigned int total_tracks;
		unsigned int number;
		unsigned int duration;

		g_variant_get(parameters, "(&s&s&s&suuu)", &title,
						&artist, &album, &genre,
						&total_tracks, &number,
						&duration);
		memset(&metadata, 0x00, sizeof(media_metadata_attributes_t));

		metadata.title = title;
		metadata.artist = artist;
		metadata.album = album;
		metadata.genre = genre;
		metadata.total_tracks = total_tracks;
		metadata.number = number;
		metadata.duration = (int64_t)duration;

		_bt_avrcp_event_cb(BLUETOOTH_EVENT_AVRCP_TRACK_CHANGED,
				result, &metadata,
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

void __bt_pbap_client_event_filter(GDBusConnection *connection,
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

	if (strcasecmp(object_path, BT_PBAP_CLIENT_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_EVENT_SERVICE) != 0)
		return;

	ret_if(signal_name == NULL);

	if (strcasecmp(signal_name, BT_PBAP_CONNECTED) == 0) {
		bt_pbap_connected_t connected = { { { 0 }, }, };
		char *address = NULL;

		g_variant_get(parameters, "(i&si)", &result, &address);
		BT_DBG("address: %s", address);

		_bt_convert_addr_string_to_type(connected.btaddr.addr,
						address);
		if (result == 0)
			connected.connected = 1;
		else
			connected.connected = 0;

		_bt_common_event_cb(BLUETOOTH_PBAP_CONNECTED,
				result, &connected,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_PBAP_DISCONNECTED) == 0) {
		bt_pbap_connected_t disconnected = { { { 0 }, }, };
		char *address = NULL;

		g_variant_get(parameters, "(i&si)", &result, &address);
		BT_DBG("address: %s", address);

		_bt_convert_addr_string_to_type(disconnected.btaddr.addr,
						address);
		disconnected.connected = 0;

		_bt_common_event_cb(BLUETOOTH_PBAP_CONNECTED,
				result, &disconnected,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_PBAP_PHONEBOOK_SIZE) == 0) {
		bt_pbap_phonebook_size_t pb_size = { { { 0 }, }, };
		char *address = NULL;
		int size = 0;

		g_variant_get(parameters, "(i&si)", &result, &address, &size);

		BT_DBG("address: %s", address);
		BT_DBG("size: %d", size);

		_bt_convert_addr_string_to_type(pb_size.btaddr.addr,
						address);
		pb_size.size = size;

		_bt_common_event_cb(BLUETOOTH_PBAP_PHONEBOOK_SIZE,
				result, &pb_size,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_PBAP_PHONEBOOK_PULL) == 0) {
		bt_pbap_phonebook_pull_t pb_pull = { { { 0 } }, };
		char *address = NULL;
		char *vcf_file = NULL;
		int success = -1;

		g_variant_get(parameters, "(i&s&si)", &result, &address, &vcf_file, &success);

		BT_DBG("address: %s", address);
		BT_DBG("vcf_file: %s", vcf_file);
		BT_DBG("success: %d", success);

		_bt_convert_addr_string_to_type(pb_pull.btaddr.addr,
						address);
		pb_pull.vcf_file = vcf_file;
		pb_pull.success = success;
		_bt_common_event_cb(BLUETOOTH_PBAP_PHONEBOOK_PULL,
				result, &pb_pull,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_PBAP_VCARD_LIST) == 0) {
		bt_pbap_vcard_list_t vc_list = { { { 0 } }, };
		char *address = NULL;
		unsigned int count = 0;
		gchar **list = NULL;
		GVariant *string_var;
		int success = -1;
		int i = 0;

		g_variant_get(parameters, "(i&s@as)", &result, &address, &string_var);

		list = (gchar **)g_variant_get_strv(string_var, &count);

		success = !result;
		BT_DBG("address: %s", address);
		for(i = 0; i < count; i++)
			BT_DBG("%s", list[i]);
		BT_DBG("success: %d", success);

		_bt_convert_addr_string_to_type(vc_list.btaddr.addr,
						address);
		vc_list.vcards = list;
		vc_list.length = count;
		vc_list.success = success;
		_bt_common_event_cb(BLUETOOTH_PBAP_VCARD_LIST,
				result, &vc_list,
				event_info->cb, event_info->user_data);

		g_variant_unref(string_var);
		//free lists
	} else if (strcasecmp(signal_name, BT_PBAP_VCARD_PULL) == 0) {
		bt_pbap_vcard_pull_t vc_pull = { { { 0 } }, };
		char *address = NULL;
		char *vcf_file = NULL;
		int success = -1;

		g_variant_get(parameters, "(i&s&si)", &result, &address, &vcf_file, &success);

		BT_DBG("address: %s", address);
		BT_DBG("vcf_file: %s", vcf_file);
		BT_DBG("success: %d", success);

		_bt_convert_addr_string_to_type(vc_pull.btaddr.addr,
						address);
		vc_pull.vcf_file = vcf_file;
		vc_pull.success = success;
		_bt_common_event_cb(BLUETOOTH_PBAP_VCARD_PULL,
				result, &vc_pull,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, BT_PBAP_SEARCH_PHONEBOOK) == 0) {
		bt_pbap_phonebook_search_list_t vc_list = { { { 0 } }, };
		char *address = NULL;
		unsigned int count = 0;
		gchar **list = NULL;
		GVariant *string_var;
		int success = -1;
		int i = 0;

		g_variant_get(parameters, "(i&s@as)", &result, &address, &string_var);

		list = (gchar **)g_variant_get_strv(string_var, &count);
		success = !result;
		BT_DBG("address: %s", address);
		for(i = 0; i < count; i++)
			BT_DBG("%s", list[i]);
		BT_DBG("success: %d", success);

		_bt_convert_addr_string_to_type(vc_list.btaddr.addr,
						address);
		vc_list.vcards = list;
		vc_list.length = count;
		vc_list.success = success;
		_bt_common_event_cb(BLUETOOTH_PBAP_PHONEBOOK_SEARCH,
				result, &vc_list,
				event_info->cb, event_info->user_data);

		g_variant_unref(string_var);
		//free lists
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
		char *address = NULL;
		char *uuid = NULL;
		char *name = NULL;
		char *path = NULL;
		int socket_fd = 0;

		g_variant_get(parameters, "(i&s&s&s&sn)", &result, &address,
						&uuid, &name, &path, &socket_fd);

		if (_check_uuid_path(path, uuid) == FALSE)
			return;

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

void __bt_hf_agent_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	BT_DBG("+\n");

	bt_event_info_t *event_info;
	int result = BLUETOOTH_ERROR_NONE;
	event_info = (bt_event_info_t *)user_data;
	ret_if(event_info == NULL);

	BT_DBG("+\n");
	if (strcasecmp(object_path, BT_HF_AGENT_PATH) != 0)
		return;
	if (strcasecmp(interface_name, BT_HF_SERVICE_INTERFACE) != 0)
		return;

	ret_if(signal_name == NULL);

	BT_DBG("%s",signal_name);
	if (strcasecmp(signal_name, "Connected") == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(s)", &address);
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "Disconnected") == 0) {
		char *address = NULL;

		g_variant_get(parameters, "(s)", &address);
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_DISCONNECTED,
				result, address,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "AudioConnected") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_AUDIO_CONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "AudioDisconnected") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_AUDIO_DISCONNECTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "Ring") == 0) {
		char *phoneno = NULL;

		g_variant_get(parameters, "(&s)", &phoneno);

		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_RING_INDICATOR,
				result, phoneno,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallWaiting") == 0) {
		char *phoneno = NULL;

		g_variant_get(parameters, "(&s)", &phoneno);

		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_WAITING,
				result, phoneno,
				event_info->cb, event_info->user_data);
	}  else if (strcasecmp(signal_name, "CallTerminated") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_TERMINATED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallStarted") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_STARTED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallEnded") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_ENDED,
				result, NULL,
				event_info->cb, event_info->user_data);
	}  else if (strcasecmp(signal_name, "NoCallsHeld") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_UNHOLD,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallsSwapped") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_SWAPPED,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallOnHold") == 0) {
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_ON_HOLD,
				result, NULL,
				event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "CallStatusUpdate") == 0) {
		int call_count;
		GVariant *var_data = NULL;
		char *number = NULL;
		int idx, dir, status, mpart;
		bt_hf_call_list_s *handle = NULL;

		g_variant_get(parameters, "(i@a(siiii))", &call_count,
								&var_data);
		BT_DBG("call count : %d",call_count);

		if (var_data){
			GVariantIter *iter = NULL;
			__bt_call_list_create(&handle);

			g_variant_get(var_data, "a(siiii)", &iter);
			while (g_variant_iter_loop(iter, "(siiii)", &number,
						&dir, &status, &mpart, &idx)) {
				BT_DBG("call number:%s, dir:%d, status : %d",
							number, dir, status);
				BT_DBG("call mpart : %d, idx : %d",mpart, idx);
				__bt_call_list_add(handle, number, dir,
							status, mpart, idx);
			}
			g_variant_iter_free(iter);
			g_variant_unref(var_data);
		}

		if (handle && (call_count == g_list_length(handle->list))) {
			handle->count = call_count;
			_bt_hf_event_cb(BLUETOOTH_EVENT_HF_CALL_STATUS,
					result, handle,
					event_info->cb, event_info->user_data);
		} else {
			BT_ERR(" Mismatch in call count : %d",call_count);
		}

		__bt_call_list_destroy(handle);
	} else if (strcasecmp(signal_name, "VoiceRecognition") == 0) {
		int status;
		g_variant_get(parameters, "(i)", &status);
		BT_DBG("status = [%d]\n", status);
		if (status)
			_bt_hf_event_cb(BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_ENABLED,
					result, NULL,
					event_info->cb, event_info->user_data);
		else
			_bt_hf_event_cb(BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_DISABLED,
					result, NULL,
					event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "VolumeSpeaker") == 0) {
		int value;
		g_variant_get(parameters, "(i)", &value);
		BT_DBG("Value = [%d]\n", value);
			_bt_hf_event_cb(BLUETOOTH_EVENT_HF_VOLUME_SPEAKER,
					result, &value,
					event_info->cb, event_info->user_data);
	} else if (strcasecmp(signal_name, "SamsungXSAT") == 0) {
		int value = 0;
		char *msg = NULL;
		bluetooth_vendor_dep_at_cmd_t cmd;
		g_variant_get(parameters, "(i&s)", &value, &msg);
		BT_DBG("Value = [%d], message = %s\n", value, msg);
		cmd.app_id =  value;
		cmd.message = msg;
		_bt_hf_event_cb(BLUETOOTH_EVENT_HF_VENDOR_DEP_CMD,
				result, &cmd,
				event_info->cb, event_info->user_data);
	}
	BT_DBG("-\n");
}

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

bt_event_info_t *_bt_event_get_cb_data(int event_type)
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
	const char *interface = BT_EVENT_SERVICE;

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
	case BT_LE_ADAPTER_EVENT:
		event_func = __bt_adapter_le_event_filter;
		path = BT_LE_ADAPTER_PATH;
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
	case BT_AVRCP_CONTROL_EVENT:
		event_func = __bt_avrcp_control_event_filter;
		path = BT_AVRCP_CONTROL_PATH;
		break;
	case BT_OPP_CLIENT_EVENT:
		event_func = __bt_opp_client_event_filter;
		path = BT_OPP_CLIENT_PATH;
		break;
	case BT_OPP_SERVER_EVENT:
		event_func = __bt_opp_server_event_filter;
		path = BT_OPP_SERVER_PATH;
		break;
	case BT_PBAP_CLIENT_EVENT:
		event_func = __bt_pbap_client_event_filter;
		path = BT_PBAP_CLIENT_PATH;
		break;
	case BT_RFCOMM_CLIENT_EVENT:
		event_func = __bt_rfcomm_client_event_filter;
		path = BT_RFCOMM_CLIENT_PATH;
		break;
	case BT_RFCOMM_SERVER_EVENT:
		event_func = __bt_rfcomm_server_event_filter;
		path = BT_RFCOMM_SERVER_PATH;
		break;
	case BT_HF_AGENT_EVENT:
		BT_DBG("BT_HF_AGENT_EVENT\n");
		event_func = __bt_hf_agent_event_filter;
		path = BT_HF_AGENT_PATH;
		interface = BT_HF_SERVICE_INTERFACE;
		break;
	case BT_A2DP_SOURCE_EVENT:
		BT_DBG("BT_A2DP_SOURCE_EVENT");
		event_func = __bt_a2dp_source_event_filter;
		path = BT_A2DP_SOURCE_PATH;
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
				NULL, interface, NULL, path, NULL, 0,
				event_func, cb_data, NULL);

	event_list = g_slist_append(event_list, cb_data);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_event(int event_type)
{
	GDBusConnection *connection_type;
	bt_event_info_t *cb_data;

	if (is_initialized == FALSE) {
		BT_ERR("Event is not registered");
		return BLUETOOTH_ERROR_NOT_INITIALIZED;
	}

	if (__bt_event_is_registered(event_type) == FALSE) {
		BT_ERR("Not registered event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	cb_data = _bt_event_get_cb_data(event_type);

	if (cb_data == NULL) {
		BT_ERR("No matched event data");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	connection_type = cb_data->conn;

	event_list = g_slist_remove(event_list, (void *)cb_data);

	retv_if(connection_type == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_connection_signal_unsubscribe(connection_type, cb_data->id);

	__bt_event_data_free((void *)cb_data);

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
		BT_DBG("bt-service is terminated");
		event_info = _bt_event_get_cb_data(BT_ADAPTER_EVENT);
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
