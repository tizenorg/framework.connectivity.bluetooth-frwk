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


#ifndef _BT_SERVICE_OPP_CLIENT_H_
#define _BT_SERVICE_OPP_CLIENT_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"
#include "bt-internal-types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_OBEX_CLIENT_AGENT_PATH "/org/obex/client_agent"

typedef enum {
	BT_TRANSFER_STATUS_QUEUED = 0x00,
	BT_TRANSFER_STATUS_STARTED,
	BT_TRANSFER_STATUS_PROGRESS,
	BT_TRANSFER_STATUS_COMPLETED,
} bt_transfer_status_t;

typedef struct {
	char path[BT_FILE_PATH_MAX];
} bt_file_path_t;

typedef struct {
	DBusGProxy *proxy;
	DBusGProxy *properties_proxy;
	char *transfer_name;
	char *file_name;
	char *transfer_path;
	bt_transfer_status_t transfer_status;
	gint64 size;
} bt_transfer_info_t;

typedef struct {
	int request_id;
	int result;

	int file_count;
	int file_offset;
	char **file_name_array;
	char *session_path;

	char *address;
	gboolean is_canceled;
	DBusGProxyCall *sending_proxy;
	bt_transfer_info_t *transfer_info;
} bt_sending_info_t;

typedef struct {
	char *address;
	char **file_path;
	int file_count;
	int request_id;
} bt_sending_data_t;


int _bt_opp_client_push_files(int request_id, DBusGMethodInvocation *context,
				bluetooth_device_address_t *remote_address,
				char **file_path, int file_count);

int _bt_opp_client_cancel_push(void);

int _bt_opp_client_cancel_all_transfers(void);

int _bt_opp_client_is_sending(gboolean *sending);

void _bt_sending_files(void);

void _bt_opc_disconnected(const char *session_path);

gboolean _bt_obex_client_progress(const char *transfer_path, int transferred);

gboolean _bt_obex_client_started(const char *transfer_path);

gboolean _bt_obex_client_completed(const char *transfer_path, gboolean success);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_OPP_CLIENT_H_*/

