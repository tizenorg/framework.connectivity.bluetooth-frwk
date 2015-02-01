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
#ifdef RFCOMM_DIRECT
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <sys/socket.h>
#endif

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

#ifdef RFCOMM_DIRECT

static GSList *rfcomm_nodes;

typedef struct {
	guint object_id;
	gchar *path;
	int id;
	char *uuid;
	int fd;
	GIOChannel *data_io;
	guint data_id;
	bluetooth_device_address_t addr;
	guint disconnect_idle_id;
} rfcomm_info_t;

static rfcomm_info_t *__find_rfcomm_info_with_id(int id)
{
	GSList *l;

	for (l = rfcomm_nodes; l != NULL; l = l->next) {
		rfcomm_info_t *info = l->data;

		if (info->id == id)
			return info;
	}

	return NULL;
}

static rfcomm_info_t *__find_rfcomm_info_with_fd(int fd)
{
	GSList *l;

	for (l = rfcomm_nodes; l != NULL; l = l->next) {
		rfcomm_info_t *info = l->data;

		if (info->fd == fd)
			return info;
	}

	return NULL;
}

static rfcomm_info_t *__find_rfcomm_info_with_path(const gchar *path)
{
	GSList *l;

	for (l = rfcomm_nodes; l != NULL; l = l->next) {
		rfcomm_info_t *info = l->data;

		if (g_strcmp0(info->path, path) == 0)
			return info;
	}

	return NULL;
}

gboolean _check_uuid_path(char *path, char *uuid)
{
	rfcomm_info_t *info = NULL;
	info = __find_rfcomm_info_with_path(path);
	if (!info)
		return FALSE;

	if (strcmp(info->uuid, uuid) == 0)
		return TRUE;

	return FALSE;
}

static void __connected_cb(rfcomm_info_t *info, bt_event_info_t *event_info)
{
	bluetooth_rfcomm_connection_t conn_info;

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_SERVER;
	g_strlcpy(conn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	conn_info.socket_fd = info->fd;
	conn_info.device_addr = info->addr;
	conn_info.server_id = info->id;

	BT_INFO_C("Connected [RFCOMM Server]");
	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
			BLUETOOTH_ERROR_NONE, &conn_info,
			event_info->cb, event_info->user_data);
}

static gboolean __rfcomm_server_disconnect(rfcomm_info_t *info)
{
	bluetooth_rfcomm_disconnection_t disconn_info;
	int fd = info->fd;
	bt_event_info_t *event_info;

	BT_INFO_C("Disconnected [RFCOMM Server]");

	if (info->data_id > 0) {
		g_source_remove(info->data_id);
		info->data_id = 0;
	}

	if (info->fd >= 0) {
		close(info->fd);
		info->fd = -1;
	}

	if (info->data_io) {
		g_io_channel_shutdown(info->data_io, TRUE, NULL);
		g_io_channel_unref(info->data_io);
		info->data_io = NULL;
	}
	info->disconnect_idle_id = 0;
	event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);
	if (event_info == NULL)
		return FALSE;

	memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
	disconn_info.device_role = RFCOMM_ROLE_SERVER;
	g_strlcpy(disconn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	disconn_info.socket_fd = fd;
	disconn_info.device_addr = info->addr;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
			BLUETOOTH_ERROR_NONE, &disconn_info,
			event_info->cb, event_info->user_data);

	BT_DBG("-");
	return FALSE;
}

static gboolean __data_received_cb(GIOChannel *chan, GIOCondition cond,
								gpointer data)
{
	char *buffer = NULL;
	gsize len = 0;
	int result = BLUETOOTH_ERROR_NONE;
	rfcomm_info_t *info = data;
	bt_event_info_t *event_info;
	bluetooth_rfcomm_received_data_t data_r;
	GIOStatus status = G_IO_STATUS_NORMAL;
	GError *err = NULL;

	retv_if(info == NULL, FALSE);

	event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR_C("RFComm Server  disconnected: %d", info->fd);

		if (info->disconnect_idle_id > 0) {
			BT_INFO("Disconnect idle still not process remove source");
			g_source_remove(info->disconnect_idle_id);
			info->disconnect_idle_id = 0;
		}

		__rfcomm_server_disconnect(info);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_LEN + 1);

	status =  g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_LEN,
			&len, &err);
	if (status != G_IO_STATUS_NORMAL) {
		BT_ERR("IO Channel read is failed with %d", status);

		g_free(buffer);
		if (err) {
			BT_ERR("IO Channel read error [%s]", err->message);
			if (status == G_IO_STATUS_ERROR &&
				!g_strcmp0(err->message, "Connection reset by peer")) {
				BT_ERR("cond : %d", cond);
				g_error_free(err);
				if (info->disconnect_idle_id > 0) {
					BT_INFO("Disconnect idle still not process remove source");
					g_source_remove(info->disconnect_idle_id);
					info->disconnect_idle_id = 0;
				}
				__rfcomm_server_disconnect(info);
				return FALSE;
			}
			g_error_free(err);
		}
		return TRUE;
	}

	if (len == 0)
		BT_ERR("Length is zero");

	if (event_info == NULL) {
		g_free(buffer);
		return TRUE;
	}

	data_r.socket_fd = info->fd;
	data_r.buffer_size = len;
	data_r.buffer = buffer;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
			result, &data_r,
			event_info->cb, event_info->user_data);

	g_free(buffer);

	return TRUE;
}

int new_server_connection(const char *path, int fd, bluetooth_device_address_t *addr)
{
	rfcomm_info_t *info;
	bt_event_info_t *event_info;

	BT_DBG("%s %d", path, fd);

	info = __find_rfcomm_info_with_path(path);
	if (info == NULL)
		return -1;

	info->fd = fd;
	memcpy(&info->addr, addr, sizeof(bluetooth_device_address_t));

	info->data_io = g_io_channel_unix_new(info->fd);

	g_io_channel_set_encoding(info->data_io, NULL, NULL);
	g_io_channel_set_flags(info->data_io, G_IO_FLAG_NONBLOCK, NULL);

	info->data_id = g_io_add_watch(info->data_io,
			   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			   __data_received_cb, info);

	event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);
	if (event_info) {
		__connected_cb(info, event_info);
	}

	return 0;
}

static rfcomm_info_t *__register_method()
{
	gchar *path;
	rfcomm_info_t *info;
	guint object_id;
	int id;

	id = __rfcomm_assign_id();
	if (id < 0)
		return NULL;

	path = g_strdup_printf("/org/socket/server/%d/%d", getpid(), id);

	object_id = _bt_register_new_conn(path, new_server_connection);
	if (object_id < 0) {
		__rfcomm_delete_id(id);
		return NULL;
	}
	info = g_new(rfcomm_info_t, 1);
	info->object_id = object_id;
	info->path = path;
	info->id = id;
	info->fd = -1;

	rfcomm_nodes = g_slist_append(rfcomm_nodes, info);

	return info;
}

void free_rfcomm_info(rfcomm_info_t *info)
{
	bt_event_info_t *event_info;

	BT_DBG("");
	if (info->disconnect_idle_id > 0) {
		BT_INFO("Disconnect idle still not process remove source");
		g_source_remove(info->disconnect_idle_id);
		info->disconnect_idle_id = 0;
	}

	__rfcomm_delete_id(info->id);
	_bt_unregister_gdbus(info->object_id);

	if (info->fd >= 0) {
		event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);
		BT_DBG("event type %d", event_info->event_type);
		__rfcomm_server_disconnect(info);
	}

	g_free(info->path);
	g_free(info->uuid);
	g_free(info);
}

void _bt_rfcomm_server_free_all()
{
	BT_DBG("Free all the servers");

	g_slist_free_full(rfcomm_nodes, (GDestroyNotify)free_rfcomm_info);
	rfcomm_nodes = NULL;
}
#endif

BT_EXPORT_API int bluetooth_rfcomm_create_socket(const char *uuid)
{
#ifdef RFCOMM_DIRECT
	rfcomm_info_t *info;
#else
	int result;
	int socket_fd = -1;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX];
#endif

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(uuid, return);
	BT_INFO("UUID Provided %s", uuid);

	if (_bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_CREATE_SOCKET)
		== BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Create socket from app >>>>>>>>>");
	info = __register_method();
	if (info == NULL)
		return -1;

	info->uuid = g_strdup(uuid);
	info->disconnect_idle_id = 0;
	return info->id;
#else

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
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_remove_socket(int socket_fd)
{
#ifdef RFCOMM_DIRECT
	rfcomm_info_t *info;
#else
	int result;
#endif

	BT_CHECK_ENABLED(return);

	if (_bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_REMOVE_SOCKET)
		== BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Remove socket request from app, fd=[%d] >>>>>>>>>>>", socket_fd);

	info = __find_rfcomm_info_with_id(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_unregister_profile(info->path);

	rfcomm_nodes = g_slist_remove(rfcomm_nodes, info);
	free_rfcomm_info(info);

	return BLUETOOTH_ERROR_NONE;
#else
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
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_server_disconnect(int socket_fd)
{
#ifdef RFCOMM_DIRECT
	rfcomm_info_t *info;

	char address[20];

	BT_INFO(">>>>>>>>RFCOMM server disconnect request from APP>>>>>>>>>");

	info = __find_rfcomm_info_with_fd(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (info->data_io == NULL)
		return BLUETOOTH_ERROR_NOT_CONNECTED;

	g_io_channel_shutdown(info->data_io, TRUE, NULL);
	g_io_channel_unref(info->data_io);
	info->data_io = NULL;

	_bt_convert_addr_type_to_string(address, info->addr.addr);
	BT_DBG("Address %s", address);
	_bt_disconnect_profile(address, info->uuid, NULL,NULL);

	info->disconnect_idle_id = g_idle_add(__rfcomm_server_disconnect, info);
	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
#else
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
#endif
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
#ifdef RFCOMM_DIRECT
	GVariant *ret;
	rfcomm_info_t *info;
	GError *error = NULL;
#else
	int result;
	gboolean native_service = TRUE;
#endif

	BT_CHECK_ENABLED(return);

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Listen & accept from app >>>>>>>>>>>");

	info = __find_rfcomm_info_with_id(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	bt_register_profile_info_t profile_info;
	int result;

	profile_info.authentication = TRUE;
	profile_info.authorization = TRUE;
	profile_info.obj_path = info->path;
	profile_info.role = NULL;
	profile_info.service = info->uuid;
	profile_info.uuid = g_strdup(BT_SPP_UUID);
	BT_INFO("uuid %s", profile_info.uuid);
	result = _bt_register_profile(&profile_info, TRUE);

	return result;

	if (ret == NULL) {
		/* dBUS-RPC is failed */
		BT_ERR("dBUS-RPC is failed");

		if (error != NULL) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
			       error->code, error->message);

			g_clear_error(&error);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(ret);

	return BLUETOOTH_ERROR_NONE;
#else
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
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_listen(int socket_fd, int max_pending_connection)
{
#ifdef RFCOMM_DIRECT
	rfcomm_info_t *info;
#else
	int result;
	gboolean native_service = FALSE;
#endif

	BT_CHECK_ENABLED(return);

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Listen >>>>>>>>>>>");

	info = __find_rfcomm_info_with_id(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	bt_register_profile_info_t profile_info;
	int result;

	profile_info.authentication = TRUE;
	profile_info.authorization = TRUE;
	profile_info.obj_path = info->path;
	profile_info.role = "server";
	profile_info.service = info->uuid;
	profile_info.uuid = g_strdup(BT_SPP_UUID);
	BT_INFO("UUID %s", info->uuid);

	result = _bt_register_profile_platform(&profile_info, TRUE);

	return result;

#else
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
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_accept_connection(int server_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &server_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_ACCEPT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_rfcomm_reject_connection(int server_fd)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INFO("+");

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &server_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_REJECT_CONNECTION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

