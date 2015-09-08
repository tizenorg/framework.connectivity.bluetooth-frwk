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


#ifndef _BT_SERVICE_OOB_H_
#define _BT_SERVICE_OOB_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

int _bt_oob_read_local_data(bt_oob_data_t *local_oob_data);

int _bt_oob_add_remote_data(
			bluetooth_device_address_t *remote_device_address,
			bt_oob_data_t *remote_oob_data);

int _bt_oob_remove_remote_data(
			bluetooth_device_address_t *remote_device_address);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_OOB_H_*/

