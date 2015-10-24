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
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <sys/socket.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"
#include "bluetooth-hid-api.h"
#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

#define HID_UUID		"00001124-0000-1000-8000-00805f9b34fb"
#define	REPORTID_MOUSE	1
#define BT_HID_BUFFER_LEN 100

/* The types of HIDP transaction */
#define BT_HIDP_TRANSACTION_SET_PROTOCOL		0x70
#define BT_HIDP_TRANSACTION_GET_IDLE			0x80
#define BT_HIDP_TRANSACTION_SET_IDLE			0x90
#define BT_HIDP_TRANSACTION_DATA			0xa0
#define BT_HIDP_TRANSACTION_DATC			0xb0
#define BT_HIDP_TRANSACTION_HANDSHAKE                   0x00
#define BT_HIDP_TRANSACTION_HID_CONTROL                 0x10
#define BT_HIDP_TRANSACTION_GET_REPORT                  0x40
#define BT_HIDP_TRANSACTION_SET_REPORT                  0x50
#define BT_HIDP_TRANSACTION_GET_PROTOCOL                0x60

#define BT_HIDP_DATA_OUT_RTYPE				0x02
#define BT_HIDP_DATA_IN_RTYPE				0x01

#define BT_HIDP_HSHK_ERROR_UNSUPPORTED_REQUEST          0x03
#define BT_HIDP_HSHK_ERROR_INVALID_PARAM                0x04
#define BT_HIDP_HSHK_ERROR_UNKNOWN                      0x0E
#define BT_HIDP_HSHK_ERROR_FATAL                        0x0F
#define BT_HIDP_HSHK_OK_SUCCESSFUL			0x00
#define BT_HIDP_HSHK_NOT_AVAILABLE			0x01
#define BT_HIDP_HSHK_ERROR_INVALID_REPORT_ID		0x02

/* The masks of BT_HIDP header */
#define BT_HIDP_HEADER_PARAMETER_MASK                   0x0f
#define BT_HIDP_HEADER_TRANSACTION_MASK                 0xf0

typedef struct {
	guint object_id;
	gchar *path;
	int id;
	char *uuid;
	GSList *device_list;
} hid_info_t;

typedef struct {
	int ctrl_fd;
	int intr_fd;
	GIOChannel *ctrl_data_io;
	GIOChannel *intr_data_io;
	guint ctrl_data_id;
	guint intr_data_id;
	char *address;
	guint disconnect_idle_id;
} hid_connected_device_info_t;

struct reports{
	guint8 type;
	guint8 rep_data[20];
}__attribute__((__packed__));

static hid_info_t *hid_info = NULL;

/* Variable for privilege, only for write API,
  before we should reduce time to bt-service dbus calling
  -1 : Don't have a permission to access API
  0 : Initial value, not yet check
  1 : Have a permission to access API
*/
static int privilege_token_send_mouse = 0;
static int privilege_token_send_key = 0;
static int privilege_token_reply = 0;

static gboolean __hid_disconnect(hid_connected_device_info_t *info);

static hid_connected_device_info_t *__find_hid_info_with_address(const char *remote_addr)
{
	GSList *l;

	for ( l = hid_info->device_list; l != NULL; l = l->next) {
		hid_connected_device_info_t *info = l->data;
		if (g_strcmp0((const char *)info->address, (const char *)remote_addr) == 0)
			return info;
	}
	return NULL;
}

static void __hid_connected_cb(hid_connected_device_info_t *info,
			bt_event_info_t *event_info)
{
	bluetooth_hid_request_t conn_info;

	memset(&conn_info, 0x00, sizeof(bluetooth_hid_request_t));
	if (info->intr_fd != -1 && info->ctrl_fd == -1)
		conn_info.socket_fd = info->intr_fd;
	else
		conn_info.socket_fd = info->ctrl_fd;
	_bt_convert_addr_string_to_type (conn_info.device_addr.addr , info->address);

	BT_INFO_C("Connected [HID Device]");
	_bt_common_event_cb(BLUETOOTH_HID_DEVICE_CONNECTED,
			BLUETOOTH_ERROR_NONE, &conn_info,
			event_info->cb, event_info->user_data);
}

static gboolean __hid_disconnect(hid_connected_device_info_t *info)
{
	bluetooth_hid_request_t disconn_info;
	int fd = info->ctrl_fd;
	bt_event_info_t *event_info;

	BT_INFO_C("Disconnected [HID Device]");
	hid_info->device_list = g_slist_remove(hid_info->device_list, info);
	if (info->ctrl_data_id > 0) {
		g_source_remove(info->ctrl_data_id);
		info->ctrl_data_id = 0;
	}
	if (info->intr_data_id > 0) {
		g_source_remove(info->intr_data_id);
		info->intr_data_id = 0;
	}

	if (info->intr_fd >= 0) {
		close(info->ctrl_fd);
		close(info->intr_fd);
		info->intr_fd = -1;
		info->ctrl_fd = -1;
	}

	if (info->ctrl_data_io) {
		g_io_channel_shutdown(info->ctrl_data_io, TRUE, NULL);
		g_io_channel_unref(info->ctrl_data_io);
		info->ctrl_data_io = NULL;
	}
	if (info->intr_data_io) {
		g_io_channel_shutdown(info->intr_data_io, TRUE, NULL);
		g_io_channel_unref(info->intr_data_io);
		info->intr_data_io = NULL;
	}
	info->disconnect_idle_id = 0;
	event_info = _bt_event_get_cb_data(BT_HID_DEVICE_EVENT);
	if (event_info == NULL)
		return FALSE;

	memset(&disconn_info, 0x00, sizeof(bluetooth_hid_request_t));
	disconn_info.socket_fd = fd;
	_bt_convert_addr_string_to_type (disconn_info.device_addr.addr , info->address);
	_bt_common_event_cb(BLUETOOTH_HID_DEVICE_DISCONNECTED,
			BLUETOOTH_ERROR_NONE, &disconn_info,
			event_info->cb, event_info->user_data);
	if (info->address)
		g_free(info->address);
	g_free(info);
	info = NULL;
	BT_DBG("-");
	return FALSE;
}

void __free_hid_info(hid_info_t *info)
{
	BT_DBG("");

	_bt_unregister_gdbus(info->object_id);

	while (info->device_list) {
		hid_connected_device_info_t *dev_info = NULL;
		dev_info = (hid_connected_device_info_t *)info->device_list->data;

		if (dev_info->disconnect_idle_id > 0) {
			BT_INFO("Disconnect idle still not process remove source");
			g_source_remove(dev_info->disconnect_idle_id);
			dev_info->disconnect_idle_id = 0;
		}
		__hid_disconnect(dev_info);
	}

	g_free(info->path);
	g_free(info->uuid);
	g_free(info);
}

static gboolean __received_cb(GIOChannel *chan, GIOCondition cond,
								gpointer data)
{
	hid_connected_device_info_t *info = data;
	GIOStatus status = G_IO_STATUS_NORMAL;
	char buffer[20];
	gsize len = 0;
	GError *err = NULL;
	guint8  header, type, param;
	bt_event_info_t *event_info;
	retv_if(info == NULL, FALSE);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_ERR_C("HID  disconnected: %d", info->ctrl_fd);
               if (info->disconnect_idle_id > 0) {
			BT_INFO("Disconnect idle still not process remove source");
			g_source_remove(info->disconnect_idle_id);
			info->disconnect_idle_id = 0;
               }
		__hid_disconnect(info);
		return FALSE;
	}
	status = g_io_channel_read_chars(chan, buffer, BT_RFCOMM_BUFFER_LEN,
			&len, &err);
	if (status == G_IO_STATUS_NORMAL) {
		BT_INFO("Parsing Data");
		bluetooth_hid_received_data_t data = {0};
		header = buffer[0];
		type = header & BT_HIDP_HEADER_TRANSACTION_MASK;
		param = header & BT_HIDP_HEADER_PARAMETER_MASK;
		BT_INFO("type %d , param %d", type, param);
		BT_INFO("Data Reveived from %s" , info->address);
		data.address = g_strdup(info->address);
		switch (type) {
			case BT_HIDP_TRANSACTION_HANDSHAKE:
				BT_INFO("TRANS HANDSHAKE");
				data.type = HTYPE_TRANS_HANDSHAKE;
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
			break;
			case BT_HIDP_TRANSACTION_HID_CONTROL:
				BT_INFO("HID CONTROL");
				data.type = HTYPE_TRANS_HID_CONTROL;
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
			break;
			case BT_HIDP_TRANSACTION_DATA:
				BT_INFO("TRANS DATA");
				data.type = HTYPE_TRANS_DATA;
				if ( param & BT_HIDP_DATA_IN_RTYPE) {
					BT_INFO("Input Report");
					data.param = PTYPE_DATA_RTYPE_INPUT;
					data.buffer_size = len;
					data.buffer = (char *) malloc(sizeof(char) * len);
					if (data.buffer)
						memcpy(data.buffer, buffer, len);
				}
				else {
					BT_INFO("Out Report");
					data.param = PTYPE_DATA_RTYPE_OUTPUT;
					data.buffer_size = len;
					data.buffer = (char *) malloc(sizeof(char) * len);
					if (data.buffer)
						memcpy(data.buffer, buffer, len);
				}
			break;
			case BT_HIDP_TRANSACTION_GET_REPORT: {
				BT_INFO("Get Report");
				data.type = HTYPE_TRANS_GET_REPORT;
				if (param & BT_HIDP_DATA_IN_RTYPE) {
					BT_INFO("Input Report");
					data.param = PTYPE_DATA_RTYPE_INPUT;
				} else {
					BT_INFO("Output Report");
					data.param = PTYPE_DATA_RTYPE_OUTPUT;
				}
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
				break;
			}
			case BT_HIDP_TRANSACTION_SET_REPORT: {
				BT_INFO("Set Report");
				data.type = HTYPE_TRANS_SET_REPORT;
				if (param & BT_HIDP_DATA_IN_RTYPE) {
					BT_INFO("Input Report");
					data.param = PTYPE_DATA_RTYPE_INPUT;
				} else {
					BT_INFO("Output Report");
					data.param = PTYPE_DATA_RTYPE_OUTPUT;
				}
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
				break;
			}
			case BT_HIDP_TRANSACTION_GET_PROTOCOL:{
				BT_INFO("Get_PROTOCOL");
				data.type = HTYPE_TRANS_GET_PROTOCOL;
				data.param = PTYPE_DATA_RTYPE_INPUT;
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
				break;
			}
			case BT_HIDP_TRANSACTION_SET_PROTOCOL:{
				BT_INFO("Set_PROTOCOL");
				data.type = HTYPE_TRANS_SET_PROTOCOL;
				data.param = PTYPE_DATA_RTYPE_INPUT;
				data.buffer_size = len;
				data.buffer = (char *) malloc(sizeof(char) * len);
				if (data.buffer)
					memcpy(data.buffer, buffer, len);
				break;
			}
			default: {
				BT_INFO("unsupported HIDP control message");
				BT_ERR("Send Handshake Message");
				guint8 type = BT_HIDP_TRANSACTION_HANDSHAKE |
					BT_HIDP_HSHK_ERROR_UNSUPPORTED_REQUEST;
				data.type = HTYPE_TRANS_UNKNOWN;
				int fd = g_io_channel_unix_get_fd(chan);
				int bytes = write(fd,  &type, sizeof(type));
				BT_INFO("Bytes Written %d", bytes);
				break;
			}
		}
		event_info = _bt_event_get_cb_data(BT_HID_DEVICE_EVENT);
		if (event_info == NULL) {
			g_free(data.buffer);
			g_free((char *)data.address);
			return FALSE;
		}

		_bt_common_event_cb(BLUETOOTH_HID_DEVICE_DATA_RECEIVED,
				BLUETOOTH_ERROR_NONE, &data,
				event_info->cb, event_info->user_data);

		g_free(data.buffer);
		g_free((char *)data.address);
	} else {
		BT_INFO("Error while reading data");
	}
	return TRUE;
}

int new_hid_connection(const char *path, int fd, bluetooth_device_address_t *addr)
{
	hid_info_t *info = NULL;
	hid_connected_device_info_t *dev_info = NULL;
	bt_event_info_t *event_info = NULL;
	char address[18];
	info = hid_info;

	if (info == NULL)
		return -1;
	_bt_convert_addr_type_to_string((char *)address, addr->addr);
	BT_INFO("Address [%s]", address);
	dev_info = __find_hid_info_with_address(address);
	if (dev_info == NULL) {
		dev_info = (hid_connected_device_info_t *)
			g_malloc0(sizeof(hid_connected_device_info_t));
		if (dev_info == NULL) {
			BT_ERR("Fail to allocation memory");
			return -1;
		}

		dev_info->intr_fd = -1;
		dev_info->ctrl_fd = -1;
		dev_info->intr_fd = fd;
		dev_info->address = g_strdup(address);
		dev_info->intr_data_io = g_io_channel_unix_new(dev_info->intr_fd);
		g_io_channel_set_encoding(dev_info->intr_data_io, NULL, NULL);
		g_io_channel_set_flags(dev_info->intr_data_io, G_IO_FLAG_NONBLOCK, NULL);

		dev_info->intr_data_id = g_io_add_watch(dev_info->intr_data_io,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				__received_cb, dev_info);
		hid_info->device_list = g_slist_append(hid_info->device_list, dev_info);
	} else {
		dev_info->ctrl_fd = fd;
		dev_info->ctrl_data_io = g_io_channel_unix_new(dev_info->ctrl_fd);
		g_io_channel_set_encoding(dev_info->ctrl_data_io, NULL, NULL);
		g_io_channel_set_flags(dev_info->ctrl_data_io, G_IO_FLAG_NONBLOCK, NULL);

		dev_info->ctrl_data_id = g_io_add_watch(dev_info->ctrl_data_io,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				__received_cb, dev_info);
	}
	if (dev_info->ctrl_fd != -1 && dev_info->intr_fd != -1) {
		event_info = _bt_event_get_cb_data(BT_HID_DEVICE_EVENT);
		if (event_info)
			__hid_connected_cb(dev_info, event_info);
	}

	return 0;
}
static hid_info_t *__register_method()
{
	int object_id;
	hid_info_t *info = NULL;
	char *path = NULL;
	path = g_strdup_printf("/org/socket/server/%d", getpid());

	object_id = _bt_register_new_conn(path, new_hid_connection);
	if (object_id < 0) {
		return NULL;
	}
	info = g_new(hid_info_t, 1);
	info->object_id = (guint)object_id;
	info->path = path;
	info->id = 0;
	info->device_list = NULL;

	return info;
}

BT_EXPORT_API int bluetooth_hid_device_init(hid_cb_func_ptr callback_ptr, void *user_data)
{
	int ret;

	/* Register HID Device events */
	BT_INFO("BT_HID_DEVICE_EVENT");
	ret = _bt_register_event(BT_HID_DEVICE_EVENT , (void *)callback_ptr, user_data);

	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to init the event handler");
		return ret;
	}

	_bt_set_user_data(BT_HID, (void *)callback_ptr, user_data);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hid_device_deinit(void)
{
	int ret;

	ret = _bt_unregister_event(BT_HID_DEVICE_EVENT);

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to deinit the event handler");
		return ret;
	}

	_bt_set_user_data(BT_HID, NULL, NULL);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hid_device_activate(void)
{
	bt_register_profile_info_t profile_info;
	int result = BLUETOOTH_ERROR_NONE;

	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_DEVICE_ACTIVATE)
		 == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	if (hid_info != NULL)
		return BLUETOOTH_ERROR_IN_PROGRESS;

	hid_info = __register_method();
	if (hid_info == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	hid_info->uuid = g_strdup(HID_UUID);

	profile_info.authentication = TRUE;
	profile_info.authorization = TRUE;
	profile_info.obj_path = hid_info->path;
	profile_info.role = g_strdup("Hid");
	profile_info.service = hid_info->uuid;
	profile_info.uuid = hid_info->uuid;

	BT_INFO("uuid %s", profile_info.uuid);
	result = _bt_register_profile_platform(&profile_info, FALSE);

	return result;
}

BT_EXPORT_API int bluetooth_hid_device_deactivate(void)
{
	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_DEVICE_DEACTIVATE)
		 == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	if (hid_info == NULL)
		return BLUETOOTH_ERROR_NOT_IN_OPERATION;

	_bt_unregister_profile(hid_info->path);

	__free_hid_info(hid_info);
	hid_info = NULL;
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hid_device_connect(const char *remote_addr)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = {0};
	hid_connected_device_info_t *info = NULL;
	int ret;
	BT_DBG("+");
	BT_CHECK_PARAMETER(remote_addr, return);

	info = __find_hid_info_with_address(remote_addr);
	if (info) {
		BT_ERR("Connection Already Exists");
		return BLUETOOTH_ERROR_ALREADY_CONNECT;
	}
	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_DEVICE_CONNECT)
		 == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}

	memcpy(device_address, remote_addr, BT_ADDRESS_STRING_SIZE);
	ret = _bt_connect_profile(device_address, HID_UUID, NULL, NULL);

	return ret;
}
BT_EXPORT_API int bluetooth_hid_device_disconnect(const char *remote_addr)
{
	if (_bt_check_privilege(BT_CHECK_PRIVILEGE, BT_HID_DEVICE_DISCONNECT)
		 == BLUETOOTH_ERROR_PERMISSION_DEINED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	}
	hid_connected_device_info_t *info = NULL;

	info = __find_hid_info_with_address(remote_addr);
	if (info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	_bt_disconnect_profile((char *)remote_addr, HID_UUID, NULL, NULL);

	info->disconnect_idle_id = g_idle_add((GSourceFunc)__hid_disconnect, info);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}
BT_EXPORT_API int bluetooth_hid_device_send_mouse_event(const char *remote_addr,
					hid_send_mouse_event_t send_event)
{
	int result;
	int written = 0;
	int socket_fd;
	hid_connected_device_info_t *info = NULL;

	switch (privilege_token_send_mouse) {
	case 0:
		result = _bt_check_privilege(BT_BLUEZ_SERVICE, BT_HID_DEVICE_SEND_MOUSE_EVENT);

		if (result == BLUETOOTH_ERROR_NONE) {
			privilege_token_send_mouse = 1; /* Have a permission */
		} else if (result == BLUETOOTH_ERROR_PERMISSION_DEINED) {
			BT_ERR("Don't have a privilege to use this API");
			privilege_token_send_mouse = -1; /* Don't have a permission */
			return BLUETOOTH_ERROR_PERMISSION_DEINED;
		} else {
			/* Just break - It is not related with permission error */
		}
		break;
	case 1:
		/* Already have a privilege */
		break;
	case -1:
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	default:
		/* Invalid privilge token value */
		return BLUETOOTH_ERROR_INTERNAL;
	}
	info = __find_hid_info_with_address(remote_addr);
	if (info == NULL) {
		BT_ERR("Connection Information not found");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	if (info->intr_fd != -1 && info->ctrl_fd == -1)
		socket_fd = info->intr_fd;
	else
		socket_fd = info->ctrl_fd;

	written = write(socket_fd, &send_event, sizeof(send_event));

	return written;
}

BT_EXPORT_API int bluetooth_hid_device_send_key_event(const char *remote_addr,
					hid_send_key_event_t send_event)
{
	int result;
	int written = 0;
	int socket_fd;
	hid_connected_device_info_t *info = NULL;

	switch (privilege_token_send_key) {
	case 0:
		result = _bt_check_privilege(BT_BLUEZ_SERVICE, BT_HID_DEVICE_SEND_KEY_EVENT);

		if (result == BLUETOOTH_ERROR_NONE) {
			privilege_token_send_key = 1; /* Have a permission */
		} else if (result == BLUETOOTH_ERROR_PERMISSION_DEINED) {
			BT_ERR("Don't have a privilege to use this API");
			privilege_token_send_key = -1; /* Don't have a permission */
			return BLUETOOTH_ERROR_PERMISSION_DEINED;
		} else {
			/* Just break - It is not related with permission error */
		}
		break;
	case 1:
		/* Already have a privilege */
		break;
	case -1:
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	default:
		/* Invalid privilge token value */
		return BLUETOOTH_ERROR_INTERNAL;
	}

	info = __find_hid_info_with_address(remote_addr);
	if (info == NULL) {
		BT_ERR("Connection Information not found");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	if (info->intr_fd != -1 && info->ctrl_fd == -1)
		socket_fd = info->intr_fd;
	else
		socket_fd = info->ctrl_fd;

	written = write(socket_fd, &send_event, sizeof(send_event));
	return written;
}

BT_EXPORT_API int bluetooth_hid_device_reply_to_report(const char *remote_addr,
				bt_hid_header_type_t htype,
				bt_hid_param_type_t ptype,
				const char *data,
				unsigned int data_len)
{
	int result;
	struct reports output_report = { 0 };
	int bytes = 0;
	hid_connected_device_info_t *info = NULL;
	info = __find_hid_info_with_address(remote_addr);
	if (info == NULL) {
		BT_ERR("Connection Information not found");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	switch (privilege_token_reply) {
	case 0:
		result = _bt_check_privilege(BT_BLUEZ_SERVICE, BT_HID_DEVICE_SEND_REPLY_TO_REPORT);

		if (result == BLUETOOTH_ERROR_NONE) {
			privilege_token_reply = 1; /* Have a permission */
		} else if (result == BLUETOOTH_ERROR_PERMISSION_DEINED) {
			BT_ERR("Don't have a privilege to use this API");
			privilege_token_reply = -1; /* Don't have a permission */
			return BLUETOOTH_ERROR_PERMISSION_DEINED;
		} else {
			/* Just break - It is not related with permission error */
		}
		break;
	case 1:
		/* Already have a privilege */
		break;
	case -1:
		return BLUETOOTH_ERROR_PERMISSION_DEINED;
	default:
		/* Invalid privilge token value */
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_INFO("htype %d ptype %d", htype, ptype);
	switch(htype) {
		case HTYPE_TRANS_GET_REPORT: {
			switch(ptype) {
				case PTYPE_DATA_RTYPE_INPUT: {
					output_report.type = BT_HIDP_TRANSACTION_DATA |
							BT_HIDP_DATA_IN_RTYPE;
					memcpy(output_report.rep_data, data, data_len);
					bytes = write(info->intr_fd, &output_report,
								sizeof(output_report));
					BT_DBG("Bytes Written %d", bytes);
					break;
				}
				default:
					BT_INFO("Not Supported");
					break;
			}
			break;
		case HTYPE_TRANS_GET_PROTOCOL: {
			BT_DBG("Replying to Get_PROTOCOL");
			output_report.type = BT_HIDP_TRANSACTION_DATA | BT_HIDP_DATA_OUT_RTYPE;
			output_report.rep_data[0] = data[0];
			bytes = write(info->intr_fd, &output_report, 2);
			BT_DBG("Bytes Written %d", bytes);
			break;
		}
		case HTYPE_TRANS_SET_PROTOCOL: {
			BT_DBG("Reply to Set_Protocol");
			output_report.type = BT_HIDP_TRANSACTION_DATA | BT_HIDP_DATA_IN_RTYPE;
			memcpy(output_report.rep_data, data, data_len);
			bytes = write(info->ctrl_fd, &output_report,
					sizeof(output_report));
			BT_DBG("Bytes Written %d", bytes);
			break;
		}
		case HTYPE_TRANS_HANDSHAKE: {
			BT_DBG("Replying Handshake");
			output_report.type = BT_HIDP_TRANSACTION_HANDSHAKE | data[0];
			memset(output_report.rep_data, 0, sizeof(output_report.rep_data));
			bytes = write(info->intr_fd,  &output_report.type,
					sizeof(output_report.type));
			BT_DBG("Bytes Written %d", bytes);
			break;
		}
			default:
				break;
		}
	}
	return bytes;
}
