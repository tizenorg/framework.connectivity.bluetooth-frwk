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
#include "bluetooth-hid-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

BT_EXPORT_API int bluetooth_hid_init(hid_cb_func_ptr callback_ptr, void *user_data)
{
	int ret;

	/* Register AVRCP events */
	ret = _bt_register_event(BT_HID_EVENT , (void *)callback_ptr, user_data);

	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to init the event handler");
		return ret;
	}

	_bt_set_user_data(BT_HID, (void *)callback_ptr, user_data);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hid_deinit(void)
{
	int ret;

	ret = _bt_unregister_event(BT_HID_EVENT);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to deinit the event handler");
		return ret;
	}

	_bt_set_user_data(BT_HID, NULL, NULL);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hid_connect(hid_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_CONNECT)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	user_info = _bt_get_user_data(BT_HID);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_HID_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_hid_disconnect(hid_device_address_t *device_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_ENABLED(return);

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_DISCONNECT)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	user_info = _bt_get_user_data(BT_HID);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, device_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_HID_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

