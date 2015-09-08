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

#ifndef _BT_CORE_ADAPTER_H_
#define _BT_CORE_ADAPTER_H_

#include <sys/types.h>
#include <sys/wait.h>
#include <dlog.h>
#include <glib.h>
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT_CORE_TYPE (bt_core_get_type())

typedef enum {
	BT_DEACTIVATED,
	BT_ACTIVATED,
	BT_ACTIVATING,
	BT_DEACTIVATING,
} bt_status_t;

typedef enum {
	BT_LE_DEACTIVATED,
	BT_LE_ACTIVATED,
	BT_LE_ACTIVATING,
	BT_LE_DEACTIVATING,
} bt_le_status_t;

typedef enum {
	BT_POWER_SAVING_MODE,
	BT_FLIGHT_MODE,
	BT_RECOVERY_MODE,
	BT_MODE_MAX
} bt_mode_e;

bt_status_t _bt_core_get_status(void);
bt_le_status_t _bt_core_get_le_status(void);
int _bt_core_get_bt_status(bt_mode_e mode);			/* Get the status of BT before passed mode */
int _bt_core_get_bt_le_status(bt_mode_e mode);			/* Get the status of BT LE before passed mode */
void _bt_core_set_bt_status(bt_mode_e mode, int status);	/* Set the status of BT before passed mode */
void _bt_core_set_bt_le_status(bt_mode_e mode, int status);	/* Set the status of BT LE before passed mode */

gboolean _bt_core_is_recovery_mode(void);
gboolean _bt_core_is_flight_mode_enabled(void);

int _bt_enable_adapter(void);
int _bt_disable_adapter(void);
int _bt_enable_adapter_le(void);
int _bt_disable_adapter_le(void);
int _bt_core_service_request_adapter(int service_function);

void _bt_core_adapter_added_cb(void);
void _bt_core_adapter_removed_cb(void);
GType bt_core_get_type (void);

void _bt_core_update_status(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_CORE_ADAPTER_H_*/
