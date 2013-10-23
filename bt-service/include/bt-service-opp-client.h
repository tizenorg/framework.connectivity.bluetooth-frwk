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

typedef struct {
	char path[BT_FILE_PATH_MAX];
} bt_file_path_t;

typedef struct {
	DBusGProxy *proxy;
	char *transfer_name;
	char *file_name;
	gint64 size;
} bt_transfer_info_t;

typedef struct {
	int request_id;
	int result;
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
				char **file_path, int file_count,
				GArray **out_param);

int _bt_opp_client_cancel_push(void);

int _bt_opp_client_cancel_all_transfers(void);

int _bt_opp_client_is_sending(gboolean *sending);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_OPP_CLIENT_H_*/

