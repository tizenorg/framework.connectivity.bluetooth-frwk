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
#include <errno.h>
#include <gio/gunixfdlist.h>
#endif

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

#ifdef RFCOMM_DIRECT

#define BT_TIMEOUT_MESSAGE "Did not receive a reply. Possible causes include: " \
			"the remote application did not send a reply, " \
			"the message bus security policy blocked the reply, " \
			"the reply timeout expired, or the network connection " \
			"was broken."

static GSList *rfcomm_clients;

/* Variable for privilege, only for write API,
  before we should reduce time to bt-service dbus calling
  -1 : Don't have a permission to access API
  0 : Initial value, not yet check
  1 : Have a permission to access API
*/
static int privilege_token;

typedef struct {
	char bt_addr[BT_ADDRESS_STRING_SIZE];
	int fd;
	int watch_id;
	gboolean disconnected;
} rfcomm_conn_info_t;

typedef struct {
	char uuid[BLUETOOTH_UUID_STRING_MAX];
	char *device_path;
	char *obj_path;
	int object_id;
	int id;
	GSList *rfcomm_conns;
	unsigned int idle_id;
} rfcomm_cb_data_t;

static void __client_connected_cb(rfcomm_cb_data_t *cb_data,
	char *dev_address, int result);

static void __bt_free_cb_data(rfcomm_cb_data_t *cb_data)
{
	BT_DBG("+");

	if (cb_data->id >= 0)
		__rfcomm_delete_id(cb_data->id);

	if (cb_data->object_id > 0)
		_bt_unregister_gdbus(cb_data->object_id);

	if (cb_data->obj_path) {
		BT_INFO("Unregister profile");
		_bt_unregister_profile(cb_data->obj_path);
	}

	if (cb_data->idle_id != 0) {
		BT_INFO("Removing idle source");
		g_source_remove(cb_data->idle_id);
	}

	g_free(cb_data->obj_path);

	g_free(cb_data->device_path);
	g_free(cb_data);
	BT_DBG("-");
}

static void rfcomm_cb_data_remove(rfcomm_cb_data_t *info)
{
	if (info) {
		BT_INFO("No more device connected remove info");
		rfcomm_clients = g_slist_remove(rfcomm_clients, info);
		__bt_free_cb_data(info);
	}
}

gint compare(gpointer *a, gpointer *b)
{
	rfcomm_conn_info_t *node = (rfcomm_conn_info_t *)a;
	char *address = (char *)b;
	return g_strcmp0(node->bt_addr, address);
}

gint compare_fd(gpointer *a, gpointer *b)
{
	rfcomm_conn_info_t *node = (rfcomm_conn_info_t *)a;
	int fd = (int )*b;
	if (node->fd == fd)
		return 0;
	return 1;
}
static void __bt_free_conn(rfcomm_conn_info_t *conn)
{
	BT_DBG("+");

	if (conn == NULL)
		return;

	if (conn->fd > 0)
		close(conn->fd);
	if (conn->watch_id > 0) {
		g_source_remove(conn->watch_id);
		conn->watch_id = 0;
	}
	g_free(conn);

	BT_DBG("-");
}

static void __rfcomm_remove_conn_info_t(rfcomm_cb_data_t *info, char *address)
{
	GSList *l = NULL;
	rfcomm_conn_info_t *conn_info = NULL;
	l = g_slist_find_custom(info->rfcomm_conns, address, (GCompareFunc)compare);
	if (l)
		conn_info = l->data;
	if (conn_info) {
		info->rfcomm_conns = g_slist_remove(info->rfcomm_conns, conn_info);
		__bt_free_conn(conn_info);
	}
}

static rfcomm_conn_info_t *__get_conn_info_from_fd(rfcomm_cb_data_t *info,
	int fd)
{
	GSList *l;
	rfcomm_conn_info_t *device_node = NULL;
	for(l = info->rfcomm_conns; l != NULL; l = l->next) {
		device_node = l->data;
		if ( device_node && device_node->fd == fd)
			return device_node;
	}
	return NULL;
}

static rfcomm_conn_info_t *__get_conn_info_from_address(rfcomm_cb_data_t *info,
		char *dev_address)
{
	GSList *l = NULL;
	rfcomm_conn_info_t *conn_info = NULL;
	l = g_slist_find_custom(info->rfcomm_conns, dev_address,
		(GCompareFunc)compare);
	if (l)
		conn_info = l->data;
	return conn_info;
}

static void __rfcomm_client_connected_cb(rfcomm_cb_data_t *info,
	char *dev_address, int result)
{
	__client_connected_cb(info, dev_address, result);
	__rfcomm_remove_conn_info_t(info, dev_address);

	if (info->rfcomm_conns == NULL)
		rfcomm_cb_data_remove(info);
}

static rfcomm_cb_data_t *__find_rfcomm_info_with_fd(int fd)
{
	GSList *l;
	GSList *device_fd;
	for (l = rfcomm_clients; l != NULL; l = l->next) {
		rfcomm_cb_data_t *info = l->data;
		device_fd = g_slist_find_custom(info->rfcomm_conns, &fd,
			(GCompareFunc)compare_fd);
		if (device_fd)
			return info;
	}

	return NULL;
}

static rfcomm_cb_data_t *__find_rfcomm_info_from_path(const char *path)
{
	GSList *l;

	for (l = rfcomm_clients; l != NULL; l = l->next) {
		rfcomm_cb_data_t *info = l->data;

		if (info != NULL)
			if (g_strcmp0(info->obj_path, path) == 0)
				return info;
	}

	return NULL;
}

static rfcomm_cb_data_t *__find_rfcomm_info_from_uuid(const char *uuid)
{
	GSList *l;

	for (l = rfcomm_clients; l != NULL; l = l->next) {
		rfcomm_cb_data_t *info = l->data;

		if (g_strcmp0(info->uuid, uuid) == 0)
			return info;
	}

	return NULL;
}

static void _bt_rfcomm_disconnect_conn_info(rfcomm_conn_info_t *conn_info,
	rfcomm_cb_data_t *info)
{
	if (conn_info == NULL)
		return;

	bluetooth_rfcomm_disconnection_t disconn_info;
	bt_event_info_t *event_info = NULL;
	if (conn_info->disconnected == FALSE)
		return;
	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL) {
		if (info->rfcomm_conns == NULL)
			rfcomm_cb_data_remove(info);
		return;
	}
	memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
	disconn_info.device_role = RFCOMM_ROLE_CLIENT;
	g_strlcpy(disconn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	BT_DBG("Disconnected FD [%d]", conn_info->fd);
	_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
					conn_info->bt_addr);

	disconn_info.socket_fd = conn_info->fd;

	BT_DBG("Disconnection Result[%d] BT_ADDRESS[%s] UUID[%s] FD[%d]",
			BLUETOOTH_ERROR_NONE, conn_info->bt_addr,
			info->uuid, conn_info->fd);
	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
			BLUETOOTH_ERROR_NONE, &disconn_info,
			event_info->cb, event_info->user_data);

	if(conn_info) {
		BT_DBG("List is present deleting it");
		__rfcomm_remove_conn_info_t(info, conn_info->bt_addr);
	}
	if (info->rfcomm_conns == NULL)
		rfcomm_cb_data_remove(info);

	BT_DBG("-");
}

static gboolean __rfcomm_client_disconnect(gpointer user_data)
{
	rfcomm_cb_data_t *info = (rfcomm_cb_data_t *) user_data;
	BT_INFO_C("Disconnected [RFCOMM Client]");
	retv_if(info == NULL, FALSE);

	if (g_slist_find(rfcomm_clients, info) == NULL) {
		BT_INFO("rfcomm resource is already freed");
		return FALSE;
	}
	info->idle_id = 0;
	g_slist_foreach(info->rfcomm_conns,
		(GFunc) _bt_rfcomm_disconnect_conn_info, info);
	BT_DBG("-");
	return FALSE;
}

static gboolean __is_error_by_disconnect(GError *err)
{
	return !g_strcmp0(err->message, "Connection reset by peer") ||
			!g_strcmp0(err->message, "Connection timed out") ||
			!g_strcmp0(err->message, "Software caused connection abort");
}

static gboolean __client_data_received_cb(GIOChannel *chan, GIOCondition cond,
								gpointer data)
{
	char *buffer = NULL;
	gsize len = 0;
	int result = BLUETOOTH_ERROR_NONE;
	rfcomm_cb_data_t *info = data;
	rfcomm_conn_info_t *conn_info = NULL;
	bt_event_info_t *event_info;
	bluetooth_rfcomm_received_data_t data_r;
	GIOStatus status = G_IO_STATUS_NORMAL;
	GError *err = NULL;
	int fd;
	BT_DBG("");

	retv_if(info == NULL, FALSE);
	fd = g_io_channel_unix_get_fd(chan);
	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR_C("RFComm Client  disconnected: %d", fd);
		conn_info = __get_conn_info_from_fd(info, fd);
		if (conn_info == NULL) {
			BT_ERR("No Connection info found with FD [%d]", fd);
			return FALSE;
		}
		conn_info->disconnected = TRUE;
		__rfcomm_client_disconnect(info);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_LEN + 1);

	status = g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_LEN,
			&len, &err);
	if (status != G_IO_STATUS_NORMAL) {
		BT_ERR("IO Channel read is failed with %d", status);

		g_free(buffer);
		if (err) {
			BT_ERR("IO Channel read error [%s]", err->message);
			if (status == G_IO_STATUS_ERROR &&
					__is_error_by_disconnect(err)) {
				BT_ERR("cond : %d", cond);
				g_error_free(err);
				conn_info = __get_conn_info_from_fd(info, fd);
				if (conn_info == NULL) {
					BT_ERR("No Connection info found with FD [%d]", fd);
					return FALSE;
				}
				conn_info->disconnected = TRUE;
				__rfcomm_client_disconnect(info);
				return FALSE;
			}
			g_error_free(err);
		}
		return TRUE;
	}

	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL) {
		g_free(buffer);
		return TRUE;
	}

	data_r.socket_fd = fd;
	data_r.buffer_size = len;
	data_r.buffer = buffer;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
			result, &data_r,
			event_info->cb, event_info->user_data);

	g_free(buffer);
	return TRUE;
}

static void __client_connected_cb(rfcomm_cb_data_t *cb_data, char *dev_address,
	int result)
{
	bluetooth_rfcomm_connection_t conn_info;
	bt_event_info_t *event_info;
	rfcomm_conn_info_t *conn_list_info = NULL;
	BT_INFO_C("Connected [RFCOMM Client]");

	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL)
		return;

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_CLIENT;
	g_strlcpy(conn_info.uuid, cb_data->uuid, BLUETOOTH_UUID_STRING_MAX);
	_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
			dev_address);
	conn_list_info = __get_conn_info_from_address(cb_data, dev_address);
	if (conn_list_info == NULL) {
		BT_ERR("Device addres %s not found in connection list", dev_address);
		return;
	}
	conn_info.socket_fd = conn_list_info->fd;
	conn_info.server_id = -1;

	BT_DBG("Connection Result[%d] BT_ADDRESS[%s] UUID[%s] FD[%d]",
			result, conn_list_info->bt_addr, cb_data->uuid, conn_list_info->fd);
	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
			result, &conn_info,
			event_info->cb, event_info->user_data);
	BT_DBG("-");
}

#endif

int new_connection(const char *path, int fd, bluetooth_device_address_t *addr)
{
	rfcomm_cb_data_t *info;
	GIOChannel *data_io;
	rfcomm_conn_info_t *conn_info = NULL;
	BT_DBG("%s %d", path, fd);
	char address[BT_ADDRESS_STRING_SIZE];
	_bt_convert_addr_type_to_string(address,
				(unsigned char *)addr);
	info = __find_rfcomm_info_from_path(path);
	if (info == NULL)
		return -1;
	conn_info = __get_conn_info_from_address(info, address);

	if (conn_info == NULL) {
		BT_ERR("Device Address %s not found in connection list", address);
		return -1;
	}
	conn_info->fd = fd;
	BT_DBG("connection info fd %d", conn_info->fd);
	data_io = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(data_io, NULL, NULL);
	g_io_channel_set_flags(data_io, G_IO_FLAG_NONBLOCK, NULL);
	conn_info->watch_id = g_io_add_watch(data_io,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				__client_data_received_cb, info);

	g_io_channel_unref(data_io);

	__client_connected_cb(info, address,BLUETOOTH_ERROR_NONE);

	return 0;
}

static void __bt_connect_response_cb(GDBusProxy *proxy, GAsyncResult *res,
							gpointer user_data)

{
	GError *error = NULL;
	rfcomm_cb_data_t *cb_data;
	char dev_address[BT_ADDRESS_STRING_SIZE];
	const char *path;
	BT_DBG("+");

	ret_if(user_data == NULL);

	cb_data = user_data;

	if (!g_dbus_proxy_call_finish(proxy, res, &error)) {
		int result;
		g_dbus_error_strip_remote_error(error);
		BT_ERR("Error : %s \n", error->message);

		if (g_strcmp0(error->message, "In Progress") == 0)
			result = BLUETOOTH_ERROR_DEVICE_BUSY;
		else
			result = BLUETOOTH_ERROR_INTERNAL;
		path = g_dbus_proxy_get_object_path(proxy);
		_bt_convert_device_path_to_address(path, dev_address);
		__rfcomm_client_connected_cb(cb_data, dev_address, result);

		g_error_free(error);
	}
	if (proxy)
		g_object_unref(proxy);

	BT_DBG("-");
}

static void __bt_discover_service_response_cb(GDBusProxy *proxy,
				GAsyncResult *res, gpointer user_data)
{
	rfcomm_cb_data_t *cb_data;
	int ret = 0;
	GError *err = NULL;
	bt_register_profile_info_t info = {0};
	int result = BLUETOOTH_ERROR_NONE;
	char dev_address[BT_ADDRESS_STRING_SIZE];
	const char *path;

	BT_DBG("+");

	ret_if(user_data == NULL);

	cb_data = user_data;

	path = g_dbus_proxy_get_object_path(proxy);

	_bt_convert_device_path_to_address(path, dev_address);
	BT_DBG("Device Adress [%s]", dev_address);
	g_dbus_proxy_call_finish(proxy, res, &err);
	if (proxy)
		g_object_unref(proxy);

	if (err != NULL) {
		g_dbus_error_strip_remote_error(err);
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);
		if (!strcmp("Operation canceled", err->message)) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		} else if (!strcmp("In Progress", err->message)) {
			result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (!strcmp("Host is down", err->message)) {
			result = BLUETOOTH_ERROR_HOST_DOWN;
		} else if (!strcmp(BT_TIMEOUT_MESSAGE, err->message)) {
			result = BLUETOOTH_ERROR_SERVICE_SEARCH_ERROR;
			ret = _bt_cancel_discovers(dev_address);
			if (ret != BLUETOOTH_ERROR_NONE)
				BT_ERR("Error: While CancelDiscovery");
		} else {
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		}
		__rfcomm_client_connected_cb(cb_data, dev_address, result);
		goto done;
	} else {
		BT_INFO("Services are Updated checking required uuid is there");
		/* Check here for uuid present */
		ret = _bt_discover_service_uuids(dev_address, (char *)cb_data->uuid);
		if (ret == BLUETOOTH_ERROR_NONE) {
			info.uuid = (char *)cb_data->uuid;
			info.obj_path = cb_data->obj_path;
			info.role = "client";

			ret = _bt_register_profile(&info, FALSE);
			if (ret < 0)
				BT_DBG("Error: register profile");
			ret = _bt_connect_profile(dev_address, cb_data->uuid,
						__bt_connect_response_cb, cb_data);

			if (ret != BLUETOOTH_ERROR_NONE) {
				BT_ERR("ConnectProfile failed");
				result = BLUETOOTH_ERROR_CONNECTION_ERROR;
				__rfcomm_client_connected_cb(cb_data, dev_address, result);
				goto done;
			}
		} else {
			BT_ERR("remote uuid not found");
			result = BLUETOOTH_ERROR_SERVICE_NOT_FOUND;
			__rfcomm_client_connected_cb(cb_data, dev_address, result);
		}
	}
done:
	if (err)
		g_clear_error(&err);
}

BT_EXPORT_API int bluetooth_rfcomm_connect(
		const bluetooth_device_address_t *remote_bt_address,
		const char *remote_uuid)
{

#ifdef RFCOMM_DIRECT
	rfcomm_cb_data_t *cb_data = NULL;
	rfcomm_conn_info_t *conn = NULL;
#else
	int result;
	int connect_type;
	bt_user_info_t *user_info;
	char uuid[BLUETOOTH_UUID_STRING_MAX];
#endif
	BT_CHECK_PARAMETER(remote_bt_address, return);
	BT_CHECK_PARAMETER(remote_uuid, return);
	BT_CHECK_ENABLED(return);

#ifdef RFCOMM_DIRECT
	BT_INFO_C("<<<<<<<<< RFCOMM Connect request from app >>>>>>>>>>>");
	int ret;
	int id, object_id;
	char *path;

	if (_bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_CLIENT_CONNECT)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	id = __rfcomm_assign_id();
	if (id < 0)
		return BLUETOOTH_ERROR_INTERNAL;

	cb_data = __find_rfcomm_info_from_uuid(remote_uuid);
	if (!cb_data) {
		path = g_strdup_printf("/org/socket/client/%d/%d", getpid(), id);

		object_id = _bt_register_new_conn(path, new_connection);
		if (object_id < 0) {
			__rfcomm_delete_id(id);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		cb_data = g_new0(rfcomm_cb_data_t, 1);
		g_strlcpy(cb_data->uuid, remote_uuid, BLUETOOTH_UUID_STRING_MAX);
		cb_data->obj_path = path;
		cb_data->object_id = object_id;
		cb_data->id = id;
	}
	conn = g_new0(rfcomm_conn_info_t, 1);
	conn->fd = -1;
	_bt_convert_addr_type_to_string(conn->bt_addr,
				(unsigned char *)remote_bt_address->addr);

	BT_DBG("Connecting to %s uuid %s", conn->bt_addr, remote_uuid);
	cb_data->rfcomm_conns = g_slist_append(cb_data->rfcomm_conns, conn);
	ret = _bt_discover_services(conn->bt_addr, (char *)remote_uuid,
				__bt_discover_service_response_cb, cb_data);
	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Error returned while service discovery");
		__rfcomm_remove_conn_info_t(cb_data, conn->bt_addr);
		if (cb_data->rfcomm_conns == NULL)
			rfcomm_cb_data_remove(cb_data);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (g_slist_find(rfcomm_clients, cb_data) == NULL) {
		BT_INFO("Adding callback information to rfcomm_clients");
		rfcomm_clients = g_slist_append(rfcomm_clients, cb_data);
	} else
		BT_INFO("Callback information is already added");

	return BLUETOOTH_ERROR_NONE;
#else
	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* connect_type:  BT_RFCOMM_UUID / BT_RFCOMM_CHANNEL*/
	/* In now, we only support to connecty using UUID */
	connect_type = BT_RFCOMM_UUID;

	if (_bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_CLIENT_CONNECT)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_bt_address,
				sizeof(bluetooth_device_address_t));

	g_strlcpy(uuid, remote_uuid, sizeof(uuid));
	g_array_append_vals(in_param2, uuid, BLUETOOTH_UUID_STRING_MAX);

	g_array_append_vals(in_param3, &connect_type, sizeof(int));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE,
				BT_RFCOMM_CLIENT_CONNECT,
				in_param1, in_param2,
				in_param3, in_param4,
				user_info->cb, user_info->user_data);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_client_is_connected(const bluetooth_device_address_t *device_address, gboolean *connected)
{
	GSList *l;
	GSList *conn_list = NULL;
	rfcomm_cb_data_t *client_info;
	rfcomm_conn_info_t *conn_info;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(connected, return);

	_bt_convert_addr_type_to_string(address, (unsigned char *)device_address->addr);
	*connected = FALSE;

	for (l = rfcomm_clients; l != NULL; l = l->next) {
		client_info = l->data;
		if (client_info == NULL)
			continue;
		for(conn_list = client_info->rfcomm_conns;
			conn_list != NULL; conn_list = conn_list->next) {
			conn_info = conn_list->data;
			if(conn_info == NULL)
				continue;

			if (g_strcmp0(address, conn_info->bt_addr) == 0) {
				*connected = TRUE;
				return BLUETOOTH_ERROR_NONE;
			}
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API gboolean bluetooth_rfcomm_is_client_connected(void)
{
	int result;
	int connected = FALSE;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE,
			BT_RFCOMM_CLIENT_IS_CONNECTED,
			in_param1, in_param2, in_param3,
			in_param4, &out_param);

	BT_DBG("result: %x", result);

	if (result == BLUETOOTH_ERROR_NONE) {
		connected = g_array_index(out_param,
				int, 0);
	} else {
		BT_ERR("Fail to send request");
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return connected;
}

BT_EXPORT_API int bluetooth_rfcomm_disconnect(int socket_fd)
{
#ifdef RFCOMM_DIRECT
	rfcomm_cb_data_t *info;
	rfcomm_conn_info_t *conn_info;
	BT_INFO_C("<<<<<<<<< RFCOMM Disconnect request from app >>>>>>>>");
	BT_CHECK_ENABLED(return);

	if (_bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_SOCKET_DISCONNECT)
	     == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	if (socket_fd < 0) {
		BT_ERR("Invalid FD");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	BT_DBG("FDD %d", socket_fd);

	info = __find_rfcomm_info_with_fd(socket_fd);
	if (info == NULL) {
		BT_DBG("Could not find in client, so check in server");
		return bluetooth_rfcomm_server_disconnect(socket_fd);
	}
	conn_info = __get_conn_info_from_fd(info, socket_fd);
	if (conn_info == NULL) {
		BT_ERR("FATAL Error");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (conn_info->watch_id <= 0) {
		BT_ERR("Invalid state");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}
	conn_info->disconnected = TRUE;
	close(socket_fd);
	BT_INFO("conn_info %s", conn_info->bt_addr);
	_bt_disconnect_profile(conn_info->bt_addr, info->uuid, NULL,NULL);
	if (info->idle_id == 0)
		info->idle_id = g_idle_add(__rfcomm_client_disconnect, info);

	return BLUETOOTH_ERROR_NONE;
#else
	int result;
	int service_function;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	/* Support the OSP */
	if (socket_fd == -1) {
		/* Cancel connect */
		service_function = BT_RFCOMM_CLIENT_CANCEL_CONNECT;
	} else {
		g_array_append_vals(in_param1, &socket_fd, sizeof(int));
		service_function = BT_RFCOMM_SOCKET_DISCONNECT;
	}

	result = _bt_send_request(BT_BLUEZ_SERVICE, service_function,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
#endif
}

BT_EXPORT_API int bluetooth_rfcomm_write(int fd, const char *buf, int length)
{
#ifdef RFCOMM_DIRECT
	int written;
#else
	char *buffer;
#endif
	int result;

	BT_CHECK_PARAMETER(buf, return);
	if (fd < 0) {
		BT_ERR("Invalid FD");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	BT_DBG("FD : %d", fd);

#ifndef RFCOMM_DIRECT
	BT_CHECK_ENABLED(return);
#endif
	retv_if(length <= 0, BLUETOOTH_ERROR_INVALID_PARAM);

#ifdef RFCOMM_DIRECT
	switch (privilege_token) {
	case 0:
		result = _bt_check_privilege(BT_BLUEZ_SERVICE, BT_RFCOMM_SOCKET_WRITE);

		if (result == BLUETOOTH_ERROR_NONE) {
			privilege_token = 1; /* Have a permission */
		} else if (result == BLUETOOTH_ERROR_PERMISSION_DEINED) {
			BT_ERR("Don't have a privilege to use this API");
			privilege_token = -1; /* Don't have a permission */
			return BLUETOOTH_ERROR_PERMISSION_DEINED;
		} else {
			/* Just break - It is not related with permission error */
		}
		break;
	case 1:
		/* Already have a privilege */
		break;
	case -1:
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	default:
		/* Invalid privilge token value */
		return BLUETOOTH_ERROR_INTERNAL;
	}

	written = write(fd, buf, length);
	/*BT_DBG("Length %d, written = %d, balance(%d)",
			 length, written, length - written); */
	return written;
#else
	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	buffer = g_malloc0(length + 1);

	memcpy(buffer, buf, length);

	g_array_append_vals(in_param1, &fd, sizeof(int));
	g_array_append_vals(in_param2, &length, sizeof(int));
	g_array_append_vals(in_param3, buffer, length);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_SOCKET_WRITE,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_DBG("result: %x", result);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_free(buffer);

	return result;
#endif
}

