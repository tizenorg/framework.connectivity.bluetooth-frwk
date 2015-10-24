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


#ifndef _BT_SERVICE_ADAPTER_LE_H_
#define _BT_SERVICE_ADAPTER_LE_H_

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
	BT_LE_AD_TYPE_INCOMP_LIST_16_BIT_SERVICE_CLASS_UUIDS = 0x02,
	BT_LE_AD_TYPE_COMP_LIST_16_BIT_SERVICE_CLASS_UUIDS = 0x03,
	BT_LE_AD_TYPE_INCOMP_LIST_128_BIT_SERVICE_CLASS_UUIDS = 0x06,
	BT_LE_AD_TYPE_COMP_LIST_128_BIT_SERVICE_CLASS_UUIDS = 0x07,
	BT_LE_AD_TYPE_SHORTENED_LOCAL_NAME = 0x08,
	BT_LE_AD_TYPE_COMPLETE_LOCAL_NAME = 0x09,
	BT_LE_AD_TYPE_LIST_16_BIT_SERVICE_SOLICITATION_UUIDS = 0x14,
	BT_LE_AD_TYPE_LIST_128_BIT_SERVICE_SOLICITATION_UUIDS = 0x15,
	BT_LE_AD_TYPE_SERVICE_DATA = 0x16,
	BT_LE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA = 0xFF,
} bt_le_advertising_data_type_e;

typedef enum {
	BT_LE_PASSIVE_SCAN = 0x00,
	BT_LE_ACTIVE_SCAN
} bt_le_scan_type_t;

typedef struct {
	char *addr;
	int data_len;
	char *data;
} bt_le_adv_info_t;

int _bt_service_adapter_le_init(void);

void _bt_service_adapter_le_deinit(void);

gboolean _bt_update_le_feature_support(const char *item, const char *value);

const char* _bt_get_adv_slot_owner(int slot_id);

int _bt_get_adv_slot_adv_handle(int slot_id);

void _bt_set_advertising_status(int slot_id, gboolean mode);

gboolean _bt_is_advertising(void);

void _bt_stop_advertising_by_terminated_process(const char* terminated_name);

int _bt_set_advertising(const char *sender, int adv_handle, gboolean enable, gboolean use_reserved_slot);

int _bt_set_custom_advertising(const char *sender, int adv_handle, gboolean enable, bluetooth_advertising_params_t *params, gboolean use_reserved_slot);

int _bt_get_advertising_data(bluetooth_advertising_data_t *adv, int *length);

int _bt_set_advertising_data(const char *sender, int adv_handle, bluetooth_advertising_data_t *data, int length, gboolean use_reserved_slot);

int _bt_get_scan_response_data(bluetooth_scan_resp_data_t *response, int *length);

int _bt_set_scan_response_data(const char *sender, int adv_handle, bluetooth_scan_resp_data_t *response, int length, gboolean use_reserved_slot);

int _bt_set_scan_parameters(bluetooth_le_scan_params_t *params);

int _bt_register_scan_filter(const char *sender, bluetooth_le_scan_filter_t *filter, int *slot_id);

int _bt_unregister_scan_filter(const char *sender, int slot_id);

int _bt_unregister_all_scan_filters(const char *sender);

int _bt_start_le_scan(const char *sender);

int _bt_stop_le_scan(const char *sender);

void _bt_disable_all_scanner_status(void);

void _bt_set_le_scan_status(gboolean mode);

gboolean _bt_is_le_scanning(void);

void _bt_set_le_scan_type(bt_le_scan_type_t type);

bt_le_scan_type_t _bt_get_le_scan_type(void);

void _bt_send_scan_result_event(const bt_remote_le_dev_info_t *le_dev_info, const bt_le_adv_info_t *adv_info);

int _bt_add_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type);

int _bt_remove_white_list(bluetooth_device_address_t *device_address, bluetooth_device_address_type_t address_type);

int _bt_clear_white_list(void);

int _bt_le_read_maximum_data_length(bluetooth_le_read_maximum_data_length_t *max_le_datalength);

int _bt_initialize_ipsp(void);

int _bt_deinitialize_ipsp(void);

void _bt_init_gatt_client_senders(void);

int _bt_insert_gatt_client_sender(char *sender);

int _bt_delete_gatt_client_sender(char *sender);

void _bt_clear_gatt_client_senders(void);

void _bt_send_char_value_changed_event(void *param);

gboolean _bt_is_set_scan_parameter(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_ADAPTER_LE_H_*/

