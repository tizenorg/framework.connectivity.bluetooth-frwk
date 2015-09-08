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


#ifndef _BT_SERVICE_ADAPTER_H_
#define _BT_SERVICE_ADAPTER_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_LE_SCAN_INTERVAL_MIN 2.5
#define BT_LE_SCAN_INTERVAL_MAX 10240
#define BT_LE_SCAN_WINDOW_MIN 2.5
#define BT_LE_SCAN_WINDOW_MAX 10240

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

int _bt_enable_adapter(void);

int _bt_disable_adapter(void);

int _bt_recover_adapter(void);

int _bt_enable_adapter_le(void);

int _bt_disable_adapter_le(void);

int _bt_reset_adapter(void);

int _bt_enable_core(void);

void _bt_handle_adapter_added(void);

void _bt_handle_adapter_removed(void);

int _bt_check_adapter(int *status);

void *_bt_get_adapter_agent(void);

void _bt_service_register_vconf_handler(void);

void _bt_service_unregister_vconf_handler(void);

void _bt_set_discovery_status(gboolean mode);

int _bt_get_local_address(bluetooth_device_address_t *local_address);

int _bt_get_local_version(bluetooth_version_t *local_version);

int _bt_get_local_name(bluetooth_device_name_t *local_name);

int _bt_set_local_name(char *local_name);

int _bt_is_service_used(char *service_uuid, gboolean *used);

int _bt_get_discoverable_mode(int *mode);

int _bt_set_discoverable_mode(int discoverable_mode, int timeout);

gboolean _bt_is_connectable(void);

int _bt_set_connectable(gboolean connectable);

int _bt_start_discovery(void);

int _bt_start_custom_discovery(bt_discovery_role_type_t role);

int _bt_cancel_discovery(void);

int _bt_get_bonded_devices(GArray **dev_list);

int _bt_get_bonded_device_info(bluetooth_device_address_t *device_address,
				bluetooth_device_info_t *dev_info);

int _bt_get_timeout_value(int *timeout);

gboolean _bt_is_discovering(void);

int _bt_enable_rssi(bluetooth_device_address_t *bd_addr, int link_type,
		int low_threshold, int in_range_threshold, int high_threshold);

int _bt_get_rssi_strength(bluetooth_device_address_t *bd_addr,
		int link_type);

gboolean _bt_get_advertising_params(bluetooth_advertising_params_t *params);

gboolean _bt_get_cancel_by_user(void);

void _bt_set_cancel_by_user(gboolean value);

gboolean _bt_get_discovering_property(bt_discovery_role_type_t discovery_type);

unsigned int _bt_get_discoverable_timeout_property(void);

void _bt_adapter_set_status(bt_status_t status);

bt_status_t _bt_adapter_get_status(void);

void _bt_adapter_set_le_status(bt_le_status_t status);

bt_le_status_t _bt_adapter_get_le_status(void);

void _bt_adapter_start_enable_timer(void);

void _bt_adapter_start_le_enable_timer(void);

void _bt_set_disabled(int result);

void _bt_set_le_disabled(int result);

int _bt_set_le_privacy(gboolean set_privacy);

int _bt_set_manufacturer_data(bluetooth_manufacturer_data_t *m_data);

int __bt_disable_cb(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_ADAPTER_H_*/

