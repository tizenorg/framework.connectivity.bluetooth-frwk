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


#ifndef _BT_SERVICE_OBEX_SERVER_H_
#define _BT_SERVICE_OBEX_SERVER_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

int _bt_register_obex_server(void);

int _bt_unregister_obex_server(void);

int _bt_obex_server_allocate(char *sender, const char *dest_path, int app_pid,
				gboolean is_native);

int _bt_obex_server_deallocate(int app_pid, gboolean is_native);

int _bt_obex_server_accept_authorize(const char *filename, gboolean is_native);

int _bt_obex_server_reject_authorize(void);

int _bt_obex_server_set_destination_path(const char *dest_path,
						gboolean is_native);

int _bt_obex_server_set_root(const char *root);

int _bt_obex_server_cancel_transfer(int transfer_id);

int _bt_obex_server_cancel_all_transfers(void);

int _bt_obex_server_is_activated(gboolean *activated);

int _bt_obex_server_check_allocation(gboolean *allocation);

int _bt_obex_server_check_termination(char *sender);

int _bt_obex_server_accept_connection(int request_id);

int _bt_obex_server_reject_connection(void);

int _bt_obex_server_is_receiving(gboolean *receiving);


void _bt_obex_transfer_progress(const char *transfer_path,
					int transferred);
void _bt_obex_transfer_completed(const char *transfer_path, gboolean success);

void _bt_obex_transfer_started(const char *transfer_path);

void _bt_obex_check_pending_transfer(const char *address);

void _bt_obex_transfer_connected(void);

void _bt_obex_transfer_disconnected(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_OBEX_SERVER_H_*/

