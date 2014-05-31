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
#endif

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

#ifdef RFCOMM_DIRECT
static const gchar rfcomm_agent_xml[] =
"<node name='/'>"
" <interface name='org.rfcomm.agent'>"
"     <method name='NewConnection'>"
"          <arg type='h' name='fd' direction='in'/>"
"          <arg type='s' name='uuid' direction='in'/>"
"          <arg type='s' name='address' direction='in'/>"
"     </method>"
"  </interface>"
"</node>";

static GDBusNodeInfo *node_info;
static GDBusConnection *service_gconn;
static GDBusProxy *serial_gproxy;
static GSList *rfcomm_nodes;

#define BT_RFCOMM_SERVER_ID_MAX 245

static int latest_id = -1;
static gboolean server_id_used[BT_RFCOMM_SERVER_ID_MAX];

typedef struct {
	guint object_id;
	gchar *path;
	int id;
	char *uuid;
	int fd;
	GIOChannel *data_io;
	guint data_id;
	bluetooth_device_address_t addr;
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

static GDBusConnection *__get_gdbus_connection()
{
	GError *err = NULL;

	if (service_gconn == NULL)
		service_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

	if (!service_gconn) {
		if (err) {
			BT_ERR("Unable to connect to dbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	return service_gconn;
}

static GDBusProxy *__bt_gdbus_get_serial_proxy(void)
{
	GDBusProxy *proxy;
	GError *err = NULL;
	char path[BT_ADAPTER_OBJECT_PATH_MAX] = {0};
	GDBusConnection *gconn;

	g_type_init();

	if (serial_gproxy)
		return serial_gproxy;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return NULL;

	if (_bt_get_adapter_path(_bt_get_system_gconn(), path) !=
							BLUETOOTH_ERROR_NONE)
		return NULL;

	proxy =  g_dbus_proxy_new_sync(gconn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			path,
			"org.bluez.SerialProxyManager",
			NULL, &err);
	if (!proxy) {
		if (err) {
			 BT_ERR("Unable to create proxy: %s", err->message);
			 g_clear_error(&err);
		}

		return NULL;
	}

	serial_gproxy = proxy;

	return proxy;
}

static void __connected_cb(rfcomm_info_t *info, bt_event_info_t *event_info)
{
	bluetooth_rfcomm_connection_t conn_info;

	BT_INFO("++++++++++++  RFCOMM Connected ++++++++++++++++ ");

	memset(&conn_info, 0x00, sizeof(bluetooth_rfcomm_connection_t));
	conn_info.device_role = RFCOMM_ROLE_SERVER;

	BT_INFO("UUID : %s", info->uuid);
	g_strlcpy(conn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	conn_info.socket_fd = info->fd;
	conn_info.device_addr = info->addr;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_CONNECTED,
			BLUETOOTH_ERROR_NONE, &conn_info,
			event_info->cb, event_info->user_data);
}

static void __rfcomm_server_disconnect(rfcomm_info_t *info,
						bt_event_info_t *event_info)
{
	bluetooth_rfcomm_disconnection_t disconn_info;
	int fd = info->fd;

	BT_INFO("--------- RFCOMM Disconnected ----------- ");

	if (info->data_id > 0) {
		g_source_remove(info->data_id);
		info->data_id = 0;
	}

	if (info->fd > 0) {
		close(info->fd);
		info->fd = -1;
	}

	if (event_info == NULL)
		return;

	BT_INFO("UUID : %s", info->uuid);
	memset(&disconn_info, 0x00, sizeof(bluetooth_rfcomm_disconnection_t));
	disconn_info.device_role = RFCOMM_ROLE_SERVER;
	g_strlcpy(disconn_info.uuid, info->uuid, BLUETOOTH_UUID_STRING_MAX);
	disconn_info.socket_fd = fd;
	disconn_info.device_addr = info->addr;

	_bt_common_event_cb(BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,
			BLUETOOTH_ERROR_NONE, &disconn_info,
			event_info->cb, event_info->user_data);

	BT_DBG("-");

}

static gboolean __data_received_cb(GIOChannel *chan, GIOCondition cond,
								gpointer data)
{
	char *buffer = NULL;
	gsize len;
	int result = BLUETOOTH_ERROR_NONE;
	rfcomm_info_t *info = data;
	bt_event_info_t *event_info;
	bluetooth_rfcomm_received_data_t data_r;

	retv_if(info == NULL, FALSE);

	event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR("server disconnected: %d", info->fd);
		__rfcomm_server_disconnect(info, event_info);
		return FALSE;
	}

	buffer = g_malloc0(BT_RFCOMM_BUFFER_LEN + 1);

	if (g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_LEN, &len,
								NULL) ==
							G_IO_STATUS_ERROR) {
		BT_ERR("IO Channel read error server");
		g_free(buffer);
		__rfcomm_server_disconnect(info, event_info);
		return FALSE;
	}

	BT_DBG("Length %d", len);
	if (len == 0) {
		BT_ERR("Read failed len=%d, fd=%d\n",
				len, info->fd);
		g_free(buffer);
		__rfcomm_server_disconnect(info, event_info);
		return FALSE;
	}

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

static void __rfcomm_method(GDBusConnection *connection,
					    const gchar *sender,
					    const gchar *object_path,
					    const gchar *interface_name,
					    const gchar *method_name,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data)
{
	bt_event_info_t *event_info;

	BT_DBG("method %s", method_name);
	if (g_strcmp0(method_name, "NewConnection") == 0) {
		char *uuid;
		int index;
		GDBusMessage *msg;
		GUnixFDList *fd_list;
		rfcomm_info_t *info;
		char *address;

		g_variant_get(parameters, "(hss)", &index, &uuid, &address);

		msg = g_dbus_method_invocation_get_message(invocation);
		fd_list = g_dbus_message_get_unix_fd_list(msg);
		if (fd_list == NULL) {
			GQuark quark = g_quark_from_string("rfcomm-agent");
			GError *err = g_error_new(quark, 0, "No fd in message");
			g_dbus_method_invocation_return_gerror(invocation, err);
			g_error_free(err);
			return;
		}

		info = __find_rfcomm_info_with_path(object_path);
		if (info == NULL) {
			BT_ERR("Invalid path %s", object_path);
			GQuark quark = g_quark_from_string("rfcomm-agent");
			GError *err = g_error_new(quark, 0, "Invalid path");
			g_dbus_method_invocation_return_gerror(invocation, err);
			g_error_free(err);
			return;
		}

		info->fd = g_unix_fd_list_get(fd_list, index, NULL);
		if (info->fd == -1) {
			BT_ERR("Invalid fd return");
			GQuark quark = g_quark_from_string("rfcomm-agent");
			GError *err = g_error_new(quark, 0, "Invalid FD return");
			g_dbus_method_invocation_return_gerror(invocation, err);
			g_error_free(err);
			return;
		}

		BT_INFO("FD : %d, UUID : %s", info->fd, uuid);
		DBG_SECURE("address %s", address);

		_bt_convert_addr_string_to_type(info->addr.addr, address);

		event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);
		if (event_info) {
			__connected_cb(info, event_info);
		}

		info->data_io = g_io_channel_unix_new(info->fd);

		g_io_channel_set_encoding(info->data_io, NULL, NULL);
		g_io_channel_set_flags(info->data_io, G_IO_FLAG_NONBLOCK, NULL);

		info->data_id = g_io_add_watch(info->data_io,
				   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				   __data_received_cb, info);

		g_io_channel_unref(info->data_io);

		g_dbus_method_invocation_return_value(invocation, NULL);
	}
}

static const GDBusInterfaceVTable method_table = {
	__rfcomm_method,
	NULL,
	NULL,
};

static int __rfcomm_assign_server_id(void)
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

	BT_INFO("Assigned Id: %d", latest_id);

	return latest_id;
}

void __rfcomm_delete_server_id(int server_id)
{
	ret_if(server_id >= BT_RFCOMM_SERVER_ID_MAX);
	ret_if(server_id < 0);

	server_id_used[server_id] = FALSE;

	/* Next server will use this ID */
	latest_id = server_id - 1;
}

static rfcomm_info_t *__register_method(GDBusConnection *connection)
{
	GError *error = NULL;
	gchar *path;
	rfcomm_info_t *info;
	guint object_id;
	int id;

	id = __rfcomm_assign_server_id();

	path = g_strdup_printf("/org/socket/agent/%d/%d", getpid(), id);

	BT_DBG("%s", path);

	object_id = g_dbus_connection_register_object(connection, path,
						node_info->interfaces[0],
						&method_table,
						NULL, NULL, &error);
	if (object_id == 0) {
		BT_ERR("Failed to register: %s", error->message);
		g_error_free(error);
		g_free(path);
		__rfcomm_delete_server_id(id);
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

void init_agent()
{
	BT_DBG("");

	if (node_info)
		return;

	node_info = g_dbus_node_info_new_for_xml(rfcomm_agent_xml, NULL);
	if (node_info == NULL) {
		BT_ERR("creation of node failed");
		return;
	}

	g_bus_own_name(G_BUS_TYPE_SYSTEM,
					"org.bt.frwk",
					G_BUS_NAME_OWNER_FLAGS_NONE,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
}

void free_rfcomm_info(rfcomm_info_t *info)
{
	bt_event_info_t *event_info;

	BT_DBG("");

	__rfcomm_delete_server_id(info->id);

	g_dbus_connection_unregister_object(service_gconn, info->object_id);

	if (info->fd >= 0) {
		event_info = _bt_event_get_cb_data(BT_RFCOMM_SERVER_EVENT);
		__rfcomm_server_disconnect(info, event_info);
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

	if (serial_gproxy) {
		g_object_unref(serial_gproxy);
		serial_gproxy = NULL;
	}

	if (service_gconn) {
		g_object_unref(service_gconn);
		service_gconn = NULL;
	}

}
#endif

BT_EXPORT_API int bluetooth_rfcomm_create_socket(const char *uuid)
{
#ifdef RFCOMM_DIRECT
	rfcomm_info_t *info;
	GDBusConnection *gconn;
#else
	int result;
	int socket_fd = -1;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX];
#endif

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(uuid, return);

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Create socket from app [%s] >>>>>>>>>", uuid);
	init_agent();

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return -1;

	info = __register_method(gconn);
	if (info == NULL)
		return -1;

	info->uuid = g_strdup(uuid);

	return info->id;
#else

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	BT_INFO("UUID : %s", uuid);
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
	GVariant *ret;
	rfcomm_info_t *info;
	GDBusProxy *proxy;
	GError *error = NULL;
#else
	int result;
#endif

	BT_CHECK_ENABLED(return);

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Disconnect request from app [%d] >>>>>>>>>>>", socket_fd);
	proxy = __bt_gdbus_get_serial_proxy();
	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	info = __find_rfcomm_info_with_id(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	ret = g_dbus_proxy_call_sync(proxy, "UnregisterProfile",
					g_variant_new("(o)", info->path),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &error);
	if (ret == NULL) {
		/* dBUS-RPC is failed */
		BT_ERR("dBUS-RPC is failed");

		if (error != NULL) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
			       error->code, error->message);

			g_clear_error(&error);
		}
	} else {
		g_variant_unref(ret);
	}

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
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &socket_fd, sizeof(int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_RFCOMM_SOCKET_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_INFO("result: %x", result);

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
#ifdef RFCOMM_DIRECT
	GVariant *ret;
	rfcomm_info_t *info;
	GDBusProxy *proxy;
	GError *error = NULL;
#else
	int result;
	gboolean native_service = TRUE;
#endif

	BT_CHECK_ENABLED(return);

#ifdef RFCOMM_DIRECT
	BT_INFO("<<<<<<<<< RFCOMM Listen & accept from app [%d] >>>>>>>>>>>", socket_fd);
	proxy = __bt_gdbus_get_serial_proxy();
	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	info = __find_rfcomm_info_with_id(socket_fd);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	ret = g_dbus_proxy_call_sync(proxy, "RegisterProfile",
					g_variant_new("(os)", info->path,
								info->uuid),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &error);
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

