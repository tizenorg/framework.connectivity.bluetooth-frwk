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


#ifndef _BT_SERVICE_AUDIO_H_
#define _BT_SERVICE_AUDIO_H_

#include <glib.h>
#include <sys/types.h>
#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int req_id;
	int type;
	int disconnection_type;
	char *address;
	gboolean ag_flag;
	GArray **out_param1;
} bt_headset_wait_t;

typedef enum {
	BT_PENDING_NONE = 0x00,
	BT_PENDING_CONNECT,
	BT_PENDING_DISCONNECT
} bt_pending_request_t;

typedef struct {
	int req_id;
	char *address;
	bt_pending_request_t  pending;
	GArray **out_param;
	int type;
} bt_audio_function_data_t;

typedef enum {
	BT_AUDIO_HSP = 0x01,
	BT_AUDIO_A2DP,
	BT_AUDIO_ALL,
} bt_audio_type_t;

typedef enum {
	BT_STATE_NONE = 0x00,
	BT_STATE_CONNECTING,
	BT_STATE_CONNECTED,
	BT_STATE_DISCONNECTING,
	BT_STATE_DISCONNECTED
} bt_headset_device_state_t;

#define BT_CONTENT_PROTECTION_PATH "/org/tizen/bluetooth/a2dpcontentprotection"
#define BT_CONTENT_PROTECTION_INTERFACE "org.tizen.bluetooth.A2dpContentProtection"

int _bt_audio_connect(int request_id, int type,
		bluetooth_device_address_t *device_address,
		GArray **out_param1);

int _bt_audio_disconnect(int request_id, int type,
		bluetooth_device_address_t *device_address,
		GArray **out_param1);

int _bt_hf_connect(int request_id,
		bluetooth_device_address_t *device_address,
		GArray **out_param1);

int _bt_hf_disconnect(int request_id,
		bluetooth_device_address_t *device_address,
		GArray **out_param1);

int _bt_audio_get_speaker_gain(unsigned int *gain);

int _bt_audio_set_speaker_gain(unsigned int gain);

int _bt_audio_set_content_protect(gboolean status);

void _bt_set_audio_wait_data_flag(gboolean flag);

bt_headset_wait_t *_bt_get_audio_wait_data(void);

void _bt_rel_wait_data(void);

void _bt_add_headset_to_list(int type, int status, const char *address);

void _bt_remove_headset_from_list(int type, const char *address);

gboolean _bt_is_headset_type_connected(int type, char *address);
void _bt_remove_from_connected_list(const char *address);

int _bt_get_device_state_from_list(int type, const char *address);

void _bt_remove_from_connected_list(const char *address);

void _bt_audio_check_pending_connect();

gboolean _bt_is_service_connected(char *address, int type);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_AUDIO_H_*/

