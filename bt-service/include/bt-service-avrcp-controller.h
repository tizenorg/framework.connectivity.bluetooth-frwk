/*
 * Bluetooth-frwk
 *
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Hocheol Seo <hocheol.seo@samsung.com>
 *		 Chanyeol Park <chanyeol.park@samsung.com>
  *		 Rakesh M K<rakesh.mk@samsung.com>
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


#ifndef _BT_SERVICE_AVRCP_CONTROLLER_H_
#define _BT_SERVICE_AVRCP_CONTROLLER_H_

#include <glib.h>
#include <sys/types.h>
#include <gio/gio.h>

#include "bluetooth-api.h"
#include "bluetooth-media-control.h"

#ifdef __cplusplus
extern "C" {
#endif


#define BT_MEDIA_CONTROL_PATH "%s/player0"

int _bt_avrcp_control_cmd(int type);

int _bt_avrcp_control_set_property(int type, unsigned int value);

int _bt_avrcp_control_get_property(int type, unsigned int *value);

int _bt_avrcp_control_get_track_info(media_metadata_attributes_t *metadata);

void _bt_handle_avrcp_control_event(GVariant *reply, const char *path);

void _bt_set_control_device_path(const char *path);

void _bt_remove_control_device_path(const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_SERVICE_AVRCP_CONTROLLER_H_*/


