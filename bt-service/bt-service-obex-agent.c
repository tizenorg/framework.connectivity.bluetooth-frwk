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

#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <gio/gio.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-obex-agent.h"
#include "marshal.h"

static GDBusConnection *conn = NULL;
static GSList *obex_agent_list = NULL;

typedef struct {
	gchar *name;
	gchar *path;

	int openobex_id;
	int obex_agent_id;

	/* callback data */
	gpointer authorize_data;
	gpointer release_data;
	gpointer request_data;
	gpointer progress_data;
	gpointer complete_data;
	gpointer error_data;

	/* callback function */
	bt_obex_authorize_cb authorize_cb;
	bt_obex_release_cb release_cb;
	bt_obex_request_cb request_cb;
	bt_obex_progress_cb progress_cb;
	bt_obex_complete_cb complete_cb;
	bt_obex_error_cb error_cb;
} bt_obex_agent_info;

static void __new_connection_method(GDBusConnection *connection,
					    const gchar *sender,
					    const gchar *object_path,
					    const gchar *interface_name,
					    const gchar *method_name,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data);
static const GDBusInterfaceVTable method_table = {
	__new_connection_method,
	NULL,
	NULL,
};

static const gchar obex_service_agent_xml1[] =
"<node name='/'>"
"  <interface name='org.openobex.Agent'>"
"    <method name='Request'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer'/>"
"     <arg type='s' name='name' direction='out'/>"
"    </method>"
"    <method name='Progress'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer'/>"
"      <arg type='t' name='transferred'/>"
"    </method>"
"    <method name='Complete'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer'/>"
 "   </method>"
"    <method name='Release'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"    </method>"
"    <method name='Error'>"
"      <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"      <arg type='o' name='transfer'/>"
"      <arg type='s' name='message'/>"
"    </method>"
"    <method name='Authorize'>"
"	<annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"		<arg type='o' name='objpath'/>"
"		<arg type='s' name='bdaddress'/>"
"		<arg type='s' name='name'/>"
"		<arg type='s' name='type'/>"
"		<arg type='i' name='length'/>"
"		<arg type='i' name='time'/>"
"		<arg type='s' name='filepath' direction='out'/>"
"	</method>"
"  </interface>"
"</node>";

static const gchar obex_service_agent_xml2[] =
"<node name='/'>"
"  <interface name='org.bluez.obex.Agent1'>"
"    <method name='AuthorizePush'>"
"    <annotation name='org.freedesktop.DBus.GLib.Async' value=''/>"
"        <arg type='o' name='objpath'/>"
"        <arg type='s' name='filepath' direction='out'/>"
"    </method>"
"  </interface>"
"</node>";

static bt_obex_agent_info *__find_obex_agent_info(char *path)
{
	GSList *l;

	for (l = obex_agent_list; l != NULL; l = l->next) {
		bt_obex_agent_info *info = l->data;

		if (g_strcmp0(info->path, path) == 0)
			return info;
	}

	return NULL;
}


static void __new_connection_method(GDBusConnection *connection,
					    const gchar *sender,
					    const gchar *object_path,
					    const gchar *interface_name,
					    const gchar *method_name,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data)
{
	BT_DBG("method_name %s", method_name);
	if (g_strcmp0(method_name, "AuthorizePush") == 0) {
		bt_obex_agent_info *info;
		char *path = NULL;
		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		if (info->authorize_cb == NULL)
			goto fail;

		g_variant_get(parameters, "(&o)", &path);

		info->authorize_cb(invocation, path,
				info->authorize_data);

		return;
	} else if (g_strcmp0(method_name, "Authorize") == 0) {
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "Request") == 0) {
		char *sender;
		bt_obex_agent_info *info;
		GDBusProxy *proxy;
		char *path = NULL;
		char *name = NULL;
		GError *err = NULL;

		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		if (conn == NULL)
			goto fail;

		sender = (char *)g_dbus_method_invocation_get_sender(invocation);

		if (info->name == NULL) {
			info->name = sender;
		} else {
			if (g_strcmp0(sender, info->name) != 0) {
				goto fail;
			}
		}

		if (info->request_cb == NULL)
			goto fail;

		g_variant_get(parameters, "(&o&s)", &path, &name);
		proxy = g_dbus_proxy_new_sync(conn, G_DBUS_CALL_FLAGS_NONE,
					NULL,
					BT_OBEX_SERVICE_NAME,
					path,
					BT_OBEX_TRANSFER_INTERFACE,
					NULL, &err);

		if (err) {
			BT_ERR("Dbus Err: %s", err->message);
			g_clear_error(&err);
			goto fail;
		}

		info->request_cb(invocation, proxy, info->request_data);
		g_object_unref(proxy);
		return;

	} else if (g_strcmp0(method_name, "Progress") == 0) {
		BT_DBG("+");

		bt_obex_agent_info *info;
		char *sender;
		char *path = NULL;
		gint64 transferred;
		GDBusProxy *proxy;
		GError *err = NULL;

		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		if (conn == NULL)
			goto fail;

		sender = (char *)g_dbus_method_invocation_get_sender(invocation);

		if (g_strcmp0(sender, info->name) != 0) {
			goto fail;
		}

		if (info->progress_cb == NULL)
			goto fail;

		g_variant_get(parameters, "(&ot)", &path, &transferred);
		proxy = g_dbus_proxy_new_sync(conn, G_DBUS_CALL_FLAGS_NONE,
					NULL,
					BT_OBEX_SERVICE_NAME,
					path,
					BT_OBEX_TRANSFER_INTERFACE,
					NULL, &err);

		if (err) {
			BT_ERR("Dbus Err: %s", err->message);
			g_clear_error(&err);
			goto fail;
		}

		info->progress_cb(invocation, proxy, transferred, info->progress_data);

		g_object_unref(proxy);

		BT_DBG("-");

		return;
	} else if (g_strcmp0(method_name, "Error") == 0) {
		bt_obex_agent_info *info;
		char *sender;
		GDBusProxy *proxy;
		char *path, *message;
		GError *err = NULL;

		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		if (conn == NULL)
			goto fail;

		sender = (char *)g_dbus_method_invocation_get_sender(invocation);

		if (g_strcmp0(sender, info->name) != 0) {
			goto fail;
		}

		if (info->error_cb == NULL)
			goto fail;
		g_variant_get(parameters, "(&o&s)", &path, &message);
		proxy = g_dbus_proxy_new_sync(conn, G_DBUS_CALL_FLAGS_NONE,
					NULL,
					BT_OBEX_SERVICE_NAME,
					path,
					BT_OBEX_TRANSFER_INTERFACE,
					NULL, &err);
		if (err) {
			BT_ERR("Dbus Err: %s", err->message);
			g_clear_error(&err);
			goto fail;
		}
		info->error_cb(invocation, proxy, message, info->progress_data);

		g_object_unref(proxy);

		return;
	} else if (g_strcmp0(method_name, "Complete") == 0) {
		bt_obex_agent_info *info;
		char *sender;
		GDBusProxy *proxy;
		char *path = NULL;
		GError *err = NULL;

		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		if (conn == NULL)
			goto fail;

		sender = (char *)g_dbus_method_invocation_get_sender(invocation);

		if (g_strcmp0(sender, info->name) != 0) {
			goto fail;
		}

		if (info->complete_cb == NULL)
			goto fail;

		g_variant_get(parameters, "(&o)", &path);
		proxy = g_dbus_proxy_new_sync(conn, G_DBUS_CALL_FLAGS_NONE,
					NULL,
					BT_OBEX_SERVICE_NAME,
					path,
					BT_OBEX_TRANSFER_INTERFACE,
					NULL, &err);
		if (err) {
			BT_ERR("Dbus Err: %s", err->message);
			g_clear_error(&err);
			goto fail;
		}

		info->complete_cb(invocation, proxy, info->complete_data);

		g_object_unref(proxy);

		return;
	} else if (g_strcmp0(method_name, "Release") == 0) {
		bt_obex_agent_info *info;
		char *sender;

		info = __find_obex_agent_info((char *)object_path);

		if (info == NULL)
			goto fail;

		sender = (char *)g_dbus_method_invocation_get_sender(invocation);

		if (info->name) {
			/*In H2 if user denies auth,release will come without request and hence
			info->name will be NULL */
			if (g_strcmp0(sender, info->name) != 0) {
				goto fail;
			}
		}

		if (info->release_cb == NULL)
			goto fail;

		info->release_cb(invocation, info->release_data);

		return;
	}
fail:
		BT_ERR("Fail case");
		g_dbus_method_invocation_return_value(invocation, NULL);
}

void _bt_obex_agent_new(char *path)
{
	bt_obex_agent_info *info = NULL;
	GError *error = NULL;

	if (conn == NULL) {
		conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
		if (error != NULL) {
			BT_ERR("Fail to get dbus: %s", error->message);
			g_error_free(error);
			return;
		}
	}
	info = (bt_obex_agent_info *)malloc (sizeof(bt_obex_agent_info));
	if (info) {
		memset(info, 0, sizeof(bt_obex_agent_info));
		info->path = g_strdup(path);
		obex_agent_list = g_slist_append(obex_agent_list, info);
	}
}

void _bt_obex_agent_destroy(char *path)
{
	bt_obex_agent_info *info = NULL;
	info = __find_obex_agent_info(path);
	if (info == NULL) {
		BT_ERR("obex agent info not found on path %s", path);
		return;
	}
	obex_agent_list = g_slist_remove(obex_agent_list, info);
	if (info->path)
		g_free(info->path);
	if (info->name)
		g_free(info->name);
	if (info->openobex_id)
		g_dbus_connection_unregister_object(conn,
			info->openobex_id);
	if (info->obex_agent_id)
		g_dbus_connection_unregister_object(conn,
			info->obex_agent_id);

	g_free(info);
}
gboolean _bt_obex_setup(const char *path)
{
	bt_obex_agent_info *info;
	GDBusProxy *proxy;
	GDBusNodeInfo *new_conn_node;
	GError *err = NULL;

	info = __find_obex_agent_info((char *)path);

	retv_if(info == NULL, FALSE);

	proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
				G_DBUS_PROXY_FLAGS_NONE,
				NULL,
				BT_OBEX_SERVICE_NAME,
				BT_OBEX_CLIENT_PATH,
				BT_OBEX_AGENT_INTERFACE,
				NULL,
				&err);

	g_free(info->name);

	if (proxy != NULL) {
		info->name = g_strdup(g_dbus_proxy_get_name(proxy));
		g_object_unref(proxy);
	} else {
		info->name = NULL;
	}

	new_conn_node = g_dbus_node_info_new_for_xml(obex_service_agent_xml1, NULL);
	if (new_conn_node == NULL)
		return FALSE;

	info->openobex_id = g_dbus_connection_register_object(conn, info->path,
						new_conn_node->interfaces[0],
						&method_table,
						NULL, NULL, &err);
	g_dbus_node_info_unref(new_conn_node);
	if (err) {
		BT_INFO("Dbus Err: %s", err->message);
		g_clear_error(&err);
		return FALSE;
	}
	if (info->openobex_id == 0)
		BT_ERR("Error while registering object");
	new_conn_node = g_dbus_node_info_new_for_xml(obex_service_agent_xml2, NULL);

	info->obex_agent_id = g_dbus_connection_register_object(conn, info->path,
						new_conn_node->interfaces[0],
						&method_table,
						NULL, NULL, &err);
	g_dbus_node_info_unref(new_conn_node);
	if (info->obex_agent_id == 0)
		BT_ERR("Error while registering object");
	if (err) {
		BT_INFO("Dbus Err: %s", err->message);
		g_clear_error(&err);
		return FALSE;
	}
	return TRUE;
}

void _bt_obex_set_authorize_cb(char *object_path,
			 bt_obex_authorize_cb func, gpointer data)
{
	bt_obex_agent_info *info = __find_obex_agent_info(object_path);;

	if (info) {
		info->authorize_cb = func;
		info->authorize_data = data;
	}
}

void _bt_obex_set_release_cb(char *object_path,
		       bt_obex_release_cb func, gpointer data)
{
	bt_obex_agent_info *info = __find_obex_agent_info(object_path);;

	/* Fix : NULL_RETURNS */
	if (info == NULL)
		return;

	info->release_cb = func;
	info->release_data = data;
}

void _bt_obex_set_request_cb(char *object_path,
		       bt_obex_request_cb func, gpointer data)
{
	bt_obex_agent_info *info = __find_obex_agent_info(object_path);;

	/* Fix : NULL_RETURNS */
	if (info == NULL)
		return;

	info->request_cb = func;
	info->request_data = data;
}

void _bt_obex_set_progress_cb(char *object_path,
			bt_obex_progress_cb func, gpointer data)
{
	bt_obex_agent_info *info = __find_obex_agent_info(object_path);;

	/* Fix : NULL_RETURNS */
	if (info == NULL)
		return;

	info->progress_cb = func;
	info->progress_data = data;
}

void _bt_obex_set_complete_cb(char *object_path,
			bt_obex_complete_cb func, gpointer data)
{
	bt_obex_agent_info *info =__find_obex_agent_info(object_path);;

	/* Fix : NULL_RETURNS */
	if (info == NULL)
		return;

	info->complete_cb = func;
	info->complete_data = data;
}

void _bt_obex_set_error_cb(char *object_path,
			bt_obex_error_cb func, gpointer data)
{
	bt_obex_agent_info *info = __find_obex_agent_info(object_path);;

	/* Fix : NULL_RETURNS */
	if (info == NULL)
		return;

	info->error_cb = func;
	info->error_data = data;
}
