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


BT_EXPORT_API int bluetooth_rfcomm_create_socket(const char *uuid)
{
	int result;
	int socket_fd = -1;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX];

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(uuid, return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_strlcpy(uuid_str, uuid, sizeof(uuid_str));
	g_array_append_vals(in_param1, uuid_str, BLUETOOTH_UUID_STRING_MAX);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_CREATE_SOCKET,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	if (result == BLUETOOTH_ERROR_NONE) {
		socket_fd = g_array_index(out_param, int, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return socket_fd;
}

BT_EXPORT_API int bluetooth_rfcomm_remove_socket(int socket_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &socket_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_REMOVE_SOCKET,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	if (result == BLUETOOTH_ERROR_NONE) {
		_bt_remove_server(socket_fd);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_rfcomm_server_disconnect(int socket_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &socket_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_SOCKET_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API gboolean bluetooth_rfcomm_is_server_uuid_available(const char *uuid)
{
	int result;
	gboolean available = TRUE;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX];

	retv_if(uuid == NULL, FALSE);
	retv_if(bluetooth_check_adapter() ==
				BLUETOOTH_ADAPTER_DISABLED, FALSE);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_strlcpy(uuid_str, uuid, sizeof(uuid_str));
	g_array_append_vals(in_param1, uuid_str, BLUETOOTH_UUID_STRING_MAX);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_IS_UUID_AVAILABLE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	if (result == BLUETOOTH_ERROR_NONE) {
		available = g_array_index(out_param, gboolean, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("available: %d", available);

	return available;
}

BT_EXPORT_API int bluetooth_rfcomm_listen_and_accept(int socket_fd, int max_pending_connection)
{
	int result;
	gboolean native_service = TRUE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &socket_fd, sizeof(int));
	g_array_append_vals(in_param2, &max_pending_connection, sizeof(int));
	g_array_append_vals(in_param3, &native_service, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_LISTEN,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_rfcomm_listen(int socket_fd, int max_pending_connection)
{
	int result;
	gboolean native_service = FALSE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &socket_fd, sizeof(int));
	g_array_append_vals(in_param2, &max_pending_connection, sizeof(int));
	g_array_append_vals(in_param3, &native_service, sizeof(gboolean));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_LISTEN,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

        if (result == BLUETOOTH_ERROR_NONE) {
                _bt_add_server(socket_fd);
        }

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_rfcomm_accept_connection(int server_fd, int *client_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &server_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_ACCEPT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	if (result == BLUETOOTH_ERROR_NONE) {
		*client_fd = g_array_index(out_param, int, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_DBG("client_fd: %d", *client_fd);

	return result;
}

BT_EXPORT_API int bluetooth_rfcomm_reject_connection(int server_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &server_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_REJECT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

