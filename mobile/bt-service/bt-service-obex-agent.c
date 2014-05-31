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

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-obex-agent.h"
#include "marshal.h"
#include "bt-obex-agent-method.h"

static DBusGConnection *obex_conn = NULL;

typedef struct {
	gchar *name;
	gchar *path;

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

#define BT_OBEX_AGENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
					BT_OBEX_TYPE_AGENT, bt_obex_agent_info))

G_DEFINE_TYPE(BtObexAgent, bt_obex_agent, G_TYPE_OBJECT)

gboolean bt_obex_agent_authorize(BtObexAgent *agent, const char *path,
			const char *bdaddress, const char *name,
			const char *type, gint length, gint time,
			     DBusGMethodInvocation *context)
{
	bt_obex_agent_info *info;
	gboolean result;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	if (info->authorize_cb == NULL)
		goto fail;

	result = info->authorize_cb(context, path, bdaddress,
				name, type, length,
				time, info->authorize_data);

	return result;
fail:
	dbus_g_method_return(context, "");
	return FALSE;
}

gboolean bt_obex_agent_request(BtObexAgent *agent, const char *path,
				   DBusGMethodInvocation *context)
{
	char *sender;
	bt_obex_agent_info *info;
	DBusGProxy *proxy;
	gboolean result;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	if (obex_conn == NULL)
		goto fail;

	sender = dbus_g_method_get_sender(context);

	BT_DBG("sender %s", sender);

	if (info->name == NULL) {
		info->name = sender;
	} else {
		if (g_strcmp0(sender, info->name) != 0) {
			g_free(sender);
			goto fail;
		}
		g_free(sender);
	}

	if (info->request_cb == NULL)
		goto fail;

	proxy = dbus_g_proxy_new_for_name(obex_conn, BT_OBEX_SERVICE_NAME,
					  path, BT_OBEX_TRANSFER_INTERFACE);

	result = info->request_cb(context, proxy, info->request_data);
	g_object_unref(proxy);

	return result;
fail:
	BT_ERR("Fail case");
	dbus_g_method_return(context, "");
	return FALSE;
}

gboolean bt_obex_agent_progress(BtObexAgent *agent, const char *path,
		    guint64 transferred, DBusGMethodInvocation *context)
{
	bt_obex_agent_info *info;
	char *sender;
	gboolean result;
	DBusGProxy *proxy;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	if (obex_conn == NULL)
		goto fail;

	sender = dbus_g_method_get_sender(context);

	if (g_strcmp0(sender, info->name) != 0) {
		g_free(sender);
		goto fail;
	}

	g_free(sender);

	if (info->progress_cb == NULL)
		goto fail;

	proxy = dbus_g_proxy_new_for_name(obex_conn, BT_OBEX_SERVICE_NAME,
					  path, BT_OBEX_TRANSFER_INTERFACE);

	result = info->progress_cb(context, proxy, transferred, info->progress_data);

	g_object_unref(proxy);

	return result;
fail:
	BT_ERR("Fail case");
	dbus_g_method_return(context, "");
	return FALSE;
}

gboolean bt_obex_agent_error(BtObexAgent *agent, const char *path,
			 const char *message, DBusGMethodInvocation *context)
{
	bt_obex_agent_info *info;
	char *sender;
	DBusGProxy *proxy;
	gboolean result;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	if (obex_conn == NULL)
		goto fail;

	sender = dbus_g_method_get_sender(context);

	if (g_strcmp0(sender, info->name) != 0) {
		g_free(sender);
		goto fail;
	}

	g_free(sender);

	if (info->error_cb == NULL)
		goto fail;

	proxy = dbus_g_proxy_new_for_name(obex_conn, BT_OBEX_SERVICE_NAME,
					  path, BT_OBEX_TRANSFER_INTERFACE);

	result = info->error_cb(context, proxy, message, info->progress_data);

	g_object_unref(proxy);

	return result;
fail:
	BT_ERR("Fail case");
	dbus_g_method_return(context, "");
	return FALSE;
}

gboolean bt_obex_agent_complete(BtObexAgent *agent, const char *path,
				    DBusGMethodInvocation *context)
{
	bt_obex_agent_info *info;
	char *sender;
	DBusGProxy *proxy;
	gboolean result;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	if (obex_conn == NULL)
		goto fail;

	sender = dbus_g_method_get_sender(context);

	if (g_strcmp0(sender, info->name) != 0) {
		g_free(sender);
		goto fail;
	}

	g_free(sender);

	if (info->complete_cb == NULL)
		goto fail;

	proxy = dbus_g_proxy_new_for_name(obex_conn, BT_OBEX_SERVICE_NAME,
					  path, BT_OBEX_TRANSFER_INTERFACE);

	result = info->complete_cb(context, proxy, info->complete_data);

	g_object_unref(proxy);

	return result;
fail:
	BT_ERR("Fail case");
	dbus_g_method_return(context, "");
	return FALSE;
}

gboolean bt_obex_agent_release(BtObexAgent *agent, DBusGMethodInvocation *context)
{
	bt_obex_agent_info *info;
	char *sender;
	gboolean result;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info == NULL)
		goto fail;

	sender = dbus_g_method_get_sender(context);

	if (info->name) {
		/*In H2 if user denies auth,release will come without request and hence
		info->name will be NULL */
		if (g_strcmp0(sender, info->name) != 0) {
			g_free(sender);
			goto fail;
		}
	}
	g_free(sender);

	if (info->release_cb == NULL)
		goto fail;

	result = info->release_cb(context, info->release_data);

	return result;
fail:
	BT_ERR("Fail case");
	dbus_g_method_return(context, "");
	return FALSE;
}

static void bt_obex_agent_init(BtObexAgent *agent)
{
	BT_DBG("agent %p", agent);
}

static void bt_obex_agent_finalize(GObject *agent)
{
	bt_obex_agent_info *info;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	if (info) {
		g_free(info->path);
		g_free(info->name);
	}

	G_OBJECT_CLASS(bt_obex_agent_parent_class)->finalize(agent);
}

static void bt_obex_agent_class_init(BtObexAgentClass *agent_class)
{
	GObjectClass *object_class;
	GError *error = NULL;

	object_class = (GObjectClass *)agent_class;

	g_type_class_add_private(agent_class, sizeof(bt_obex_agent_info));

	object_class->finalize = bt_obex_agent_finalize;

	obex_conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

	if (error != NULL) {
		BT_ERR("Fail to get dbus: %s", error->message);
		g_error_free(error);
	}

	dbus_g_object_type_install_info(BT_OBEX_TYPE_AGENT,
				&dbus_glib_bt_obex_agent_object_info);
}

BtObexAgent *_bt_obex_agent_new(void)
{
	BtObexAgent *agent;

	agent = BT_OBEX_AGENT(g_object_new(BT_OBEX_TYPE_AGENT, NULL));

	return agent;
}

gboolean _bt_obex_setup(BtObexAgent *agent, const char *path)
{
	bt_obex_agent_info *info;
	DBusGProxy *proxy;
	GObject *object;

	info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	retv_if(obex_conn == NULL, FALSE);
	retv_if(info == NULL, FALSE);
	retv_if(info->path != NULL, FALSE);

	info->path = g_strdup(path);

	proxy = dbus_g_proxy_new_for_name_owner(obex_conn, BT_OBEX_SERVICE_NAME,
						BT_OBEX_CLIENT_PATH,
						BT_OBEX_AGENT_INTERFACE, NULL);
	g_free(info->name);

	if (proxy != NULL) {
		info->name = g_strdup(dbus_g_proxy_get_bus_name(proxy));
		g_object_unref(proxy);
	} else {
		info->name = NULL;
	}

	object = dbus_g_connection_lookup_g_object(obex_conn, info->path);
	if (object != NULL)
		g_object_unref(object);

	dbus_g_connection_register_g_object(obex_conn, info->path, G_OBJECT(agent));

	dbus_g_object_register_marshaller(marshal_VOID__OBJECT_BOOLEAN,
					  G_TYPE_NONE, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_BOOLEAN,
					  G_TYPE_INVALID);

	dbus_g_object_register_marshaller(marshal_VOID__INT_INT,
					  G_TYPE_NONE, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);
	return TRUE;
}

void _bt_obex_set_authorize_cb(BtObexAgent *agent,
			 bt_obex_authorize_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->authorize_cb = func;
	info->authorize_data = data;
}

void _bt_obex_set_release_cb(BtObexAgent *agent,
		       bt_obex_release_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->release_cb = func;
	info->release_data = data;
}

void _bt_obex_set_request_cb(BtObexAgent *agent,
		       bt_obex_request_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->request_cb = func;
	info->request_data = data;
}

void _bt_obex_set_progress_cb(BtObexAgent *agent,
			bt_obex_progress_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->progress_cb = func;
	info->progress_data = data;
}

void _bt_obex_set_complete_cb(BtObexAgent *agent,
			bt_obex_complete_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->complete_cb = func;
	info->complete_data = data;
}

void _bt_obex_set_error_cb(BtObexAgent *agent,
			bt_obex_error_cb func, gpointer data)
{
	bt_obex_agent_info *info = BT_OBEX_AGENT_GET_PRIVATE(agent);

	info->error_cb = func;
	info->error_data = data;
}
