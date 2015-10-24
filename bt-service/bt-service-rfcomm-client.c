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
#include <fcntl.h>

#include <gio/gio.h>
#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-rfcomm-server.h"

typedef struct {
	int req_id;
	char *channel;
	char *address;
	char *uuid;
	GDBusProxy *rfcomm_proxy;
} rfcomm_function_data_t;

rfcomm_function_data_t *rfcomm_info;
GSList *client_list;

static int __bt_rfcomm_terminate_client(int socket_fd)
{
	return BLUETOOTH_ERROR_NONE;
}


int _bt_rfcomm_connect_using_uuid(int request_id,
			bluetooth_device_address_t *device_address,
			char *remote_uuid)
{
	return BLUETOOTH_ERROR_NONE;
}

/* Range of the Channel : 0 <= channel <= 30 */
int _bt_rfcomm_connect_using_channel(int request_id,
			bluetooth_device_address_t *device_address,
			char *channel)
{
	return BLUETOOTH_ERROR_NONE;
}

/* Be used in RFCOMM client /server */
int _bt_rfcomm_disconnect(int socket_fd)
{
	return __bt_rfcomm_terminate_client(socket_fd);
}

/* Be used in RFCOMM client /server */
int _bt_rfcomm_write(int socket_fd, char *buf, int length)
{
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_cancel_connect(void)
{
	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_is_connected(gboolean *connected)
{
	BT_CHECK_PARAMETER(connected, return);

	*connected = (client_list == NULL || g_slist_length(client_list) == 0) ?
					FALSE : TRUE;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_is_device_connected(bluetooth_device_address_t *device_address,
					gboolean *connected)
{
	GSList *l;
	bt_rfcomm_info_t *client_info;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };

	BT_CHECK_PARAMETER(device_address, return);
	BT_CHECK_PARAMETER(connected, return);

	_bt_convert_addr_type_to_string(address, device_address->addr);

	*connected = FALSE;

	for (l = client_list; l != NULL; l = l->next) {
		client_info = l->data;

		if (client_info == NULL)
			continue;

		if (g_strcmp0(address, client_info->address) == 0) {
			*connected = TRUE;
			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_rfcomm_client_disconnect_all(void)
{
	GSList *l;
	bt_rfcomm_info_t *client_info;

	for (l = client_list; l != NULL; l = l->next) {
		client_info = l->data;

		if (client_info == NULL)
			continue;

		_bt_rfcomm_disconnect(client_info->fd);
	}

	return BLUETOOTH_ERROR_NONE;
}

