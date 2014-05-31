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

#include <string.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

BT_EXPORT_API int bluetooth_obex_server_init(const char *dst_path)
{
	int result;
	int app_pid;
	bt_user_info_t *user_info;
	gboolean native_service = TRUE;
	char path[BT_FILE_PATH_MAX];

	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() != BT_NO_SERVER)
		return BLUETOOTH_ERROR_AGENT_ALREADY_EXIST;

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	app_pid = getpid();

	g_strlcpy(path, dst_path, sizeof(path));
	g_array_append_vals(in_param1, path, BT_FILE_PATH_MAX);
	g_array_append_vals(in_param2, &native_service, sizeof(gboolean));
	g_array_append_vals(in_param3, &app_pid, sizeof(int));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_ALLOCATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		_bt_set_obex_server_id(BT_NATIVE_SERVER);
		 _bt_register_event(BT_OPP_SERVER_EVENT, user_info->cb,
		 			user_info->user_data);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_deinit(void)
{
	int result;
	int app_pid;
	gboolean native_service = TRUE;

	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() != BT_NATIVE_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	app_pid = getpid();

	g_array_append_vals(in_param1, &native_service, sizeof(gboolean));
	g_array_append_vals(in_param2, &app_pid, sizeof(int));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_DEALLOCATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	_bt_set_obex_server_id(BT_NO_SERVER);
	 _bt_unregister_event(BT_OPP_SERVER_EVENT);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_init_without_agent(const char *dst_path)
{
	int result;
	int app_pid;
	bt_user_info_t *user_info;
	gboolean native_service = FALSE;
	char path[BT_FILE_PATH_MAX];

	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() != BT_NO_SERVER)
		return BLUETOOTH_ERROR_AGENT_ALREADY_EXIST;

	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	app_pid = getpid();

	g_strlcpy(path, dst_path, sizeof(path));
	g_array_append_vals(in_param1, path, BT_FILE_PATH_MAX);
	g_array_append_vals(in_param2, &native_service, sizeof(gboolean));
	g_array_append_vals(in_param3, &app_pid, sizeof(int));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_ALLOCATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		_bt_set_obex_server_id(BT_CUSTOM_SERVER);
		_bt_register_event(BT_OPP_SERVER_EVENT, user_info->cb,
		 			user_info->user_data);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_deinit_without_agent(void)
{
	int result;
	int app_pid;
	gboolean native_service = FALSE;

	BT_CHECK_ENABLED(return);

	/* Can't call this API after using bluetooth_obex_server_init
	     in same process */
	if (_bt_get_obex_server_id() != BT_CUSTOM_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	app_pid = getpid();

	g_array_append_vals(in_param1, &native_service, sizeof(gboolean));
	g_array_append_vals(in_param2, &app_pid, sizeof(int));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_DEALLOCATE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	_bt_set_obex_server_id(BT_NO_SERVER);
	_bt_unregister_event(BT_OPP_SERVER_EVENT);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API gboolean bluetooth_obex_server_is_activated(void)
{
	int result;
	gboolean is_activated = FALSE;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_IS_ACTIVATED,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		is_activated = g_array_index(out_param, gboolean, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return is_activated;
}

BT_EXPORT_API int bluetooth_obex_server_accept_connection(void)
{
	int result;

	/* Can't use this API in native server
	    In native server, bt-agent will control the connection
	    using system popup */
	if (_bt_get_obex_server_id() != BT_CUSTOM_SERVER)
		return BLUETOOTH_ERROR_INTERNAL;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_ACCEPT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_reject_connection(void)
{
	int result;

	/* Can't use this API in native server
	    In native server, bt-agent will control the connection
	    using system popup */
	if (_bt_get_obex_server_id() != BT_CUSTOM_SERVER)
		return BLUETOOTH_ERROR_INTERNAL;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_REJECT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_accept_authorize(const char *filename)
{
	int result;
	char name[BT_FILE_PATH_MAX];

	BT_CHECK_PARAMETER(filename, return);
	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() != BT_NATIVE_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_strlcpy(name, filename, sizeof(name));
	g_array_append_vals(in_param1, name, BT_FILE_PATH_MAX);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_ACCEPT_FILE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}


BT_EXPORT_API int bluetooth_obex_server_reject_authorize(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() != BT_NATIVE_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_REJECT_FILE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_set_destination_path(const char *dst_path)
{
	int result;
	int server_id;
	gboolean native_service = FALSE;
	char path[BT_FILE_PATH_MAX];

	BT_CHECK_PARAMETER(dst_path, return);
	BT_CHECK_ENABLED(return);

	server_id = _bt_get_obex_server_id();

	retv_if(server_id == BT_NO_SERVER,
			BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	native_service = (server_id == BT_NATIVE_SERVER) ? TRUE : FALSE;

	g_strlcpy(path, dst_path, sizeof(path));
	g_array_append_vals(in_param1, path, BT_FILE_PATH_MAX);
	g_array_append_vals(in_param2, &native_service, sizeof(native_service));

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_SET_PATH,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}


BT_EXPORT_API int bluetooth_obex_server_set_root(const char *root)
{
	int result;
	char root_path[BT_FILE_PATH_MAX];

	BT_CHECK_PARAMETER(root, return);
	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() == BT_NO_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_strlcpy(root_path, root, sizeof(root_path));
	g_array_append_vals(in_param1, root_path, BT_FILE_PATH_MAX);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_SET_ROOT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_cancel_transfer(int transfer_id)
{
	int result;
	int server_type;
	int service_function = BT_OBEX_SERVER_CANCEL_TRANSFER;

	BT_CHECK_ENABLED(return);

	server_type = _bt_get_obex_server_id();

	if (server_type == BT_NO_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;
	else if (server_type == BT_CUSTOM_SERVER)
		service_function = BT_OBEX_SERVER_CANCEL_ALL_TRANSFERS;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &transfer_id, sizeof(int));

	result = _bt_send_request(BT_OBEX_SERVICE, service_function,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_cancel_all_transfers(void)
{
	int result;

	BT_CHECK_ENABLED(return);

	if (_bt_get_obex_server_id() == BT_NO_SERVER)
		return BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_CANCEL_ALL_TRANSFERS,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_obex_server_is_receiving(gboolean *is_receiving)
{
	int result;

	*is_receiving = FALSE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_OBEX_SERVICE, BT_OBEX_SERVER_IS_RECEIVING,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*is_receiving = g_array_index(out_param, gboolean, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

