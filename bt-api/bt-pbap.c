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
#include <vconf.h>
#include <syspopup_caller.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

static char is_pbap_initialized = FALSE;

#define BT_CHECK_PBAP_INITIALIZED(func) \
	if (is_pbap_initialized == FALSE) \
	{ \
		BT_ERR("BT PBAP Client is not initiatized"); \
		func BLUETOOTH_ERROR_NOT_INITIALIZED; \
	} \


BT_EXPORT_API int bluetooth_pbap_init(void)
{
	bt_user_info_t *user_info;
	int ret;

	BT_CHECK_ENABLED(return);
	if (is_pbap_initialized)
		return BLUETOOTH_ERROR_ALREADY_INITIALIZED;

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = _bt_register_event(BT_PBAP_CLIENT_EVENT, user_info->cb,
					user_info->user_data);
	if (ret == 0)
		is_pbap_initialized = TRUE;

	return ret;
}

BT_EXPORT_API int bluetooth_pbap_deinit(void)
{
	int ret;
	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	ret = _bt_unregister_event(BT_PBAP_CLIENT_EVENT);

	if (ret == 0)
		is_pbap_initialized = FALSE;

	return ret;
}

BT_EXPORT_API int bluetooth_pbap_connect(const bluetooth_device_address_t *address)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
				sizeof(bluetooth_device_address_t));
	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_CONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_disconnect(const bluetooth_device_address_t *address)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
				sizeof(bluetooth_device_address_t));
	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_get_phonebook_size(const bluetooth_device_address_t *address,
		bt_pbap_folder_t *folder)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));

	g_array_append_vals(in_param2, folder, sizeof(bt_pbap_folder_t));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_GET_PHONEBOOK_SIZE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_get_phonebook(const bluetooth_device_address_t *address,
		bt_pbap_folder_t *folder, bt_pbap_pull_parameters_t *app_param)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
				sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, folder, sizeof(bt_pbap_folder_t));
	g_array_append_vals(in_param3, app_param, sizeof(bt_pbap_pull_parameters_t));
	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_GET_PHONEBOOK,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_get_list(const bluetooth_device_address_t *address,
		bt_pbap_folder_t *folder, bt_pbap_list_parameters_t *app_param)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address,
				sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, folder, sizeof(bt_pbap_folder_t));
	g_array_append_vals(in_param3, app_param, sizeof(bt_pbap_list_parameters_t));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_GET_LIST,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_pull_vcard(const bluetooth_device_address_t *address,
		bt_pbap_folder_t *folder, bt_pbap_pull_vcard_parameters_t *app_param)
{
	BT_DBG("+");
	int result;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, folder, sizeof(bt_pbap_folder_t));
	g_array_append_vals(in_param3, app_param, sizeof(bt_pbap_pull_vcard_parameters_t));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_PULL_VCARD,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_pbap_phonebook_search(const bluetooth_device_address_t *address,
		bt_pbap_folder_t *folder, bt_pbap_search_parameters_t *app_param)
{
	BT_DBG("+");
	int result;
	BT_CHECK_ENABLED(return);
	BT_CHECK_PBAP_INITIALIZED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, address, sizeof(bluetooth_device_address_t));
	g_array_append_vals(in_param2, folder, sizeof(bt_pbap_folder_t));
	g_array_append_vals(in_param3, app_param, sizeof(bt_pbap_search_parameters_t));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_PBAP_PHONEBOOK_SEARCH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("-");
	return result;
}
