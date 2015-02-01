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

#ifndef _BT_SERVICE_RFCOMM_SERVER_H_
#define _BT_SERVICE_RFCOMM_SERVER_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_RFCOMM_SERVER_H_*/

typedef struct {
	int server_id;
	int accept_id;
	int server_type;
	int control_fd;
	int data_fd;
	guint control_id;
	guint data_id;
	char *serial_path;
	char *uuid;
	char *sender;
	char *remote_address;
	GIOChannel *control_io;
	GIOChannel *data_io;
	DBusGProxy *serial_proxy;
	DBusGProxy *manager_proxy;
} bt_rfcomm_server_info_t;

int _bt_rfcomm_create_socket(char *sender, char *uuid);

int _bt_rfcomm_remove_socket(int socket_fd);

int _bt_rfcomm_listen(int socket_fd, int max_pending, gboolean is_native);

int _bt_rfcomm_is_uuid_available(char *uuid, gboolean *available);

int _bt_rfcomm_accept_connection(void);

int _bt_rfcomm_reject_connection(void);

int _bt_rfcomm_server_disconnect(int data_fd);

bt_rfcomm_server_info_t *_bt_rfcomm_get_server_info_using_uuid(char *uuid);

int _bt_rfcomm_server_disconnect_all_connection(void);

int _bt_rfcomm_server_check_existence(gboolean *existence);

int _bt_rfcomm_server_check_termination(char *name);

