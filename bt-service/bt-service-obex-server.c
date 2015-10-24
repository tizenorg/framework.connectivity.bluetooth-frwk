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

#include <stdio.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <dirent.h>
#ifdef TIZEN_MDM_ENABLE
#include <bt-service-mdm.h>
#endif
#include <vconf.h>

#include <gio/gio.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-obex-agent.h"
#include "bt-service-obex-server.h"
#include "bt-service-agent.h"

#define DBUS_TIMEOUT 20 * 1000 /* 20 Seconds */
#define BT_OBEX_SERVER_AGENT_PATH "/org/obex/server_agent"

#define BT_OBEX_SERVICE "org.bluez.obex"
#define BT_OBEX_MANAGER "org.bluez.obex.AgentManager1"
#define BT_OBEX_PATH "/org/bluez/obex"


typedef struct {
	char *filename;
	char *file_path;
	char *path;
	char *type;
	char *device_name;
	int transfer_id;
	gint64 file_size;
	char *address;
} bt_transfer_info_t;

typedef struct {
	GDBusMethodInvocation *reply_context;
	guint64 file_size;
	char *filename;
	char *file_path;
	char *device_name;
	char *transfer_path;
	char *address;
} bt_auth_info_t;

typedef struct {
	char *dest_path;
	char *sender;
	int app_pid;
} bt_server_info_t;

typedef struct {
	GDBusProxy *proxy;
	int server_type;
	int accept_id;
	bt_auth_info_t *auth_info;
	bt_server_info_t *native_server;
	bt_server_info_t *custom_server;
} bt_obex_agent_info_t;

static GSList *transfers;
static bt_obex_agent_info_t agent_info;

int _bt_obex_get_native_pid(void)
{
	return agent_info.native_server->app_pid;
}

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

bt_transfer_info_t *__bt_find_transfer_by_address(const char *address)
{
	BT_DBG("+");
	GSList *l;
	bt_transfer_info_t *transfer;

	retv_if(address == NULL, NULL);

	for (l = transfers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		if (g_strcmp0(transfer->address, address) == 0)
			return transfer;
	}
	BT_DBG("-");
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
	g_free(auto_info->address);
	g_free(auto_info);
}

static void __bt_free_transfer_info(bt_transfer_info_t *transfer_info)
{
	ret_if(transfer_info == NULL);

	g_free(transfer_info->path);
	g_free(transfer_info->filename);
	g_free(transfer_info->file_path);
	g_free(transfer_info->type);
	g_free(transfer_info->device_name);
	g_free(transfer_info->address);
	g_free(transfer_info);
}

void _bt_obex_check_pending_transfer(const char *address)
{
	BT_DBG("+");
	GVariant *param = NULL;
	bt_transfer_info_t *transfer_info = __bt_find_transfer_by_address(address);
	if (transfer_info != NULL) {
		int result = BLUETOOTH_ERROR_CANCEL;
		param = g_variant_new("(isssstii)", result,
					transfer_info->filename,
					transfer_info->type,
					transfer_info->device_name,
					transfer_info->file_path,
					transfer_info->file_size,
					transfer_info->transfer_id,
					agent_info.server_type);
		_bt_send_event(BT_OPP_SERVER_EVENT,
			BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,
			param);
		transfers = g_slist_remove(transfers, transfer_info);
		__bt_free_transfer_info(transfer_info);
	}
	BT_DBG("-");
}

static char *__bt_get_remote_device_name(const char *bdaddress)
{
	char *device_path = NULL;
	char *name = NULL;
	GVariant *value;
	GVariant *result = NULL;
	GError *err = NULL;
	GDBusProxy *device_proxy;
	GDBusConnection *conn;

	retv_if(bdaddress == NULL, NULL);

	device_path = _bt_get_device_object_path((char *)bdaddress);
	retv_if(device_path == NULL, NULL);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, NULL);
	BT_INFO("Device_path %s", device_path);
	device_proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_BLUEZ_NAME,
						device_path,
						BT_PROPERTIES_INTERFACE,
						NULL, &err);

	g_free(device_path);
	retv_if(device_proxy == NULL, NULL);

	result = g_dbus_proxy_call_sync(device_proxy, "GetAll",
			g_variant_new("(s)", BT_DEVICE_INTERFACE),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&err);
	if (err) {
		BT_ERR("DBus Error : %s", err->message);
		g_clear_error(&err);
		return NULL;
	}
	if (result == NULL) {
		BT_ERR("g_dbus_proxy_call_sync function return NULL");
		return NULL;
	}
	g_variant_get(result, "(@a{sv})", &value);
	g_variant_unref(result);

	g_object_unref(device_proxy);
	if (value) {
		GVariant *temp_value = g_variant_lookup_value(value, "Alias",
			G_VARIANT_TYPE_STRING);
		g_variant_get(temp_value, "s", &name);
		if (temp_value)
			g_variant_unref(temp_value);
		BT_INFO("Alias Name: %s", name);
	}

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

static GDBusProxy *__bt_get_transfer_proxy(const char *transfer_path)
{
	GDBusConnection *conn;
	GDBusProxy *proxy;
	GError *err = NULL;

	conn = _bt_get_session_gconn();
	retv_if(conn == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
					NULL, BT_OBEX_SERVICE_NAME,
					transfer_path,
					BT_OBEX_TRANSFER_INTERFACE,
					NULL, &err);

	if (err) {
		BT_ERR("Error : %s", err->message);
		g_clear_error(&err);
		return NULL;
	}

	return proxy;
}

static GDBusProxy *__bt_get_transfer_properties_proxy(const char *transfer_path)
{
	GDBusConnection *conn;
	GDBusProxy *proxy;
	GError *err = NULL;
	conn = _bt_get_session_gconn();
	retv_if(conn == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
					NULL,BT_OBEX_SERVICE_NAME,
					transfer_path,
					BT_PROPERTIES_INTERFACE,
					NULL, &err);
	if (err) {
		BT_ERR("Error : %s", err->message);
		g_clear_error(&err);
		return NULL;
	}
	return proxy;
}

static int __bt_get_transfer_properties(bt_transfer_info_t *transfer_info,
					const char *transfer_path)
{
	GDBusProxy *transfer_proxy;
	char *bdaddress = NULL;
	GVariant *result = NULL;
	GError *err = NULL;
	GVariantIter *iter = NULL;
	BT_CHECK_PARAMETER(transfer_info, return);
	BT_CHECK_PARAMETER(transfer_path, return);

	transfer_proxy = __bt_get_transfer_properties_proxy(transfer_path);

	retv_if(transfer_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(transfer_proxy, "GetAll",
			g_variant_new("(s)", BT_OBEX_TRANSFER_INTERFACE),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&err);

	if (err) {
		BT_ERR("DBus Error : %s", err->message);
		g_clear_error(&err);
		goto fail;
	}
	if (result == NULL) {
		BT_ERR("g_dbus_proxy_call_sync function return NULL");
		goto fail;
	}

	g_variant_get(result, "(a{sv})", &iter);
	g_variant_unref(result);
	if (iter) {
		const gchar *key;
		GVariant *val;
		gsize len = 0;
		while (g_variant_iter_loop(iter, "{sv}", &key, &val)) {
			if (g_strcmp0(key, "Operation") == 0) {
				transfer_info->type = g_variant_dup_string(val, &len);
			} else if (g_strcmp0(key, "Name") == 0) {
				transfer_info->filename = g_variant_dup_string(val, &len);
			} else if (g_strcmp0(key, "Size") == 0) {
				transfer_info->file_size = g_variant_get_uint64(val);
			} else if (g_strcmp0(key, "Address") == 0) {
				transfer_info->address = g_variant_dup_string(val, &len);
				BT_INFO("addressss %s", transfer_info->address);
			} else if (g_strcmp0(key, "Filename") == 0) {
				transfer_info->file_path = g_variant_dup_string(val, &len);
				if (!transfer_info->file_path)
					transfer_info->file_path = g_strdup(transfer_info->filename);
			}
		}
		g_variant_iter_free(iter);
		if (transfer_info->type == NULL)
			goto fail;
		if (transfer_info->address == NULL)
			goto fail;

		transfer_info->device_name = __bt_get_remote_device_name(transfer_info->address);

		if (!transfer_info->device_name)
			transfer_info->device_name = g_strdup(bdaddress);
		transfer_info->path = g_strdup(transfer_path);
		transfer_info->transfer_id = __bt_get_transfer_id(transfer_path);
	}

	g_object_unref(transfer_proxy);
	return BLUETOOTH_ERROR_NONE;

fail:
	g_object_unref(transfer_proxy);
	return BLUETOOTH_ERROR_INTERNAL;
}

static gboolean __bt_authorize_cb(GDBusMethodInvocation *context,
					const char *path,
					gpointer user_data)
{
	char *device_name = NULL;
	int result = BLUETOOTH_ERROR_NONE;
	GDBusProxy *transfer_properties_proxy;
	char * bdaddress = NULL;
	GVariant *ret;
	GVariantIter *iter;
	GVariant *param = NULL;
	GError *err = NULL;

	BT_DBG(" path [%s] \n", path);

	transfer_properties_proxy = __bt_get_transfer_properties_proxy(path);

	retv_if(transfer_properties_proxy == NULL, FALSE);

	ret = g_dbus_proxy_call_sync(transfer_properties_proxy, "GetAll",
			g_variant_new("(s)", BT_OBEX_TRANSFER_INTERFACE),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&err);
	if (err) {
		BT_ERR("DBus Error : %s", err->message);
		g_clear_error(&err);
		return FALSE;
	}
	if (ret == NULL) {
		BT_ERR("g_dbus_proxy_call_sync function return NULL");
		return FALSE;
	}
	g_variant_get(ret, "(a{sv})", &iter);
	g_variant_unref(ret);
	if (iter == NULL) {
		g_object_unref(transfer_properties_proxy);
		return FALSE;
	}

#ifdef TIZEN_MDM_ENABLE
	mdm_bt_allow_t mode = MDM_BT_ALLOWED;
#endif

	__bt_free_auth_info(agent_info.auth_info);

	agent_info.auth_info = g_malloc(sizeof(bt_auth_info_t));

	memset(agent_info.auth_info, 0, sizeof(bt_auth_info_t));

	agent_info.auth_info->reply_context = context;

	agent_info.auth_info->transfer_path = g_strdup(path);

	if (iter) {
		const gchar *key;
		GVariant *val;
		gsize len = 0;
		while (g_variant_iter_loop(iter, "{sv}", &key, &val)) {
			if (g_strcmp0(key, "Name") == 0) {
				agent_info.auth_info->filename = g_variant_dup_string(val,&len);
			} else if (g_strcmp0(key, "Address") == 0) {
				bdaddress = g_variant_dup_string(val, &len);
			} else if (g_strcmp0(key, "Size") == 0) {
				agent_info.auth_info->file_size = g_variant_get_uint64(val);
			}
		}
		g_variant_iter_free(iter);
	}

	device_name = __bt_get_remote_device_name(bdaddress);

	if (!device_name)
		device_name = g_strdup(bdaddress);

	agent_info.auth_info->device_name = device_name;

	g_object_unref(transfer_properties_proxy);

	if (agent_info.server_type == BT_CUSTOM_SERVER) {
		/* No need to send the event */
		_bt_obex_server_accept_authorize(agent_info.auth_info->filename, FALSE);
		return TRUE;
	}
	param = g_variant_new("(ist)", result,
				agent_info.auth_info->filename,
				agent_info.auth_info->file_size);
	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE,
		param);

	return TRUE;
}

void _bt_obex_transfer_started(const char *transfer_path)
{
	bt_transfer_info_t *transfer_info;
	request_info_t *req_info;
	GVariant *out_param1 = NULL;
	GVariant *param = NULL;
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
		transfer_info->file_path = agent_info.auth_info->file_path;
		transfer_info->address = g_strdup(agent_info.auth_info->address);
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
		GArray *g_out_param1 = NULL;
		g_out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
		if (out_param1 == NULL) {
			out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
					g_out_param1->data, g_out_param1->len,
					TRUE, NULL, NULL);
		}

		g_dbus_method_invocation_return_value(req_info->context,
				g_variant_new("(iv)", result, out_param1));
		g_array_free(g_out_param1, TRUE);
		_bt_delete_request_list(req_info->req_id);
	}
done:
	transfers = g_slist_append(transfers, transfer_info);

	BT_DBG("Transfer id %d\n", transfer_info->transfer_id);
	param = g_variant_new("(isstii)", result,
				transfer_info->filename,
				transfer_info->type,
				transfer_info->file_size,
				transfer_info->transfer_id,
				agent_info.server_type);
	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,
		param);
}

void _bt_obex_transfer_progress(const char *transfer_path,
					int transferred)
{
	BT_DBG("+");
	bt_transfer_info_t *transfer_info;
	int progress = 0;
	GVariant *param = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	transfer_info = __bt_find_transfer_by_path(transfer_path);
	ret_if(transfer_info == NULL);

	progress = (int)(((gdouble)transferred /
			(gdouble)transfer_info->file_size) * 100);

	param = g_variant_new("(isstiii)", result,
				transfer_info->filename,
				transfer_info->type,
				transfer_info->file_size,
				transfer_info->transfer_id,
				progress,
				agent_info.server_type);
	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS,
		param);
	BT_DBG("-");
}

void _bt_obex_transfer_completed(const char *transfer_path, gboolean success)
{
	bt_transfer_info_t *transfer_info;
	GVariant *param = NULL;
	int result;
	BT_DBG("Transfer [%s] Success [%d] \n", transfer_path, success);

	result = (success == TRUE) ? BLUETOOTH_ERROR_NONE
				: BLUETOOTH_ERROR_CANCEL;

	transfer_info = __bt_find_transfer_by_path(transfer_path);

	if (transfer_info == NULL) {
		BT_DBG("Very small files receiving case, did not get Active status from obexd");
		if (agent_info.auth_info == NULL ||
				g_strcmp0(transfer_path,
				agent_info.auth_info->transfer_path) != 0) {
			BT_ERR("auth_info is NULL, returning");
			return;
		}

		transfer_info = g_new0(bt_transfer_info_t, 1);

		transfer_info->filename = g_strdup(agent_info.auth_info->filename);
		transfer_info->file_size = agent_info.auth_info->file_size;
		transfer_info->type = g_strdup(TRANSFER_PUT);
		transfer_info->path = g_strdup(agent_info.auth_info->transfer_path);
		transfer_info->device_name = g_strdup(agent_info.auth_info->device_name);
		transfer_info->transfer_id = __bt_get_transfer_id(transfer_path);
		transfer_info->file_path = agent_info.auth_info->file_path;

		param = g_variant_new("(isstii)", result,
					transfer_info->filename,
					transfer_info->type,
					transfer_info->file_size,
					transfer_info->transfer_id,
					agent_info.server_type);
		_bt_send_event(BT_OPP_SERVER_EVENT,
			BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,
			param);
	}
	param = g_variant_new("(isssstii)", result,
				transfer_info->filename,
				transfer_info->type,
				transfer_info->device_name,
				transfer_info->file_path,
				transfer_info->file_size,
				transfer_info->transfer_id,
				agent_info.server_type);
	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,
		param);
	transfers = g_slist_remove(transfers, transfer_info);
	__bt_free_transfer_info(transfer_info);
}

void _bt_obex_transfer_connected()
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	GVariant *param = NULL;

	param = g_variant_new("(i)", result);

	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED,
		param);
	BT_DBG("-");
}

void _bt_obex_transfer_disconnected()
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	GVariant *param = NULL;

	param = g_variant_new("(i)", result);
	_bt_send_event(BT_OPP_SERVER_EVENT,
		BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_DISCONNECTED,
		param);
	BT_DBG("-");
}

int _bt_register_obex_server(void)
{
	GDBusConnection *g_conn;
	GDBusProxy *manager_proxy;
	GError *g_error = NULL;

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_obex_agent_new(BT_OBEX_SERVER_AGENT_PATH);

	_bt_obex_setup(BT_OBEX_SERVER_AGENT_PATH);

	_bt_obex_set_authorize_cb(BT_OBEX_SERVER_AGENT_PATH,
					__bt_authorize_cb, NULL);

	manager_proxy = g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_OBEX_SERVICE,
						BT_OBEX_PATH,
						BT_OBEX_MANAGER,
						NULL, &g_error);

	if (manager_proxy == NULL) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_dbus_proxy_call_sync(manager_proxy, "RegisterAgent",
				g_variant_new("(o)", BT_OBEX_SERVER_AGENT_PATH),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&g_error);

	if (g_error != NULL) {
		BT_ERR("Agent registration failed: %s\n", g_error->message);
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

	retv_if(agent_info.proxy == NULL,
				BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(agent_info.proxy, "UnregisterAgent",
				g_variant_new("(o)", BT_OBEX_SERVER_AGENT_PATH),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&g_error);
	if (g_error != NULL) {
		BT_ERR("Agent unregistration failed: %s", g_error->message);
		g_error_free(g_error);
	}
	_bt_obex_agent_destroy(BT_OBEX_SERVER_AGENT_PATH);
	g_object_unref(agent_info.proxy);
	agent_info.proxy = NULL;

	return BLUETOOTH_ERROR_NONE;
}

gboolean __bt_check_folder_path(const char *dest_path)
{
	DIR *dp;

	retv_if(dest_path == NULL, FALSE);

	dp = opendir(dest_path);

	if (dp == NULL) {
		BT_ERR("The directory does not exist");
		return FALSE;
	}

	closedir(dp);

	return TRUE;
}

int _bt_obex_server_allocate(char *sender, const char *dest_path, int app_pid, gboolean is_native)
{
	BT_DBG("+");
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
		_bt_register_osp_server_in_agent(BT_OBEX_SERVER, NULL, NULL, -1);
	}
	BT_DBG("-");
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
							NULL, NULL, -1);
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

	g_dbus_method_invocation_return_value(agent_info.auth_info->reply_context,
		g_variant_new("(s)",&file_path));
	agent_info.auth_info->reply_context = NULL;
	agent_info.auth_info->file_path = g_strdup(file_path);

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

	g_dbus_method_invocation_return_gerror(agent_info.auth_info->reply_context,
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

	DIR *dp = NULL;

	dp = opendir(dest_path);

	if (dp == NULL) {
		BT_ERR("The directory does not exist");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	closedir(dp);

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
	GVariant *folder = NULL;
	char *string = "Root";
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

	folder = g_variant_new_string(root);
	g_dbus_proxy_call_sync(agent_info.proxy, "SetProperty",
			g_variant_new("(sv)", string, folder),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&g_error);

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
	GDBusProxy *proxy;
	GError *err = NULL;
	BT_DBG("+");
	transfer = __bt_find_transfer_by_id(transfer_id);

	retv_if(transfer == NULL, BLUETOOTH_ERROR_NOT_FOUND);
	proxy = __bt_get_transfer_proxy(transfer->path);

	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_proxy_call_sync(proxy, "Cancel", NULL,
		G_DBUS_CALL_FLAGS_NONE,
		DBUS_TIMEOUT, NULL, &err);
	if (err) {
		BT_ERR("Dbus Err: %s", err->message);
		g_clear_error(&err);
	}
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
	GVariant *out_param1 = NULL;
	int result = BLUETOOTH_ERROR_TIMEOUT;

	/* Already reply in _bt_obex_transfer_started */
	retv_if(agent_info.accept_id == 0, FALSE);

	req_info = _bt_get_request_info(agent_info.accept_id);
	if (req_info == NULL || req_info->context == NULL) {
		BT_ERR("info is NULL");
		return FALSE;
	}

	agent_info.accept_id = 0;
	GArray *g_out_param1 = NULL;
	g_out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	if (out_param1 == NULL) {
		out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
				g_out_param1->data, g_out_param1->len,
				TRUE, NULL, NULL);
	}

	g_dbus_method_invocation_return_value(req_info->context,
			g_variant_new("(iv)", result, out_param1));
	g_array_free(g_out_param1, TRUE);
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

