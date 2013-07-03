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

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <dirent.h>
#include <vconf.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-obex-agent.h"
#include "bt-service-obex-server.h"
#include "bt-service-agent.h"

#define BT_OBEX_SERVER_AGENT_PATH "/org/obex/server_agent"
#define BT_OBEX_SERVICE "org.openobex"
#define BT_OBEX_MANAGER "org.openobex.Manager"

typedef struct {
	char *filename;
	char *path;
	char *type;
	char *device_name;
	int transfer_id;
	gint64 file_size;
} bt_transfer_info_t;

typedef struct {
	DBusGMethodInvocation *reply_context;
	gint64 file_size;
	char *filename;
	char *device_name;
	char *transfer_path;
} bt_auth_info_t;

typedef struct {
	char *dest_path;
	char *sender;
	int app_pid;
} bt_server_info_t;

typedef struct {
	BtObexAgent *obex_agent;
	DBusGProxy *proxy;
	int server_type;
	int accept_id;
	bt_auth_info_t *auth_info;
	bt_server_info_t *native_server;
	bt_server_info_t *custom_server;
} bt_obex_agent_info_t;

static GSList *transfers;
static bt_obex_agent_info_t agent_info;

static GQuark __bt_obex_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

static bt_transfer_info_t *__bt_find_transfer_by_id(int transfer_id)
{
	GSList *l;
	bt_transfer_info_t *transfer;

	for (l = transfers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		if (transfer->transfer_id == transfer_id)
			return transfer;
	}

	return NULL;
}

static bt_transfer_info_t *__bt_find_transfer_by_path(const char *transfer_path)
{
	GSList *l;
	bt_transfer_info_t *transfer;

	retv_if(transfer_path == NULL, NULL);

	for (l = transfers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		if (g_strcmp0(transfer->path, transfer_path) == 0)
			return transfer;
	}

	return NULL;
}

static void __bt_free_server_info(bt_server_info_t *server_info)
{
	ret_if(server_info == NULL);

	g_free(server_info->sender);
	g_free(server_info->dest_path);
	g_free(server_info);
}

static void __bt_free_auth_info(bt_auth_info_t *auto_info)
{
	ret_if(auto_info == NULL);

	g_free(auto_info->filename);
	g_free(auto_info->transfer_path);
	g_free(auto_info->device_name);
	g_free(auto_info);
}

static void __bt_free_transfer_info(bt_transfer_info_t *transfer_info)
{
	ret_if(transfer_info == NULL);

	g_free(transfer_info->path);
	g_free(transfer_info->filename);
	g_free(transfer_info->type);
	g_free(transfer_info->device_name);
	g_free(transfer_info);
}

static char *__bt_get_remote_device_name(const char *bdaddress)
{
	GError *error = NULL;
	char *device_path = NULL;
	char *name = NULL;
	GHashTable *hash = NULL;
	GValue *value;
	DBusGProxy *device_proxy;
	DBusGProxy *adapter_proxy;
	DBusGConnection *conn;

	retv_if(bdaddress == NULL, NULL);

	adapter_proxy = _bt_get_adapter_proxy();
	retv_if(adapter_proxy == NULL, NULL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, NULL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", NULL,
			  G_TYPE_STRING, bdaddress, G_TYPE_INVALID,
			  DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);

	retv_if(device_path == NULL, NULL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, NULL);
	if (!dbus_g_proxy_call(device_proxy, "GetProperties", &error,
			G_TYPE_INVALID,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			&hash, G_TYPE_INVALID)) {
		if (error) {
			BT_ERR( "error in GetBasicProperties [%s]\n", error->message);
			g_error_free(error);
		}
		g_object_unref(device_proxy);
		return NULL;
	}

	if (hash != NULL) {
		value = g_hash_table_lookup(hash, "Alias");
		name = value ? g_value_dup_string(value) : NULL;
		g_hash_table_destroy(hash);
	}

	g_object_unref(device_proxy);

	return name;
}

static int __bt_get_transfer_id(const char *path)
{
	char *tmp = NULL;
	if (path == NULL)
		return -1;

	tmp = strrchr(path, 'r') + 1;
	retv_if(tmp == NULL, -1);

	return atoi(tmp);
}

static DBusGProxy *__bt_get_transfer_proxy(const char *transfer_path)
{
	DBusGConnection *conn;
	DBusGProxy *proxy;

	conn = _bt_get_session_gconn();
	retv_if(conn == NULL, NULL);

	proxy = dbus_g_proxy_new_for_name(conn,
					BT_OBEXD_INTERFACE,
					transfer_path,
					BT_OBEXD_TRANSFER_INTERFACE);

	return proxy;
}

static int __bt_get_transfer_properties(bt_transfer_info_t *transfer_info,
					const char *transfer_path)
{
	GHashTable *hash = NULL;
	GValue *value;
	DBusGProxy *transfer_proxy;
	char *bdaddress;

	BT_CHECK_PARAMETER(transfer_info, return);
	BT_CHECK_PARAMETER(transfer_path, return);

	transfer_proxy = __bt_get_transfer_proxy(transfer_path);

	retv_if(transfer_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(transfer_proxy, "GetProperties", NULL,
	                        G_TYPE_INVALID,
	                        dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
	                        &hash, G_TYPE_INVALID);

	if (hash == NULL) {
		g_object_unref(transfer_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	value = g_hash_table_lookup(hash, "Operation");
	transfer_info->type = value ? g_strdup(g_value_get_string(value)) : NULL;
	if (!transfer_info->type)
		goto fail;

	value = g_hash_table_lookup(hash, "Filename");
	transfer_info->filename = value ? g_strdup(g_value_get_string(value)) : NULL;
	if (!transfer_info->filename)
		goto fail;

	value = g_hash_table_lookup(hash, "Size");
	transfer_info->file_size  = value ? g_value_get_uint64(value) : 0;

	transfer_info->path = g_strdup(transfer_path);
	transfer_info->transfer_id = __bt_get_transfer_id(transfer_path);

	value = g_hash_table_lookup(hash, "Address");
	bdaddress = value ? (char *)g_value_get_string(value) : NULL;
	if (!bdaddress)
		goto fail;

	transfer_info->device_name = __bt_get_remote_device_name(bdaddress);
	if (!transfer_info->device_name)
		transfer_info->device_name = g_strdup(bdaddress);

	g_hash_table_destroy(hash);
	g_object_unref(transfer_proxy);
	return BLUETOOTH_ERROR_NONE;

fail:
	g_hash_table_destroy(hash);
	g_object_unref(transfer_proxy);
	return BLUETOOTH_ERROR_INTERNAL;
}

static gboolean __bt_authorize_cb(DBusGMethodInvocation *context,
					const char *path,
					const char *bdaddress,
					const char *name,
					const char *type,
					gint length,
					gint time,
					gpointer user_data)
{
	char *device_name = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG(" File name [%s] Address [%s] Type [%s] length [%d] path [%s] \n",
	    name, bdaddress, type, length, path);

	__bt_free_auth_info(agent_info.auth_info);

	agent_info.auth_info = g_malloc(sizeof(bt_auth_info_t));

	agent_info.auth_info->reply_context = context;
	agent_info.auth_info->filename = g_strdup(name);
	agent_info.auth_info->file_size = length;
	agent_info.auth_info->transfer_path = g_strdup(path);

	device_name = __bt_get_remote_device_name(bdaddress);

	if (!device_name)
		device_name = g_strdup(bdaddress);

	agent_info.auth_info->device_name = device_name;

	if (agent_info.server_type == BT_CUSTOM_SERVER) {
		/* No need to send the event */
		_bt_obex_server_accept_authorize(agent_info.auth_info->filename, FALSE);
		return TRUE;
	}

	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &agent_info.auth_info->filename,
		DBUS_TYPE_UINT64, &agent_info.auth_info->file_size,
		DBUS_TYPE_INVALID);

	return TRUE;
}

void _bt_obex_transfer_started(const char *transfer_path)
{
	bt_transfer_info_t *transfer_info;
	request_info_t *req_info;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("%s", transfer_path);

	transfer_info = g_malloc0(sizeof(bt_transfer_info_t));

	if (agent_info.auth_info != NULL
	     && g_strcmp0(transfer_path, agent_info.auth_info->transfer_path) == 0) {
		transfer_info->filename = g_strdup(agent_info.auth_info->filename);
		transfer_info->file_size = agent_info.auth_info->file_size;
		transfer_info->type = g_strdup(TRANSFER_PUT);
		transfer_info->path = g_strdup(agent_info.auth_info->transfer_path);
		transfer_info->device_name = g_strdup(agent_info.auth_info->device_name);
		transfer_info->transfer_id = __bt_get_transfer_id(transfer_path);
	} else {
		if (__bt_get_transfer_properties(transfer_info, transfer_path) < 0) {
			BT_ERR("Get Properties failed");
			__bt_free_auth_info(agent_info.auth_info);
			__bt_free_transfer_info(transfer_info);
			agent_info.auth_info = NULL;
			return;
		}
		agent_info.server_type = BT_FTP_SERVER;
	}

	__bt_free_auth_info(agent_info.auth_info);
	agent_info.auth_info = NULL;

	if (agent_info.server_type == BT_CUSTOM_SERVER) {
		if (agent_info.custom_server == NULL) {
			__bt_free_transfer_info(transfer_info);
			return;
		}

		req_info = _bt_get_request_info(agent_info.accept_id);
		if (req_info == NULL || req_info->context == NULL) {
			BT_ERR("info is NULL");
			goto done;
		}

		agent_info.accept_id = 0;
		result = BLUETOOTH_ERROR_NONE;

		out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
		out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

		g_array_append_vals(out_param2, &result, sizeof(int));

		dbus_g_method_return(req_info->context, out_param1, out_param2);

		g_array_free(out_param1, TRUE);
		g_array_free(out_param2, TRUE);

		_bt_delete_request_list(req_info->req_id);
	}
done:
	transfers = g_slist_append(transfers, transfer_info);

	BT_DBG("Transfer id %d\n", transfer_info->transfer_id);

	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &transfer_info->filename,
		DBUS_TYPE_STRING, &transfer_info->type,
		DBUS_TYPE_UINT64, &transfer_info->file_size,
		DBUS_TYPE_INT32, &transfer_info->transfer_id,
		DBUS_TYPE_INT32, &agent_info.server_type,
		DBUS_TYPE_INVALID);
}

void _bt_obex_transfer_progress(const char *transfer_path,
					int total,
					int transferred)
{
	bt_transfer_info_t *transfer_info;
	int progress = 0;
	int result = BLUETOOTH_ERROR_NONE;

	transfer_info = __bt_find_transfer_by_path(transfer_path);
	ret_if(transfer_info == NULL);

	progress = (int)(((gdouble)transferred /(gdouble)total) * 100);

	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &transfer_info->filename,
		DBUS_TYPE_STRING, &transfer_info->type,
		DBUS_TYPE_UINT64, &transfer_info->file_size,
		DBUS_TYPE_INT32, &transfer_info->transfer_id,
		DBUS_TYPE_INT32, &progress,
		DBUS_TYPE_INT32, &agent_info.server_type,
		DBUS_TYPE_INVALID);
}

void _bt_obex_transfer_completed(const char *transfer_path, gboolean success)
{
	bt_transfer_info_t *transfer_info;
	int result;
	BT_DBG("Transfer [%s] Success [%d] \n", transfer_path, success);

	result = (success == TRUE) ? BLUETOOTH_ERROR_NONE
				: BLUETOOTH_ERROR_CANCEL;

	transfer_info = __bt_find_transfer_by_path(transfer_path);
	ret_if(transfer_info == NULL);

	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,
		DBUS_TYPE_INT32, &result,
		DBUS_TYPE_STRING, &transfer_info->filename,
		DBUS_TYPE_STRING, &transfer_info->type,
		DBUS_TYPE_STRING, &transfer_info->device_name,
		DBUS_TYPE_UINT64, &transfer_info->file_size,
		DBUS_TYPE_INT32, &transfer_info->transfer_id,
		DBUS_TYPE_INT32, &agent_info.server_type,
		DBUS_TYPE_INVALID);

	transfers = g_slist_remove(transfers, transfer_info);
	__bt_free_transfer_info(transfer_info);
}

int _bt_register_obex_server(void)
{
	DBusGConnection *g_conn;
	DBusGProxy *manager_proxy;
	GError *g_error = NULL;

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!agent_info.obex_agent) {
		agent_info.obex_agent = _bt_obex_agent_new();

		retv_if(agent_info.obex_agent == NULL, BLUETOOTH_ERROR_INTERNAL);

		_bt_obex_setup(agent_info.obex_agent, BT_OBEX_SERVER_AGENT_PATH);

		_bt_obex_set_authorize_cb(agent_info.obex_agent,
					__bt_authorize_cb, NULL);
	}

	manager_proxy = dbus_g_proxy_new_for_name(g_conn, BT_OBEX_SERVICE,
						"/", BT_OBEX_MANAGER);

	if (manager_proxy == NULL) {
		g_object_unref(agent_info.obex_agent);
		agent_info.obex_agent = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_g_proxy_call(manager_proxy, "RegisterAgent", &g_error,
			  DBUS_TYPE_G_OBJECT_PATH, BT_OBEX_SERVER_AGENT_PATH,
			  G_TYPE_INVALID, G_TYPE_INVALID);
	if (g_error != NULL) {
		BT_ERR("Agent registration failed: %s\n", g_error->message);
		g_object_unref(agent_info.obex_agent);
		agent_info.obex_agent = NULL;
		g_object_unref(manager_proxy);
		g_error_free(g_error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	agent_info.proxy = manager_proxy;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_obex_server(void)
{
	GError *g_error = NULL;

	retv_if(agent_info.obex_agent == NULL,
				BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST);

	retv_if(agent_info.proxy == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(agent_info.proxy, "UnregisterAgent", &g_error,
			  DBUS_TYPE_G_OBJECT_PATH, BT_OBEX_SERVER_AGENT_PATH,
			  G_TYPE_INVALID, G_TYPE_INVALID);
	if (g_error != NULL) {
		BT_ERR("Agent unregistration failed: %s", g_error->message);
		g_error_free(g_error);
	}

	g_object_unref(agent_info.proxy);
	agent_info.proxy = NULL;

	g_object_unref(agent_info.obex_agent);
	agent_info.obex_agent = NULL;

	return BLUETOOTH_ERROR_NONE;
}

gboolean __bt_check_folder_path(const char *dest_path)
{
	int ret;

	retv_if(dest_path == NULL, TRUE);

	ret = access(dest_path, W_OK);

	if (ret < 0) {
		BT_ERR("The directory can not be accessible: [%d]", ret);
		return FALSE;
	}

	return TRUE;
}

int _bt_obex_server_allocate(char *sender, const char *dest_path, int app_pid, gboolean is_native)
{
	if (__bt_check_folder_path(dest_path) == FALSE)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (is_native == TRUE) {
		retv_if(agent_info.native_server,
				BLUETOOTH_ERROR_DEVICE_BUSY);

		/* Force to change the control to native */
		agent_info.native_server = g_malloc0(sizeof(bt_server_info_t));
		agent_info.native_server->dest_path = g_strdup(dest_path);
		agent_info.native_server->sender = g_strdup(sender);
		agent_info.native_server->app_pid = app_pid;
		agent_info.server_type = BT_NATIVE_SERVER;
		_bt_unregister_osp_server_in_agent(BT_OBEX_SERVER, NULL);
	} else {
		retv_if(agent_info.custom_server,
				BLUETOOTH_ERROR_DEVICE_BUSY);

		/* Force to change the control to custom */
		agent_info.custom_server = g_malloc0(sizeof(bt_server_info_t));
		agent_info.custom_server->dest_path = g_strdup(dest_path);
		agent_info.custom_server->sender = g_strdup(sender);
		agent_info.custom_server->app_pid = app_pid;
		agent_info.server_type = BT_CUSTOM_SERVER;
		_bt_register_osp_server_in_agent(BT_OBEX_SERVER, NULL);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_deallocate(int app_pid, gboolean is_native)
{
	if (is_native == TRUE) {
		retv_if(agent_info.native_server == NULL,
				BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST);

		retv_if(agent_info.native_server->app_pid != app_pid,
				BLUETOOTH_ERROR_ACCESS_DENIED);

		__bt_free_server_info(agent_info.native_server);
		agent_info.native_server = NULL;

		/* Change the control to custom */
		if (agent_info.custom_server) {
			agent_info.server_type = BT_CUSTOM_SERVER;
			_bt_register_osp_server_in_agent(BT_OBEX_SERVER,
							NULL);
		}
	} else {
		retv_if(agent_info.custom_server == NULL,
				BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST);

		retv_if(agent_info.custom_server->app_pid != app_pid,
				BLUETOOTH_ERROR_ACCESS_DENIED);

		__bt_free_server_info(agent_info.custom_server);
		agent_info.custom_server = NULL;

		_bt_unregister_osp_server_in_agent(BT_OBEX_SERVER, NULL);

		/* Change the control to native */
		if (agent_info.native_server)
			agent_info.server_type = BT_NATIVE_SERVER;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_accept_authorize(const char *filename, gboolean is_native)
{
	char file_path[BT_FILE_PATH_MAX] = { 0 };
	bt_server_info_t *server_info;

	BT_CHECK_PARAMETER(filename, return);

	retv_if(agent_info.auth_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	retv_if(agent_info.auth_info->reply_context == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	if (is_native == TRUE)
		server_info = agent_info.native_server;
	else
		server_info = agent_info.custom_server;

	retv_if(server_info == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (server_info->dest_path != NULL)
		snprintf(file_path, sizeof(file_path), "%s/%s",
			server_info->dest_path, filename);
	else
		snprintf(file_path, sizeof(file_path), "%s", filename);

	dbus_g_method_return(agent_info.auth_info->reply_context,
				file_path);

	agent_info.auth_info->reply_context = NULL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_reject_authorize(void)
{
	GError *g_error;

	retv_if(agent_info.auth_info->reply_context == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	g_error = g_error_new(__bt_obex_error_quark(),
			BT_OBEX_AGENT_ERROR_CANCEL,
			"CancelledByUser");

	dbus_g_method_return_error(agent_info.auth_info->reply_context,
				g_error);
	g_error_free(g_error);

	__bt_free_auth_info(agent_info.auth_info);
	agent_info.auth_info = NULL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_set_destination_path(const char *dest_path,
						gboolean is_native)
{
	bt_server_info_t *server_info;

	BT_CHECK_PARAMETER(dest_path, return);

	if (__bt_check_folder_path(dest_path) == FALSE)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	if (is_native == TRUE)
		server_info = agent_info.native_server;
	else
		server_info = agent_info.custom_server;

	retv_if(server_info == NULL,
			BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST);

	g_free(server_info->dest_path);
	server_info->dest_path = g_strdup(dest_path);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_set_root(const char *root)
{
	GError *g_error = NULL;
	GValue folder = { 0 };
	DIR *dp = NULL;

	BT_CHECK_PARAMETER(root, return);

	retv_if(agent_info.proxy == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	dp = opendir(root);

	if (dp == NULL) {
		BT_ERR("The directory does not exist");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	closedir(dp);

	g_value_init(&folder, G_TYPE_STRING);
	g_value_set_string(&folder, root);

	dbus_g_proxy_call(agent_info.proxy, "SetProperty",
			&g_error, G_TYPE_STRING, "Root",
			G_TYPE_VALUE, &folder, G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&folder);

	if (g_error) {
		BT_ERR("SetProperty Fail: %s", g_error->message);
		g_error_free(g_error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_cancel_transfer(int transfer_id)
{
	bt_transfer_info_t *transfer = NULL;
	DBusGProxy *proxy;

	transfer = __bt_find_transfer_by_id(transfer_id);
	retv_if(transfer == NULL, BLUETOOTH_ERROR_NOT_FOUND);

	proxy = __bt_get_transfer_proxy(transfer->path);

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call_no_reply(proxy, "Cancel",
				G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref(proxy);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_cancel_all_transfers(void)
{
	GSList *l;
	bt_transfer_info_t *transfer;

	for (l = transfers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		_bt_obex_server_cancel_transfer(transfer->transfer_id);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_is_activated(gboolean *activated)
{
	BT_CHECK_PARAMETER(activated, return);

	if (agent_info.custom_server) {
		*activated = TRUE;
	} else {
		*activated = FALSE;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_check_allocation(gboolean *allocation)
{
	BT_CHECK_PARAMETER(allocation, return);

	if (agent_info.native_server || agent_info.custom_server) {
		*allocation = TRUE;
	} else {
		*allocation = FALSE;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_check_termination(char *sender)
{
	BT_CHECK_PARAMETER(sender, return);

	if (agent_info.native_server) {
		if (g_strcmp0(sender, agent_info.native_server->sender) == 0) {
			_bt_obex_server_deallocate(agent_info.native_server->app_pid,
						TRUE);
		}
	}

	if (agent_info.custom_server) {
		if (g_strcmp0(sender, agent_info.custom_server->sender) == 0) {
			_bt_obex_server_deallocate(agent_info.custom_server->app_pid,
						FALSE);
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_obex_server_is_receiving(gboolean *receiving)
{
	BT_CHECK_PARAMETER(receiving, return);

	if (transfers == NULL || g_slist_length(transfers) == 0) {
		*receiving = FALSE;
	} else {
		*receiving = TRUE;
	}

	return BLUETOOTH_ERROR_NONE;
}

gboolean __bt_obex_server_accept_timeout_cb(gpointer user_data)
{
	request_info_t *req_info;
	GArray *out_param1;
	GArray *out_param2;
	int result = BLUETOOTH_ERROR_TIMEOUT;

	/* Already reply in _bt_obex_transfer_started */
	retv_if(agent_info.accept_id == 0, FALSE);

	req_info = _bt_get_request_info(agent_info.accept_id);
	if (req_info == NULL || req_info->context == NULL) {
		BT_ERR("info is NULL");
		return FALSE;
	}

	agent_info.accept_id = 0;

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
int _bt_obex_server_accept_connection(int request_id)
{
	if (!_bt_agent_reply_authorize(TRUE))
		return BLUETOOTH_ERROR_INTERNAL;

	agent_info.accept_id = request_id;

	g_timeout_add(BT_SERVER_ACCEPT_TIMEOUT,
			(GSourceFunc)__bt_obex_server_accept_timeout_cb,
			NULL);

	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_obex_server_reject_connection(void)
{
	if (!_bt_agent_reply_authorize(FALSE))
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

