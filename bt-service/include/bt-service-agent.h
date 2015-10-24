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

#ifndef BT_SERVICE_AGENT_H
#define BT_SERVICE_AGENT_H

#include <stdint.h>
#include <glib.h>
#include <unistd.h>
#include <dlog.h>
#include <stdio.h>

#undef LOG_TAG
#define LOG_TAG	"BLUETOOTH_FRWK_SERVICE"
#define ERR(fmt, args...) SLOGE(fmt, ##args)

#define BT_MAX_EVENT_STR_LENGTH	50

#ifndef TIZEN_WEARABLE
#define BT_FILE_VISIBLE_TIME "file/private/libug-setting-bluetooth-efl/visibility_time"
#endif

typedef enum {
	HS_PROFILE_UUID = ((unsigned short)0x1108),		/**<HS*/
	AUDIO_SOURCE_UUID = ((unsigned short)0x110A),		/**<AUDIO SOURCE*/
	AUDIO_SINK_UUID = ((unsigned short)0x110B),		/**<AUDIO SINK*/
	AV_REMOTE_CONTROL_TARGET_UUID = ((unsigned short)0x110C),/**<AV REMOTE CONTROL TARGET*/
	ADVANCED_AUDIO_PROFILE_UUID = ((unsigned short)0x110D),	/**<A2DP*/
	AV_REMOTE_CONTROL_UUID = ((unsigned short)0x110E),	/**<AV REMOTE CONTROL UUID*/
	HF_PROFILE_UUID = ((unsigned short)0x111E),		/**<HF*/
} bt_agent_service_uuid_list_t;

typedef enum {
	BT_AGENT_EVENT_PIN_REQUEST,
	BT_AGENT_EVENT_PASSKEY_CONFIRM_REQUEST,
	BT_AGENT_EVENT_PASSKEY_AUTO_ACCEPTED,
	BT_AGENT_EVENT_PASSKEY_REQUEST,
	BT_AGENT_EVENT_PASSKEY_DISPLAY_REQUEST,
	BT_AGENT_EVENT_AUTHORIZE_REQUEST,
	BT_AGENT_EVENT_CONFIRM_MODE_REQUEST,
	BT_AGENT_EVENT_APP_CONFIRM_REQUEST,
	BT_AGENT_EVENT_FILE_RECEIVED,
	BT_AGENT_EVENT_KEYBOARD_PASSKEY_REQUEST,
	BT_AGENT_EVENT_SECURITY,
	BT_AGENT_EVENT_TERMINATE,
	BT_AGENT_EVENT_EXCHANGE_REQUEST,
	BT_AGENT_EVENT_PBAP_REQUEST,
	BT_AGENT_EVENT_MAP_REQUEST,
	BT_AGENT_EVENT_SYSTEM_RESET_REQUEST,
	BT_AGENT_EVENT_LEGACY_PAIR_FAILED_FROM_REMOTE,
} bt_agent_event_type_t;

typedef struct {
	int type;
	char *uuid;
	char *path;
	int fd;
} bt_agent_osp_server_t;

void* _bt_create_agent(const char *path, gboolean adapter);

void _bt_destroy_agent(void *agent);

gboolean _bt_agent_is_canceled(void);
void _bt_agent_set_canceled(gboolean value);

gboolean _bt_agent_register_osp_server(const gint type,
		const char *uuid, char *path, int fd);

gboolean _bt_agent_unregister_osp_server(const gint type, const char *uuid);

gboolean _bt_agent_reply_authorize(gboolean accept);

int _bt_agent_reply_cancellation(void);

int _bt_launch_system_popup(bt_agent_event_type_t event_type,
							const char *device_name,
							char *passkey,
							const char *filename,
							const char *agent_path);
#endif
