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

BT_EXPORT_API int bluetooth_oob_read_local_data(bt_oob_data_t *local_oob_data)
{
	int result;

	BT_CHECK_PARAMETER(local_oob_data, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_OOB_READ_LOCAL_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*local_oob_data = g_array_index(out_param,
			bt_oob_data_t, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_oob_add_remote_data(
			const bluetooth_device_address_t *remote_device_address,
			bt_oob_data_t *remote_oob_data)
{
	int result;

	BT_CHECK_PARAMETER(remote_device_address, return);
	BT_CHECK_PARAMETER(remote_oob_data, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_device_address,
				sizeof(bluetooth_device_address_t));

	g_array_append_vals(in_param2, remote_oob_data, sizeof(bt_oob_data_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_OOB_ADD_REMOTE_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_oob_remove_remote_data(
			const bluetooth_device_address_t *remote_device_address)
{
	int result;

	BT_CHECK_PARAMETER(remote_device_address, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_device_address,
				sizeof(bluetooth_device_address_t));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_OOB_REMOVE_REMOTE_DATA,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

