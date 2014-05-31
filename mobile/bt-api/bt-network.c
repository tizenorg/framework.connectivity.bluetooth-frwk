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

BT_EXPORT_API int bluetooth_network_activate_server()
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_NETWORK_ACTIVATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_network_deactivate_server(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_NETWORK_DEACTIVATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_network_connect(const bluetooth_device_address_t *device_address,
						bluetooth_network_role_t role,
						char *custom_uuid)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address,
				sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, &role, sizeof(int));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_NETWORK_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_network_disconnect(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address,
					sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_NETWORK_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_network_server_disconnect(const bluetooth_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address,
					sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE,
				BT_NETWORK_SERVER_DISCONNECT,
				in_param1, in_param2, in_param3, in_param4,
				user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

