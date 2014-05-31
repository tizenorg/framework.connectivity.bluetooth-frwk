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
#include <sys/socket.h>
#include <sys/un.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-rfcomm-server.h"
#include "bt-service-agent.h"

/* Range of RFCOMM server ID : 0 ~ 244 */
#define BT_RFCOMM_SERVER_ID_MAX 245

#define BT_RFCOMM_PROXY_ADDRESS "x00/bluez/rfcomm"
#define BT_RFCOMM_SOCKET_ADDRESS "/bluez/rfcomm"

typedef struct {
	int data_fd;
	char *uuid;
	char *remote_address;
} bt_rfcomm_event_info_t;

GSList *server_list;
static int latest_id = -1;
static gboolean server_id_used[BT_RFCOMM_SERVER_ID_MAX];

int __bt_rfcomm_assign_server_id(void)
{
	int index;

	BT_DBG("latest_id: %d", latest_id);

	index = latest_id + 1;

	if (index >= BT_RFCOMM_SERVER_ID_MAX)
		index = 0;

	BT_DBG("index: %d", index);

	while (server_id_used[index] == TRUE) {
		if (index == latest_id) {
			/* No available ID */
			BT_ERR("All request ID is used");
			return -1;
		}

		index++;

		if (index >= BT_RFCOMM_SERVER_ID_MAX)
			index = 0;
	}

	latest_id = index;
	server_id_used[index] = TRUE;

	BT_DBG("Assigned Id: %d", latest_id);

	return latest_id;
}

void __bt_rfcomm_delete_server_id(int server_id)
{
	ret_if(server_id >= BT_RFCOMM_SERVER_ID_MAX);
	ret_if(server_id < 0);

	server_id_used[server_id] = FALSE;

	/* Next server will use this ID */
	latest_id = server_id - 1;
}


static bt_rfcomm_server_info_t *__bt_rfcomm_get_server_info(int control_fd)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	retv_if(control_fd <= 0, NULL);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (control_fd == server_info->control_fd)
			return server_info;
	}

	return NULL;
}

bt_rfcomm_server_info_t *_bt_rfcomm_get_server_info_using_data_fd(int data_fd)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	retv_if(data_fd <= 0, NULL);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (data_fd == server_info->data_fd)
			return server_info;
	}

	return NULL;
}

static DBusGProxy *__bt_rfcomm_get_serial_manager_proxy(void)
{
	DBusGProxy *proxy;
	DBusGConnection *g_conn;
	char *adapter_path;

	BT_DBG("+");

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, NULL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, NULL);

	proxy = dbus_g_proxy_new_for_name(g_conn, BT_BLUEZ_NAME,
			adapter_path, BT_SERIAL_MANAGER_INTERFACE);

	g_free(adapter_path);

	BT_DBG("-");

	return proxy;
}

static DBusGProxy *__bt_get_serial_proxy(char *serial_path)
{
	DBusGProxy *proxy;
	DBusGConnection *g_conn;

	g_conn = _bt_get_system_gconn();
	retv_if(g_conn == NULL, NULL);

	proxy = dbus_g_proxy_new_for_name(g_conn, BT_BLUEZ_NAME,
			serial_path, BT_SERIAL_PROXY_INTERFACE);

	return proxy;
}

static char *__bt_rfcomm_get_proxy_address(int server_id)
{
	BT_DBG("+");

	return g_strdup_printf("%s%d",
				BT_RFCOMM_PROXY_ADDRESS,
				server_id);
}

int __bt_rfcomm_get_socket(int server_id)
{
	int result;
	int socket_fd;
	char *socket_address = NULL;
	struct sockaddr_un server_addr;

	retv_if(server_id < 0, -1);

	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	retv_if(socket_fd < 0, -1);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = PF_UNIX;

	socket_address = g_strdup_printf("%s%d",
				BT_RFCOMM_SOCKET_ADDRESS,
				server_id);

	BT_DBG("socket_address: %s", socket_address);

	g_strlcpy(server_addr.sun_path + 1, socket_address,
					sizeof(server_addr.sun_path));

	if (bind(socket_fd, (struct sockaddr *)&server_addr,
					sizeof(server_addr)) < 0) {
		BT_ERR("Can't Bind Sock");
		goto fail;
	}

	BT_DBG("socket_fd = %d", socket_fd);

	result = _bt_set_socket_non_blocking(socket_fd);

	if (result != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Cannot set the tty");
		goto fail;
	}

	g_free(socket_address);
	return socket_fd;
fail:
	g_free(socket_address);
	close(socket_fd);
	return -1;
}

int _bt_rfcomm_create_socket(char *sender, char *uuid)
{
	DBusGProxy *serial_manager = NULL;
	DBusGProxy *serial_proxy = NULL;
	GError *error = NULL;
	char *proxy_address = NULL;
	char *serial_path = NULL;
	int server_id;
	int socket_fd;
	bt_rfcomm_server_info_t *server_info;

	BT_CHECK_PARAMETER(uuid, return);

	server_id = __bt_rfcomm_assign_server_id();
	retv_if(server_id < 0, BLUETOOTH_ERROR_INTERNAL);

	serial_manager = __bt_rfcomm_get_serial_manager_proxy();
	if (serial_manager == NULL)
		goto fail;

	proxy_address = __bt_rfcomm_get_proxy_address(server_id);
	if (proxy_address == NULL)
		goto fail;

	dbus_g_proxy_call(serial_manager, "CreateProxy", NULL,
			G_TYPE_STRING, uuid,
			G_TYPE_STRING, proxy_address,
			G_TYPE_INVALID,
			G_TYPE_STRING, &serial_path,
			G_TYPE_INVALID);

	if (serial_path == NULL)
		goto fail;

	BT_INFO("serial_path: %s", serial_path);

	serial_proxy = __bt_get_serial_proxy(serial_path);
	if (serial_proxy == NULL)
		goto fail;

	if (!dbus_g_proxy_call(serial_proxy, "Enable", &error,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (error != NULL) {
			BT_ERR("Enable Error: %s\n", error->message);
			g_error_free(error);
		}
		g_object_unref(serial_proxy);
		goto fail;
	}

	socket_fd = __bt_rfcomm_get_socket(server_id);
	if (socket_fd < 0) {
		BT_ERR("Can't get socket");
		goto fail;
	}

	server_info = g_malloc0(sizeof(bt_rfcomm_server_info_t));
	server_info->server_id = server_id;
	server_info->serial_proxy = serial_proxy;
	server_info->manager_proxy = serial_manager;
	server_info->serial_path = g_strdup(serial_path);
	server_info->uuid = g_strdup(uuid);
	server_info->sender = g_strdup(sender);
	server_info->control_fd = socket_fd;

	server_list = g_slist_append(server_list, server_info);

	g_free(proxy_address);

	return socket_fd;
fail:
	__bt_rfcomm_delete_server_id(server_id);
	g_free(proxy_address);

	if (serial_manager) {
		if (serial_path) {
			dbus_g_proxy_call(serial_manager, "RemoveProxy", NULL,
					G_TYPE_STRING, serial_path,
					G_TYPE_INVALID,
					G_TYPE_INVALID);
		}
		g_object_unref(serial_manager);
	}

	if (serial_proxy)
		g_object_unref(serial_proxy);

	return BLUETOOTH_ERROR_INTERNAL;
}

static gboolean __bt_rfcomm_server_data_received_cb(GIOChannel *chan,
						GIOCondition cond,
						gpointer data)
{
	char *buffer = NULL;
	gsize len;
	int result = BLUETOOTH_ERROR_NONE;
	bt_rfcomm_server_info_t *server_info = data;

	retv_if(server_info == NULL, FALSE);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR("Unix server  disconnected: %d", server_info->data_fd);
		_bt_rfcomm_server_disconnect(server_info->data_fd);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_MAX + 1);

	if (g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_MAX, &len, NULL) ==
							G_IO_STATUS_ERROR) {
		BT_ERR("IO Channel read error server");
		g_free(buffer);
		_bt_rfcomm_server_disconnect(server_info->data_fd);
		return FALSE;
	}

	if (len == 0) {
		BT_ERR("Read failed len=%d, fd=%d\n",
				len, server_info->data_fd);
		g_free(buffer);
		_bt_rfcomm_server_disconnect(server_info->data_fd);
		return FALSE;
	}

	_bt_send_event(BT_RFCOMM_SERVER_EVENT,
		BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_INT16, &server_info->data_fd,
		DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
		&buffer, len,
		DBUS_TYPE_INVALID);

	g_free(buffer);

	return TRUE;
}

int __bt_rfcomm_server_get_address(bt_rfcomm_server_info_t *server_info)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	DBusMessageIter reply_iter;
	DBusMessageIter reply_iter_entry;
	DBusConnection *conn;
	const char *property;

	BT_CHECK_PARAMETER(server_info, return);

	/* GetInfo Proxy Part */
	msg = dbus_message_new_method_call(BT_BLUEZ_NAME,
					server_info->serial_path,
					BT_SERIAL_PROXY_INTERFACE,
					"GetInfo");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&error);

	conn = _bt_get_system_conn();

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
	dbus_message_unref(msg);

	if (reply == NULL) {
		BT_ERR("Can't Call GetInfo Proxy");
		if (dbus_error_is_set(&error)) {
			BT_ERR("%s", error.message);
			dbus_error_free(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_init(reply, &reply_iter);

	if (dbus_message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_ARRAY) {
		BT_ERR("Can't get reply arguments - DBUS_TYPE_ARRAY");
		goto fail;
	}

	dbus_message_iter_recurse(&reply_iter, &reply_iter_entry);

	/*Parse the dict */
	while (dbus_message_iter_get_arg_type(&reply_iter_entry) ==
						DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter dict_entry;
		DBusMessageIter dict_entry_val;

		dbus_message_iter_recurse(&reply_iter_entry, &dict_entry);

		dbus_message_iter_get_basic(&dict_entry, &property);

		if (g_strcmp0("connected", property) == 0) {
			dbus_bool_t value;

			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			dbus_message_iter_get_basic(&dict_entry_val, &value);

			if (value == FALSE)
				goto fail;

			/*Parsing the address */
			dbus_message_iter_next(&reply_iter_entry);
			dbus_message_iter_recurse(&reply_iter_entry, &dict_entry);
			dbus_message_iter_get_basic(&dict_entry, &property);
			BT_DBG("String received...... = %s", property);

			if (g_strcmp0("address", property) == 0) {
				if (!dbus_message_iter_next(&dict_entry)) {
					BT_ERR("Failed getting next dict entry\n");
					goto fail;
				}

				if (dbus_message_iter_get_arg_type(&dict_entry) !=
								DBUS_TYPE_VARIANT) {
					BT_ERR("Failed get arg type varient\n");
					goto fail;
				}

				/*Getting the value of the varient*/
				dbus_message_iter_recurse(&dict_entry,
							  &dict_entry_val);

				if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
									DBUS_TYPE_STRING) {
					BT_ERR("Failed get arg type string\n");
					goto fail;
				}

				/*get  value string address*/
				dbus_message_iter_get_basic(&dict_entry_val, &property);

				BT_DBG("Address = %s\n", property);

				g_free(server_info->remote_address);
				server_info->remote_address = g_strdup(property);
			}

		}

		dbus_message_iter_next(&reply_iter_entry);
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;

fail:
	dbus_message_unref(reply);
	return BLUETOOTH_ERROR_INTERNAL;
}

static gboolean __bt_rfcomm_server_connected_cb(GIOChannel *chan,
							GIOCondition cond,
							gpointer data)
{
	bt_rfcomm_server_info_t *server_info;
	request_info_t *req_info;
	int client_sock;
	int addr_len;
	struct sockaddr_un sock_addr;
	int result = BLUETOOTH_ERROR_NONE;

	BT_INFO("rfcomm_server.server_io_channel has %d", cond);

	server_info = data;
	retv_if(server_info == NULL, FALSE);

	if (cond & G_IO_NVAL)
		return FALSE;

	if (cond & (G_IO_HUP | G_IO_ERR)) {
		_bt_rfcomm_remove_socket(server_info->control_fd);
		return FALSE;
	}

	memset(&sock_addr, 0, sizeof(struct sockaddr_un));
	addr_len = sizeof(struct sockaddr_un);

	client_sock = accept(server_info->control_fd,
			     (struct sockaddr *)&sock_addr,
			     (socklen_t *)&addr_len);

	if (client_sock < 0) {
		BT_ERR("Server Accept Error");
		return TRUE;

	} else {
		BT_INFO("Accept Client Sock.(%d)", client_sock);

	}

	if (_bt_set_socket_non_blocking(client_sock) < 0)
		BT_ERR("Error in set non blocking!\n");

	server_info->data_fd = client_sock;
	server_info->data_io = g_io_channel_unix_new(client_sock);

	g_io_channel_set_encoding(server_info->data_io, NULL, NULL);
#ifdef GIOOUT_Q_WRITE
	g_io_channel_set_buffered(server_info->data_io, FALSE);
	server_info->tx_queue = g_queue_new();
#endif
	g_io_channel_set_flags(server_info->data_io, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(server_info->data_io, TRUE);

	server_info->data_id =
	    g_io_add_watch(server_info->data_io,
			   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			   __bt_rfcomm_server_data_received_cb, server_info);

	g_io_channel_unref(server_info->data_io);

	__bt_rfcomm_server_get_address(server_info);

	if (server_info->remote_address == NULL)
		server_info->remote_address = g_strdup("");

	if (server_info->server_type == BT_CUSTOM_SERVER) {
		int result;
		GArray *out_param1;
		GArray *out_param2;

		req_info = _bt_get_request_info(server_info->accept_id);
		if (req_info == NULL || req_info->context == NULL) {
			BT_DBG("info is NULL");
			goto done;
		}

		server_info->accept_id = 0;
		result = BLUETOOTH_ERROR_NONE;

		out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
		out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

		g_array_append_vals(out_param1, &server_info->data_fd,
					sizeof(int));
		g_array_append_vals(out_param2, &result, sizeof(int));

		dbus_g_method_return(req_info->context, out_param1, out_param2);

		g_array_free(out_param1, TRUE);
		g_array_free(out_param2, TRUE);

		_bt_delete_request_list(req_info->req_id);
	}

done:
	BT_INFO("UUID : %s", server_info->uuid);
	_bt_send_event(BT_RFCOMM_SERVER_EVENT,
		BLUETOOTH_EVENT_RFCOMM_CONNECTED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &server_info->remote_address,
		DBUS_TYPE_STRING, &server_info->uuid,
		DBUS_TYPE_INT16, &server_info->data_fd,
		DBUS_TYPE_INVALID);

	BT_DBG("-");
	return TRUE;
}


int _bt_rfcomm_listen(int socket_fd, int max_pending, gboolean is_native)
{
	int io_id;
	bt_rfcomm_server_info_t *server_info;
	GIOChannel *io_channel;

	server_info = __bt_rfcomm_get_server_info(socket_fd);
	retv_if(server_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(server_info->control_io != NULL, BLUETOOTH_ERROR_DEVICE_BUSY);

	if (listen(socket_fd, max_pending) != 0) {
		BT_DBG("Fail to listen");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	io_channel = g_io_channel_unix_new(socket_fd);
	server_info->control_io = io_channel;

	g_io_channel_set_close_on_unref(io_channel, TRUE);
	g_io_channel_set_encoding(io_channel, NULL, NULL);
	g_io_channel_set_flags(io_channel, G_IO_FLAG_NONBLOCK, NULL);

	io_id = g_io_add_watch(io_channel,
			   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			   __bt_rfcomm_server_connected_cb,
			   server_info);

	server_info->control_id = io_id;
	g_io_channel_unref(io_channel);

	/* BT_CUSTOM_SERVER / BT_NATIVE_SERVER*/
	if (is_native) {
		server_info->server_type = BT_NATIVE_SERVER;
	} else {
		server_info->server_type = BT_CUSTOM_SERVER;
		_bt_register_osp_server_in_agent(BT_RFCOMM_SERVER,
						server_info->uuid);
	}

	return BLUETOOTH_ERROR_NONE;
}

#ifdef GIOOUT_Q_WRITE
static void write_info_free(bt_rfcomm_write_info_t *write_info)
{
	g_free(write_info->buf);
	g_free(write_info);
}
#endif

int _bt_rfcomm_remove_socket(int socket_fd)
{
	bt_rfcomm_server_info_t *server_info;
	int result = BLUETOOTH_ERROR_NONE;

	BT_INFO("+");

	server_info = __bt_rfcomm_get_server_info(socket_fd);
	retv_if(server_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	if (server_info->serial_proxy) {
		if (!dbus_g_proxy_call(server_info->serial_proxy, "Disable",
					NULL,
					G_TYPE_INVALID,
					G_TYPE_INVALID)) {
			BT_DBG("Fail to disable");
		}
	}

	if (server_info->manager_proxy && server_info->serial_path) {
		if (!dbus_g_proxy_call(server_info->manager_proxy,
				"RemoveProxy", NULL,
				G_TYPE_STRING, server_info->serial_path,
				G_TYPE_INVALID,
				G_TYPE_INVALID)) {
			BT_DBG("Fail to remove proxy");
		}
	}

	if (server_info->server_type == BT_CUSTOM_SERVER) {
		_bt_unregister_osp_server_in_agent(BT_RFCOMM_SERVER,
						server_info->uuid);
	}

	_bt_send_event(BT_RFCOMM_SERVER_EVENT,
		BLUETOOTH_EVENT_RFCOMM_SERVER_REMOVED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_INT16, &server_info->data_fd,
		DBUS_TYPE_INVALID);

	_bt_rfcomm_server_disconnect(server_info->data_fd);

	if (server_info->control_id > 0)
		g_source_remove(server_info->control_id);

	if (server_info->control_fd > 0)
		close(server_info->control_fd);

	server_list = g_slist_remove(server_list, server_info);

	__bt_rfcomm_delete_server_id(server_info->server_id);

	g_free(server_info->serial_path);
	g_free(server_info->uuid);
	g_free(server_info->sender);
#ifdef GIOOUT_Q_WRITE
	g_queue_foreach(server_info->tx_queue, (GFunc) write_info_free, NULL);
	g_queue_free(server_info->tx_queue);
	if (server_info->write_id > 0)
		g_source_remove(server_info->write_id);
	g_free(server_info->tx_buf);
#endif
	g_free(server_info);

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_rfcomm_server_disconnect_cb(void *data)
{
	bt_rfcomm_event_info_t *event_info = data;
	int result = BLUETOOTH_ERROR_NONE;

	retv_if(event_info == NULL, BLUETOOTH_ERROR_NONE);
	retv_if(event_info->uuid == NULL, BLUETOOTH_ERROR_NONE);

	BT_INFO("%s", event_info->uuid);

	if (event_info->remote_address == NULL)
		event_info->remote_address = g_strdup("");

	_bt_send_event(BT_RFCOMM_SERVER_EVENT,
		BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &event_info->remote_address,
		DBUS_TYPE_STRING, &event_info->uuid,
		DBUS_TYPE_INT16, &event_info->data_fd,
		DBUS_TYPE_INVALID);

	g_free(event_info->uuid);
	g_free(event_info->remote_address);
	g_free(event_info);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_disconnect(int data_fd)
{
	bt_rfcomm_server_info_t *server_info;
	bt_rfcomm_event_info_t *event_info;

	BT_INFO("+");

	retv_if(data_fd <= 0, BLUETOOTH_ERROR_INVALID_PARAM);

	server_info = _bt_rfcomm_get_server_info_using_data_fd(data_fd);
	retv_if(server_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	if (server_info->data_id > 0)
		g_source_remove(server_info->data_id);

	if (server_info->data_fd > 0)
		close(server_info->data_fd);

	event_info = g_malloc0(sizeof(bt_rfcomm_event_info_t));
	event_info->data_fd = server_info->data_fd;
	event_info->remote_address = g_strdup(server_info->remote_address);
	event_info->uuid = g_strdup(server_info->uuid);

	/* Send the disconnected event after return the function */
	g_idle_add((GSourceFunc)__bt_rfcomm_server_disconnect_cb, event_info);

	g_free(server_info->remote_address);
	server_info->remote_address = NULL;
	server_info->data_fd = -1;
	server_info->data_id = 0;
	server_info->data_io = NULL;

	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_rfcomm_is_uuid_available(char *uuid, gboolean *available)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	BT_CHECK_PARAMETER(uuid, return);
	BT_CHECK_PARAMETER(available, return);

	*available = FALSE;

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (g_ascii_strcasecmp(uuid, server_info->uuid) == 0) {
			*available = TRUE;
			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

gboolean __bt_rfcomm_server_accept_timeout_cb(gpointer user_data)
{
	BT_INFO("+");

	bt_rfcomm_server_info_t *server_info;
	request_info_t *req_info;
	GArray *out_param1;
	GArray *out_param2;
	int result = BLUETOOTH_ERROR_TIMEOUT;

	server_info = (bt_rfcomm_server_info_t *)user_data;

	/* Already reply in __bt_rfcomm_server_connected_cb */
	retv_if(server_info == NULL, FALSE);
	retv_if(server_info->accept_id == 0, FALSE);

	req_info = _bt_get_request_info(server_info->accept_id);
	if (req_info == NULL || req_info->context == NULL) {
		BT_ERR("info is NULL");
		return FALSE;
	}

	server_info->accept_id = 0;

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(req_info->context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	_bt_delete_request_list(req_info->req_id);

	return FALSE;
}

/* To support the BOT  */
int _bt_rfcomm_accept_connection(int server_fd, int request_id)
{
	BT_INFO("+");

	bt_rfcomm_server_info_t *server_info;

	server_info = __bt_rfcomm_get_server_info(server_fd);
	retv_if(server_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(server_info->server_type != BT_CUSTOM_SERVER,
					BLUETOOTH_ERROR_INVALID_PARAM);

	if (!_bt_agent_reply_authorize(TRUE))
		return BLUETOOTH_ERROR_INTERNAL;

	server_info->accept_id = request_id;

	g_timeout_add(BT_SERVER_ACCEPT_TIMEOUT,
			(GSourceFunc)__bt_rfcomm_server_accept_timeout_cb,
			server_info);

	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_rfcomm_reject_connection(int server_fd)
{
	BT_INFO("+");

	bt_rfcomm_server_info_t *server_info;

	server_info = __bt_rfcomm_get_server_info(server_fd);
	retv_if(server_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(server_info->server_type != BT_CUSTOM_SERVER,
					BLUETOOTH_ERROR_INVALID_PARAM);

	if (!_bt_agent_reply_authorize(FALSE))
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

bt_rfcomm_server_info_t *_bt_rfcomm_get_server_info_using_uuid(char *uuid)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	retv_if(uuid == NULL, NULL);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (g_strcmp0(server_info->uuid, uuid) == 0)
			return server_info;
	}

	return NULL;
}

int _bt_rfcomm_server_disconnect_all_connection(void)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		_bt_rfcomm_disconnect(server_info->data_fd);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_check_existence(gboolean *existence)
{
	BT_CHECK_PARAMETER(existence, return);

	if (server_list && g_slist_length(server_list) > 0) {
		*existence = TRUE;
	} else {
		*existence = FALSE;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_check_termination(char *name)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	BT_CHECK_PARAMETER(name, return);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (g_strcmp0(server_info->sender, name) == 0) {
			BT_ERR("Socket server application has terminated abnormally");
			_bt_rfcomm_remove_socket(server_info->control_fd);
		}
	}

	return BLUETOOTH_ERROR_NONE;
}


