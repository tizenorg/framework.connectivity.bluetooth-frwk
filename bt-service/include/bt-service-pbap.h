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

#ifndef BT_SERVICE_PBAP_H
#define BT_SERVICE_PBAP_H

#include <stdint.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <unistd.h>
#include <dlog.h>

#include <stdio.h>

#include <dbus/dbus-glib.h>

#undef LOG_TAG
#define LOG_TAG	"BLUETOOTH_FRWK_SERVICE"
#define ERR(fmt, args...) SLOGE(fmt, ##args)

int _bt_pbap_connect(const bluetooth_device_address_t *address);

int _bt_pbap_disconnect(const bluetooth_device_address_t *address);

int _bt_pbap_get_phonebook_size(const bluetooth_device_address_t *address,
		int source, int type);

int _bt_pbap_get_phonebook(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_pull_parameters_t *app_param);

int _bt_pbap_get_list(const bluetooth_device_address_t *address, int source,
		int type, bt_pbap_list_parameters_t *app_param);

int _bt_pbap_pull_vcard(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_pull_vcard_parameters_t *app_param);

int _bt_pbap_phonebook_search(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_search_parameters_t *app_param);

void _bt_pbap_obex_transfer_completed(const char *transfer_path, gboolean transfer_status);

void _bt_obex_pbap_client_disconnect(char *path);
#endif
