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

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-rfcomm-server.h"

typedef struct {
	int req_id;
	char *channel;
	char *address;
	char *uuid;
	DBusGProxy *rfcomm_proxy;
} rfcomm_function_data_t;

rfcomm_function_data_t *rfcomm_info;
GSList *client_list;

static bt_rfcomm_info_t *__bt_rfcomm_get_client_info(int socket_fd)
{
	GSList *l;
	bt_rfcomm_info_t *client_info;

	for (l = client_list; l != NULL; l = l->next) {
		client_info = l->data;

		if (client_info == NULL)
			continue;

		if (socket_fd == client_info->fd)
			return client_info;
	}

	return NULL;
}

static int __bt_rfcomm_open_socket(char *dev_node)
{
	int socket_fd;

	socket_fd = open(dev_node, O_RDWR | O_NOCTTY);

	if (socket_fd < 0) {
		BT_ERR("Can't open TTY : %s(%d)", dev_node, socket_fd);
		return socket_fd;
	}

	BT_INFO("/dev/rfcomm fd = %d", socket_fd);

	if (_bt_set_non_blocking_tty(socket_fd) < 0) {
		/* Even if setting the tty fails we will continue */
		BT_ERR("Unable to set /dev/rfcomm fd = %d", socket_fd);
	}

	return socket_fd;
}

static int __bt_rfcomm_disconnect_request(int socket_fd)
{
	DBusGConnection *conn;
	DBusGProxy *adapter_proxy;
	DBusGProxy *rfcomm_proxy;
	GError *error = NULL;
	bt_rfcomm_info_t *client_info;
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	retv_if(address == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	client_info = __bt_rfcomm_get_client_info(socket_fd);
	retv_if(client_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, client_info->address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	rfcomm_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_SERIAL_INTERFACE);

	BT_INFO("device path: %s", device_path);
	g_free(device_path);

	retv_if(rfcomm_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INFO("device node: %s", client_info->dev_node);
	if (!dbus_g_proxy_call(rfcomm_proxy, "Disconnect",
			&error,
			G_TYPE_STRING, client_info->dev_node,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (error) {
			BT_ERR("Disconnect Call Error, %s", error->message);
			g_error_free(error);
		}
		g_object_unref(rfcomm_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_object_unref(rfcomm_proxy);

	return BLUETOOTH_ERROR_NONE;
}

#ifdef GIOOUT_Q_WRITE
static void write_info_free(bt_rfcomm_write_info_t *write_info)
{
	g_free(write_info->buf);
	g_free(write_info);
}
#endif

static int __bt_rfcomm_disconnect_cb(void *data)
{
	int result = BLUETOOTH_ERROR_NONE;
	bt_rfcomm_info_t *client_info = data;

	retv_if(client_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INFO("UUID : %s", client_info->uuid);

	_bt_send_event(BT_RFCOMM_CLIENT_EVENT,
		BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &client_info->address,
		DBUS_TYPE_STRING, &client_info->uuid,
		DBUS_TYPE_INT16, &client_info->fd,
		DBUS_TYPE_INVALID);

	client_list = g_slist_remove(client_list, client_info);

	g_source_remove(client_info->io_event);
	close(client_info->fd);
	g_free(client_info->dev_node);
	g_free(client_info->address);
	g_free(client_info->uuid);
#ifdef GIOOUT_Q_WRITE
	g_queue_foreach(client_info->tx_queue, (GFunc) write_info_free, NULL);
	g_queue_free(client_info->tx_queue);
	if (client_info->write_id > 0)
		g_source_remove(client_info->write_id);
	g_free(client_info->tx_buf);
#endif
	g_free(client_info);

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_rfcomm_cancel_connect_cb(void *data)
{
	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;
	bluetooth_rfcomm_connection_t conn_info;
	request_info_t *req_info;
	GArray *out_param1;
	GArray *out_param2;

	retv_if(rfcomm_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	req_info = _bt_get_request_info(rfcomm_info->req_id);

	retv_if(req_info == NULL, BLUETOOTH_ERROR_INTERNAL);
	retv_if(req_info->context == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INFO("+");

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_CLIENT;
	g_strlcpy(conn_info.uuid, rfcomm_info->uuid,
				BLUETOOTH_UUID_STRING_MAX);
	conn_info.socket_fd = -1;
	_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
					rfcomm_info->address);

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	g_array_append_vals(out_param1, &conn_info,
					sizeof(bluetooth_rfcomm_connection_t));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);

	g_object_unref(rfcomm_info->rfcomm_proxy);
	g_free(rfcomm_info->address);
	g_free(rfcomm_info->uuid);
	g_free(rfcomm_info->channel);
	g_free(rfcomm_info);
	rfcomm_info = NULL;

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_rfcomm_terminate_client(int socket_fd)
{
	BT_INFO("+");

	int result;
	bt_rfcomm_info_t *client_info;

	client_info = __bt_rfcomm_get_client_info(socket_fd);
	retv_if(client_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	result = __bt_rfcomm_disconnect_request(socket_fd);

	if (result != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to disconnect socket");
		return result;
	}

	/* Send the disconnected event after return the function */
	g_idle_add((GSourceFunc)__bt_rfcomm_disconnect_cb, client_info);

	return BLUETOOTH_ERROR_NONE;
}

static gboolean __bt_rfcomm_client_data_received_cb(GIOChannel *chan,
							GIOCondition cond,
							gpointer data)
{
	char *buffer = NULL;
	gsize len;
	int result = BLUETOOTH_ERROR_NONE;
	bt_rfcomm_info_t *client_info = data;

	BT_DBG("condition: %d", cond);

	retv_if(client_info == NULL, FALSE);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR("Unix client disconnected (fd=%d)\n", client_info->fd);
		__bt_rfcomm_terminate_client(client_info->fd);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_MAX + 1);

	if (g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_MAX,
				&len, NULL) == G_IO_STATUS_ERROR) {
		BT_ERR("IO Channel read error client");
		g_free(buffer);
		__bt_rfcomm_terminate_client(client_info->fd);
		return FALSE;
	}

	if (len == 0) {
		BT_ERR("Read failed len=%d, fd=%d\n", len, client_info->fd);
		g_free(buffer);
		__bt_rfcomm_terminate_client(client_info->fd);
		return FALSE;
	}

	_bt_send_event(BT_RFCOMM_CLIENT_EVENT,
		BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_INT16, &client_info->fd,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
		&buffer, len,
		DBUS_TYPE_INVALID);

	g_free(buffer);

	return TRUE;
}

static void __bt_rfcomm_connected_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				       gpointer user_data)
{
	BT_DBG("+\n");
	GError *err = NULL;
	gchar *rfcomm_device_node;
	int socket_fd = -1;
	int result = BLUETOOTH_ERROR_NONE;
	bt_rfcomm_info_t *client_info = NULL;
	request_info_t *req_info;
	bluetooth_rfcomm_connection_t conn_info;
	GArray *out_param1;
	GArray *out_param2;

	dbus_g_proxy_end_call(proxy, call, &err,
			G_TYPE_STRING, &rfcomm_device_node, G_TYPE_INVALID);

	g_object_unref(proxy);

	if (rfcomm_info == NULL) {
		BT_ERR("rfcomm_info == NULL");
		goto done;
	}

	if (err != NULL) {
		BT_ERR("Error occured in connecting port [%s]", err->message);

		if (!strcmp("Host is down", err->message))
			result = BLUETOOTH_ERROR_HOST_DOWN;
		else
			result = BLUETOOTH_ERROR_CONNECTION_ERROR;

		goto dbus_return;
	}

	BT_DBG("Succss Connect REMOTE Device RFCOMM Node[%s]", rfcomm_device_node);

	socket_fd = __bt_rfcomm_open_socket(rfcomm_device_node);

	if (socket_fd < 0) {
		int retry_count = 10;
		do {
			BT_ERR("Fail to open socket[%d] retry_count[%d]", socket_fd, retry_count);
			usleep(10*1000);		/* 10 ms */
			socket_fd = __bt_rfcomm_open_socket(rfcomm_device_node);
		} while (socket_fd < 0 && retry_count-- > 0);

		if (socket_fd < 0) {
			BT_ERR("Fail to open socket: %d", socket_fd);
			goto dbus_return;
		}
	}

	client_info = g_malloc0(sizeof(bt_rfcomm_info_t));

	client_info->fd = socket_fd;
	client_info->dev_node = g_strdup(rfcomm_device_node);
	client_info->address = g_strdup(rfcomm_info->address);
	client_info->uuid = g_strdup(rfcomm_info->uuid);
	client_info->io_channel = g_io_channel_unix_new(socket_fd);
	g_io_channel_set_encoding(client_info->io_channel, NULL, NULL);
#ifdef GIOOUT_Q_WRITE
	g_io_channel_set_buffered(client_info->io_channel, FALSE);
	client_info->tx_queue = g_queue_new();
#endif
	client_info->io_event = g_io_add_watch(client_info->io_channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				__bt_rfcomm_client_data_received_cb,
				client_info);

	g_io_channel_set_close_on_unref(client_info->io_channel, TRUE);
	g_io_channel_set_flags(client_info->io_channel,
				G_IO_FLAG_NONBLOCK, NULL);

	client_list = g_slist_append(client_list, client_info);

	BT_INFO("UUID : %s", rfcomm_info->uuid);

	_bt_send_event(BT_RFCOMM_CLIENT_EVENT,
		BLUETOOTH_EVENT_RFCOMM_CONNECTED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &rfcomm_info->address,
		DBUS_TYPE_STRING, &rfcomm_info->uuid,
		DBUS_TYPE_INT16, &socket_fd,
		DBUS_TYPE_INVALID);

dbus_return:
	req_info = _bt_get_request_info(rfcomm_info->req_id);

	if (req_info == NULL || req_info->context == NULL)
		goto done;

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_CLIENT;
	g_strlcpy(conn_info.uuid, rfcomm_info->uuid,
				BLUETOOTH_UUID_STRING_MAX);

	if (client_info)
		conn_info.socket_fd = client_info->fd;
	else
		conn_info.socket_fd = -1;

	_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
					rfcomm_info->address);

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	g_array_append_vals(out_param1, &conn_info,
					sizeof(bluetooth_rfcomm_connection_t));
	g_array_append_vals(out_param2, &result, sizeof(int));
	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);
	_bt_delete_request_list(req_info->req_id);
done:
	if (err)
		g_error_free(err);

	ret_if(rfcomm_info == NULL);

	g_free(rfcomm_info->address);
	g_free(rfcomm_info->uuid);
	g_free(rfcomm_info->channel);
	g_free(rfcomm_info);
	rfcomm_info = NULL;
}

static void __bt_rfcomm_discover_services_cb(DBusGProxy *proxy, DBusGProxyCall *call,
						    gpointer user_data)
{
	GError *err = NULL;
	GHashTable *hash = NULL;
	const char *dev_path = NULL;
	DBusGConnection *conn;
	DBusGProxy *rfcomm_proxy;
	int result = BLUETOOTH_ERROR_NONE;
	GArray *out_param1;
	GArray *out_param2;
	request_info_t *req_info;
	bluetooth_rfcomm_connection_t conn_info;

	dbus_g_proxy_end_call(proxy, call, &err,
			dbus_g_type_get_map("GHashTable",
			G_TYPE_UINT, G_TYPE_STRING),
			&hash, G_TYPE_INVALID);

	if (err != NULL) {
		BT_ERR("Error occured in Proxy call [%s]\n", err->message);
		result = BLUETOOTH_ERROR_CONNECTION_ERROR;
		g_error_free(err);
		goto fail;
	}

	g_hash_table_destroy(hash);

	if (rfcomm_info == NULL) {
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	conn = _bt_get_system_gconn();
	if (conn == NULL) {
		BT_ERR("ERROR: Can't get on system bus");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	dev_path = dbus_g_proxy_get_path(proxy);

	rfcomm_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
						dev_path,
						BT_SERIAL_INTERFACE);

	g_object_unref(proxy);
	proxy = NULL;

	if (rfcomm_proxy == NULL) {
		BT_ERR("Failed to get the rfcomm proxy\n");
		result = BLUETOOTH_ERROR_SERVICE_NOT_FOUND;
		goto fail;
	}

	rfcomm_info->rfcomm_proxy = rfcomm_proxy;

	BT_INFO("Try to connect");
	if (!dbus_g_proxy_begin_call(rfcomm_proxy, "Connect",
			(DBusGProxyCallNotify)__bt_rfcomm_connected_cb,
			NULL,	/*user_data*/
			NULL,	/*destroy*/
			G_TYPE_STRING, rfcomm_info->uuid,
			G_TYPE_INVALID)) {
		BT_ERR("RFCOMM connect Dbus Call Error");
		g_object_unref(rfcomm_proxy);
		result = BLUETOOTH_ERROR_INTERNAL;
		goto fail;
	}

	BT_DBG("-\n");

	return;
fail:
	if (proxy)
		g_object_unref(proxy);

	ret_if(rfcomm_info == NULL);

	req_info = _bt_get_request_info(rfcomm_info->req_id);

	if (req_info && req_info->context) {
		memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));

		conn_info.device_role = RFCOMM_ROLE_CLIENT;
		g_strlcpy(conn_info.uuid, rfcomm_info->uuid,
					BLUETOOTH_UUID_STRING_MAX);

		conn_info.socket_fd = -1;
		_bt_convert_addr_string_to_type(conn_info.device_addr.addr,
						rfcomm_info->address);

		out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
		out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

		g_array_append_vals(out_param1, &conn_info,
						sizeof(bluetooth_rfcomm_connection_t));
		g_array_append_vals(out_param2, &result, sizeof(int));

		dbus_g_method_return(req_info->context, out_param1, out_param2);

		g_array_free(out_param1, TRUE);
		g_array_free(out_param2, TRUE);
		_bt_delete_request_list(req_info->req_id);
	}

	g_free(rfcomm_info->address);
	g_free(rfcomm_info->uuid);
	g_free(rfcomm_info);
	rfcomm_info = NULL;
}

int _bt_rfcomm_connect_using_uuid(int request_id,
			bluetooth_device_address_t *device_address,
			char *remote_uuid)
{
	DBusGConnection *conn;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy;
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(remote_uuid, return);
	retv_if(rfcomm_info != NULL, BLUETOOTH_ERROR_DEVICE_BUSY);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	rfcomm_info = g_malloc0(sizeof(rfcomm_function_data_t));
	rfcomm_info->address = g_strdup(address);
	rfcomm_info->uuid = g_strdup(remote_uuid);
	rfcomm_info->req_id = request_id;

	BT_INFO("Start DiscoverServices");
	if (!dbus_g_proxy_begin_call(device_proxy, "DiscoverServices",
			(DBusGProxyCallNotify)__bt_rfcomm_discover_services_cb,
			NULL, NULL,
			G_TYPE_STRING, rfcomm_info->uuid,
			G_TYPE_INVALID)) {
		BT_ERR("Could not call dbus proxy\n");
		g_object_unref(device_proxy);
		g_free(rfcomm_info->address);
		g_free(rfcomm_info->uuid);
		g_free(rfcomm_info);
		rfcomm_info = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

/* Range of the Channel : 0 <= channel <= 30 */
int _bt_rfcomm_connect_using_channel(int request_id,
			bluetooth_device_address_t *device_address,
			char *channel)
{
	DBusGConnection *conn;
	DBusGProxy *adapter_proxy;
	DBusGProxy *device_proxy;
	gchar *device_path = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(address, return);
	retv_if(rfcomm_info != NULL, BLUETOOTH_ERROR_DEVICE_BUSY);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, address, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_NOT_PAIRED);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
						device_path,
						BT_SERIAL_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	rfcomm_info = g_malloc0(sizeof(rfcomm_function_data_t));
	rfcomm_info->address = g_strdup(address);
	rfcomm_info->channel = g_strdup(channel);
	rfcomm_info->req_id = request_id;
	rfcomm_info->rfcomm_proxy = device_proxy;

	if (!dbus_g_proxy_begin_call(device_proxy, "Connect",
			(DBusGProxyCallNotify)__bt_rfcomm_connected_cb,
			NULL,	/*user_data*/
			NULL,	/*destroy*/
			G_TYPE_STRING, channel,
			G_TYPE_INVALID)) {
		BT_ERR("RFCOMM connect Dbus Call Error");
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_DBG("-\n");

	return BLUETOOTH_ERROR_NONE;
}

/* Be used in RFCOMM client /server */
int _bt_rfcomm_disconnect(int socket_fd)
{
	bt_rfcomm_info_t *socket_info;

	BT_INFO("+");

	socket_info = __bt_rfcomm_get_client_info(socket_fd);
	if (socket_info == NULL)
		return _bt_rfcomm_server_disconnect(socket_fd);

	return __bt_rfcomm_terminate_client(socket_fd);
}

#ifdef GIOOUT_Q_WRITE
static gboolean write_server_data(GIOChannel *io, GIOCondition cond,
							gpointer user_data)
{
	bt_rfcomm_server_info_t *server_info = user_data;
	bt_rfcomm_write_info_t *write_info;
	GIOStatus status;
	gsize written;
	gchar *buf;

	BT_DBG("+");

	if (cond & G_IO_NVAL)
		return FALSE;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		BT_INFO("Disconnected");
		goto stop_write;
	}

	if (server_info->current_tx_len == 0) {
		BT_DBG("tx len is zero");

		write_info = g_queue_pop_head(server_info->tx_queue);
		if (write_info == NULL)
			goto stop_write;
		server_info->current_tx_buf = write_info->buf;
		server_info->tx_buf = write_info->buf;
		server_info->current_tx_len = write_info->len;

		g_free(write_info);
	}

	if (server_info->current_tx_len == 0) {
		goto stop_write;
	}


	buf = server_info->current_tx_buf;

	status = g_io_channel_write_chars(server_info->data_io, buf,
					server_info->current_tx_len,
					&written, NULL);
	if (status != G_IO_STATUS_NORMAL)
		goto stop_write;

	BT_DBG("Written %d", written);

	server_info->current_tx_len -= written;
	if (server_info->current_tx_len == 0) {
		g_free(server_info->tx_buf);
		server_info->tx_buf = NULL;
		server_info->current_tx_buf = NULL;
	} else {
		BT_DBG("still data to write");
		server_info->current_tx_buf += written;
	}

	if (server_info->current_tx_len > 0 ||
				g_queue_get_length(server_info->tx_queue) > 0) {
		BT_DBG("Keep the gio handler");
		return TRUE;
	}

stop_write:
	server_info->write_id = 0;

	BT_DBG("-");

	return FALSE;
}

static gboolean write_client_data(GIOChannel *io, GIOCondition cond,
							gpointer user_data)
{
	bt_rfcomm_info_t *client_info = user_data;
	bt_rfcomm_write_info_t *write_info;
	GIOStatus status;
	gsize written;
	gchar *buf;

	BT_DBG("+");

	if (cond & G_IO_NVAL)
		return FALSE;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		BT_INFO("Disconnected");
		goto stop_write;
	}

	if (client_info->current_tx_len == 0) {
		BT_DBG("tx len is zero");

		write_info = g_queue_pop_head(client_info->tx_queue);
		if (write_info == NULL)
			goto stop_write;

		client_info->current_tx_buf = write_info->buf;
		client_info->tx_buf = write_info->buf;
		client_info->current_tx_len = write_info->len;

		g_free(write_info);
	}

	if (client_info->current_tx_len == 0) {
		goto stop_write;
	}

	buf = client_info->current_tx_buf;
	status = g_io_channel_write_chars(client_info->io_channel , buf,
					client_info->current_tx_len,
					&written, NULL);
	if (status != G_IO_STATUS_NORMAL)
		goto stop_write;

	BT_DBG("Written %d", written);

	client_info->current_tx_len -= written;
	if (client_info->current_tx_len == 0) {
		g_free(client_info->tx_buf);
		client_info->tx_buf = NULL;
		client_info->current_tx_buf = NULL;
	} else {
		BT_DBG("still data to write");
		client_info->current_tx_buf += written;
	}

	if (client_info->current_tx_len > 0 ||
				g_queue_get_length(client_info->tx_queue) > 0) {
		BT_DBG("Keep the gio handler");
		return TRUE;
	}

stop_write:
	client_info->write_id = 0;
	BT_DBG("-");

	return FALSE;
}
#endif

/* Be used in RFCOMM client /server */
int _bt_rfcomm_write(int socket_fd, char *buf, int length)
{
#ifdef GIOOUT_Q_WRITE
	bt_rfcomm_server_info_t *server_info = NULL;
	GIOCondition cond;
	bt_rfcomm_write_info_t *write_info = NULL;
	bt_rfcomm_info_t *client_info;
#else
	int wbytes = 0;
	int written;
#endif

	retv_if(buf == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	BT_DBG("Length %d", length);

#ifdef GIOOUT_Q_WRITE
	server_info = _bt_rfcomm_get_server_info_using_data_fd(socket_fd);
	if (server_info == NULL) {
		client_info = __bt_rfcomm_get_client_info(socket_fd);
		if (client_info == NULL) {
			BT_ERR("Invalid socket fd");
			return BLUETOOTH_ERROR_NOT_IN_OPERATION;
		}
	}

	write_info = g_new(bt_rfcomm_write_info_t, 1);
	write_info->buf = g_malloc(length);
	memcpy(write_info->buf, buf, length);
	write_info->len = length;

	cond = G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL;

	if (server_info) {
		g_queue_push_tail(server_info->tx_queue, write_info);
		if (server_info->write_id > 0) {
			BT_DBG("Write already in  progress");
			return BLUETOOTH_ERROR_NONE;
		}
		server_info->write_id = g_io_add_watch(server_info->data_io, cond,
					write_server_data, server_info);
	} else {
		g_queue_push_tail(client_info->tx_queue, write_info);
		if (client_info->write_id > 0) {
			BT_DBG("Write already in  progress");
			return BLUETOOTH_ERROR_NONE;
		}
		client_info->write_id = g_io_add_watch(client_info->io_channel ,
					cond, write_client_data, client_info);
	}
#else
	while (wbytes < length) {
		written = write(socket_fd, buf + wbytes, length - wbytes);
		if (written <= 0) {
			BT_DBG("Status:- Written=%d, Reason=%s",
						written, strerror(errno));
			if (errno == EAGAIN) {
				BT_DBG("Resourse is busy so sleep for 500 ms");
				usleep(500*1000); /* 500 ms */
				continue;
			} else {
				BT_ERR("Rfcomm write failed.. Check transport");
				return BLUETOOTH_ERROR_NOT_IN_OPERATION;
			}
		}

		/* Synchronize the sending buffer */
		sync();
		fsync(socket_fd);

		wbytes += written;
	}
#endif

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_cancel_connect(void)
{
	GError *error = NULL;
	char *input_param;

	BT_INFO("+");

	retv_if(rfcomm_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);
	retv_if(rfcomm_info->rfcomm_proxy == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	if (rfcomm_info->uuid)
		input_param = rfcomm_info->uuid;
	else
		input_param = rfcomm_info->channel;

	retv_if(input_param == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!dbus_g_proxy_call(rfcomm_info->rfcomm_proxy,
			"Disconnect",
			&error,
			G_TYPE_STRING, input_param,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (error) {
			BT_ERR("Disconnect Dbus Call Error, %s", error->message);
			g_error_free(error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* Send the connected event after return the function */
	g_idle_add((GSourceFunc) __bt_rfcomm_cancel_connect_cb, NULL);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_is_connected(gboolean *connected)
{
	BT_CHECK_PARAMETER(connected, return);

	*connected = (client_list == NULL || g_slist_length(client_list) == 0) ?
					FALSE : TRUE;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_is_device_connected(bluetooth_device_address_t *device_address,
					gboolean *connected)
{
	GSList *l;
	bt_rfcomm_info_t *client_info;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(connected, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	*connected = FALSE;

	for (l = client_list; l != NULL; l = l->next) {
		client_info = l->data;

		if (client_info == NULL)
			continue;

		if (g_strcmp0(address, client_info->address) == 0) {
			*connected = TRUE;
			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_client_disconnect_all(void)
{
	GSList *l;
	bt_rfcomm_info_t *client_info;

	for (l = client_list; l != NULL; l = l->next) {
		client_info = l->data;

		if (client_info == NULL)
			continue;

		_bt_rfcomm_disconnect(client_info->fd);
	}

	return BLUETOOTH_ERROR_NONE;
}

