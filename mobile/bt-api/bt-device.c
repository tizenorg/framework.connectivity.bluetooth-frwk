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

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

BT_EXPORT_API int bluetooth_bond_device(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_BOND_DEVICE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_cancel_bonding(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_CANCEL_BONDING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_unbond_device(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_UNBOND_DEVICE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_bonded_device(const bluetooth_device_address_t *device_address,
					      bluetooth_device_info_t *dev_info)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(dev_info, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_BONDED_DEVICE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		if (out_param->len > 0) {
			*dev_info = g_array_index(out_param,
					bluetooth_device_info_t, 0);
		} else {
			BT_DBG("out_param length is 0!!");
		}
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_remote_device(const bluetooth_device_address_t *device_address)
{
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_search_service(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_SEARCH_SERVICE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_cancel_service_search(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_CANCEL_SEARCH_SERVICE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_alias(const bluetooth_device_address_t *device_address,
				      const char *alias)
{
	int result;
	char alias_name[BLUETOOTH_DEVICE_NAME_LENGTH_MAX];

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(alias, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_strlcpy(alias_name, alias, sizeof(alias_name));
	g_array_append_vals(in_param2, alias_name, BLUETOOTH_DEVICE_NAME_LENGTH_MAX);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_ALIAS,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_authorize_device(const bluetooth_device_address_t *device_address,
					     gboolean authorized)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &authorized, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_AUTHORIZATION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_is_device_connected(const bluetooth_device_address_t *device_address,
				bluetooth_service_type_t type,
				gboolean *is_connected)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(is_connected, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &type, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_IS_DEVICE_CONNECTED,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*is_connected = g_array_index(out_param, gboolean, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_connect_le(const bluetooth_device_address_t *device_address)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_CONNECT_LE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_disconnect_le(const bluetooth_device_address_t *device_address)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_DISCONNECT_LE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_read_rssi(const bluetooth_device_address_t *device_address)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_READ_RSSI,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}
