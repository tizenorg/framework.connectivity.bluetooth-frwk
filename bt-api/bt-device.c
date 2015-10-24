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

BT_EXPORT_API int bluetooth_bond_device(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_BOND_DEVICE)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

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

BT_EXPORT_API int bluetooth_bond_device_by_type(
	const bluetooth_device_address_t *device_address,
	bluetooth_conn_type_t conn_type)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	if(conn_type == BLUETOOTH_DEV_CONN_BREDR)
		BT_CHECK_ENABLED(return);
	else if(conn_type == BLUETOOTH_DEV_CONN_LE)
		BT_CHECK_ENABLED_LE(return);
	else if(conn_type == BLUETOOTH_DEV_CONN_DEFAULT) {
		BT_CHECK_ENABLED(return);
		BT_CHECK_ENABLED_LE(return);
	}

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_BOND_DEVICE_BY_TYPE)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &conn_type, sizeof(unsigned short));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_BOND_DEVICE_BY_TYPE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_cancel_bonding(void)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

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
	BT_CHECK_ENABLED_ANY(return);

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_UNBOND_DEVICE)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

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
	BT_CHECK_ENABLED_ANY(return);

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
			BT_ERR("out_param length is 0!!");
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
	BT_CHECK_ENABLED_ANY(return);

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_SEARCH_SERVICE)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

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

	BT_CHECK_ENABLED_ANY(return);

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
	BT_CHECK_ENABLED_ANY(return);

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
	BT_CHECK_ENABLED_ANY(return);

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
	BT_CHECK_ENABLED_ANY(return);

#ifdef RFCOMM_DIRECT
	if (type & BLUETOOTH_RFCOMM_SERVICE) {
		result = bluetooth_rfcomm_client_is_connected(device_address, is_connected);
		if (*is_connected == FALSE)
			result = bluetooth_rfcomm_server_is_connected(device_address, is_connected);

		return result;
	}
#endif

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

BT_EXPORT_API int bluetooth_connect_le(const bluetooth_device_address_t *device_address, gboolean auto_connect)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &auto_connect, sizeof(gboolean));

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_CONNECT_LE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_disconnect_le(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_DISCONNECT_LE,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_enable_rssi(const bluetooth_device_address_t *remote_address,
		int link_type, bt_rssi_threshold_t *rssi_threshold)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &link_type, sizeof(int));
	g_array_append_vals(in_param3, rssi_threshold, sizeof(bt_rssi_threshold_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_ENABLE_RSSI,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_rssi_strength(const bluetooth_device_address_t *remote_address, int link_type)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &link_type, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_RSSI,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_le_conn_update(const bluetooth_device_address_t *address,
					const bluetooth_le_connection_param_t *parameters)
{
	int result;

	BT_CHECK_ENABLED_ANY(return);
	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(parameters, return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
			sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, parameters,
			sizeof(bluetooth_le_connection_param_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_LE_CONN_UPDATE,
			in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_get_connected_link_type(
		const bluetooth_device_address_t *device_address,
		bluetooth_connected_link_t *connected)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED_ANY(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_CONNECTED_LINK_TYPE,
			in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*connected = g_array_index(out_param, guint, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_set_pin_code(
		const bluetooth_device_address_t *device_address,
		const bluetooth_device_pin_code_t *pin_code)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(pin_code, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, pin_code, sizeof(bluetooth_device_pin_code_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_PIN_CODE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_unset_pin_code(
		const bluetooth_device_address_t *device_address)
{
	int result;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_UNSET_PIN_CODE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_update_le_connection_mode(const bluetooth_device_address_t *address,
		const bluetooth_le_connection_mode_t mode)
{
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(address, return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
			sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &mode,
			sizeof(bluetooth_le_connection_mode_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_UPDATE_LE_CONNECTION_MODE,
			in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_passkey_reply(char *passkey, gboolean reply)
{
	int result;

	char str_passkey[BLUETOOTH_DEVICE_PASSKEY_LENGTH_MAX];

	BT_CHECK_PARAMETER(passkey, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_strlcpy(str_passkey, passkey, sizeof(str_passkey));
	g_array_append_vals(in_param1, str_passkey, BLUETOOTH_DEVICE_PASSKEY_LENGTH_MAX);
	g_array_append_vals(in_param2, &reply, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_PASSKEY_REPLY,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_passkey_confirmation_reply(gboolean reply)
{
	int result;

	BT_CHECK_ENABLED(return);
	BT_INIT_PARAMS();

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	g_array_append_vals(in_param1, &reply, sizeof(gboolean));
	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_PASSKEY_CONFIRMATION_REPLY,
			in_param1, in_param2, in_param3, in_param4, &out_param);
	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}
