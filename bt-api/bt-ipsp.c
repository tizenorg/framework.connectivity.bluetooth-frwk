/*
 * Bluetooth-frwk
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Paras Kumar <paras.kumar@samsung.com>
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
#include "bluetooth-ipsp-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

BT_EXPORT_API int bluetooth_le_ipsp_init(void)
{
	int ret = IPSP_ERROR_NONE;

	BT_CHECK_ENABLED_LE(return);

	BT_INIT_PARAMS();

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	ret = _bt_send_request(BT_BLUEZ_SERVICE, BT_LE_IPSP_INIT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return ret;
}

BT_EXPORT_API int bluetooth_le_ipsp_deinit(void)
{
	int ret = IPSP_ERROR_NONE;

	BT_CHECK_ENABLED_LE(return);

	BT_INIT_PARAMS();

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	ret = _bt_send_request(BT_BLUEZ_SERVICE, BT_LE_IPSP_DEINIT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return ret;
}

BT_EXPORT_API int bluetooth_le_ipsp_connect(const ipsp_device_address_t *device_address)
{
	int ret = IPSP_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED_LE(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(ipsp_device_address_t));

	ret = _bt_send_request(BT_BLUEZ_SERVICE, BT_LE_IPSP_CONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return ret;
}

BT_EXPORT_API int bluetooth_le_ipsp_disconnect(const ipsp_device_address_t *device_address)
{
	int ret = IPSP_ERROR_NONE;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED_LE(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(ipsp_device_address_t));

	ret = _bt_send_request(BT_BLUEZ_SERVICE, BT_LE_IPSP_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return ret;
}
