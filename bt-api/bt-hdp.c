/*
 * bluetooth-frwk
 *
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <dbus/dbus.h>
#include <string.h>

#include "bluetooth-api.h"
#include "bt-common.h"
#include "bt-internal-types.h"

#define HDP_BUFFER_SIZE 1024
#define BLUEZ_HDP_MANAGER_INTERFACE  "org.bluez.HealthManager"
#define BLUEZ_HDP_DEVICE_INTERFACE  "org.bluez.HealthDevice"
#define BLUEZ_HDP_CHANNEL_INTERFACE  "org.bluez.HealthChannel"

typedef struct {
	char *obj_channel_path;
	int fd;
	guint watch_id;
} hdp_obj_info_t;

typedef struct {
	void *app_handle;
	GSList *obj_info;
} hdp_app_list_t;


/**********************************************************************
*		Static Functions declaration				*
***********************************************************************/
static int __bt_hdp_internal_create_application(unsigned int data_type,
						int role,
						bt_hdp_qos_type_t channel_type,
						char **app_handle);

static DBusHandlerResult __bt_hdp_internal_event_filter(DBusConnection *sys_conn,
							DBusMessage *msg,
							void *data);

static void __bt_hdp_internal_handle_connect(DBusMessage *msg);

static void __bt_hdp_internal_handle_disconnect(DBusMessage *msg);

static void __bt_hdp_internal_handle_property_changed(DBusMessage *msg);

static int __bt_hdp_internal_add_filter(void);

static int __bt_hdp_internal_acquire_fd(const char *path);

static guint __bt_hdp_internal_watch_fd(int file_desc, const char *path);

static gboolean __bt_hdp_internal_data_received(GIOChannel *gio,
						GIOCondition cond,
						gpointer data);

static int __bt_hdp_internal_destroy_application(const char *app_handle);

static void __bt_hdp_internal_remove_filter(void);

static hdp_app_list_t *__bt_hdp_internal_gslist_find_app_handler(void *app_handle);

static hdp_obj_info_t *__bt_hdp_internal_gslist_obj_find_using_fd(int fd);

static hdp_obj_info_t *__bt_hdp_internal_gslist_obj_find_using_path(const char *obj_channel_path);

/*Global Variables*/
static DBusConnection *g_hdp_dus_conn;

static GSList *g_app_list = NULL;

/**********************************************************************
*			Health device APIs (HDP)			*
***********************************************************************/

BT_EXPORT_API int bluetooth_hdp_activate(unsigned short data_type,
					bt_hdp_role_type_t role,
					bt_hdp_qos_type_t channel_type,
					char **app_handle)
{
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");

	BT_CHECK_ENABLED(return);

	/*For source role is mandatory */
	if (role == HDP_ROLE_SOURCE && channel_type == HDP_QOS_ANY) {
		BT_DBG("For source, type is mandatory - Reliable/Streaming");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	result = __bt_hdp_internal_create_application(data_type, role,
						channel_type, app_handle);

	return result;
}

static void __bt_hdp_obj_info_free(hdp_obj_info_t *info)
{
	if (info) {
		g_source_remove(info->watch_id);
		close(info->fd);
		g_free(info->obj_channel_path);
		g_free(info);
	}
}

static int __bt_hdp_internal_create_application(unsigned int data_type,
					int role,
					bt_hdp_qos_type_t channel_type,
					char **app_handle)
{
	DBusMessage *msg;
	DBusMessage *reply;
	const char *svalue;
	const char *key_type;
	char *app_path;
	hdp_app_list_t *list;
	DBusError err;
	DBusMessageIter iter;
	DBusMessageIter array_iter;
	DBusMessageIter entry;
	DBusMessageIter variant;
	guint16 value;
	DBusConnection *conn;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, "/org/bluez",
					  BLUEZ_HDP_MANAGER_INTERFACE,
					  "CreateApplication");

	retv_if(msg == NULL, BLUETOOTH_ERROR_NO_RESOURCES);

	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
		DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING DBUS_TYPE_STRING_AS_STRING
		DBUS_TYPE_VARIANT_AS_STRING DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
		&array_iter);

	key_type = "DataType";
	value = (guint16) data_type;
	dbus_message_iter_open_container(&array_iter, DBUS_TYPE_DICT_ENTRY,
					NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_type);
	dbus_message_iter_open_container(&entry,
		DBUS_TYPE_VARIANT, DBUS_TYPE_UINT16_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT16, &value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(&array_iter, &entry);

	key_type = "Role";

	/*0-Source,1-Sink*/
	svalue = (role == HDP_ROLE_SINK) ? "Sink" : "Source";
	dbus_message_iter_open_container(&array_iter, DBUS_TYPE_DICT_ENTRY,
					NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_type);
	dbus_message_iter_open_container(&entry,
		DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &svalue);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(&array_iter, &entry);

	key_type = "Description";
	svalue = "Health Device";
	dbus_message_iter_open_container(&array_iter, DBUS_TYPE_DICT_ENTRY,
					NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_type);
	dbus_message_iter_open_container(&entry,
		DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &variant);
	dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &svalue);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(&array_iter, &entry);

	if (role == HDP_ROLE_SOURCE) {
		key_type = "ChannelType";
		if (channel_type == HDP_QOS_RELIABLE)
			svalue = "Reliable";
		else if (channel_type == HDP_QOS_STREAMING)
			svalue = "Streaming";

		dbus_message_iter_open_container(&array_iter,
			DBUS_TYPE_DICT_ENTRY, NULL, &entry);
		dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING,
						&key_type);
		dbus_message_iter_open_container(&entry,
			DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING,
			&variant);
		dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING,
						&svalue);
		dbus_message_iter_close_container(&entry, &variant);
		dbus_message_iter_close_container(&array_iter, &entry);
	}

	dbus_message_iter_close_container(&iter, &array_iter);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(
					conn, msg,
					-1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR(" HDP:dbus Can't create application");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH,
				&app_path, DBUS_TYPE_INVALID)) {

		BT_ERR(" HDP: Can't get reply arguments from Dbus");

		if (dbus_error_is_set(&err)) {
			BT_ERR("Error: %s", err.message);
			dbus_error_free(&err);
		}

		dbus_message_unref(reply);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	BT_DBG("Created health application: %s", (char *)app_path);

	ret = __bt_hdp_internal_add_filter();

	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Funtion failed");
		return ret;
	}

	list = g_new0(hdp_app_list_t, 1);
	list->app_handle = (void *)g_strdup(app_path);
	*app_handle = list->app_handle;

	g_app_list = g_slist_append(g_app_list, list);

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_hdp_internal_add_filter(void)
{
	DBusError dbus_error;

	BT_DBG("+");

	/*Single process only one signal registration is required */
	if (g_hdp_dus_conn) {
		BT_ERR("g_hdp_dus_conn already exist");
		goto done;
	}

	/* Add the filter for HDP client functions */
	dbus_error_init(&dbus_error);

	g_hdp_dus_conn = _bt_get_system_conn();
	retv_if(g_hdp_dus_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_connection_add_filter(g_hdp_dus_conn,
				__bt_hdp_internal_event_filter, NULL, NULL);

	dbus_bus_add_match(g_hdp_dus_conn,
			"type='signal',interface=" BLUEZ_HDP_DEVICE_INTERFACE,
			&dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		g_hdp_dus_conn = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}

done:
	BT_DBG("-\n");
	return BLUETOOTH_ERROR_NONE;

}

static DBusHandlerResult __bt_hdp_internal_event_filter(DBusConnection *sys_conn,
					DBusMessage *msg, void *data)
{
	const char *path = dbus_message_get_path(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	BT_DBG("Path = %s\n", path);
	if (path == NULL || g_strcmp0(path, "/") == 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_is_signal(msg, BLUEZ_HDP_DEVICE_INTERFACE,
					"ChannelConnected"))
		__bt_hdp_internal_handle_connect(msg);

	else if (dbus_message_is_signal(msg, BLUEZ_HDP_DEVICE_INTERFACE,
					"ChannelDeleted"))
		__bt_hdp_internal_handle_disconnect(msg);

	else if (dbus_message_is_signal(msg, BLUEZ_HDP_DEVICE_INTERFACE,
					"PropertyChanged"))
		__bt_hdp_internal_handle_property_changed(msg);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static void __bt_hdp_internal_handle_connect(DBusMessage *msg)
{
	const char *path = dbus_message_get_path(msg);
	const char *obj_channel_path;
	bt_user_info_t *user_info;
	int ret;

	BT_DBG("+********Signal - ChannelConnected******\n\n");
	BT_DBG("Path = %s", path);

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH,
				&obj_channel_path, DBUS_TYPE_INVALID)) {
		BT_ERR("Unexpected parameters in ChannelConnected signal");
		return;
	}

	BT_DBG("Channel connected, Path = %s", obj_channel_path);

	ret = __bt_hdp_internal_acquire_fd(obj_channel_path);
	if (ret != BLUETOOTH_ERROR_NONE) {
		user_info = _bt_get_user_data(BT_COMMON);
		if (user_info->cb) {
			_bt_common_event_cb(BLUETOOTH_EVENT_HDP_CONNECTED,
					BLUETOOTH_ERROR_CONNECTION_ERROR, NULL,
					user_info->cb, user_info->user_data);
		}
	}

	BT_DBG("-");
}

static void __bt_hdp_internal_handle_disconnect(DBusMessage *msg)
{
	const char *path = dbus_message_get_path(msg);
	const char *obj_channel_path;
	char address[BT_ADDRESS_STRING_SIZE] = { 0, };
	bluetooth_device_address_t device_addr = { {0} };
	bt_hdp_disconnected_t dis_ind;
	hdp_obj_info_t *info;
	bt_user_info_t *user_info;

	BT_DBG("+********Signal - ChannelDeleted ******\n\n");
	BT_DBG("Path = %s", path);

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH,
				&obj_channel_path, DBUS_TYPE_INVALID)) {
		BT_ERR("Unexpected parameters in ChannelDeleted signal");
		return;
	}

	BT_DBG("Channel Deleted, Path = %s", obj_channel_path);

	info = __bt_hdp_internal_gslist_obj_find_using_path(obj_channel_path);
	if (!info) {
		BT_ERR("No obj info for ob_channel_path [%s]\n", obj_channel_path);
		return;
	}

	/*Since bluetoothd is not sending the ChannelDeleted signal */
	_bt_device_path_to_address(path, address);

	_bt_convert_addr_string_to_type(device_addr.addr, address);

	dis_ind.channel_id = info->fd;
	dis_ind.device_address = device_addr;

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info->cb) {
		_bt_common_event_cb(BLUETOOTH_EVENT_HDP_DISCONNECTED,
				BLUETOOTH_ERROR_NONE, &dis_ind,
				user_info->cb, user_info->user_data);
	}

	BT_DBG(" Removed connection from list\n");

	__bt_hdp_obj_info_free(info);

}

static void __bt_hdp_internal_handle_property_changed(DBusMessage *msg)
{
	const char *path = dbus_message_get_path(msg);
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	const char *property;
	const char *obj_main_channel_path;

	BT_DBG("+*******Signal - PropertyChanged*******\n");
	BT_DBG("Path = %s", path);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter) != DBUS_TYPE_STRING) {
		BT_DBG("This is bad format dbus");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	BT_DBG("Property (%s)\n", property);

	if (0 == g_strcmp0(property, "MainChannel")) {
		BT_DBG("Property MainChannel received");

		dbus_message_iter_next(&item_iter);

		dbus_message_iter_recurse(&item_iter, &value_iter);

		dbus_message_iter_get_basic(&value_iter,
						&obj_main_channel_path);
		BT_DBG("Path = %s", path);

		BT_DBG("Main Channel  Path = %s", obj_main_channel_path);
	}
	BT_DBG("-*************\n");
}

static int __bt_hdp_internal_acquire_fd(const char *path)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0, };
	bluetooth_device_address_t device_addr = { {0} };
	DBusMessageIter reply_iter;
	DBusMessageIter reply_iter_entry;
	const char *property;
	char *type_qos = NULL;
	char *device = NULL;;
	char *app_handle = NULL;;
	hdp_app_list_t *list = NULL;;
	DBusMessage *msg;
	DBusMessage *reply;
	DBusConnection *conn;
	bt_hdp_connected_t conn_ind;
	DBusError err;
	int fd;
	bt_user_info_t *user_info;

	BT_DBG("+");

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, path,
					  BLUEZ_HDP_CHANNEL_INTERFACE,
					  "Acquire");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg,
							-1, &err);

	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR(" HDP:****** dbus Can't create application ****");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (!dbus_message_get_args(reply, &err, DBUS_TYPE_UNIX_FD, &fd,
				DBUS_TYPE_INVALID)) {
		BT_ERR(" HDP:dbus Can't get reply arguments");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		goto error;
	}

	dbus_message_unref(reply);

	BT_DBG("File Descriptor = %d, Dev_path = %s \n", fd, path);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, path,
			BLUEZ_HDP_CHANNEL_INTERFACE, "GetProperties");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg,
							-1, &err);

	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR(" HDP:dbus Can't get the reply");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_message_iter_init(reply, &reply_iter);

	if (dbus_message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_ARRAY) {
		BT_ERR("Can't get reply arguments - DBUS_TYPE_ARRAY\n");
		goto error;
	}

	dbus_message_iter_recurse(&reply_iter, &reply_iter_entry);

	/*Parse the dict */
	while (dbus_message_iter_get_arg_type(&reply_iter_entry) ==
						DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter dict_entry, dict_entry_val;
		dbus_message_iter_recurse(&reply_iter_entry, &dict_entry);
		dbus_message_iter_get_basic(&dict_entry, &property);
		BT_DBG("String received = %s\n", property);

		if (g_strcmp0("Type", property) == 0) {
			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
						DBUS_TYPE_STRING)
				continue;

			dbus_message_iter_get_basic(&dict_entry_val, &type_qos);

		} else if (g_strcmp0("Device", property) == 0) {
			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
						DBUS_TYPE_OBJECT_PATH)
				continue;

			dbus_message_iter_get_basic(&dict_entry_val, &device);

		} else if (g_strcmp0("Application", property) == 0) {
			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
						DBUS_TYPE_OBJECT_PATH)
				continue;

			dbus_message_iter_get_basic(&dict_entry_val,
							&app_handle);
		}
		dbus_message_iter_next(&reply_iter_entry);
	}

	BT_DBG("QOS = %s, Device = %s, Apphandler = %s",
			type_qos, device, app_handle);

	if (NULL == type_qos || NULL == app_handle) {
		BT_ERR("Pasing failed\n");
		goto error;
	}

	list = __bt_hdp_internal_gslist_find_app_handler((void *)app_handle);

	/*Only process register with app handle receive the Connected event */
	if (NULL == list) {
		BT_DBG("**** Could not locate the list for %s*****\n", app_handle);
		goto error;
	}

	hdp_obj_info_t *info = g_new0(hdp_obj_info_t, 1);
	info->fd = fd;
	info->obj_channel_path = g_strdup(path);
	info->watch_id = __bt_hdp_internal_watch_fd(fd, info->obj_channel_path);
	list->obj_info = g_slist_append(list->obj_info, info);

	_bt_device_path_to_address(path, address);

	_bt_convert_addr_string_to_type(device_addr.addr, address);

	conn_ind.app_handle = app_handle;
	conn_ind.channel_id = fd;
	conn_ind.device_address = device_addr;
	conn_ind.type = (g_strcmp0(type_qos, "Reliable") == 0) ?
			HDP_QOS_RELIABLE : HDP_QOS_STREAMING;

	BT_DBG("Going to give callback\n");

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info->cb) {
		_bt_common_event_cb(BLUETOOTH_EVENT_HDP_CONNECTED,
				BLUETOOTH_ERROR_NONE, &conn_ind,
				user_info->cb, user_info->user_data);
	}

	dbus_message_unref(reply);

	BT_DBG("Updated fd in the list*\n");
	BT_DBG("-\n");

	return BLUETOOTH_ERROR_NONE;
 error:
	dbus_message_unref(reply);
	return BLUETOOTH_ERROR_INTERNAL;
}

static guint __bt_hdp_internal_watch_fd(int file_desc, const char *path)
{
	GIOChannel *gio;
	guint id;

	BT_DBG("+");

	gio = g_io_channel_unix_new(file_desc);

	g_io_channel_set_close_on_unref(gio, TRUE);

	id = g_io_add_watch(gio, G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			__bt_hdp_internal_data_received, (void *)path);
	BT_DBG("-");
	return id;
}


static void __bt_hdp_internal_handle_disconnect_cb(int sk, const char *path)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0, };
	bluetooth_device_address_t device_addr = { {0} };
	bt_hdp_disconnected_t dis_ind;
	hdp_obj_info_t *info;
	bt_user_info_t *user_info;

	BT_DBG("******** Socket Error  ******\n");

	info = __bt_hdp_internal_gslist_obj_find_using_path(path);
	ret_if(info == NULL);

	/*Since bluetoothd is not sending the ChannelDeleted signal */
	_bt_device_path_to_address(path, address);

	_bt_convert_addr_string_to_type(device_addr.addr, address);

	dis_ind.channel_id = sk;
	dis_ind.device_address = device_addr;

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info->cb) {
		_bt_common_event_cb(BLUETOOTH_EVENT_HDP_DISCONNECTED,
				BLUETOOTH_ERROR_NONE, &dis_ind,
				user_info->cb, user_info->user_data);
	}

	BT_DBG(" Removed connection from list\n");

	__bt_hdp_obj_info_free(info);
}

static gboolean __bt_hdp_internal_data_received(GIOChannel *gio,
					GIOCondition cond, gpointer data)
{
	char buff[HDP_BUFFER_SIZE] = { 0, };
	int sk;
	int act_read;
	bt_hdp_data_ind_t data_ind = { 0, };
	const char *path = (const char *)data;
	bt_user_info_t *user_info;

	BT_DBG("+");

	sk = g_io_channel_unix_get_fd(gio);

	if (cond & (G_IO_NVAL | G_IO_HUP | G_IO_ERR)) {
		BT_DBG("GIOCondition %d.............path = %s\n", cond, path);
		 __bt_hdp_internal_handle_disconnect_cb(sk, path);
		return FALSE;
	}

	act_read = recv(sk, (void *)buff, sizeof(buff), 0);

	if (act_read > 0) {
		BT_DBG("Received data of %d\n", act_read);
	} else {
		BT_DBG("Read failed.....\n");
		return FALSE;
	}

	data_ind.channel_id = sk;
	data_ind.buffer = buff;
	data_ind.size = act_read;

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info->cb) {
		_bt_common_event_cb(BLUETOOTH_EVENT_HDP_DATA_RECEIVED,
				BLUETOOTH_ERROR_NONE, &data_ind,
				user_info->cb, user_info->user_data);
	}

	BT_DBG("-\n");

	return TRUE;
}

BT_EXPORT_API int bluetooth_hdp_deactivate(const char *app_handle)
{
	BT_DBG("+");

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(app_handle, return);

	return __bt_hdp_internal_destroy_application(app_handle);
}

static hdp_app_list_t *__bt_hdp_internal_gslist_find_app_handler(void *app_handle)
{
	GSList *l;

	retv_if(g_app_list == NULL, NULL);

	BT_DBG("List length = %d\n", g_slist_length(g_app_list));

	for (l = g_app_list; l != NULL; l = l->next) {
		hdp_app_list_t *list = l->data;

		if (list) {
			if (0 == g_strcmp0((char *)list->app_handle,
						(char *)app_handle))
				return list;
		}
	}
	return NULL;
}

static hdp_obj_info_t *__bt_hdp_internal_gslist_obj_find_using_fd(int fd)
{
	GSList *l;
	GSList *iter;

	retv_if(g_app_list == NULL, NULL);

	BT_DBG("List length = %d\n", g_slist_length(g_app_list));

	for (l = g_app_list; l != NULL; l = l->next) {
		hdp_app_list_t *list = l->data;
		if (!list)
			return NULL;

		for (iter = list->obj_info; iter != NULL; iter = iter->next) {
			hdp_obj_info_t *info = iter->data;
			if (!info)
				return NULL;

			if (fd == info->fd)
				return info;
		}
	}
	return NULL;
}

static hdp_obj_info_t *__bt_hdp_internal_gslist_obj_find_using_path(const char *obj_channel_path)
{
	GSList *l;
	GSList *iter;
	hdp_obj_info_t *info = NULL;

	retv_if(g_app_list == NULL, NULL);

	BT_DBG("List length = %d\n", g_slist_length(g_app_list));
	for (l = g_app_list; l != NULL; l = l->next) {
		hdp_app_list_t *list = l->data;
		if (!list)
			return NULL;

		for (iter = list->obj_info; iter != NULL; iter = iter->next) {
			 info = iter->data;
			if (!info)
				return NULL;

			if (0 == g_strcmp0(info->obj_channel_path, obj_channel_path)) {
				list->obj_info = g_slist_remove(list->obj_info, info);
				return info;
			}
		}
	}
	return NULL;
}

static gboolean  __bt_hdp_internal_destroy_application_cb(gpointer data)
{
	const char *app_handle;
	hdp_app_list_t *list = NULL;
	app_handle = (const char *)data;

	BT_DBG("+");

	list = __bt_hdp_internal_gslist_find_app_handler((void *)app_handle);
	if (NULL == list) {
		BT_DBG("**** list not found for %s ******\n", app_handle);
		return FALSE;
	}

	g_app_list = g_slist_remove(g_app_list, list);

	g_free(list->app_handle);
	g_slist_foreach(list->obj_info, (GFunc)__bt_hdp_obj_info_free, NULL);
	g_free(list);

	BT_DBG("List length = %d\n", g_slist_length(g_app_list));

	if (0 == g_slist_length(g_app_list))
		__bt_hdp_internal_remove_filter();
	BT_DBG("-");
	return FALSE;
}

static int __bt_hdp_internal_destroy_application(const char *app_handle)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	DBusConnection *conn;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, "/org/bluez",
			BLUEZ_HDP_MANAGER_INTERFACE, "DestroyApplication");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &app_handle,
					DBUS_TYPE_INVALID);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg,
							-1, &err);
	dbus_message_unref(msg);
	if (!reply) {
		BT_ERR(" HDP:dbus Can't Destroy application");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	BT_DBG("Destroyed health application: %s", (char *)app_handle);

	g_idle_add(__bt_hdp_internal_destroy_application_cb,
			(gpointer)app_handle);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_hdp_internal_remove_filter(void)
{
	BT_DBG("+");

	ret_if(g_hdp_dus_conn == NULL);

	dbus_connection_remove_filter(g_hdp_dus_conn,
					__bt_hdp_internal_event_filter, NULL);

	g_hdp_dus_conn = NULL;	/*should not unref here, bcz no ++reff */

	BT_DBG("-");
}

BT_EXPORT_API int bluetooth_hdp_send_data(unsigned int channel_id,
					    const char *buffer,
					    unsigned int size)
{
	int wbytes = 0;
	int written = 0;

	BT_DBG("+");

	BT_CHECK_ENABLED(return);

	if ((channel_id == 0) || (NULL == buffer) || (size == 0)) {
		BT_DBG("Invalid arguments..\n");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	} else {
		while (wbytes < size) {
			written = write(channel_id, buffer + wbytes, size - wbytes);
			if (written <= 0) {
				BT_DBG("write failed..\n");
				return BLUETOOTH_ERROR_NOT_IN_OPERATION;
			}
			wbytes += written;
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_hdp_connect_request_cb(DBusPendingCall *call, void *user_data)
{
	bt_hdp_connected_t *conn_ind = user_data;
	bt_user_info_t *user_info;
	DBusMessage *msg = NULL;
	DBusError err;
	char *obj_connect_path = NULL;
	DBusMessageIter args;

	msg = dbus_pending_call_steal_reply(call);
	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, msg)) {
		BT_DBG("write failed.. %s\n", err.message);
		dbus_error_free(&err);
		goto done;
	}
	if (!dbus_message_iter_init(msg, &args)) {
		BT_DBG("message has no arguments\n");
		goto done;
	}
	dbus_message_iter_get_basic(&args, &obj_connect_path);
	BT_DBG("Obj Path returned = %s\n", obj_connect_path);
	g_free(obj_connect_path);
done:
	user_info = _bt_get_user_data(BT_COMMON);
	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_HDP_CONNECTED,
				BLUETOOTH_ERROR_CONNECTION_ERROR, conn_ind,
				user_info->cb, user_info->user_data);
	}
	g_free((void *)conn_ind->app_handle);
	g_free(conn_ind);
	dbus_message_unref(msg);
}


BT_EXPORT_API int bluetooth_hdp_connect(const char *app_handle,
			bt_hdp_qos_type_t channel_type,
			const bluetooth_device_address_t *device_address)
{
	DBusError err;
	DBusMessage *msg;
	DBusConnection *conn = NULL;
	DBusPendingCall *pending;
	bt_hdp_connected_t *param;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char default_adapter_path[BT_ADAPTER_OBJECT_PATH_MAX + 1] = { 0 };
	char *dev_path = NULL;
	char *role;

	BT_DBG("+");

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(app_handle, return);
	BT_CHECK_PARAMETER(device_address, return);

	if (channel_type == HDP_QOS_RELIABLE) {
		role = "Reliable";
	} else if (channel_type == HDP_QOS_STREAMING) {
		role = "Streaming";
	} else if (channel_type == HDP_QOS_ANY) {
		role = "Any";
	} else {
		BT_ERR("Invalid channel_type %d", channel_type);
		return BLUETOOTH_ERROR_ACCESS_DENIED;
	}

	conn = _bt_get_system_conn();

	/* If the adapter path is wrong, we can think the BT is not enabled. */
	if (_bt_get_adapter_path_dbus(conn, default_adapter_path) < 0) {
		BT_ERR("Could not get adapter path\n");
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}
	BT_DBG("adapter path is : %s \n", default_adapter_path);
	_bt_convert_addr_type_to_string(address,
				(unsigned char *)device_address->addr);

	BT_DBG("create conection to %s", address);

	dev_path = g_strdup_printf("%s/dev_%s", default_adapter_path,
								 address);

	if (dev_path == NULL) {
		return BLUETOOTH_ERROR_MEMORY_ALLOCATION;
	}

	g_strdelimit(dev_path, ":", '_');

	BT_DBG("path: %s", dev_path);

	BT_DBG("app path %s\n", app_handle);

	param = g_new0(bt_hdp_connected_t, 1);
	param->app_handle = g_strdup(app_handle);
	memcpy(&param->device_address, device_address,
					 BLUETOOTH_ADDRESS_LENGTH);
	param->type = channel_type;

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, dev_path,
			BLUEZ_HDP_DEVICE_INTERFACE, "CreateChannel");
	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);
	dbus_message_append_args(msg,
				DBUS_TYPE_OBJECT_PATH, &app_handle,
				DBUS_TYPE_STRING , &role,
				DBUS_TYPE_INVALID);
	dbus_error_init(&err);
	if (dbus_connection_send_with_reply(conn, msg,
					&pending, -1) == FALSE) {
		if (dbus_error_is_set(&err)) {
			BT_ERR("HDP connection Dbus Call Error %s",
							err.message);
			dbus_error_free(&err);
		}
		g_free((void *)param->app_handle);
		g_free(param);
		dbus_message_unref(msg);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_pending_call_set_notify(pending, __bt_hdp_connect_request_cb,
								param, NULL);
	dbus_message_unref(msg);
	g_free(dev_path);
	BT_DBG("Destroyed health application: %s", (char *)app_handle);
	return BLUETOOTH_ERROR_NONE;
}

static void __bt_hdp_disconnect_request_cb(DBusPendingCall *call,
							 void *user_data)
{
	bt_hdp_disconnected_t *disconn_ind = user_data;
	DBusMessage *message;
	DBusError err;
	bt_user_info_t *user_info;
	message = dbus_pending_call_steal_reply(call);
	dbus_error_init(&err);
	if (dbus_set_error_from_message(&err, message)) {
		BT_ERR("HDP disconnection Dbus Call Error: %s\n", err.message);
		dbus_error_free(&err);
		user_info = _bt_get_user_data(BT_COMMON);

		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_HDP_DISCONNECTED,
					BLUETOOTH_ERROR_CONNECTION_ERROR,
					disconn_ind, user_info->cb,
					user_info->user_data);
		}
	} else {
		BT_DBG("HDP disconnection Dbus Call is done\n");
	}
	dbus_message_unref(message);
	g_free(disconn_ind);
}

BT_EXPORT_API int bluetooth_hdp_disconnect(unsigned int channel_id,
			const bluetooth_device_address_t *device_address)
{
	DBusMessage *msg;
	DBusPendingCall *pending;
	DBusConnection *conn = NULL;
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char default_adapter_path[BT_ADAPTER_OBJECT_PATH_MAX + 1] = { 0 };
	char *dev_path = NULL;
	bt_hdp_disconnected_t *param;

	BT_DBG("+\n");

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(device_address, return);

	hdp_obj_info_t *info = __bt_hdp_internal_gslist_obj_find_using_fd(
								channel_id);
	if (NULL == info) {
		BT_ERR("*** Could not locate the list for %d*****\n",
								 channel_id);
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	conn = _bt_get_system_conn();

	if (_bt_get_adapter_path_dbus(conn, default_adapter_path) < 0) {
		BT_ERR("Could not get adapter path\n");
		return BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
	}
	BT_DBG("adapter path is : %s \n", default_adapter_path);
	_bt_convert_addr_type_to_string(address,
				(unsigned char *)device_address->addr);

	BT_DBG("create conection to  %s\n", address);

	dev_path = g_strdup_printf("%s/dev_%s", default_adapter_path, address);

	if (dev_path == NULL) {
		return BLUETOOTH_ERROR_MEMORY_ALLOCATION;
	}

	g_strdelimit(dev_path, ":", '_');

	BT_DBG("path  %s\n", dev_path);

	param = g_new0(bt_hdp_disconnected_t, 1);
	param->channel_id = channel_id;
	memcpy(&param->device_address, device_address,
						 BLUETOOTH_ADDRESS_LENGTH);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, dev_path,
			BLUEZ_HDP_DEVICE_INTERFACE, "DestroyChannel");
	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);
	dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH,
				info->obj_channel_path,
				DBUS_TYPE_INVALID);
	if (dbus_connection_send_with_reply(conn, msg,
					&pending, -1) == FALSE) {
		BT_ERR("HDP connection Dbus Call Error");
		g_free(param);
		dbus_message_unref(msg);
		g_free(dev_path);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(msg);
	dbus_pending_call_set_notify(pending, __bt_hdp_disconnect_request_cb,
								 param, NULL);
	g_free(dev_path);

	BT_DBG("Destroyed health application: \n");
	return BLUETOOTH_ERROR_NONE;
}
