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

#ifndef SC_CORE_AGENT_H
#define SC_CORE_AGENT_H

#include <stdint.h>
#include <glib.h>
#include <gio/gio.h>

#define BLUEZ_DEVICE_INTERFACE	"org.bluez.Device"

typedef enum {
	GAP_AGENT_EXEC_NO_OPERATION,
	GAP_AGENT_EXEC_PAIRING,
	GAP_AGENT_EXEC_AUTHORZATION,
	GAP_AGENT_EXEC_CONFIRM_MODE,
} GapAgentExecType;

typedef struct _GapAgentPrivate GapAgentPrivate;

typedef gboolean(*GapAgentPasskeyFunc) (GapAgentPrivate *agent,
						GDBusProxy *device);
typedef gboolean(*GapAgentDisplayFunc) (GapAgentPrivate *agent, GDBusProxy *device,
								guint passkey);
typedef gboolean(*GapAgentConfirmFunc) (GapAgentPrivate *agent, GDBusProxy *device,
								guint passkey);
typedef gboolean(*GapAgentAuthorizeFunc) (GapAgentPrivate *agent,
					GDBusProxy *device, const char *uuid);
typedef gboolean(*GapAgentConfirmModeFunc) (GapAgentPrivate *agent,
					const char *mode, const char *sender,
					gboolean need_popup, void *data);
typedef gboolean(*GapAgentCancelFunc) (GapAgentPrivate *agent,
							const char *address);
typedef gboolean(*GapAgentIgnoreAutoPairingFunc) (const char *address);
typedef uint8_t bool_t;

typedef struct {
	GapAgentPasskeyFunc pincode_func;
	GapAgentDisplayFunc display_func;
	GapAgentPasskeyFunc passkey_func;
	GapAgentConfirmFunc confirm_func;
	GapAgentAuthorizeFunc authorize_func;
	GapAgentCancelFunc pairing_cancel_func;
	GapAgentCancelFunc authorization_cancel_func;
} GAP_AGENT_FUNC_CB;

typedef enum {
	GAP_AGENT_ACCEPT,
	GAP_AGENT_REJECT,
	GAP_AGENT_CANCEL,
	GAP_AGENT_TIMEOUT,
	GAP_AGENT_ACCEPT_ALWAYS,
} GAP_AGENT_ACCEPT_TYPE_T;

struct _GapAgentPrivate {
	gchar *busname;
	gchar *path;
	GDBusProxy *adapter;

	GDBusProxy *agent_manager;

	GDBusProxy *dbus_proxy;

	GapAgentExecType exec_type;
	GDBusMethodInvocation *reply_context;

	char pairing_addr[18];
	char authorize_addr[18];

	GSList *osp_servers;

	GAP_AGENT_FUNC_CB cb;
	gboolean canceled;
};

void _gap_agent_setup_dbus(GapAgentPrivate *agent, GAP_AGENT_FUNC_CB *func_cb,
					const char *path, GDBusProxy *adapter);
gboolean _gap_agent_register(GapAgentPrivate *agent);
void _gap_agent_reset_dbus(GapAgentPrivate *agent);

gboolean gap_agent_reply_pin_code(GapAgentPrivate *agent, const guint accept,
						const char *pin_code,
						GDBusMethodInvocation *context);
gboolean gap_agent_reply_passkey(GapAgentPrivate *agent, const guint accept,
						const char *passkey,
						GDBusMethodInvocation *context);
gboolean gap_agent_reply_confirmation(GapAgentPrivate *agent, const guint accept,
		GDBusMethodInvocation *context);
gboolean gap_agent_reply_authorize(GapAgentPrivate *agent, const guint accept,
		GDBusMethodInvocation *context);

gboolean _gap_agent_exist_osp_server(GapAgentPrivate *agent, int type, char *uuid);

bt_agent_osp_server_t *_gap_agent_get_osp_server(GapAgentPrivate *agent, int type,
					char *uuid);

gchar* _gap_agent_get_path(GapAgentPrivate *agent);

gboolean _gap_agent_is_canceled(GapAgentPrivate *agent);

void _gap_agent_set_canceled(GapAgentPrivate *agent, gboolean value);

gboolean _gap_agent_register_osp_server(GapAgentPrivate *agent,
						const gint type,
						const char *uuid,
						const char *path,
						int fd);

gboolean _gap_agent_unregister_osp_server(GapAgentPrivate *agent,
						const gint type,
						const char *uuid);

#endif
