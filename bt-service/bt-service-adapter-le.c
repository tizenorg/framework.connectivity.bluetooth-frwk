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
//#include <dbus/dbus-glib.h>
//#include <dbus/dbus.h>
#include <gio/gio.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <vconf.h>
#include <syspopup_caller.h>
#include <aul.h>

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-adapter.h"
#include "bt-service-adapter-le.h"


#define BT_ADV_INTERVAL_MIN 20 /* msec */
#define BT_ADV_INTERVAL_MAX 10240
#define BT_ADV_INTERVAL_SPLIT 0.625
#define BT_DEFAULT_ADV_MIN_INTERVAL 500
#define BT_DEFAULT_ADV_MAX_INTERVAL 500
#define BT_ADV_FILTER_POLICY_DEFAULT	0x00
#define BT_ADV_TYPE_DEFAULT	0x00
#define BT_ADV_FILTER_POLICY_ALLOW_SCAN_CONN_WL_ONLY	0x03

typedef struct {
	int adv_inst_max;
	int rpa_offloading;
	int max_filter;
} bt_adapter_le_feature_info_t;

typedef struct {
	char *sender;
	int adv_handle;
	gboolean is_advertising;
} bt_adapter_le_adv_slot_t;

typedef struct {
	char *sender;
	GSList *filter_list;
	gboolean is_scanning;
} bt_adapter_le_scanner_t;

static bluetooth_advertising_params_t adv_params = {
	BT_DEFAULT_ADV_MIN_INTERVAL,
	BT_DEFAULT_ADV_MAX_INTERVAL,
	BT_ADV_FILTER_POLICY_DEFAULT,
	BT_ADV_TYPE_DEFAULT};
static bluetooth_advertising_data_t adv_data = { {0} };
static int adv_data_len;
static bluetooth_scan_resp_data_t resp_data = { {0} };
static int resp_data_len;

static bt_adapter_le_feature_info_t le_feature_info = { 1, 0, 0 };
static bt_adapter_le_adv_slot_t *le_adv_slot = NULL;

GSList *scanner_list = NULL;
static gboolean is_le_set_scan_parameter = FALSE;
static gboolean is_le_scanning = FALSE;
static gboolean scan_filter_enabled = FALSE;
static bt_le_scan_type_t le_scan_type = BT_LE_PASSIVE_SCAN;

static GSList *gatt_client_senders = NULL;


gboolean _bt_is_set_scan_parameter(void)
{
	return is_le_set_scan_parameter;
}

void _bt_init_gatt_client_senders(void)
{
	_bt_clear_request_list();
}

int _bt_insert_gatt_client_sender(char *sender)
{
	char *info;

	retv_if(sender == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	info = g_strdup(sender);
	retv_if(info == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	gatt_client_senders = g_slist_append(gatt_client_senders, info);

	BT_DBG("insert sender: %s", sender);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_delete_gatt_client_sender(char *sender)
{
	GSList *l;
	char *info;

	BT_DBG("remove sender: %s", sender);

	for (l = gatt_client_senders; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (g_strcmp0(info, sender) == 0) {
			BT_DBG("remove info");
			gatt_client_senders = g_slist_remove(gatt_client_senders, info);
			g_free(info);
			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NOT_FOUND;
}

void _bt_clear_gatt_client_senders(void)
{
	if (gatt_client_senders) {
		g_slist_foreach(gatt_client_senders, (GFunc)g_free, NULL);
		g_slist_free(gatt_client_senders);
		gatt_client_senders = NULL;
	}
}

static void __bt_send_foreach_event(gpointer data, gpointer user_data)
{
	char *sender = data;
	GVariant *param = user_data;

	_bt_send_event_to_dest(sender, BT_DEVICE_EVENT,BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
					param);
}

void _bt_send_char_value_changed_event(void *param)
{
	g_slist_foreach(gatt_client_senders, __bt_send_foreach_event,
					(gpointer)param);
}

void __bt_free_le_adv_slot(void)
{
	int i;

	if (le_adv_slot == NULL)
		return;

	for (i = 0; i < le_feature_info.adv_inst_max; i++) {
		if (le_adv_slot[i].sender)
			g_free(le_adv_slot[i].sender);
	}
	g_free(le_adv_slot);
	le_adv_slot = NULL;
}

int _bt_service_adapter_le_init(void)
{
	le_adv_slot = g_malloc0(sizeof(bt_adapter_le_adv_slot_t) * le_feature_info.adv_inst_max);

	return BLUETOOTH_ERROR_NONE;
}

void _bt_service_adapter_le_deinit(void)
{
	__bt_free_le_adv_slot();
}

gboolean _bt_update_le_feature_support(const char *item, const char *value)
{
	if (item== NULL || value == NULL)
		return FALSE;

	if (g_strcmp0(item, "adv_inst_max") == 0) {
		if (atoi(value) != le_feature_info.adv_inst_max) {
			__bt_free_le_adv_slot();
			le_feature_info.adv_inst_max = atoi(value);
			le_adv_slot = g_malloc0(sizeof(bt_adapter_le_adv_slot_t) * le_feature_info.adv_inst_max);
		}
	} else if (g_strcmp0(item, "rpa_offloading") == 0) {
		le_feature_info.rpa_offloading = atoi(value);
	} else if (g_strcmp0(item, "max_filter") == 0) {
		le_feature_info.max_filter = atoi(value);
	} else {
		BT_DBG("No registered item");
		return FALSE;
	}

	return TRUE;
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

int __bt_get_available_adv_slot_id(const char *sender, int adv_handle, gboolean use_reserved_slot)
{
	int i;

	if (le_adv_slot == NULL)
		return -1;

	for (i = 0; i < le_feature_info.adv_inst_max; i++) {
		if (le_adv_slot[i].sender == NULL)
			continue;
		if ((g_strcmp0(le_adv_slot[i].sender, sender) == 0) && (le_adv_slot[i].adv_handle == adv_handle))
			return i;
	}

	if (le_feature_info.adv_inst_max <= 2)
		i = 0;
	else if (le_feature_info.adv_inst_max > 2 && use_reserved_slot == TRUE)
		i = 0;
	else
		i = 2;

	for (; i < le_feature_info.adv_inst_max; i++) {
		if (le_adv_slot[i].sender == NULL)
			return i;
	}

	return -1;
}

void __bt_register_adv_slot_owner(const char *sender, int adv_handle, int slot_id)
{
	if (le_adv_slot[slot_id].sender == NULL) {
		le_adv_slot[slot_id].sender = strdup(sender);
		le_adv_slot[slot_id].adv_handle = adv_handle;
	}
}

void __bt_unregister_adv_slot_owner(int slot_id)
{
	g_free(le_adv_slot[slot_id].sender);
	le_adv_slot[slot_id].sender = NULL;
	le_adv_slot[slot_id].adv_handle = 0;
}

const char* _bt_get_adv_slot_owner(int slot_id)
{
	if (le_adv_slot == NULL)
		return NULL;

	return le_adv_slot[slot_id].sender;
}

int _bt_get_adv_slot_adv_handle(int slot_id)
{
	if (le_adv_slot == NULL)
		return 0;

	return le_adv_slot[slot_id].adv_handle;
}

void _bt_set_advertising_status(int slot_id, gboolean mode)
{
	le_adv_slot[slot_id].is_advertising = mode;
}

gboolean _bt_is_advertising(void)
{
	gboolean status = FALSE;
	int i;

	for (i = 0; i < le_feature_info.adv_inst_max; i++) {
		if (le_adv_slot[i].is_advertising == TRUE)
			status = TRUE;
	}

	return status;
}

void _bt_stop_advertising_by_terminated_process(const char* terminated_name)
{
	int i;

	if (le_adv_slot == NULL)
		return;

	for (i = 0; i < le_feature_info.adv_inst_max; i++) {
		if (le_adv_slot[i].sender != NULL) {
			if (strcasecmp(terminated_name, le_adv_slot[i].sender) == 0) {
				BT_ERR("Stop advertising by terminated process(%s).", terminated_name);
				_bt_set_advertising(terminated_name, le_adv_slot[i].adv_handle, FALSE, FALSE);
			}
		}
	}
}

gboolean _bt_get_advertising_params(bluetooth_advertising_params_t *params)
{
	if (params == NULL)
		return FALSE;

	memcpy(params, &adv_params, sizeof(bluetooth_advertising_params_t));

	return TRUE;
}

int _bt_set_advertising(const char *sender, int adv_handle, gboolean enable, gboolean use_reserved_slot)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	int slot_id;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to start advertising in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	slot_id = __bt_get_available_adv_slot_id(sender, adv_handle, use_reserved_slot);
	if (slot_id == -1) {
		BT_ERR("There is NO available slot!!");
		return BLUETOOTH_ERROR_NO_RESOURCES;
	}

	if (le_adv_slot[slot_id].is_advertising == TRUE && enable == TRUE)
		return BLUETOOTH_ERROR_IN_PROGRESS;

	if (le_adv_slot[slot_id].sender != NULL && le_adv_slot[slot_id].is_advertising == FALSE && enable == FALSE)
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "SetAdvertising",
				g_variant_new("(bi)", enable, slot_id),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error) {
		BT_ERR("SetAdvertising Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (enable == TRUE)
		__bt_register_adv_slot_owner(sender, adv_handle, slot_id);

	le_adv_slot[slot_id].is_advertising = enable;
	BT_INFO("Set advertising [%d]", enable);

	if (ret)
		g_variant_unref(ret);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_custom_advertising(const char *sender, int adv_handle,
				gboolean enable, bluetooth_advertising_params_t *params, gboolean use_reserved_slot)
{
	GDBusProxy *proxy;
	GVariant *ret;
	GError *error = NULL;
	guint32 min = 0;
	guint32 max = 0;
	int slot_id;

	BT_CHECK_PARAMETER(params, return);

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to start advertising in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	slot_id = __bt_get_available_adv_slot_id(sender, adv_handle, use_reserved_slot);
	if (slot_id == -1) {
		BT_ERR("There is NO available slot!!");
		return BLUETOOTH_ERROR_NO_RESOURCES;
	}

	if (le_adv_slot[slot_id].is_advertising == TRUE && enable == TRUE)
		return BLUETOOTH_ERROR_IN_PROGRESS;

	if (le_adv_slot[slot_id].sender != NULL && le_adv_slot[slot_id].is_advertising == FALSE && enable == FALSE)
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;

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

	ret = g_dbus_proxy_call_sync(proxy, "SetAdvertisingParameters",
			g_variant_new("(uuuui)", min, max, 
			params->filter_policy, params->type,
			slot_id), G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, &error);

	if (error) {
		BT_ERR("SetAdvertisingParameters Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	adv_params.interval_min = params->interval_min;
	adv_params.interval_max = params->interval_max;
	adv_params.filter_policy = params->filter_policy;
	adv_params.type= params->type;

	if (ret)
		g_variant_unref(ret);

	ret = g_dbus_proxy_call_sync(proxy, "SetAdvertising",
				g_variant_new("(bi)", enable, slot_id),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error) {
		BT_ERR("SetAdvertising Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (enable == TRUE)
		__bt_register_adv_slot_owner(sender, adv_handle, slot_id);
	else
		__bt_unregister_adv_slot_owner(slot_id);

	le_adv_slot[slot_id].is_advertising = enable;
	BT_INFO_C("Set advertising [%d]", enable);
	if (ret)
		g_variant_unref(ret);

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

int _bt_set_advertising_data(const char *sender, int adv_handle,
				bluetooth_advertising_data_t *adv, int length, gboolean use_reserved_slot)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret, *ad_data, *param = NULL;
	GVariant *temp = NULL;
	GVariantBuilder *builder;
	int i;
	char *old_mdata = NULL;
	char *new_mdata = NULL;
	int old_len = 0;
	int new_len = 0;
	int slot_id;

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

	slot_id = __bt_get_available_adv_slot_id(sender, adv_handle, use_reserved_slot);
	if (slot_id == -1) {
		BT_ERR("There is NO available slot!!");
		return BLUETOOTH_ERROR_NO_RESOURCES;
	}

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", adv->data[i]);
	}

	temp = g_variant_new("ay", builder);
	g_variant_builder_unref(builder);
	ret = g_dbus_proxy_call_sync(proxy, "SetAdvertisingData",
				g_variant_new("(@ayi)", temp, slot_id),
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("SetAdvertisingData Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_register_adv_slot_owner(sender, adv_handle, slot_id);

	__bt_get_ad_data_by_type((char *)adv_data.data, adv_data_len, 0xff,
			&old_mdata, &old_len);
	__bt_get_ad_data_by_type((char *)adv->data, length, 0xff,
			&new_mdata, &new_len);
	if (old_len != new_len ||
			(old_mdata && new_mdata &&
			 memcmp(old_mdata, new_mdata, new_len))) {
	       ad_data = g_variant_new_from_data((const GVariantType *)"ay",
                                            new_mdata, new_len, TRUE, NULL, NULL);
		param = g_variant_new("(@ay)", ad_data);
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_ADVERTISING_MANUFACTURER_DATA_CHANGED,
				param);
	}
	g_free(new_mdata);
	g_free(old_mdata);

	memset(&adv_data, 0x00, sizeof(bluetooth_advertising_data_t));
	memcpy(&adv_data, adv, length);
	adv_data_len = length;

	BT_INFO("Set advertising data");
	if (ret)
		g_variant_unref(ret);

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

int _bt_set_scan_response_data(const char *sender, int adv_handle,
				bluetooth_scan_resp_data_t *response, int length, gboolean use_reserved_slot)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret, *scan_data,  *param = NULL;
	GVariant *temp = NULL;
	GVariantBuilder *builder;
	int i;
	char *old_mdata = NULL;
	char *new_mdata = NULL;
	int old_len = 0;
	int new_len = 0;
	int slot_id;

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

	slot_id = __bt_get_available_adv_slot_id(sender, adv_handle, use_reserved_slot);
	if (slot_id == -1) {
		BT_ERR("There is NO available slot!!");
		return BLUETOOTH_ERROR_NO_RESOURCES;
	}
	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));
	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", response->data[i]);
	}

	temp = g_variant_new("ay", builder);
	g_variant_builder_unref(builder);
	ret = g_dbus_proxy_call_sync(proxy, "SetScanRespData",
				g_variant_new("(@ayi)", temp, slot_id),
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("SetScanRespData Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_register_adv_slot_owner(sender, adv_handle, slot_id);

	/* Compare with previous scan resp data */
	__bt_get_ad_data_by_type((char *)resp_data.data, resp_data_len, 0xff,
			&old_mdata, &old_len);
	__bt_get_ad_data_by_type((char *)response->data, length, 0xff,
			&new_mdata, &new_len);
	if (old_len != new_len ||
			(old_mdata && new_mdata &&
			 memcmp(old_mdata, new_mdata, new_len))) {
		scan_data = g_variant_new_from_data((const GVariantType *)"ay",
                                            new_mdata, new_len, TRUE, NULL, NULL);
		param = g_variant_new("(@ay)", scan_data);
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED,
				param);
	}
	g_free(new_mdata);
	g_free(old_mdata);

	memset(&resp_data, 0x00, sizeof(bluetooth_scan_resp_data_t));
	memcpy(&resp_data, response, length);
	resp_data_len = length;

	if (ret)
		g_variant_unref(ret);
	BT_INFO("Set scan response data");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_scan_parameters(bluetooth_le_scan_params_t *params)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
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

	ret = g_dbus_proxy_call_sync(proxy, "SetScanParameters",
			g_variant_new("(uuu)", params->type, itv, win),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &error);

	if (error) {
		BT_ERR("SetScanParameters Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	_bt_set_le_scan_type(params->type);

	is_le_set_scan_parameter = TRUE;

	if (ret)
		g_variant_unref(ret);
	BT_INFO("Set scan parameters");
	return BLUETOOTH_ERROR_NONE;
}

bt_adapter_le_scanner_t* __bt_find_scanner_from_list(const char *sender)
{
	GSList *l;
	bt_adapter_le_scanner_t *scanner;

	for (l = scanner_list; l != NULL; l = g_slist_next(l)) {
		scanner = l->data;
		if (g_strcmp0(scanner->sender, sender) == 0)
			return scanner;
	}

	return NULL;
}

int __bt_get_available_scan_filter_slot_id(void)
{
	GSList *l;
	bt_adapter_le_scanner_t *scanner;
	GSList *fl;
	bluetooth_le_scan_filter_t *filter_data;
	gboolean *slot_check_list = NULL;
	int i;

	if (le_feature_info.max_filter == 0) {
		BT_ERR("Scan filter is NOT Supported");
		return -1;
	}
	slot_check_list = g_malloc0(sizeof(gboolean) * le_feature_info.max_filter);
	if (slot_check_list == NULL) {
		BT_ERR("Fail to allocate memory");
		return -1;
	}

	for (l = scanner_list; l != NULL; l = g_slist_next(l)) {
		scanner = l->data;
		for (fl = scanner->filter_list; fl != NULL; fl = g_slist_next(fl)) {
			filter_data = fl->data;
			if (filter_data->slot_id < le_feature_info.max_filter) {
				slot_check_list[filter_data->slot_id] = TRUE;
			}
		}
	}

	for (i = 0; i < le_feature_info.max_filter; i++) {
		if (slot_check_list[i] == FALSE) {
			g_free(slot_check_list);
			return i;
		}
	}

	BT_ERR("There is NO available slot for scan filter.");
	g_free(slot_check_list);
	return -1;
}

int _bt_register_scan_filter(const char *sender, bluetooth_le_scan_filter_t *filter, int *slot_id)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret, *param;
	GVariant *arr_uuid_param = NULL, *arr_uuid_mask_param = NULL;
	GVariant *arr_data_param = NULL, *arr_data_mask_param = NULL;
	GArray *arr_uuid = NULL;
	GArray *arr_uuid_mask = NULL;
	GArray *arr_data = NULL;
	GArray *arr_data_mask = NULL;
	bt_adapter_le_scanner_t *scanner = NULL;
	bluetooth_le_scan_filter_t *filter_data = NULL;
	int feature_selection = 0;

	*slot_id = __bt_get_available_scan_filter_slot_id();
	if (*slot_id == -1)
		return BLUETOOTH_ERROR_NO_RESOURCES;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_ADDRESS) {
		char address[BT_ADDRESS_STRING_SIZE] = { 0 };
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_ADDRESS;

		_bt_convert_addr_type_to_string(address, filter->device_address.addr);

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_ADDRESS,	// filter_type
					*slot_id,	// filter_index
					0,	// company_id
					0,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					address,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);	// p_mask

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove",
							param, G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);
	}

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_NAME) {
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_NAME;

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_NAME,	// filter_type
					*slot_id,	// filter_index
					0,	// company_id
					0,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					filter->device_name,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove",
						param, G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);
	}

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_UUID) {
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_UUID;

		arr_uuid = g_array_new(TRUE, TRUE, sizeof(guint8));
		arr_uuid_mask = g_array_new(TRUE, TRUE, sizeof(guint8));

		g_array_append_vals(arr_uuid, filter->service_uuid.data.data, filter->service_uuid.data_len * sizeof(guint8));
		g_array_append_vals(arr_uuid_mask, filter->service_uuid_mask.data.data, filter->service_uuid_mask.data_len * sizeof(guint8));

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_uuid->data, arr_uuid->len, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_uuid_mask->data, arr_uuid_mask->len, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_UUID,	// filter_type
					*slot_id,	// filter_index
					0,	// company_id
					0,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					NULL,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove",
						param, G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);

		g_array_free(arr_uuid, TRUE);
		g_array_free(arr_uuid_mask, TRUE);
		g_array_free(arr_data, TRUE);
		g_array_free(arr_data_mask, TRUE);
	}

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_SOLICITATION_UUID) {
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_SOLICITATION_UUID;

		arr_uuid = g_array_new(TRUE, TRUE, sizeof(guint8));
		arr_uuid_mask = g_array_new(TRUE, TRUE, sizeof(guint8));

		g_array_append_vals(arr_uuid, filter->service_solicitation_uuid.data.data, filter->service_solicitation_uuid.data_len * sizeof(guint8));
		g_array_append_vals(arr_uuid_mask, filter->service_solicitation_uuid_mask.data.data, filter->service_solicitation_uuid_mask.data_len * sizeof(guint8));

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_uuid->data, arr_uuid->len, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_uuid_mask->data, arr_uuid_mask->len, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_SOLICITATION_UUID,	// filter_type
					*slot_id,	// filter_index
					0,	// company_id
					0,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					NULL,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove", param,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);

		g_array_free(arr_uuid, TRUE);
		g_array_free(arr_uuid_mask, TRUE);
	}

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_DATA) {
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_DATA;

		arr_data = g_array_new(TRUE, TRUE, sizeof(guint8));
		arr_data_mask = g_array_new(TRUE, TRUE, sizeof(guint8));

		g_array_append_vals(arr_data, filter->service_data.data.data, filter->service_data.data_len * sizeof(guint8));
		g_array_append_vals(arr_data_mask, filter->service_data_mask.data.data, filter->service_data_mask.data_len * sizeof(guint8));

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_data->data, arr_data->len, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_data_mask->data, arr_data_mask->len, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_DATA,	// filter_type
					*slot_id,	// filter_index
					0,	// company_id
					0,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					NULL,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove", param,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);

		g_array_free(arr_data, TRUE);
		g_array_free(arr_data_mask, TRUE);
	}

	if (filter->added_features & BLUETOOTH_LE_SCAN_FILTER_FEATURE_MANUFACTURER_DATA) {
		feature_selection |= BLUETOOTH_LE_SCAN_FILTER_FEATURE_MANUFACTURER_DATA;

		arr_data = g_array_new(TRUE, TRUE, sizeof(guint8));
		arr_data_mask = g_array_new(TRUE, TRUE, sizeof(guint8));

		g_array_append_vals(arr_data, filter->manufacturer_data.data.data, filter->manufacturer_data.data_len * sizeof(guint8));
		g_array_append_vals(arr_data_mask, filter->manufacturer_data_mask.data.data, filter->manufacturer_data_mask.data_len * sizeof(guint8));

		arr_uuid_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_uuid_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
											NULL, 0, TRUE, NULL, NULL);
		arr_data_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_data->data, arr_data->len, TRUE, NULL, NULL);
		arr_data_mask_param = g_variant_new_from_data((const GVariantType *)"ay",
                                            arr_data_mask->data, arr_data_mask->len, TRUE, NULL, NULL);

		param = g_variant_new("(iiiiii@ay@aysu@ay@ay)",
					0,	// client_if
					0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
					BLUETOOTH_LE_SCAN_FILTER_FEATURE_MANUFACTURER_DATA,	// filter_type
					*slot_id,	// filter_index
					filter->manufacturer_id,	// company_id
					0xFFFF,	// company_id_mask
					arr_uuid_param,	// p_uuid
					arr_uuid_mask_param,	// p_uuid_mask
					NULL,	// string
					0,	// address_type
					arr_data_param,	// p_data
					arr_data_mask_param);

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_add_remove", param,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_add_remove Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);

		g_array_free(arr_data, TRUE);
		g_array_free(arr_data_mask, TRUE);
	}

	BT_DBG("Filter selection %.2x", feature_selection);

	param = g_variant_new("(iiiiiiiiiiii)",
				0,	// client_if
				0,	// action (Add - 0x00, Delete - 0x01, Clear - 0x02)
				*slot_id,	// filter_index
				feature_selection,	// feat_seln
				0,	// list_logic_type (OR - 0x00, AND - 0x01)
				1,	// filt_logic_type (OR - 0x00, AND - 0x01)
				-127,	// rssi_high_thres
				-127,	// rssi_low_thres
				0,	// dely_mode (Immediate - 0x00, on found - 0x01, batched - 0x02)
				0,	// found_timeout
				0,	// lost_timeout
				0);	// found_timeout_cnt
	ret = g_dbus_proxy_call_sync(proxy, "scan_filter_param_setup",
				param, G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("scan_filter_add_remove Fail: %s", error->message);
		g_clear_error(&error);
	}

	scanner = __bt_find_scanner_from_list(sender);
	if (scanner == NULL) {
		scanner = g_malloc0(sizeof(bt_adapter_le_scanner_t));
		if (scanner) {
			scanner->sender = strdup(sender);
			scanner_list = g_slist_append(scanner_list, scanner);
		}
	}

	filter_data = g_malloc0(sizeof(bluetooth_le_scan_filter_t));
	if (filter_data) {
		memcpy(filter_data, filter, sizeof(bluetooth_le_scan_filter_t));
		filter_data->slot_id = *slot_id;

		if (scanner)
			scanner->filter_list = g_slist_append(scanner->filter_list, filter_data);
	}

	if (ret)
		g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_scan_filter(const char *sender, int slot_id)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	bt_adapter_le_scanner_t *scanner = NULL;
	bluetooth_le_scan_filter_t *filter_data = NULL;
	GSList *l;
	gboolean is_slot_id_found = FALSE;

	scanner = __bt_find_scanner_from_list(sender);
	if (scanner == NULL) {
		BT_ERR("There is NO available scanner.");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	for (l = scanner->filter_list; l != NULL; l = g_slist_next(l)) {
		filter_data = l->data;
		if (filter_data->slot_id == slot_id) {
			is_slot_id_found = TRUE;
			break;
		}
	}
	if (is_slot_id_found == FALSE) {
		BT_ERR("There is NO registered slot.");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "scan_filter_clear",
				g_variant_new("(ii)", 0, slot_id),
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("scan_filter_clear Fail: %s", error->message);
		g_clear_error(&error);
	}

	scanner->filter_list = g_slist_remove(scanner->filter_list, filter_data);
	g_free(filter_data);

	if (ret)
		g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_all_scan_filters(const char *sender)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	bt_adapter_le_scanner_t *scanner = NULL;
	bluetooth_le_scan_filter_t *filter_data = NULL;
	GSList *l;

	scanner = __bt_find_scanner_from_list(sender);
	if (scanner == NULL) {
		BT_ERR("There is NO available scanner.");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	for (l = scanner->filter_list; l != NULL; l = g_slist_next(l)) {
		filter_data = l->data;

		ret = g_dbus_proxy_call_sync(proxy, "scan_filter_clear",
					g_variant_new("(ii)", 0, filter_data->slot_id),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

		if (error) {
			BT_ERR("scan_filter_clear Fail: %s", error->message);
			g_clear_error(&error);
		}
		if (ret)
			g_variant_unref(ret);
	}

	g_slist_free_full(scanner->filter_list, g_free);
	scanner->filter_list = NULL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_start_le_scan(const char *sender)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	bt_adapter_le_scanner_t *scanner = __bt_find_scanner_from_list(sender);

	if (scanner == NULL) {
		scanner = g_malloc0(sizeof(bt_adapter_le_scanner_t));
		retv_if(scanner == NULL, BLUETOOTH_ERROR_INTERNAL);

		scanner->sender = strdup(sender);
		scanner_list = g_slist_append(scanner_list, scanner);
	}

	if (scanner->is_scanning == TRUE) {
		BT_ERR("BT is already in LE scanning");
		return BLUETOOTH_ERROR_IN_PROGRESS;
	}
	scanner->is_scanning = TRUE;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (_bt_is_le_scanning()) {
		if (scan_filter_enabled == TRUE) {
			if (scanner->filter_list == NULL) {
				ret = g_dbus_proxy_call_sync(proxy, "scan_filter_enable",
						g_variant_new("(ib)", 0, FALSE),
						G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

				if (error) {
					BT_ERR("scan_filter_clear Fail: %s", error->message);
					g_clear_error(&error);
				}

				if (ret)
					g_variant_unref(ret);
				BT_INFO("Disable LE Scan Filter");
				scan_filter_enabled = FALSE;
			} else {
				BT_INFO("LE Filter Scan is continue");
			}
		} else {
			BT_INFO("LE Full Scan is already on progress");
		}
		return BLUETOOTH_ERROR_NONE;
	} else {
		if (is_le_set_scan_parameter == FALSE) {
			/* Set default scan parameter same with BT_ADAPTER_LE_SCAN_MODE_LOW_ENERGY */
			bluetooth_le_scan_params_t scan_params;
			scan_params.type = BT_LE_ACTIVE_SCAN;
			scan_params.interval = 5000;
			scan_params.window = 500;
			_bt_set_scan_parameters(&scan_params);
		}

		if (scanner->filter_list == NULL) {
			BT_INFO("Start LE Full Scan");
			scan_filter_enabled = FALSE;
		} else {
			ret = g_dbus_proxy_call_sync(proxy, "scan_filter_enable",
					g_variant_new("(ib)", 0, TRUE),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

			if (error) {
				BT_ERR("scan_filter_clear Fail: %s", error->message);
				g_clear_error(&error);
			}

			if (ret)
				g_variant_unref(ret);
			BT_INFO("Enable LE Scan Filter");
			scan_filter_enabled = TRUE;
		}
	}

	ret = g_dbus_proxy_call_sync(proxy, "StartLEDiscovery",
				NULL,G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("StartLEDiscovery Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (ret)
		g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_stop_le_scan(const char *sender)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	bt_adapter_le_scanner_t *scanner = __bt_find_scanner_from_list(sender);
	GSList *l;
	gboolean next_scanning = FALSE;
	gboolean need_scan_filter = TRUE;

	if (scanner == NULL || scanner->is_scanning == FALSE)
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;

	scanner->is_scanning = FALSE;

	for (l = scanner_list; l != NULL; l = g_slist_next(l)) {
		scanner = l->data;
		if (scanner->is_scanning == TRUE) {
			next_scanning = TRUE;
			if (scanner->filter_list == NULL)
				need_scan_filter = FALSE;
		}
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (next_scanning == TRUE) {
		if (scan_filter_enabled == FALSE && need_scan_filter == TRUE) {
			ret = g_dbus_proxy_call_sync(proxy, "scan_filter_enable",
					g_variant_new("(ib)", 0, TRUE),
					G_DBUS_CALL_FLAGS_NONE,
					-1, NULL, &error);

			if (error) {
				BT_ERR("scan_filter_clear Fail: %s", error->message);
				g_clear_error(&error);
			}

			if (ret)
				g_variant_unref(ret);
			BT_INFO("Enable LE Scan Filter");
			scan_filter_enabled = TRUE;
		}
		return BLUETOOTH_ERROR_NONE;
	} else {
		if (scan_filter_enabled == TRUE) {
			ret = g_dbus_proxy_call_sync(proxy, "scan_filter_enable",
					g_variant_new("(ib)", 0, FALSE),
					G_DBUS_CALL_FLAGS_NONE,
					-1, NULL, &error);

			if (error) {
				BT_ERR("scan_filter_clear Fail: %s", error->message);
				g_clear_error(&error);
			}

			if (ret)
				g_variant_unref(ret);
			BT_INFO("Disable LE Scan Filter");
		} else {
			BT_INFO("Just stop LE scan");
		}
	}

	ret = g_dbus_proxy_call_sync(proxy, "StopLEDiscovery",
				NULL,G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);
	if (ret == NULL) {
		BT_ERR("LE Scan stop failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	scan_filter_enabled = FALSE;
	is_le_set_scan_parameter = FALSE;
	if (ret)
		g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

void _bt_disable_all_scanner_status(void)
{
	GSList *l;
	bt_adapter_le_scanner_t *scanner;

	for (l = scanner_list; l != NULL; l = g_slist_next(l)) {
		scanner = l->data;
		scanner->is_scanning = FALSE;
	}
}

void _bt_set_le_scan_status(gboolean mode)
{
	is_le_scanning = mode;
}

gboolean _bt_is_le_scanning(void)
{
	return is_le_scanning;
}

void _bt_set_le_scan_type(bt_le_scan_type_t type)
{
	le_scan_type = type;
}

bt_le_scan_type_t _bt_get_le_scan_type(void)
{
	return le_scan_type;
}

static gboolean __bt_check_scan_result_uuid(const char *adv_data,
		int adv_data_len, const char *svc_uuid, int uuid_len,
		const char *uuid_mask, char ad_type)
{
	char *data = NULL;
	int data_len = 0;
	int i;

	__bt_get_ad_data_by_type((char*)adv_data, adv_data_len,
			ad_type, &data, &data_len);
	if (data != NULL) {
		_bt_swap_byte_ordering(data, data_len);
		for (i = 0; i < data_len; i += uuid_len) {
			if (uuid_len > (data_len - i))
				break;

			if (_bt_byte_arr_cmp_with_mask(data + i,
				svc_uuid, uuid_mask, uuid_len) == 0) {
				g_free(data);
				return TRUE;
			}
		}
		g_free(data);
	}

	return FALSE;
}

static gboolean __bt_check_scan_result_with_filter(const char *device_address,
		const char *adv_data, int adv_data_len,
		const char *scan_data, int scan_data_len,
		const bt_adapter_le_scanner_t *scanner)
{
	GSList *l;
	bluetooth_le_scan_filter_t *filter_data = NULL;
	char *data = NULL;
	int data_len = 0;
	gboolean is_matched = FALSE;

	if (scanner->filter_list == NULL) {
		BT_INFO("This scanner is on Full Scan.");
		return TRUE;
	}

	for (l = scanner->filter_list; l != NULL; l = g_slist_next(l)) {
		filter_data = l->data;

		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_ADDRESS) {
			char address[BT_ADDRESS_STRING_SIZE] = { 0 };

			_bt_convert_addr_type_to_string(address,
					filter_data->device_address.addr);
			if (strncmp(address, device_address,
					BT_ADDRESS_STRING_SIZE) != 0)
				continue;
		}

		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_UUID) {
			is_matched = FALSE;

			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_INCOMP_LIST_16_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_COMP_LIST_16_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_INCOMP_LIST_128_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_COMP_LIST_128_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_INCOMP_LIST_16_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_COMP_LIST_16_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_INCOMP_LIST_128_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_uuid.data.data,
				filter_data->service_uuid.data_len,
				(char*)filter_data->service_uuid_mask.data.data,
				BT_LE_AD_TYPE_COMP_LIST_128_BIT_SERVICE_CLASS_UUIDS)
				== TRUE)
				is_matched = TRUE;

			if (is_matched == FALSE)
				continue;
		}
		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_SOLICITATION_UUID) {
			is_matched = FALSE;

			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_solicitation_uuid.data.data,
				filter_data->service_solicitation_uuid.data_len,
				(char*)filter_data->service_solicitation_uuid_mask.data.data,
				BT_LE_AD_TYPE_LIST_16_BIT_SERVICE_SOLICITATION_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(adv_data,
				adv_data_len,
				(char*)filter_data->service_solicitation_uuid.data.data,
				filter_data->service_solicitation_uuid.data_len,
				(char*)filter_data->service_solicitation_uuid_mask.data.data,
				BT_LE_AD_TYPE_LIST_128_BIT_SERVICE_SOLICITATION_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_solicitation_uuid.data.data,
				filter_data->service_solicitation_uuid.data_len,
				(char*)filter_data->service_solicitation_uuid_mask.data.data,
				BT_LE_AD_TYPE_LIST_16_BIT_SERVICE_SOLICITATION_UUIDS)
				== TRUE)
				is_matched = TRUE;
			if (__bt_check_scan_result_uuid(scan_data,
				scan_data_len,
				(char*)filter_data->service_solicitation_uuid.data.data,
				filter_data->service_solicitation_uuid.data_len,
				(char*)filter_data->service_solicitation_uuid_mask.data.data,
				BT_LE_AD_TYPE_LIST_128_BIT_SERVICE_SOLICITATION_UUIDS)
				== TRUE)
				is_matched = TRUE;

			if (is_matched == FALSE)
				continue;
		}
		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_DEVICE_NAME) {
			char name[BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX] = {0, };
			data = NULL;
			data_len = 0;
			is_matched = FALSE;

			__bt_get_ad_data_by_type((char*)adv_data, adv_data_len,
					BT_LE_AD_TYPE_COMPLETE_LOCAL_NAME,
					&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				memcpy(name, data, data_len);
				name[data_len] = '\0';
				g_free(data);
				data = NULL;
				if (strncmp(filter_data->device_name,
						name, data_len) == 0)
					is_matched = TRUE;
			}
			__bt_get_ad_data_by_type((char*)scan_data,
				scan_data_len,
				BT_LE_AD_TYPE_COMPLETE_LOCAL_NAME,
				&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				memcpy(name, data, data_len);
				name[data_len] = '\0';
				g_free(data);
				data = NULL;
				if (strncmp(filter_data->device_name,
						name, data_len) == 0)
					is_matched = TRUE;
			}

			if (is_matched == FALSE)
				continue;
		}
		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_MANUFACTURER_DATA) {
			data = NULL;
			data_len = 0;
			is_matched = FALSE;

			__bt_get_ad_data_by_type((char*)adv_data,
				adv_data_len,
				BT_LE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
				&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				if (_bt_byte_arr_cmp_with_mask(data,
					(char*)filter_data->manufacturer_data.data.data,
					(char*)filter_data->manufacturer_data_mask.data.data,
					data_len) == 0) {
					is_matched = TRUE;
				}
				g_free(data);
				data = NULL;
			}
			__bt_get_ad_data_by_type((char*)scan_data,
				scan_data_len,
				BT_LE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
				&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				if (_bt_byte_arr_cmp_with_mask(data,
					(char*)filter_data->manufacturer_data.data.data,
					(char*)filter_data->manufacturer_data_mask.data.data,
					data_len) == 0) {
					is_matched = TRUE;
				}
				g_free(data);
				data = NULL;
			}

			if (is_matched == FALSE)
				continue;
		}
		if (filter_data->added_features &
			BLUETOOTH_LE_SCAN_FILTER_FEATURE_SERVICE_DATA) {
			data = NULL;
			data_len = 0;
			is_matched = FALSE;

			__bt_get_ad_data_by_type((char*)adv_data,
				adv_data_len,
				BT_LE_AD_TYPE_SERVICE_DATA,
				&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				if (_bt_byte_arr_cmp_with_mask(data,
					(char*)filter_data->service_data.data.data,
					(char*)filter_data->service_data_mask.data.data,
					data_len) == 0) {
					is_matched = TRUE;
				}
				g_free(data);
				data = NULL;
			}
			__bt_get_ad_data_by_type((char*)scan_data,
				scan_data_len,
				BT_LE_AD_TYPE_SERVICE_DATA,
				&data, &data_len);
			if (data != NULL) {
				if (data_len >= BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX)
					data_len = BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 1;
				if (_bt_byte_arr_cmp_with_mask(data,
					(char*)filter_data->service_data.data.data,
					(char*)filter_data->service_data_mask.data.data,
					data_len) == 0) {
					is_matched = TRUE;
				}
				g_free(data);
				data = NULL;
			}

			if (is_matched == FALSE)
				continue;
		}

		BT_INFO("The scan result is conformable.");
		return TRUE;
	}

	BT_INFO("The scan result is NOT conformable.");
	return FALSE;
}

void _bt_send_scan_result_event(const bt_remote_le_dev_info_t *le_dev_info,
				const bt_le_adv_info_t *adv_info)
{
	int result = BLUETOOTH_ERROR_NONE;
	GSList *l;
	GVariant *scan_data_param, *adv_data_param;
	GVariant *param;
	bt_adapter_le_scanner_t *scanner = NULL;
	const char *adv_data = NULL;
	int adv_data_len = 0;
	const char *scan_data = NULL;
	int scan_data_len = 0;

	ret_if(le_dev_info == NULL);
	if (_bt_get_le_scan_type() == BT_LE_ACTIVE_SCAN)
		ret_if(adv_info == NULL);

	if (_bt_get_le_scan_type() == BT_LE_PASSIVE_SCAN) {
		adv_data = le_dev_info->adv_data;
		adv_data_len = le_dev_info->adv_data_len;
		scan_data = le_dev_info->adv_data;
		scan_data_len = 0;
	} else {
		adv_data = adv_info->data;
		adv_data_len = adv_info->data_len;
		scan_data = le_dev_info->adv_data;
		scan_data_len = le_dev_info->adv_data_len;
	}

	for (l = scanner_list; l != NULL; l = g_slist_next(l)) {
		scanner = l->data;
		if (scanner->is_scanning == FALSE)
			continue;

		if (__bt_check_scan_result_with_filter(le_dev_info->address,
			adv_data, adv_data_len, scan_data, scan_data_len,
			scanner) == FALSE)
			continue;

		adv_data_param = g_variant_new_from_data((const GVariantType *)"ay",
							adv_data, adv_data_len, TRUE, NULL, NULL);
		scan_data_param = g_variant_new_from_data((const GVariantType *)"ay",
							scan_data, scan_data_len, TRUE, NULL, NULL);

		param = g_variant_new("(isnnn@ayn@ay)",
					result,
					le_dev_info->address,
					le_dev_info->addr_type,
					le_dev_info->rssi,
					adv_data_len,
					adv_data_param,
					scan_data_len,
					scan_data_param);

		_bt_send_event_to_dest(scanner->sender, BT_LE_ADAPTER_EVENT,
				BLUETOOTH_EVENT_REMOTE_LE_DEVICE_FOUND, param);
	}
}

int _bt_add_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type)
{
	GDBusProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GError *error = NULL;
	GVariant *ret;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to add white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	BT_CHECK_PARAMETER(device_address, return);

	if (address_type != BLUETOOTH_DEVICE_PUBLIC_ADDRESS &&
		address_type != BLUETOOTH_DEVICE_RANDOM_ADDRESS)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_convert_addr_type_to_string(address, device_address->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "AddDeviceWhiteList",
			  g_variant_new("(su)", address, address_type),
			  G_DBUS_CALL_FLAGS_NONE, -1,
			  NULL, &error);

	if (error) {
		BT_ERR("AddDeviceWhiteList Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (ret)
		g_variant_unref(ret);
	BT_INFO("Add white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_remove_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type)
{
	GDBusProxy *proxy;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	GError *error = NULL;
	GVariant *ret;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to remove white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	BT_CHECK_PARAMETER(device_address, return);

	if (address_type != BLUETOOTH_DEVICE_PUBLIC_ADDRESS &&
		address_type != BLUETOOTH_DEVICE_RANDOM_ADDRESS)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_convert_addr_type_to_string(address, device_address->addr);

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "RemoveDeviceWhiteList",
			  g_variant_new("(su)", address, address_type),
			  G_DBUS_CALL_FLAGS_NONE, -1,
			  NULL, &error);

	if (error) {
		BT_ERR("RemoveDeviceWhiteList Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (ret)
		g_variant_unref(ret);
	BT_INFO("Remove white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_clear_white_list(void)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;

	if (__bt_is_factory_test_mode()) {
		BT_ERR("Unable to clear white list in factory binary !!");
		return BLUETOOTH_ERROR_NOT_SUPPORT;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "ClearDeviceWhiteList",
				NULL,G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

	if (error) {
		BT_ERR("ClearDeviceWhiteList Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (ret)
		g_variant_unref(ret);

	BT_INFO("Clear white list");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_initialize_ipsp(void)
{
	BT_DBG("+");
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "InitializeIpsp",
				NULL,G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);
	if (error) {
		BT_ERR("Initialize IPSP Failed :[%s]", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (ret)
		g_variant_unref(ret);

	BT_INFO("IPSP initialization called successfully");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_deinitialize_ipsp(void)
{
	BT_DBG("+");
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *ret;
	bt_le_status_t le_status = _bt_adapter_get_le_status();

	if (_bt_adapter_get_status() != BT_ACTIVATED &&
		_bt_adapter_get_le_status() != BT_LE_ACTIVATED) {
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_proxy_call_sync(proxy, "DeinitializeIpsp",
				NULL,G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);
	if (error) {
		BT_ERR("De-Initialize IPSP Failed :[%s]", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (ret)
		g_variant_unref(ret);

	BT_INFO("IPSP De-initialization called successfully");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_le_read_maximum_data_length(
		bluetooth_le_read_maximum_data_length_t *max_le_datalength)
{
	GError *error = NULL;
	GDBusProxy *proxy;
	GVariant *reply = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	guint16 max_tx_octets, max_tx_time;
	guint16 max_rx_octets, max_rx_time;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	reply = g_dbus_proxy_call_sync(proxy, "LEReadMaximumDataLength",
			NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	g_object_unref(proxy);

	if (reply == NULL) {
		BT_ERR("LEReadMaximumDataLength dBUS-RPC failed");
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(reply ,"(qqqq)", &max_tx_octets, &max_tx_time,
				&max_rx_octets, &max_rx_time);

	max_le_datalength->max_tx_octets = max_tx_octets;
	max_le_datalength->max_tx_time = max_tx_time;
	max_le_datalength->max_rx_octets = max_rx_octets;
	max_le_datalength->max_rx_time = max_rx_time;

	g_variant_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}
int _bt_le_write_host_suggested_default_data_length(
	const unsigned int def_tx_Octets, const unsigned int def_tx_Time)
{
	GError *error = NULL;
	GDBusProxy *proxy;
	GVariant *reply = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	reply = g_dbus_proxy_call_sync(proxy,
			"LEWriteHostSuggestedDataLength",
			g_variant_new("(qq)", def_tx_Octets, def_tx_Time),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&error);

	g_object_unref(proxy);

	if (reply == NULL) {
		BT_ERR("_bt_le_write_host_suggested_default_data_length dBUS-RPC failed");
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_le_read_host_suggested_default_data_length(
		bluetooth_le_read_host_suggested_data_length_t *def_data_length)
{
	GError *error = NULL;
	GDBusProxy *proxy;
	GVariant *reply = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	guint16 def_tx_octets, def_tx_time;

	proxy = _bt_get_adapter_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	reply = g_dbus_proxy_call_sync(proxy, "LEReadHostSuggestedDataLength",
			NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	if (reply == NULL) {
		BT_ERR("LEReadHostSuggestedDataLength dBUS-RPC failed");
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(reply ,"(qq)", &def_tx_octets, &def_tx_time);

	def_data_length->def_tx_octets = def_tx_octets;
	def_data_length->def_tx_time = def_tx_time;

	g_variant_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_le_set_data_length(bluetooth_device_address_t *device_address,
	const unsigned int max_tx_Octets, const unsigned int max_tx_Time)
{
	GError *error = NULL;
	GDBusProxy *proxy;
	GVariant *reply = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	guint16 txOctets = max_tx_Octets;
	guint16 txTime = max_tx_Time;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GDBusConnection *conn;
	GDBusProxy *device_proxy;

	_bt_convert_addr_type_to_string(address, device_address->addr);

	device_path = _bt_get_device_object_path(&address);

	if (device_path == NULL) {
		BT_DBG("Device path is null");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("conn == NULL");
		g_free(device_path);
		return NULL;
	}

	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								device_path, BT_DEVICE_INTERFACE,  NULL, NULL);

	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(device_proxy,
					"LESetDataLength",
					g_variant_new("(qq)", txOctets, txTime),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

	g_object_unref(device_proxy);

	if (error) {
		 BT_ERR("LESetDataLength error: [%s]", error->message);
		 g_error_free(error);
		 return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}
