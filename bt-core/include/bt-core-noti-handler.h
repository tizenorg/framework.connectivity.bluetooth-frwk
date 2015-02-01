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

#ifndef _BT_CORE_NOTI_HANDLER_H_
#define _BT_CORE_NOTI_HANDLER_H_

#include "bt-internal-types.h"

#ifdef __cplusplus
extern "C" {
#endif

void _bt_set_flightmode_request(gboolean value);
gboolean _bt_is_flightmode_request(void);

void _bt_core_init_vconf_value(void);

void _bt_core_handle_flight_mode_noti(void);
void _bt_core_handle_power_saving_mode_noti(void);
void _bt_core_unregister_vconf_handler(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_CORE_NOTI_HANDLER_H_*/
