/*
 *  Bluetooth-frwk
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

#ifndef __BT_SERVICE_OBEX_AGENT_H
#define __BT_SERVICE_OBEX_AGENT_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	BT_OBEX_AGENT_ERROR_REJECT,
	BT_OBEX_AGENT_ERROR_CANCEL,
	BT_OBEX_AGENT_ERROR_TIMEOUT,
} bt_agent_error_t;

G_BEGIN_DECLS
typedef struct {
	GObject parent;
} BtObexAgent;

typedef struct {
	GObjectClass parent_class;
} BtObexAgentClass;

GType bt_obex_agent_get_type(void);

#define BT_OBEX_TYPE_AGENT (bt_obex_agent_get_type())
#define BT_OBEX_AGENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BT_OBEX_TYPE_AGENT, BtObexAgent))
#define BT_OBEX_AGENT_CLASS(agent_class) (G_TYPE_CHECK_CLASS_CAST((agent_class), BT_OBEX_TYPE_AGENT, BtObexAgentClass))
#define BT_OBEX_GET_AGENT_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BT_OBEX_TYPE_AGENT, BtObexAgentClass))

typedef gboolean(*bt_obex_authorize_cb)(GDBusMethodInvocation *context,
					   const char *path,
					   gpointer data);

typedef gboolean(*bt_obex_request_cb)(GDBusMethodInvocation *context,
				GDBusProxy *transfer,
				gpointer data);

typedef gboolean(*bt_obex_progress_cb)(GDBusMethodInvocation *context,
				GDBusProxy *transfer,
				guint64 transferred,
				gpointer data);

typedef gboolean(*bt_obex_error_cb)(GDBusMethodInvocation *context,
				GDBusProxy *transfer,
				const char *message,
				gpointer data);

typedef gboolean(*bt_obex_complete_cb)(GDBusMethodInvocation *context,
				GDBusProxy *transfer,
				gpointer data);

typedef gboolean(*bt_obex_release_cb)(GDBusMethodInvocation *context,
				gpointer data);

G_END_DECLS

void _bt_obex_set_authorize_cb(char *object_path,
			 bt_obex_authorize_cb func,
			 gpointer data);

void _bt_obex_set_request_cb(char *object_path,
		       bt_obex_request_cb func,
		       gpointer data);

void _bt_obex_set_progress_cb(char *object_path,
			bt_obex_progress_cb func,
			gpointer data);

void _bt_obex_set_error_cb(char *object_path,
			bt_obex_error_cb func,
			gpointer data);

void _bt_obex_set_complete_cb(char *object_path,
			bt_obex_complete_cb func,
			gpointer data);

void _bt_obex_set_release_cb(char *object_path,
		       bt_obex_release_cb func,
		       gpointer data);

void _bt_obex_agent_new(char *path);

void _bt_obex_agent_destroy(char *path);

gboolean _bt_obex_setup(const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BT_SERVICE_OBEX_AGENT_H */
