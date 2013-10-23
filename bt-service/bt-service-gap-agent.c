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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vconf.h>
#include <vconf-keys.h>

#include "bt-service-common.h"
#include "bt-service-agent.h"
#include "bt-service-gap-agent.h"
#include "bt-service-adapter.h"
#include "bt-service-device.h"

static DBusGConnection *connection = NULL;

typedef enum {
	GAP_AGENT_EXEC_NO_OPERATION,
	GAP_AGENT_EXEC_PAIRING,
	GAP_AGENT_EXEC_AUTHORZATION,
	GAP_AGENT_EXEC_CONFIRM_MODE,
} GapAgentExecType;

typedef struct _GapAgentPrivate GapAgentPrivate;

struct _GapAgentPrivate {
	gchar *busname;
	gchar *path;
	DBusGProxy *adapter;
	DBusGProxy *dbus_proxy;

	GapAgentExecType exec_type;
	DBusGMethodInvocation *reply_context;

	char pairing_addr[18];
	char authorize_addr[18];

	GSList *osp_servers;

	GAP_AGENT_FUNC_CB cb;
	gboolean canceled;
};

G_DEFINE_TYPE(GapAgent, gap_agent, G_TYPE_OBJECT);

static gboolean gap_agent_request_pin_code(GapAgent *agent,
						const char *path,
						DBusGMethodInvocation *context);

static gboolean gap_agent_request_passkey(GapAgent *agent, const char *path,
						DBusGMethodInvocation *context);

static gboolean gap_agent_display_passkey(GapAgent *agent, const char *path,
						guint passkey,
						DBusGMethodInvocation *context);

static gboolean gap_agent_request_confirmation(GapAgent *agent,
						const char *path,
						guint passkey,
						DBusGMethodInvocation *context);

static gboolean gap_agent_authorize(GapAgent *agent, const char *path,
						const char *uuid,
						DBusGMethodInvocation *context);

static gboolean gap_agent_cancel(GapAgent *agent,
						DBusGMethodInvocation *context);

static gboolean gap_agent_release(GapAgent *agent,
						DBusGMethodInvocation *context);

static gboolean gap_agent_confirm_mode_change(GapAgent *agent,
						const char *mode,
						DBusGMethodInvocation *context);

static gboolean gap_agent_get_discoverable_timeout(GapAgent *agent,
						DBusGMethodInvocation *context);

#include "bt-gap-agent-method.h"

typedef enum {
	GAP_AGENT_ERROR_REJECT,
	GAP_AGENT_ERROR_CANCEL,
	GAP_AGENT_ERROR_TIMEOUT,
} GapAgentError;

#define GAP_AGENT_ERROR (gap_agent_error_quark())

static GQuark gap_agent_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GError *gap_agent_error(GapAgentError error, const char *err_msg)
{
	return g_error_new(GAP_AGENT_ERROR, error, err_msg, NULL);
}

static void gap_agent_init(GapAgent *agent)
{
	BT_DBG("agent %p\n", agent);
}

static void gap_agent_finalize(GObject *agent)
{
	BT_DBG("Free agent %p\n", agent);

	G_OBJECT_CLASS(gap_agent_parent_class)->finalize(agent);
}

static void gap_agent_class_init(GapAgentClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;
	GError *error = NULL;

	BT_DBG("class %p\n", klass);

	g_type_class_add_private(klass, sizeof(GapAgentPrivate));

	object_class->finalize = gap_agent_finalize;

	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

	if (error != NULL) {
		g_printerr("Connecting to system bus failed: %s\n",
								error->message);
		g_error_free(error);
	}

	dbus_g_object_type_install_info(GAP_TYPE_AGENT,
					&dbus_glib_gap_agent_object_info);
}

GapAgent *_gap_agent_new(void)
{
	GapAgent *agent;

	agent = GAP_GET_AGENT(g_object_new(GAP_TYPE_AGENT, NULL));

	BT_DBG("agent %p\n", agent);

	return agent;
}

static gboolean gap_agent_request_pin_code(GapAgent *agent,
						const char *path,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	DBusGProxy *device;
	gboolean result;
	char *addr;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Request : agent %p sender %s priv->busname %s Device Path :%s\n",
				agent, sender, priv->busname, path);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (!priv->cb.passkey_func) {
		g_free(sender);
		return FALSE;
	}

	device = dbus_g_proxy_new_from_proxy(priv->adapter,
						BLUEZ_DEVICE_INTERFACE, path);
	if (device == NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No proxy for device");
		BT_DBG("Fail to make device proxy\n");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	}

	priv->exec_type = GAP_AGENT_EXEC_PAIRING;
	priv->reply_context = context;

	addr = strstr(path, "dev_");
	if (addr != NULL) {
		char *pos = NULL;
		addr += 4;
		g_strlcpy(priv->pairing_addr, addr, sizeof(priv->pairing_addr));

		while ((pos = strchr(priv->pairing_addr, '_')) != NULL) {
			*pos = ':';
		}
	}

	result = priv->cb.pincode_func(agent, device);

	g_object_unref(device);

	g_free(sender);
	return result;
}

static gboolean gap_agent_request_passkey(GapAgent *agent, const char *path,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	DBusGProxy *device;
	gboolean result;
	char *addr;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Request passkey : agent %p sender %s priv->busname %s Device Path :%s\n", agent,
	    sender, priv->busname, path);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (!priv->cb.passkey_func) {
		g_free(sender);
		return FALSE;
	}

	device = dbus_g_proxy_new_from_proxy(priv->adapter,
						BLUEZ_DEVICE_INTERFACE,
						path);
	if (device == NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No proxy for device");
		BT_DBG("Fail to make device proxy\n");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	}

	priv->exec_type = GAP_AGENT_EXEC_PAIRING;
	priv->reply_context = context;

	addr = strstr(path, "dev_");
	if (addr != NULL) {
		char *pos = NULL;
		addr += 4;
		g_strlcpy(priv->pairing_addr, addr, sizeof(priv->pairing_addr));

		while ((pos = strchr(priv->pairing_addr, '_')) != NULL) {
			*pos = ':';
		}
	}

	result = priv->cb.passkey_func(agent, device);

	g_object_unref(device);

	g_free(sender);
	return result;

}

static gboolean gap_agent_display_passkey(GapAgent *agent, const char *path,
						guint passkey,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	DBusGProxy *device;
	gboolean result;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Request passkey display : agent %p sender %s priv->busname %s Device Path :%s\n",
	    agent, sender, priv->busname, path);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (!priv->cb.display_func) {
		g_free(sender);
		return FALSE;
	}

	device = dbus_g_proxy_new_from_proxy(priv->adapter,
						BLUEZ_DEVICE_INTERFACE,
						path);
	if (device == NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No proxy for device");
		BT_DBG("Fail to make device proxy\n");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	}

	dbus_g_method_return(context);

	result = priv->cb.display_func(agent, device, passkey);

	g_object_unref(device);

	g_free(sender);
	return result;
}

static gboolean gap_agent_request_confirmation(GapAgent *agent,
						const char *path,
						guint passkey,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	DBusGProxy *device;
	gboolean result;
	char *addr;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Request passkey confirmation : agent %p sender %s priv->busname %s Device Path :%s\n",
	    agent, sender, priv->busname, path);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (!priv->cb.confirm_func) {
		g_free(sender);
		return FALSE;
	}

	device = dbus_g_proxy_new_from_proxy(priv->adapter,
						BLUEZ_DEVICE_INTERFACE,
						path);
	if (device == NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No proxy for device");
		BT_DBG("Fail to make device proxy\n");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	}

	priv->exec_type = GAP_AGENT_EXEC_PAIRING;
	priv->reply_context = context;

	addr = strstr(path, "dev_");
	if (addr != NULL) {
		char *pos = NULL;
		addr += 4;
		g_strlcpy(priv->pairing_addr, addr, sizeof(priv->pairing_addr));

		while ((pos = strchr(priv->pairing_addr, '_')) != NULL) {
			*pos = ':';
		}
	}

	result = priv->cb.confirm_func(agent, device, passkey);

	g_object_unref(device);

	g_free(sender);
	return result;
}

static gboolean gap_agent_authorize(GapAgent *agent, const char *path,
						const char *uuid,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	DBusGProxy *device;
	gboolean result;
	char *addr;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Request authorization : agent %p sender %s priv->busname %s Device Path :%s\n", agent,
	    sender, priv->busname, path);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (!priv->cb.authorize_func) {
		g_free(sender);
		return FALSE;
	}

	device = dbus_g_proxy_new_from_proxy(priv->adapter,
						BLUEZ_DEVICE_INTERFACE,
						path);
	if (device == NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No proxy for device");
		BT_DBG("Fail to make device proxy\n");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	}

	priv->exec_type = GAP_AGENT_EXEC_AUTHORZATION;
	priv->reply_context = context;

	addr = strstr(path, "dev_");
	if (addr != NULL) {
		char *pos = NULL;
		addr += 4;
		g_strlcpy(priv->authorize_addr, addr,
						sizeof(priv->authorize_addr));

		while ((pos = strchr(priv->authorize_addr, '_')) != NULL) {
			*pos = ':';
		}
	}

	result = priv->cb.authorize_func(agent, device, uuid);

	g_object_unref(device);

	g_free(sender);
	return result;
}

static gboolean gap_agent_confirm_mode_change(GapAgent *agent,
						const char *mode,
						DBusGMethodInvocation *context)
{
	BT_DBG("");

	dbus_g_method_return(context);
	return TRUE;
}

static gboolean gap_agent_cancel(GapAgent *agent,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);
	gboolean result = FALSE;

	if (sender == NULL)
		return FALSE;

	BT_DBG("Cancelled : agent %p sender %s\n", agent, sender);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	if (priv->cb.authorization_cancel_func &&
			priv->exec_type == GAP_AGENT_EXEC_AUTHORZATION) {
		result = priv->cb.authorization_cancel_func(agent,
							priv->authorize_addr);
		memset(priv->authorize_addr, 0x00,
						sizeof(priv->authorize_addr));
	} else if (priv->cb.pairing_cancel_func &&
				priv->exec_type == GAP_AGENT_EXEC_PAIRING) {
		result = priv->cb.pairing_cancel_func(agent,
							priv->pairing_addr);
		memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));
	}

	if (priv->exec_type != GAP_AGENT_EXEC_CONFIRM_MODE &&
	    		priv->exec_type != GAP_AGENT_EXEC_NO_OPERATION &&
						priv->reply_context != NULL) {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
						"Rejected by remote cancel");
		dbus_g_method_return_error(priv->reply_context, error);
		g_error_free(error);
	}

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;

	g_free(sender);
	return result;
}

static gboolean gap_agent_release(GapAgent *agent,
						DBusGMethodInvocation *context)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	char *sender = dbus_g_method_get_sender(context);

	if (sender == NULL)
		return FALSE;

	BT_DBG("Released : agent %p sender %s\n", agent, sender);

	if (g_strcmp0(sender, priv->busname) != 0) {
		g_free(sender);
		return FALSE;
	}

	dbus_g_method_return(context);

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;

	memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));
	memset(priv->authorize_addr, 0x00, sizeof(priv->authorize_addr));

	g_free(sender);
	return TRUE;
}

static bt_agent_osp_server_t *__gap_agent_find_server(GSList *servers,
							int type,
							const char *uuid)
{
	GSList *l;
	bt_agent_osp_server_t *transfer;

	for (l = servers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		/* No uuid in obex server */
		if (type == BT_OBEX_SERVER &&
			transfer->type == BT_OBEX_SERVER)
			return transfer;

		if (g_strcmp0(transfer->uuid, uuid) == 0)
			return transfer;
	}

	return NULL;
}

static void __gap_agent_remove_osp_servers(GSList *osp_servers)
{
	GSList *l;
	bt_agent_osp_server_t *server;

	for (l = osp_servers; l != NULL; l = g_slist_next(l)) {
		server = l->data;

		if (server == NULL)
			continue;

		g_free(server->uuid);
		g_free(server);
	}
}

gboolean _gap_agent_register_osp_server(GapAgent *agent,
						const gint type,
						const char *uuid)
{
	bt_agent_osp_server_t *server;

	BT_DBG("+");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv == NULL)
		return FALSE;

	/* type:  BT_OBEX_SERVER / BT_RFCOMM_SERVER*/
	if (type > BT_RFCOMM_SERVER)
		return FALSE;

	server = g_malloc0(sizeof(bt_agent_osp_server_t));

	server->type = type;

	if (type == BT_RFCOMM_SERVER)
		server->uuid = g_strdup(uuid);

	priv->osp_servers = g_slist_append(priv->osp_servers, server);

	BT_DBG("-");

	return TRUE;
}

gboolean _gap_agent_unregister_osp_server(GapAgent *agent,
						const gint type,
						const char *uuid)
{
	bt_agent_osp_server_t *server;

	BT_DBG("+");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv == NULL)
		return FALSE;

	/* type:  BT_OBEX_SERVER / BT_RFCOMM_SERVER*/
	if (type > BT_RFCOMM_SERVER)
		return FALSE;

	server = __gap_agent_find_server(priv->osp_servers, type, uuid);

	if (server == NULL)
		return FALSE;

	priv->osp_servers = g_slist_remove(priv->osp_servers, server);

	g_free(server->uuid);
	g_free(server);

	BT_DBG("-");

	return TRUE;
}

gboolean gap_agent_reply_pin_code(GapAgent *agent, const guint accept,
						const char *pin_code,
				      		DBusGMethodInvocation *context)
{
	BT_DBG("+\n");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv->exec_type != GAP_AGENT_EXEC_NO_OPERATION &&
						priv->reply_context != NULL) {
		if (accept == GAP_AGENT_ACCEPT) {
			dbus_g_method_return(priv->reply_context, pin_code);
			priv->canceled = FALSE;
		} else {
			GError *error = NULL;
			switch (accept) {
			case GAP_AGENT_CANCEL:
				error = gap_agent_error(GAP_AGENT_ERROR_CANCEL,
								"CanceledbyUser");
				priv->canceled = TRUE;
				break;
			case GAP_AGENT_TIMEOUT:
			case GAP_AGENT_REJECT:
			default:
				error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
								"Pairing request rejected");
				priv->canceled = FALSE;
				break;
			}
			dbus_g_method_return_error(priv->reply_context, error);
			g_error_free(error);
		}
	}

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;
	memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));

	BT_DBG("-\n");

	return TRUE;
}

gboolean gap_agent_reply_passkey(GapAgent *agent, const guint accept,
						const char *passkey,
				     		DBusGMethodInvocation *context)
{
	BT_DBG("+\n");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv->exec_type != GAP_AGENT_EXEC_NO_OPERATION &&
						priv->reply_context != NULL) {
		if (accept == GAP_AGENT_ACCEPT) {
			guint pass_key = atoi(passkey);
			dbus_g_method_return(priv->reply_context, pass_key);
			priv->canceled = FALSE;
		} else {
			GError *error = NULL;
			switch (accept) {
			case GAP_AGENT_CANCEL:
				error = gap_agent_error(GAP_AGENT_ERROR_CANCEL,
								"CanceledbyUser");
				priv->canceled = TRUE;
				break;
			case GAP_AGENT_TIMEOUT:
			case GAP_AGENT_REJECT:
			default:
				error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
								"Passkey request rejected");
				priv->canceled = FALSE;
				break;
			}
			dbus_g_method_return_error(priv->reply_context, error);
			g_error_free(error);
		}
	}

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;
	memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));

	BT_DBG("-\n");

	return TRUE;
}

gboolean gap_agent_reply_confirmation(GapAgent *agent, const guint accept,
					  DBusGMethodInvocation *context)
{
	BT_DBG("+\n");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv->exec_type != GAP_AGENT_EXEC_NO_OPERATION &&
						priv->reply_context != NULL) {
		if (accept == GAP_AGENT_ACCEPT) {
			dbus_g_method_return(priv->reply_context);
			priv->canceled = FALSE;
		} else {
			GError *error = NULL;
			switch (accept) {
			case GAP_AGENT_CANCEL:
				error = gap_agent_error(GAP_AGENT_ERROR_CANCEL,
								"CanceledbyUser");
				priv->canceled = TRUE;
				break;
			case GAP_AGENT_TIMEOUT:
			case GAP_AGENT_REJECT:
			default:
				error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
								"Confirmation request rejected");
				priv->canceled = FALSE;
				break;
			}
			dbus_g_method_return_error(priv->reply_context, error);
			g_error_free(error);
		}
	}

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;
	memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));

	BT_DBG("-\n");

	return TRUE;
}

gboolean gap_agent_reply_authorize(GapAgent *agent, const guint accept,
				       DBusGMethodInvocation *context)
{
	gboolean ret = TRUE;

	BT_DBG("+\n");

	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv->exec_type != GAP_AGENT_EXEC_NO_OPERATION &&
						priv->reply_context != NULL) {
		if (accept == GAP_AGENT_ACCEPT) {
			dbus_g_method_return(priv->reply_context);
		} else if (accept == GAP_AGENT_ACCEPT_ALWAYS) {
			bluetooth_device_address_t addr = {{0,}};
			int result;

			_bt_convert_addr_string_to_type(addr.addr,
							priv->authorize_addr);

			result = _bt_set_authorization(&addr, TRUE);
			if (result == BLUETOOTH_ERROR_NONE) {
				BT_DBG("Device added to trusted");
			}

			dbus_g_method_return(priv->reply_context);
		} else {
			GError *error = NULL;
			switch (accept) {
			case GAP_AGENT_CANCEL:
				error = gap_agent_error(GAP_AGENT_ERROR_CANCEL,
								"CanceledbyUser");
				break;
			case GAP_AGENT_TIMEOUT:
			case GAP_AGENT_REJECT:
			default:
				error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
								"Authorization request rejected");
				break;
			}
			dbus_g_method_return_error(priv->reply_context, error);
			g_error_free(error);
		}

		if (context)
			dbus_g_method_return(context);
	} else {
		GError *error = gap_agent_error(GAP_AGENT_ERROR_REJECT,
							"No context");
		BT_DBG("No context");

		if (context)
			dbus_g_method_return_error(context, error);

		g_error_free(error);
		ret = FALSE;
	}

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	priv->reply_context = NULL;
	memset(priv->authorize_addr, 0x00, sizeof(priv->authorize_addr));

	BT_DBG("-\n");

	return ret;
}

static gboolean gap_agent_get_discoverable_timeout(GapAgent *agent,
						DBusGMethodInvocation *context)
{
	BT_DBG("+\n");

	int timeout;

	_bt_get_timeout_value(&timeout);

	dbus_g_method_return(context, timeout);

	BT_DBG("-\n");

	return TRUE;
}

gboolean _gap_agent_register(GapAgent *agent)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	GError *error = NULL;

	dbus_g_proxy_call(priv->adapter, "RegisterAgent", &error,
				DBUS_TYPE_G_OBJECT_PATH, priv->path,
				G_TYPE_STRING, "DisplayYesNo", G_TYPE_INVALID,
				G_TYPE_INVALID);
	if (error != NULL) {
		BT_DBG("Agent registration failed: %s\n", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

void _gap_agent_setup_dbus(GapAgent *agent, GAP_AGENT_FUNC_CB *func_cb,
							const char *path,
							DBusGProxy *adapter)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);
	GObject *object;
	DBusGProxy *proxy;

	priv->path = g_strdup(path);

	object = dbus_g_connection_lookup_g_object(connection, priv->path);
	if (object != NULL)
		g_object_unref(object);

	dbus_g_connection_register_g_object(connection, priv->path,
							G_OBJECT(agent));

	memcpy(&priv->cb, func_cb, sizeof(GAP_AGENT_FUNC_CB));

	priv->exec_type = GAP_AGENT_EXEC_NO_OPERATION;
	memset(priv->pairing_addr, 0x00, sizeof(priv->pairing_addr));
	memset(priv->authorize_addr, 0x00, sizeof(priv->authorize_addr));
	priv->reply_context = NULL;

	priv->adapter = g_object_ref(adapter);

	proxy = dbus_g_proxy_new_for_name_owner(connection,
				dbus_g_proxy_get_bus_name(priv->adapter),
				dbus_g_proxy_get_path(priv->adapter),
				dbus_g_proxy_get_interface(priv->adapter),
				NULL);
	if (proxy != NULL) {
		priv->busname = g_strdup(dbus_g_proxy_get_bus_name(proxy));
		g_object_unref(proxy);
	} else
		priv->busname = g_strdup(dbus_g_proxy_get_bus_name(adapter));
}

void _gap_agent_reset_dbus(GapAgent *agent)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	dbus_g_connection_unregister_g_object(connection, G_OBJECT(agent));

	if (priv->osp_servers) {
		__gap_agent_remove_osp_servers(priv->osp_servers);
		g_slist_free(priv->osp_servers);
		priv->osp_servers = NULL;
	}

	g_object_ref(priv->adapter);
	priv->adapter = NULL;

	g_free(priv->path);
	priv->path = NULL;

	g_free(priv->busname);
	priv->busname = NULL;
}

gboolean _gap_agent_exist_osp_server(GapAgent *agent, int type, char *uuid)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	if (priv == NULL)
		return FALSE;

	if (__gap_agent_find_server(priv->osp_servers,
				type, uuid) != NULL) {
		return TRUE;
	}

	return FALSE;
}

gchar* _gap_agent_get_path(GapAgent *agent)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	return priv->path;
}

gboolean _gap_agent_is_canceled(GapAgent *agent)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	return priv->canceled;
}

void _gap_agent_set_canceled(GapAgent *agent, gboolean value)
{
	GapAgentPrivate *priv = GAP_AGENT_GET_PRIVATE(agent);

	priv->canceled = value;
}
