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
#include <vconf.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

static gboolean is_le_scanning = FALSE;

BT_EXPORT_API int bluetooth_check_adapter_le(void)
{
	int ret;
	int value;

	ret = _bt_get_adapter_path(_bt_gdbus_get_system_gconn(), NULL);

	if (ret != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ADAPTER_LE_DISABLED;
	}

	ret = vconf_get_int(VCONFKEY_BT_LE_STATUS, &value);
	if (ret != 0) {
		BT_ERR("fail to get vconf key!");
		return ret;
	}

	BT_DBG("value : %d", value);
	return value == VCONFKEY_BT_LE_STATUS_ON ? BLUETOOTH_ADAPTER_LE_ENABLED :
						BLUETOOTH_ADAPTER_LE_DISABLED;
}

BT_EXPORT_API int bluetooth_enable_adapter_le(void)
{
	int result;

	retv_if(bluetooth_check_adapter_le() == BLUETOOTH_ADAPTER_LE_ENABLED,
				BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_ENABLE_ADAPTER_LE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	return result;
}

BT_EXPORT_API int bluetooth_disable_adapter_le(void)
{
	int result;
	retv_if(bluetooth_check_adapter_le() == BLUETOOTH_ADAPTER_LE_DISABLED,
				BLUETOOTH_ERROR_DEVICE_NOT_ENABLED);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_DISABLE_ADAPTER_LE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

void _bt_set_le_scan_status(gboolean mode)
{
	BT_DBG("set LE scan mode : %d", mode);
	is_le_scanning = mode;
}

BT_EXPORT_API gboolean bluetooth_is_le_scanning(void)
{
	return is_le_scanning;
}

BT_EXPORT_API int bluetooth_start_le_discovery(void)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_START_LE_DISCOVERY,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	if (result == BLUETOOTH_ERROR_NONE)
		_bt_set_le_scan_status(TRUE);

	return result;
}

BT_EXPORT_API int bluetooth_stop_le_discovery(void)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_STOP_LE_DISCOVERY,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	if (result == BLUETOOTH_ERROR_NONE)
		_bt_set_le_scan_status(FALSE);

	return result;
}

BT_EXPORT_API int bluetooth_is_le_discovering(void)
{
	int result;
	int is_discovering = FALSE;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_IS_LE_DISCOVERYING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		is_discovering = g_array_index(out_param,
				int, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return is_discovering;
}

BT_EXPORT_API int bluetooth_register_scan_filter(bluetooth_le_scan_filter_t *filter, int *slot_id)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, filter, sizeof(bluetooth_le_scan_filter_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_REGISTER_SCAN_FILTER,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*slot_id = g_array_index(out_param, int, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_unregister_scan_filter(int slot_id)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &slot_id, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_UNREGISTER_SCAN_FILTER,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_unregister_all_scan_filters(void)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_UNREGISTER_ALL_SCAN_FILTERS,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

#ifdef TIZEN_WEARABLE
gboolean __bluetooth_is_privileged_process(void)
{
	FILE *fp= NULL;
	char path[30] = {0, };
	char buf[256] = {0, };

	snprintf(path, sizeof(path), "/proc/%d/cmdline", getpid());
	fp = fopen(path, "r");
	if (fp == NULL)
		return FALSE;

	if (fgets(buf, 256, fp) != NULL) {
		if (strstr(buf, "weconnd") != NULL) {
			fclose(fp);
			return TRUE;
		}
	}

	fclose(fp);
	return FALSE;
}
#endif

BT_EXPORT_API int bluetooth_set_advertising(int handle, gboolean enable)
{
	int result;
	gboolean use_reserved_slot = FALSE;

	BT_CHECK_ENABLED_ANY(return);

#ifdef TIZEN_WEARABLE
	use_reserved_slot = __bluetooth_is_privileged_process();
#endif

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &handle, sizeof(int));
	g_array_append_vals(in_param2, &enable, sizeof(gboolean));
	g_array_append_vals(in_param3, &use_reserved_slot, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_ADVERTISING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_custom_advertising(int handle, gboolean enable,
						bluetooth_advertising_params_t *params)
{
	int result;
	gboolean use_reserved_slot = FALSE;

	BT_CHECK_ENABLED_ANY(return);

#ifdef TIZEN_WEARABLE
	use_reserved_slot = __bluetooth_is_privileged_process();
#endif

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &handle, sizeof(int));
	g_array_append_vals(in_param2, &enable, sizeof(gboolean));
	g_array_append_vals(in_param3, params, sizeof(bluetooth_advertising_params_t));
	g_array_append_vals(in_param4, &use_reserved_slot, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_CUSTOM_ADVERTISING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_advertising_data(bluetooth_advertising_data_t *adv_data, int *length)
{
	int result;
	guint8 *data;

	BT_CHECK_PARAMETER(adv_data, return);
	BT_CHECK_PARAMETER(length, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_ADVERTISING_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		data = &g_array_index(out_param, guint8, 0);
		*length = out_param->len;

		memset(adv_data, 0x00, sizeof(bluetooth_advertising_data_t));
		memcpy(adv_data->data, data, *length);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_advertising_data(int handle, const bluetooth_advertising_data_t *value, int length)
{
	int result;
	gboolean use_reserved_slot = FALSE;

	BT_CHECK_PARAMETER(value, return);
	BT_CHECK_ENABLED_ANY(return);

	if (length > BLUETOOTH_ADVERTISING_DATA_LENGTH_MAX - 3)
		return BLUETOOTH_ERROR_INVALID_PARAM;

#ifdef TIZEN_WEARABLE
	use_reserved_slot = __bluetooth_is_privileged_process();
#endif

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &handle, sizeof(int));
	g_array_append_vals(in_param2, value, sizeof(bluetooth_advertising_data_t));
	g_array_append_vals(in_param3, &length, sizeof(int));
	g_array_append_vals(in_param4, &use_reserved_slot, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_ADVERTISING_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_scan_response_data(bluetooth_scan_resp_data_t *value, int *length)
{
	int result;
	guint8 *data;

	BT_CHECK_PARAMETER(value, return);
	BT_CHECK_PARAMETER(length, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_SCAN_RESPONSE_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		data = &g_array_index(out_param, guint8, 0);
		*length = out_param->len;

		memset(value, 0x00, sizeof(bluetooth_scan_resp_data_t));
		memcpy(value->data, data, *length);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_scan_response_data(int handle,
			const bluetooth_scan_resp_data_t *value, int length)
{
	int result;
	gboolean use_reserved_slot = FALSE;

	BT_CHECK_PARAMETER(value, return);
	BT_CHECK_ENABLED_ANY(return);

	if (length > BLUETOOTH_SCAN_RESP_DATA_LENGTH_MAX)
		return BLUETOOTH_ERROR_INVALID_PARAM;

#ifdef TIZEN_WEARABLE
	use_reserved_slot = __bluetooth_is_privileged_process();
#endif

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &handle, sizeof(int));
	g_array_append_vals(in_param2, value, length);
	g_array_append_vals(in_param3, &length, sizeof(int));
	g_array_append_vals(in_param4, &use_reserved_slot, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_SCAN_RESPONSE_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_scan_parameters(bluetooth_le_scan_params_t *params)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, params, sizeof(bluetooth_le_scan_params_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_SCAN_PARAMETERS,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_is_advertising(gboolean *is_advertising)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_IS_ADVERTISING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*is_advertising = g_array_index(out_param, int, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_add_white_list(bluetooth_device_address_t *address, bluetooth_device_address_type_t address_type)
{
	int result;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &address_type, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_ADD_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_remove_white_list(bluetooth_device_address_t *address, bluetooth_device_address_type_t address_type)
{
	int result;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &address_type, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_REMOVE_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_clear_white_list(void)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_CLEAR_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_enable_le_privacy(gboolean enable_privacy)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &enable_privacy, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_LE_PRIVACY,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_check_privilege_advertising_parameter(void)
{
	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_SET_ADVERTISING_PARAMETERS)
		     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_le_register_callback(bluetooth_cb_func_ptr callback_ptr, void *user_data)
{
	int ret;

	ret = _bt_register_event(BT_LE_ADAPTER_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE &&
	    ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to register BT_LE_ADAPTER_EVENT event : %d", ret);
		return ret;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_le_unregister_callback(void)
{
	_bt_unregister_event(BT_LE_ADAPTER_EVENT);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_le_read_maximum_data_length(
			bluetooth_le_read_maximum_data_length_t *max_le_datalength)
{
	BT_CHECK_ENABLED_ANY(return);
	BT_INIT_PARAMS();
	int result;
	bluetooth_le_read_maximum_data_length_t *datalength = NULL;

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE,
		BT_LE_READ_MAXIMUM_DATA_LENGTH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		datalength = &g_array_index(out_param,
			bluetooth_le_read_maximum_data_length_t, 0);
		max_le_datalength->max_tx_octets  = datalength->max_tx_octets;
		max_le_datalength->max_tx_time = datalength->max_tx_time;
		max_le_datalength->max_rx_octets = datalength->max_rx_octets;
		max_le_datalength->max_rx_time = datalength->max_rx_time;
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_le_write_host_suggested_default_data_length(
	const unsigned int def_tx_Octets, const unsigned int def_tx_Time)
{
	BT_CHECK_ENABLED_ANY(return);
	BT_INIT_PARAMS();

	int result;

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &def_tx_Octets, sizeof(guint));
	g_array_append_vals(in_param2, &def_tx_Time, sizeof(guint));

	result = _bt_send_request(BT_BLUEZ_SERVICE,
		BT_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to Write the host suggested default data length values : %d", result);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_le_read_suggested_default_data_length(
	bluetooth_le_read_host_suggested_data_length_t *le_data_length)
{
	BT_CHECK_ENABLED_ANY(return);
	BT_INIT_PARAMS();

	int result;
	bluetooth_le_read_host_suggested_data_length_t *data_values = NULL;
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE,
		BT_LE_READ_HOST_SUGGESTED_DATA_LENGTH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		data_values = &g_array_index(out_param,
			bluetooth_le_read_host_suggested_data_length_t, 0);

		le_data_length->def_tx_octets = data_values->def_tx_octets;
		le_data_length->def_tx_time = data_values->def_tx_time;
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_le_set_data_length(bluetooth_device_address_t *address,
	const unsigned int max_tx_octets, const unsigned int max_tx_time)
{
	BT_CHECK_ENABLED_ANY(return);
	BT_INIT_PARAMS();

	int result;

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &max_tx_octets, sizeof(guint));
	g_array_append_vals(in_param3, &max_tx_time, sizeof(guint));

	result = _bt_send_request(BT_BLUEZ_SERVICE,
		BT_LE_SET_DATA_LENGTH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to Set data length values : %d", result);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}
