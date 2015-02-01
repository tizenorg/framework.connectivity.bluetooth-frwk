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

#include <string.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

static void __bt_get_file_size(char **files, unsigned long *file_size, int *count)
{
	int file_count = 0;
	unsigned long size = 0;

	while (files[file_count] != NULL) {
		size = size + strlen(files[file_count]);
		file_count++;
	}

	*count = file_count;
	*file_size = size;
}

BT_EXPORT_API int bluetooth_opc_init(void)
{
	bt_user_info_t *user_info;

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	return _bt_register_event(BT_OPP_CLIENT_EVENT, user_info->cb, user_info->user_data);
}

BT_EXPORT_API int bluetooth_opc_deinit(void)
{
	return _bt_unregister_event(BT_OPP_CLIENT_EVENT);
}

BT_EXPORT_API int bluetooth_opc_push_files(bluetooth_device_address_t *remote_address,
		   		 char **file_name_array)
{
	int result;
	int i;
	int file_count;
	unsigned long size;
	bt_user_info_t *user_info;
	char filename[BT_FILE_PATH_MAX];

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_PARAMETER(file_name_array, return);
	BT_CHECK_ENABLED(return);

	__bt_get_file_size(file_name_array, &size, &file_count);
	retv_if(file_count == 0, BLUETOOTH_ERROR_INVALID_PARAM);

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	for (i = 0; i < file_count; i++) {
		if (strlen(file_name_array[i]) >= sizeof(filename)) {
			BT_ERR("[%s] has too long path.", file_name_array[i]);
			BT_FREE_PARAMS(in_param1, in_param2, in_param3,
				       in_param4, out_param);
			return BLUETOOTH_ERROR_INVALID_PARAM;
		}
		g_strlcpy(filename, file_name_array[i], sizeof(filename));
		g_array_append_vals(in_param2, filename, BT_FILE_PATH_MAX);
	}

	g_array_append_vals(in_param3, &file_count, sizeof(int));

	result = _bt_send_request_async(BT_OBEX_SERVICE, BT_OPP_PUSH_FILES,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_opc_cancel_push(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OPP_CANCEL_PUSH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API gboolean bluetooth_opc_session_is_exist(void)
{
	int result;
	gboolean exist = FALSE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OPP_IS_PUSHING_FILES,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		exist = g_array_index(out_param, gboolean, 0);
		BT_DBG("Exist %d", exist);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_opc_is_sending(gboolean *is_sending)
{
	int result;

	*is_sending = FALSE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OPP_IS_PUSHING_FILES,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*is_sending = g_array_index(out_param, gboolean, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

