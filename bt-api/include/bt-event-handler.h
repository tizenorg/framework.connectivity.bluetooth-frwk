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

#ifndef _BT_EVENT_HANDLER_H_
#define _BT_EVENT_HANDLER_H_

#include <sys/types.h>
#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_MATCH_RULE \
	"type='signal'," \
	"interface='%s'," \
	"path='%s'"

typedef struct {
	int event_type;
	guint id;
	GDBusConnection *conn;
	void *cb;
	void *user_data;
} bt_event_info_t;

int _bt_init_event_handler(void);

int _bt_deinit_event_handler(void);

int _bt_register_event(int event_type, void *event_cb, void *user_data);

int _bt_unregister_event(int event_type);

bt_event_info_t *_bt_event_get_cb_data(int event_type);

void _bt_add_server(int server_fd);

void _bt_remove_server(int server_fd);

void _bt_add_push_request_id(int request_id);

void _bt_set_obex_server_id(int server_type);

int _bt_get_obex_server_id(void);

void _bt_register_name_owner_changed(void);

void _bt_unregister_name_owner_changed(void);

#ifdef __cplusplus
}
#endif

#endif /* _BT_EVENT_HANDLER_H_ */
