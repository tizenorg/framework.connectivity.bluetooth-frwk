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

//#include <dbus/dbus-glib.h>
//#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-rfcomm-server.h"
#include "bt-service-agent.h"

/* Range of RFCOMM server ID : 0 ~ 244 */
#define BT_RFCOMM_SERVER_ID_MAX 245

#define BT_RFCOMM_PROXY_ADDRESS "x00/bluez/rfcomm"
#define BT_RFCOMM_SOCKET_ADDRESS "/bluez/rfcomm"

typedef struct {
	int data_fd;
	char *uuid;
	char *remote_address;
} bt_rfcomm_event_info_t;

GSList *server_list;
bt_rfcomm_server_info_t *_bt_rfcomm_get_server_info_using_uuid(char *uuid)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	retv_if(uuid == NULL, NULL);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (g_strcmp0(server_info->uuid, uuid) == 0)
			return server_info;
	}

	return NULL;
}

int _bt_rfcomm_create_socket(char *sender, char *uuid)
{
	return BLUETOOTH_ERROR_INTERNAL;
}

int __bt_rfcomm_server_get_address(bt_rfcomm_server_info_t *server_info)
{
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_listen(int socket_fd, int max_pending, gboolean is_native)
{
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_remove_socket(int socket_fd)
{
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_disconnect(int data_fd)
{
	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_rfcomm_is_uuid_available(char *uuid, gboolean *available)
{
	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_rfcomm_accept_connection(void)
{
	BT_DBG("+");
	if (!_bt_agent_reply_authorize(TRUE))
		return BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

/* To support the BOT  */
int _bt_rfcomm_reject_connection(void)
{
	BT_DBG("+");
	if (!_bt_agent_reply_authorize(FALSE))
		return BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_disconnect_all_connection(void)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		_bt_rfcomm_disconnect(server_info->data_fd);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_check_existence(gboolean *existence)
{
	BT_CHECK_PARAMETER(existence, return);

	if (server_list && g_slist_length(server_list) > 0) {
		*existence = TRUE;
	} else {
		*existence = FALSE;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_server_check_termination(char *name)
{
	GSList *l;
	bt_rfcomm_server_info_t *server_info;

	BT_CHECK_PARAMETER(name, return);

	for (l = server_list; l != NULL; l = l->next) {
		server_info = l->data;

		if (server_info == NULL)
			continue;

		if (g_strcmp0(server_info->sender, name) == 0) {
			_bt_rfcomm_remove_socket(server_info->control_fd);
		}
	}

	return BLUETOOTH_ERROR_NONE;
}


