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

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <mime_type.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-opp-client.h"
#include "bt-service-obex-agent.h"

#define DBUS_STRUCT_STRING_STRING (dbus_g_type_get_struct("GValueArray", \
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))

static void __data_list_ptr_array_add(GPtrArray *ptr_array,
					const gchar *name, const gchar *type)
{
	GValue value = { 0, };

	g_value_init(&value, DBUS_STRUCT_STRING_STRING);
	g_value_take_boxed(&value,
			dbus_g_type_specialized_construct(
						DBUS_STRUCT_STRING_STRING));

	dbus_g_type_struct_set(&value,
				0, g_strdup(name),
				1, g_strdup(type),
				G_MAXUINT);

	g_ptr_array_add(ptr_array, g_value_get_boxed(&value));
}


static BtObexAgent *opc_obex_agent = NULL;
static GSList *transfer_list = NULL;

bt_sending_info_t *sending_info;

static gboolean __bt_release_callback(DBusGMethodInvocation *context,
					gpointer user_data);

static gboolean __bt_request_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					gpointer user_data);

static gboolean __bt_progress_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					guint64 transferred,
					gpointer user_data);

static gboolean __bt_complete_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					gpointer user_data);

static gboolean __bt_error_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					const char *message,
					gpointer user_data);


static int __bt_opp_client_start_sending(int request_id, char *address,
					char **file_name_array, int file_count);

static int __bt_opp_client_agent_init(void)
{
	opc_obex_agent = _bt_obex_agent_new();
	retv_if(opc_obex_agent == NULL, BLUETOOTH_ERROR_INTERNAL);

	_bt_obex_set_release_cb(opc_obex_agent,
				    __bt_release_callback, NULL);
	_bt_obex_set_request_cb(opc_obex_agent,
				    __bt_request_callback, NULL);
	_bt_obex_set_progress_cb(opc_obex_agent,
				     __bt_progress_callback, NULL);
	_bt_obex_set_complete_cb(opc_obex_agent,
				     __bt_complete_callback, NULL);
	_bt_obex_set_error_cb(opc_obex_agent,
				__bt_error_callback, NULL);

	_bt_obex_setup(opc_obex_agent, BT_OBEX_CLIENT_AGENT_PATH);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_opp_client_agent_deinit(void)
{
	ret_if(opc_obex_agent == NULL);

	g_object_unref(opc_obex_agent);
	opc_obex_agent = NULL;
}

static GQuark __bt_opc_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

static void __bt_free_transfer_info(bt_transfer_info_t *info)
{
	ret_if(info == NULL);

	if (info->proxy)
		g_object_unref(info->proxy);

	g_free(info->transfer_name);
	g_free(info->file_name);
	g_free(info);
}

static void __bt_free_sending_info(bt_sending_info_t *info)
{
	ret_if(info == NULL);

	/* Free the sending variable */
	__bt_free_transfer_info(info->transfer_info);

	g_free(info->address);
	g_free(info);
}

static void __bt_value_free(GValue *value)
{
	g_value_unset(value);
	g_free(value);
}

static gboolean __bt_cancel_push_cb(gpointer data)
{
	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;

	retv_if(sending_info == NULL, FALSE);

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	__bt_opp_client_agent_deinit();

	/* Operate remain works */
	if (g_slist_length(transfer_list) > 0) {
		bt_sending_data_t *node = NULL;

		node = transfer_list->data;
		if (node == NULL) {
			BT_DBG("data is NULL");
			return FALSE;
		}

		transfer_list = g_slist_remove(transfer_list, node);

		if (__bt_opp_client_start_sending(node->request_id,
				node->address, node->file_path,
				node->file_count) != BLUETOOTH_ERROR_NONE) {
			BT_DBG("Fail to start sending");
		}
	}

	return FALSE;
}

static gboolean __bt_progress_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					guint64 transferred,
					gpointer user_data)
{
	int percentage_progress;
	gint64 size;
	int result = BLUETOOTH_ERROR_NONE;

	dbus_g_method_return(context);

	retv_if(sending_info == NULL, TRUE);
	retv_if(sending_info->transfer_info == NULL, TRUE);

	size = sending_info->transfer_info->size;

	if (size != 0)
		percentage_progress = (int)(((gdouble)transferred /
				(gdouble)size) * 100);
	else
		percentage_progress = 0;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &percentage_progress,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	return TRUE;
}

static gboolean __bt_complete_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;

	dbus_g_method_return(context);

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	return TRUE;
}

static gboolean __bt_request_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					gpointer user_data)
{
	GValue *value;
	const char *transfer_name;
	const char *file_name;
	int size;
	int result = BLUETOOTH_ERROR_NONE;
	GHashTable *hash = NULL;
	GError *error;

	if (sending_info == NULL || sending_info->is_canceled == TRUE) {
		result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		goto canceled;
	}

	dbus_g_method_return(context, "");

	__bt_free_transfer_info(sending_info->transfer_info);

	sending_info->transfer_info = g_malloc0(sizeof(bt_transfer_info_t));
	sending_info->transfer_info->proxy = g_object_ref(transfer);

	dbus_g_proxy_call(transfer, "GetProperties", NULL,
	                        G_TYPE_INVALID,
	                        dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
	                        &hash, G_TYPE_INVALID);

	if (hash == NULL)
		goto fail;

	value = g_hash_table_lookup(hash, "Name");
	transfer_name = value ? g_value_get_string(value) : NULL;

	value = g_hash_table_lookup(hash, "Filename");
	file_name = value ? g_value_get_string(value) : NULL;

	value = g_hash_table_lookup(hash, "Size");
	size = value ? g_value_get_uint64(value) : 0;

	sending_info->transfer_info->transfer_name = g_strdup(transfer_name);
	sending_info->transfer_info->file_name = g_strdup(file_name);
	sending_info->transfer_info->size = size;
	sending_info->result = BLUETOOTH_ERROR_NONE;

	g_hash_table_destroy(hash);

	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	return TRUE;
canceled:
	error = g_error_new(__bt_opc_error_quark(), BT_OBEX_AGENT_ERROR_CANCEL,
			"CancelledByUser");

	dbus_g_method_return_error(context, error);
	g_error_free(error);

	return FALSE;
fail:
	result = BLUETOOTH_ERROR_INTERNAL;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	__bt_opp_client_agent_deinit();

	return TRUE;
}

static void __bt_free_sending_data(gpointer data)
{
	int i;
	bt_sending_data_t *info = data;

	ret_if(info == NULL);

	for (i = 0; i < info->file_count; i++) {
		g_free(info->file_path[i]);
	}

	_bt_delete_request_id(info->request_id);

	g_free(info->file_path);
	g_free(info->address);
	g_free(info);
}

static gboolean __bt_release_callback(DBusGMethodInvocation *context,
					gpointer user_data)
{
	dbus_g_method_return(context);

	retv_if(sending_info == NULL, FALSE);

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			DBUS_TYPE_INT32, &sending_info->result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	__bt_opp_client_agent_deinit();

	/* Operate remain works */
	if (g_slist_length(transfer_list) > 0) {
		bt_sending_data_t *data = NULL;

		data = transfer_list->data;
		if (data == NULL)
			goto fail;

		transfer_list = g_slist_remove(transfer_list, data);

		if (__bt_opp_client_start_sending(data->request_id,
				data->address, data->file_path,
				data->file_count) != BLUETOOTH_ERROR_NONE) {
			goto fail;
		}
	}

	return TRUE;
fail:
	g_slist_free_full(transfer_list,
				(GDestroyNotify)__bt_free_sending_data);
	transfer_list = NULL;
	return TRUE;
}

static gboolean __bt_error_callback(DBusGMethodInvocation *context,
					DBusGProxy *transfer,
					const char *message,
					gpointer user_data)
{
	int result;

	dbus_g_method_return(context);

	retv_if(sending_info == NULL, FALSE);
	retv_if(sending_info->transfer_info == NULL, FALSE);

	if (sending_info->is_canceled == TRUE)  {
		result = BLUETOOTH_ERROR_CANCEL_BY_USER;
	} else if (g_strcmp0(message, "Forbidden") == 0) {
		result = BLUETOOTH_ERROR_ACCESS_DENIED;
	} else if (g_str_has_prefix(message,
		"Transport endpoint is not connected") == TRUE) {
		result = BLUETOOTH_ERROR_NOT_CONNECTED;
	} else if (g_strcmp0(message, "Database full") == 0) {
		result = BLUETOOTH_ERROR_OUT_OF_MEMORY;
	} else {
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	sending_info->result = result;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);
	return TRUE;
}

static void __bt_send_files_cb(DBusGProxy *proxy, DBusGProxyCall *call,
				void *user_data)
{
	GError *error = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	if (dbus_g_proxy_end_call(proxy, call, &error,
					G_TYPE_INVALID) == FALSE) {

		BT_ERR("%s", error->message);
		g_error_free(error);

		result = BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(proxy);
	ret_if(sending_info == NULL);

	sending_info->sending_proxy = NULL;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	if (result != BLUETOOTH_ERROR_NONE) {
		__bt_free_sending_info(sending_info);
		sending_info = NULL;

		/* Operate remain works */
		if (g_slist_length(transfer_list) > 0) {
			bt_sending_data_t *data = NULL;

			data = transfer_list->data;
			if (data == NULL)
				goto fail;

			transfer_list = g_slist_remove(transfer_list, data);

			if (__bt_opp_client_start_sending(data->request_id,
					data->address, data->file_path,
					data->file_count) != BLUETOOTH_ERROR_NONE) {
				goto fail;
			}
		}

	}
	return;

fail:
	g_slist_free_full(transfer_list,
			(GDestroyNotify)__bt_free_sending_data);
	transfer_list = NULL;
}

static void __bluetooth_ptr_array_free(gpointer data)
{
	GValue value = { 0, };

	gchar *name = NULL;
	gchar *type = NULL;

	if(data == NULL)
		return;

	g_value_init(&value, DBUS_STRUCT_STRING_STRING);
	g_value_set_boxed(&value, data);

	dbus_g_type_struct_get(&value,
			0, &name,
			1, &type,
			G_MAXUINT);

	g_free(name);
	g_free(type);
}

static int __bt_opp_client_start_sending(int request_id, char *address,
					char **file_name_array, int file_count)
{
	GHashTable *hash;
	GValue *value;
	GPtrArray *ptr_array;
	DBusGConnection *g_conn;
	DBusGProxy *client_proxy;
	DBusGProxyCall *proxy_call;
	char *agent_path;
	char *ext;
	char *mime_type = NULL;
	int i;
	int result = MIME_TYPE_ERROR_NONE;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(file_name_array, return);

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	client_proxy =  dbus_g_proxy_new_for_name(g_conn, BT_OBEX_SERVICE_NAME,
					"/", BT_OBEX_CLIENT_INTERFACE);

	retv_if(client_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
				     NULL, (GDestroyNotify)__bt_value_free);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string(value, address);
	g_hash_table_insert(hash, "Destination", value);

	__bt_free_sending_info(sending_info);

	sending_info = g_malloc0(sizeof(bt_sending_info_t));
	sending_info->address = g_strdup(address);
	sending_info->request_id = request_id;

	__bt_opp_client_agent_deinit();
	__bt_opp_client_agent_init();

	agent_path = g_strdup(BT_OBEX_CLIENT_AGENT_PATH);

	ptr_array = g_ptr_array_new_with_free_func(__bluetooth_ptr_array_free);

	for (i = 0; i < file_count; i++) {
		ext = strchr(file_name_array[i], '.');
		if (ext != NULL) {
			result = mime_type_get_mime_type(ext + 1, &mime_type);
			if (result != MIME_TYPE_ERROR_NONE)
				BT_ERR("mime_type_get_mime_type failed %d", result);
		}

		__data_list_ptr_array_add(ptr_array, file_name_array[i],
								mime_type);

		if (mime_type) {
			free(mime_type);
			mime_type = NULL;
		}
	}

	proxy_call = dbus_g_proxy_begin_call(client_proxy, "SendFiles",
			__bt_send_files_cb, NULL, NULL,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
					    G_TYPE_VALUE), hash,
			dbus_g_type_get_collection("GPtrArray",
					DBUS_STRUCT_STRING_STRING), ptr_array,
			DBUS_TYPE_G_OBJECT_PATH, agent_path,
			G_TYPE_INVALID);

	g_free(agent_path);

	if (proxy_call == NULL) {
			BT_ERR("Fail to Send files");
			g_hash_table_destroy(hash);
			g_object_unref(client_proxy);
			__bt_free_sending_info(sending_info);
			__bt_opp_client_agent_deinit();
			sending_info = NULL;
			return BLUETOOTH_ERROR_INTERNAL;
	}

	sending_info->sending_proxy = proxy_call;
	g_hash_table_destroy(hash);

	if (ptr_array)
		g_ptr_array_free(ptr_array, TRUE);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_push_files(int request_id, DBusGMethodInvocation *context,
				bluetooth_device_address_t *remote_address,
				char **file_path, int file_count,
				GArray **out_param)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_sending_data_t *data;
	int result = BLUETOOTH_ERROR_NONE;
	int i;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_PARAMETER(file_path, return);

	/* Implement the queue */
	_bt_convert_addr_type_to_string(address, remote_address->addr);

	if (sending_info == NULL) {
		result = __bt_opp_client_start_sending(request_id,
						address, file_path, file_count);
		if (result != BLUETOOTH_ERROR_NONE)
			return result;
	} else {
		/* Insert data in the queue */
		data = g_malloc0(sizeof(bt_sending_data_t));
		data->file_path = g_new0(char *, file_count + 1);
		data->address = g_strdup(address);
		data->file_count = file_count;
		data->request_id = request_id;

		for (i = 0; i < file_count; i++) {
			data->file_path[i] = g_strdup(file_path[i]);
			DBG_SECURE("file[%d]: %s", i, data->file_path[i]);
		}

		transfer_list = g_slist_append(transfer_list, data);
	}

	g_array_append_vals(*out_param, &request_id,
				sizeof(int));

	return result;
}

int _bt_opp_client_cancel_push(void)
{
	DBusGConnection *g_conn;
	DBusGProxy *client_proxy;

	retv_if(sending_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	sending_info->is_canceled = TRUE;

	if (sending_info->transfer_info) {
		dbus_g_proxy_call_no_reply(sending_info->transfer_info->proxy,
					"Cancel", G_TYPE_INVALID);
	} else {
		retv_if(sending_info->sending_proxy == NULL,
					BLUETOOTH_ERROR_INTERNAL);

		g_conn = _bt_get_session_gconn();
		retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

		client_proxy =	dbus_g_proxy_new_for_name(g_conn, BT_OBEX_SERVICE_NAME,
						"/", BT_OBEX_CLIENT_INTERFACE);

		retv_if(client_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		dbus_g_proxy_cancel_call(client_proxy,
					sending_info->sending_proxy);

		g_idle_add(__bt_cancel_push_cb, NULL);
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_cancel_all_transfers(void)
{
	if (transfer_list) {
		g_slist_free_full(transfer_list,
			(GDestroyNotify)__bt_free_sending_data);

		transfer_list = NULL;
	}

	_bt_opp_client_cancel_push();

	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_is_sending(gboolean *sending)
{
	BT_CHECK_PARAMETER(sending, return);

	*sending = sending_info ? TRUE : FALSE;

	return BLUETOOTH_ERROR_NONE;
}
