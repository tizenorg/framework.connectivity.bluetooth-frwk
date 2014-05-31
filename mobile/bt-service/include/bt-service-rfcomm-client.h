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

#ifndef _BT_SERVICE_RFCOMM_CLIENT_H_
#define _BT_SERVICE_RFCOMM_CLIENT_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

int _bt_rfcomm_connect_using_uuid(int request_id,
			bluetooth_device_address_t *device_address,
			char *remote_uuid);

int _bt_rfcomm_connect_using_channel(int request_id,
			bluetooth_device_address_t *device_address,
			char *channel);

int _bt_rfcomm_disconnect(int socket_fd);

int _bt_rfcomm_write(int socket_fd, char *buf, int length);

int _bt_rfcomm_cancel_connect(void);

int _bt_rfcomm_is_device_connected(bluetooth_device_address_t *device_address,
					gboolean *connected);

int _bt_rfcomm_is_connected(gboolean *connected);

int _bt_rfcomm_client_disconnect_all(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_RFCOMM_CLIENT_H_*/

