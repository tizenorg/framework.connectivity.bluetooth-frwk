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

#ifndef _BT_SERVICE_UTIL_H_
#define _BT_SERVICE_UTIL_H_

#include <sys/types.h>
#include <dbus/dbus-glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BT_NODE_NAME_LEN 50

typedef struct {
	int req_id;
	int service_function;
	char name[BT_NODE_NAME_LEN];
	DBusGMethodInvocation *context;
} request_info_t;


void _bt_init_request_id(void);

int _bt_assign_request_id(void);

void _bt_delete_request_id(int request_id);


void _bt_init_request_list(void);

int _bt_insert_request_list(int req_id, int service_function,
			char *name, DBusGMethodInvocation *context);

int _bt_delete_request_list(int req_id);

request_info_t *_bt_get_request_info(int req_id);

void _bt_clear_request_list(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_UTIL_H_*/

