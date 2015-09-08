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

#ifndef __BT_SERVICE_HEADSET_CONNECTION__
#define __BT_SERVICE_HEADSET_CONNECTION__

#undef LOG_TAG
#define LOG_TAG	"BLUETOOTH_FRWK_SERVICE"
#define ERR(fmt, args...) SLOGE(fmt, ##args)

#define CONNECT_TIMEOUT (3 * 1000)

void _bt_headset_set_local_connection(gboolean value);
gboolean _bt_headset_get_local_connection();
void _bt_start_timer_for_connection(char *remote_address, int connection_type);
void __bt_connection_manager_set_state(char *remote_address, int event);


#endif
