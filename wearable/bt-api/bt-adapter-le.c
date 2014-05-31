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

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"


BT_EXPORT_API int bluetooth_set_advertising(gboolean enable)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &enable, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_ADVERTISING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_custom_advertising(gboolean enable,
						float interval_min, float interval_max,
						guint8 filter_policy)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &enable, sizeof(gboolean));
	g_array_append_vals(in_param2, &interval_min, sizeof(float));
	g_array_append_vals(in_param3, &interval_max, sizeof(float));
	g_array_append_vals(in_param4, &filter_policy, sizeof(guint8));

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
	BT_CHECK_ENABLED(return);

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

BT_EXPORT_API int bluetooth_set_advertising_data(const bluetooth_advertising_data_t *value, int length)
{
	int result;

	BT_CHECK_PARAMETER(value, return);
	BT_CHECK_ENABLED(return);

	if (length > BLUETOOTH_ADVERTISE_DATA_LENGTH_MAX - 3)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, value, sizeof(bluetooth_advertising_data_t));
	g_array_append_vals(in_param2, &length, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_ADVERTISING_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_scan_response_data(
			const bluetooth_scan_resp_data_t *value, int length)
{
	int result;

	BT_CHECK_PARAMETER(value, return);
	BT_CHECK_ENABLED(return);

	if (length > BLUETOOTH_SCAN_RESP_DATA_LENGTH_MAX)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, value, length);
	g_array_append_vals(in_param2, &length, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_SCAN_RESPONSE_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_is_advertising(gboolean *is_advertising)
{
	int result;

	BT_CHECK_ENABLED(return);

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

BT_EXPORT_API int bluetooth_add_white_list(bluetooth_device_address_t *address)
{
	int result;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_ADD_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_remove_white_list(bluetooth_device_address_t *address)
{
	int result;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_REMOVE_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_clear_white_list(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_CLEAR_WHITE_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

