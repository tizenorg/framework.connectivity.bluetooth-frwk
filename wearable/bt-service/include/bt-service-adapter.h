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

typedef enum {
	BT_DEACTIVATED,
	BT_ACTIVATED,
	BT_ACTIVATING,
	BT_DEACTIVATING,
} bt_status_t;

int _bt_enable_adapter(void);

int _bt_disable_adapter(void);

int _bt_reset_adapter(void);

void _bt_handle_adapter_added(void);

void _bt_handle_adapter_removed(void);

int _bt_check_adapter(int *status);

void *_bt_get_adapter_agent(void);

void _bt_set_discovery_status(gboolean mode);

void _bt_set_advertising_status(gboolean mode);

int _bt_get_local_address(bluetooth_device_address_t *local_address);

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

gboolean _bt_is_advertising(void);

gboolean _bt_get_advertising_params(bluetooth_advertising_params_t *params);

gboolean _bt_get_cancel_by_user(void);

void _bt_set_cancel_by_user(gboolean value);

gboolean _bt_get_discoverying_property(void);

int _bt_get_discoverable_timeout_property(void);

bt_status_t _bt_adapter_get_status(void);

void _bt_handle_flight_mode_noti(void);

//void _bt_handle_power_saving_mode_noti(void);

void _bt_set_disabled(int result);

int _bt_set_advertising(gboolean enable);

int _bt_set_custom_advertising(gboolean enable, float interval_min,
				float interval_max, guint8 filter_policy);

int _bt_get_advertising_data(bluetooth_advertising_data_t *adv, int *length);

int _bt_set_advertising_data(bluetooth_advertising_data_t *data, int length);

int _bt_set_scan_response_data(bluetooth_scan_resp_data_t *response, int length);

int _bt_add_white_list(bluetooth_device_address_t *device_address);

int _bt_remove_white_list(bluetooth_device_address_t *device_address);

int _bt_clear_white_list(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_ADAPTER_H_*/

