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

#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-util.h"

static GSList *req_list = NULL;

/* available request id : 0 ~ 244 */
#define BT_REQUEST_ID_RANGE_MAX 245

static int assigned_id;
static gboolean req_id_used[BT_REQUEST_ID_RANGE_MAX];

void _bt_init_request_id(void)
{
	assigned_id = 0;
	memset(req_id_used, 0x00, BT_REQUEST_ID_RANGE_MAX);
}

int _bt_assign_request_id(void)
{
	int index;

	index = assigned_id + 1;

	if (index >= BT_REQUEST_ID_RANGE_MAX)
		index = 0;

	while (req_id_used[index] == TRUE) {
		if (index == assigned_id) {
			/* No available ID */
			BT_ERR("All request ID is used");
			return -1;
		}

		index++;

		if (index >= BT_REQUEST_ID_RANGE_MAX)
			index = 0;
	}

	assigned_id = index;
	req_id_used[index] = TRUE;

	return assigned_id;
}

void _bt_delete_request_id(int request_id)
{
	ret_if(request_id >= BT_REQUEST_ID_RANGE_MAX);
	ret_if(request_id < 0);

	req_id_used[request_id] = FALSE;
}

void _bt_init_request_list(void)
{
	_bt_clear_request_list();
}

/* insert request next to head */
int _bt_insert_request_list(int req_id, int service_function,
			char *name, DBusGMethodInvocation *context)
{
	request_info_t *info;

	info = g_malloc0(sizeof(request_info_t));
	/* Fix : NULL_RETURNS */
	retv_if(info == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	info->req_id = req_id;
	info->service_function = service_function;
	info->context = context;

	req_list = g_slist_append(req_list, info);

	return BLUETOOTH_ERROR_NONE;
}

request_info_t *_bt_get_request_info(int req_id)
{
	GSList *l;
	request_info_t *info;

	for (l = req_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->req_id == req_id)
			return info;
	}

	return NULL;
}

/* delete request which has the target req_id */
int _bt_delete_request_list(int req_id)
{
	GSList *l;
	request_info_t *info;

	for (l = req_list; l != NULL; l = g_slist_next(l)) {
		info = l->data;
		if (info == NULL)
			continue;

		if (info->req_id == req_id) {
			req_list = g_slist_remove(req_list, info);
			_bt_delete_request_id(info->req_id);
			g_free(info);
			return BLUETOOTH_ERROR_NONE;
		}
	}

	return BLUETOOTH_ERROR_NOT_FOUND;
}

void _bt_clear_request_list(void)
{
	if (req_list) {
		g_slist_foreach(req_list, (GFunc)g_free, NULL);
		g_slist_free(req_list);
		req_list = NULL;
	}
}

