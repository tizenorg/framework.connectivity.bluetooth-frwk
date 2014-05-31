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

typedef struct {
	char uuid[BLUETOOTH_UUID_STRING_MAX];
	char bt_addr[BT_ADDRESS_STRING_SIZE];
	char *device_path;
	int fd;
} rfcomm_cb_data_t;

static void __bt_free_cb_data(rfcomm_cb_data_t *cb_data)
{
	BT_DBG("+");
	g_free(cb_data->device_path);
	g_free(cb_data);
	BT_DBG("-");
}

static void __rfcomm_client_disconnect(rfcomm_cb_data_t *info)
{
	bluetooth_rfcomm_disconnection_t disconn_info;
	bt_event_info_t *event_info = NULL;
	BT_INFO("--------- RFCOMM Disconnected ----------- ");
	ret_if(info == NULL);

	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL) {
		__bt_free_cb_data(info);
		return;
	}

	memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
	disconn_info.device_role = RFCOMM_ROLE_CLIENT;

	BT_INFO("UUID : %s", info->uuid);
	g_strlcpy(disconn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	disconn_info.socket_fd = info->fd;
	_bt_convert_addr_string_to_type(disconn_info.device_addr.addr,
							info->bt_addr);

	DBG_SECURE("Disconnection Result[%d] BT_ADDRESS[%s] UUID[%s] FD[%d]",
			BLUETOOTH_ERROR_NONE, info->bt_addr,
			info->uuid, info->fd);
	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
			BLUETOOTH_ERROR_NONE, &disconn_info,
			event_info->cb, event_info->user_data);

	__bt_free_cb_data(info);
	BT_DBG("-");
}

static gboolean __client_data_received_cb(GIOChannel *chan, GIOCondition cond,
								gpointer data)
{
	char *buffer = NULL;
	gsize len;
	int result = BLUETOOTH_ERROR_NONE;
	rfcomm_cb_data_t *info = data;
	bt_event_info_t *event_info;
	bluetooth_rfcomm_received_data_t data_r;

	retv_if(info == NULL, FALSE);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR("RFComm Client  disconnected: %d", info->fd);
		__rfcomm_client_disconnect(info);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_LEN + 1);

	if (g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_LEN,
				&len, NULL) == G_IO_STATUS_ERROR) {
		BT_ERR("IO Channel read error server");
		g_free(buffer);
		__rfcomm_client_disconnect(info);
		return FALSE;
	}

	if (len == 0) {
		BT_ERR("Read failed len=%d, fd=%d\n", len, info->fd);
		g_free(buffer);
		__rfcomm_client_disconnect(info);
		return FALSE;
	}

	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL)
		return TRUE;

	data_r.socket_fd = info->fd;
	data_r.buffer_size = len;
	data_r.buffer = buffer;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
			result, &data_r,
			event_info->cb, event_info->user_data);

	g_free(buffer);
	return TRUE;
}

static void __client_connected_cb(rfcomm_cb_data_t *cb_data, int result)
{
	bluetooth_rfcomm_connection_t conn_info;
	bt_event_info_t *event_info;
	BT_DBG("+");

	event_info = _bt_event_get_cb_data(BT_RFCOMM_CLIENT_EVENT);
	if (event_info == NULL)
		return;

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_CLIENT;
	g_strlcpy(conn_info.uuid, cb_data->uuid, BLUETOOTH_UUID_STRING_MAX);

	_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
			cb_data->bt_addr);
	conn_info.socket_fd = cb_data->fd;

	DBG_SECURE("Connection Result[%d] BT_ADDRESS[%s] UUID[%s] FD[%d]",
			result, cb_data->bt_addr, cb_data->uuid, cb_data->fd);
	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
			result, &conn_info,
			event_info->cb, event_info->user_data);
	BT_DBG("-");
}

static void __bt_rfcomm_connect_fd_response_cb(GObject *source_object,
		GAsyncResult *res, rfcomm_cb_data_t *user_data)
{
	int fd = -1;
	int index = -1;
	gint length = 0;
	gint *fd_array;
	GDBusConnection *conn;
	GError *error = NULL;
	GVariant *value;
	GUnixFDList *fd_list;
	GIOChannel *data_io;
	BT_DBG("+");

	ret_if(user_data == NULL);

	conn = _bt_gdbus_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("Unable to get connection");
		__client_connected_cb(user_data,
				BLUETOOTH_ERROR_CONNECTION_ERROR);
		__bt_free_cb_data(user_data);
		return;
	}

	value = g_dbus_connection_call_with_unix_fd_list_finish(conn,
						&fd_list, res, &error);

	if (fd_list == NULL || value == NULL) {
		BT_ERR("No fd in message. Either fd_list or value is NULL.");
		if (error) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: message[%s]",
							error->message);
			g_clear_error(&error);
		}
		__client_connected_cb(user_data,
				BLUETOOTH_ERROR_SERVICE_NOT_FOUND);
		__bt_free_cb_data(user_data);
		return;
	}

	g_variant_get(value, "(h)", &index);
	fd_array = g_unix_fd_list_steal_fds(fd_list, &length);
	fd = fd_array[index];
	BT_DBG("FD list Length:[%d] Index:[%d]", length, index);
	g_free(fd_array);
	g_variant_unref(value);

	if (fd < 0) {
		BT_ERR("No fd in message");
		__client_connected_cb(user_data,
				BLUETOOTH_ERROR_CONNECTION_ERROR);
		__bt_free_cb_data(user_data);
		return;
	}
	user_data->fd = fd;
	BT_INFO("UUID : %s, fd : %d", user_data->uuid, fd);
	DBG_SECURE("BT Address:[%s]", user_data->bt_addr);

	__client_connected_cb(user_data, BLUETOOTH_ERROR_NONE);

	data_io = g_io_channel_unix_new(fd);
	g_io_channel_set_encoding(data_io, NULL, NULL);
	g_io_channel_set_flags(data_io, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(data_io,
	   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
	   __client_data_received_cb, user_data);

	g_io_channel_unref(data_io);
	BT_DBG("-");
}

static char *__bt_rfcomm_get_device_path(const char *device_addr)
{
#ifdef __ENABLE_GDBUS__
	GDBusConnection *conn = NULL;
	GVariant *result = NULL;
	GDBusProxy *adapter_proxy = NULL;
	GError *error = NULL;
	gchar *device_path = NULL;
	BT_DBG("+");

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, NULL);

	adapter_proxy = _bt_gdbus_get_adapter_proxy(conn);
	retv_if(adapter_proxy == NULL, NULL);

	result = g_dbus_proxy_call_sync(adapter_proxy,
					"FindDevice",
					g_variant_new("(s)", device_addr),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL,
					&error);
	g_object_unref(adapter_proxy);
	if (!result) {
		if (error != NULL) {
			BT_ERR("Fail to get DefaultAdapter (Error: %s)",
							error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Fail to get DefaultAdapter");
		return NULL;
	}

	if (g_strcmp0(g_variant_get_type_string(result), "(o)")) {
		BT_ERR("Incorrect result\n");
		g_variant_unref(result);
		return NULL;
	}

	g_variant_get(result, "(o)", &device_path);
	g_variant_unref(result);
#else
	DBusGConnection *conn;
	DBusGProxy *adapter_proxy;
	gchar *device_path = NULL;
	GError *error = NULL;
	BT_DBG("+");

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, NULL);

	adapter_proxy = _bt_get_adapter_proxy(conn);
	retv_if(adapter_proxy == NULL, NULL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", &error,
		G_TYPE_STRING, device_addr, G_TYPE_INVALID,
		DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	g_object_unref(adapter_proxy);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]",
					error->message, device_addr);
		g_error_free(error);
		return NULL;
	}
#endif

	DBG_SECURE("Device Address[%s] Device Path[%s]", device_addr, device_path);
	BT_DBG("-");
	return device_path;
}

static void __bt_rfcomm_client_discover_services_cb(GObject *source,
			GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *value;
	GDBusConnection *conn;
	GDBusProxy *proxy = NULL;
	rfcomm_cb_data_t *cb_data = NULL;
	int err = BLUETOOTH_ERROR_INTERNAL;
	GVariant *result = NULL;
	GError *error_var = NULL;

	BT_DBG("+");

	ret_if(user_data == NULL);

	proxy = (GDBusProxy *)source;
	cb_data = (rfcomm_cb_data_t *)user_data;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		if (error) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: message[%s], errCode[%x]",
							error->message, error->code);
			if (g_strrstr(error->message, "InProgress"))
				err = BLUETOOTH_ERROR_IN_PROGRESS;

			if (error->code == G_IO_ERROR_TIMED_OUT) {
				err =  BLUETOOTH_ERROR_SERVICE_SEARCH_ERROR;
				result = g_dbus_proxy_call_sync(proxy,
					"CancelDiscovery", NULL,
					G_DBUS_CALL_FLAGS_NONE, -1, NULL,
					&error_var);
				if (result == NULL) {
					BT_ERR("D-Bus API failure: message[%s]",
							error_var->message);
					g_clear_error(&error_var);
				} else {
					BT_ERR("Service discovery cancelled due to timeout");
					g_variant_unref(result);
				}
			}
			g_clear_error(&error);
		}
		BT_ERR("Unable to get GVariant *value");
		__client_connected_cb(cb_data, err);
		__bt_free_cb_data(cb_data);
		return;
	}

	conn = _bt_gdbus_get_system_gconn();
	if (conn == NULL) {
		__client_connected_cb(cb_data,
				BLUETOOTH_ERROR_CONNECTION_ERROR);
		__bt_free_cb_data(cb_data);
		BT_ERR("Unable to get connection");
		return;
	}

	BT_INFO("UUID : %s", cb_data->uuid);
	DBG_SECURE("ADDRESS:[%s], PATH:[%s]", cb_data->bt_addr, cb_data->device_path);
	g_dbus_connection_call_with_unix_fd_list(conn,
			BT_BLUEZ_NAME, cb_data->device_path,
			BT_SERIAL_INTERFACE, "ConnectFD",
			g_variant_new("(s)", cb_data->uuid),
			NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL,
			(GAsyncReadyCallback)__bt_rfcomm_connect_fd_response_cb,
			cb_data);

	BT_DBG("-");
}
#endif

BT_EXPORT_API int bluetooth_rfcomm_connect(
		const bluetooth_device_address_t *remote_bt_address,
		const char *remote_uuid)
{

#ifdef RFCOMM_DIRECT
	GDBusConnection *conn;
	GDBusProxy *service_proxy = NULL;
	GError *error = NULL;
	rfcomm_cb_data_t *cb_data = NULL;
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
	BT_INFO("<<<<<<<<< RFCOMM Connect request from app [%s] >>>>>>>>>>>", remote_uuid);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	cb_data = g_new0(rfcomm_cb_data_t, 1);
	g_strlcpy(cb_data->uuid, remote_uuid, BLUETOOTH_UUID_STRING_MAX);
	_bt_convert_addr_type_to_string(cb_data->bt_addr,
			(unsigned char *)remote_bt_address->addr);
	cb_data->device_path = __bt_rfcomm_get_device_path(cb_data->bt_addr);

	if (cb_data->device_path == NULL) {
		BT_ERR("Unable to get Device Path");
		 __bt_free_cb_data(cb_data);
		 return BLUETOOTH_ERROR_INTERNAL;
	}
	cb_data->fd = -1;

	service_proxy = g_dbus_proxy_new_sync(conn,
				G_DBUS_PROXY_FLAGS_NONE, NULL,
				BT_BLUEZ_NAME, cb_data->device_path,
				BT_DEVICE_INTERFACE,
				NULL, &error);

	if (error) {
		 BT_ERR("Unable to create proxy: %s", error->message);
		 g_clear_error(&error);
		 __bt_free_cb_data(cb_data);
		 return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!service_proxy) {
		BT_ERR("Unable to create proxy: Proxy NULL");
		__bt_free_cb_data(cb_data);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	DBG_SECURE("UUID:[%s] ADDRESS:[%s]", cb_data->uuid, cb_data->bt_addr);
	g_dbus_proxy_call(service_proxy,
			"DiscoverServices",
			g_variant_new("(s)", remote_uuid),
			G_DBUS_CALL_FLAGS_NONE, BT_SERVICE_DISCOVERY_TIMEOUT,
			NULL, __bt_rfcomm_client_discover_services_cb,
			cb_data);
	return BLUETOOTH_ERROR_NONE;
#else
	user_info = _bt_get_user_data(BT_COMMON);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	/* connect_type:  BT_RFCOMM_UUID / BT_RFCOMM_CHANNEL*/
	/* In now, we only support to connecty using UUID */
	connect_type = BT_RFCOMM_UUID;

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
	BT_INFO("<<<<<<<<< RFCOMM Disconnect request from app [%d] >>>>>>>>", socket_fd);
	BT_CHECK_ENABLED(return);

	if (socket_fd < 0) {
		BT_ERR("Invalid FD");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	BT_DBG("FD %d", socket_fd);

	if (close(socket_fd) == 0) {
		BT_INFO("Disconnected. FD Closed");
		return BLUETOOTH_ERROR_NONE;
	} else {
		BT_ERR("Could not close FD.");
	}

	return BLUETOOTH_ERROR_INVALID_PARAM;
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
	int result;
	char *buffer;
#endif

	BT_CHECK_PARAMETER(buf, return);
#ifndef RFCOMM_DIRECT
	BT_CHECK_ENABLED(return);
#endif
	retv_if(length <= 0, BLUETOOTH_ERROR_INVALID_PARAM);

#ifdef RFCOMM_DIRECT
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

