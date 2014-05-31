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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define BLUEZ_DEVICE_INTERFACE	"org.bluez.Device"

#define GAP_TYPE_AGENT (gap_agent_get_type())
#define GAP_GET_AGENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GAP_TYPE_AGENT, GapAgent))
#define GAP_IS_AGENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GAP_TYPE_AGENT))

#define GAP_AGENT_CLASS(class) (G_TYPE_CHECK_CLASS_CAST((class), GAP_TYPE_AGENT, \
										GapAgentClass))
#define GAP_GET_AGENT_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GAP_TYPE_AGENT, \
										GapAgentClass))
#define GAP_IS_AGENT_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE((class), GAP_TYPE_AGENT))

#define GAP_AGENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GAP_TYPE_AGENT, \
									GapAgentPrivate))

typedef struct _GapAgent GapAgent;
typedef struct _GapAgentClass GapAgentClass;

typedef gboolean(*GapAgentPasskeyFunc) (GapAgent *agent,
							DBusGProxy *device);
typedef gboolean(*GapAgentDisplayFunc) (GapAgent *agent, DBusGProxy *device,
								guint passkey);
typedef gboolean(*GapAgentConfirmFunc) (GapAgent *agent, DBusGProxy *device,
								guint passkey);
typedef gboolean(*GapAgentAuthorizeFunc) (GapAgent *agent,
						DBusGProxy *device,
						const char *uuid);
typedef gboolean(*GapAgentConfirmModeFunc) (GapAgent *agent,
					const char *mode, const char *sender,
					gboolean need_popup, void *data);
typedef gboolean(*GapAgentCancelFunc) (GapAgent *agent,
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

struct _GapAgent {
	GObject parent;
};

struct _GapAgentClass {
	GObjectClass parent_class;
};

GapAgent *_gap_agent_new(void);
void _gap_agent_setup_dbus(GapAgent *agent, GAP_AGENT_FUNC_CB *func_cb,
					const char *path, DBusGProxy *adapter);
gboolean _gap_agent_register(GapAgent *agent);
void _gap_agent_reset_dbus(GapAgent *agent);

gboolean gap_agent_reply_pin_code(GapAgent *agent, const guint accept,
						const char *pin_code,
				      		DBusGMethodInvocation *context);
gboolean gap_agent_reply_passkey(GapAgent *agent, const guint accept,
						const char *passkey,
				     		DBusGMethodInvocation *context);
gboolean gap_agent_reply_confirmation(GapAgent *agent, const guint accept,
					  DBusGMethodInvocation *context);
gboolean gap_agent_reply_authorize(GapAgent *agent, const guint accept,
				       DBusGMethodInvocation *context);

gboolean _gap_agent_exist_osp_server(GapAgent *agent, int type, char *uuid);

gchar* _gap_agent_get_path(GapAgent *agent);

gboolean _gap_agent_is_canceled(GapAgent *agent);

void _gap_agent_set_canceled(GapAgent *agent, gboolean value);

gboolean _gap_agent_register_osp_server(GapAgent *agent,
						const gint type,
						const char *uuid);

gboolean _gap_agent_unregister_osp_server(GapAgent *agent,
						const gint type,
						const char *uuid);

#endif
