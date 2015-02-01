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

static GSList *transfer_list = NULL;

bt_sending_info_t *sending_info;
static int file_offset = 0;

static gboolean __bt_sending_release();
static void _bt_remove_session();

static int __bt_opp_client_start_sending(int request_id, char *address,
					char **file_name_array, int file_count);

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

	if (info->properties_proxy)
		g_object_unref(info->properties_proxy);


	g_free(info->transfer_name);
	g_free(info->file_name);
	g_free(info);
}

static void __bt_free_sending_info(bt_sending_info_t *info)
{
	ret_if(info == NULL);

	/* Free the sending variable */
	__bt_free_transfer_info(info->transfer_info);

	g_free(info->file_name_array);

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
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;

	retv_if(sending_info == NULL, FALSE);
	sending_info->result = result;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	_bt_opp_client_event_deinit();

	BT_DBG("Length of transfer list is %d", g_slist_length(transfer_list));

	 /*Operate remain works*/
	if (g_slist_length(transfer_list) > 0) {
		bt_sending_data_t *node = NULL;

		node = transfer_list->data;
		if (node == NULL) {
			BT_ERR("data is NULL");
			return FALSE;
		}

		transfer_list = g_slist_remove(transfer_list, node);

		if (__bt_opp_client_start_sending(node->request_id,
				node->address, node->file_path,
				node->file_count) != BLUETOOTH_ERROR_NONE) {
			BT_ERR("Fail to start sending");
		}
	}
	BT_DBG("-");
	return FALSE;
}

gboolean _bt_obex_client_progress(const char *transfer_path, int transferred)
{
	BT_DBG("+");

	int percentage_progress;
	gint64 size;
	int result = BLUETOOTH_ERROR_NONE;

	retv_if(sending_info == NULL, TRUE);
	retv_if(sending_info->transfer_info == NULL, TRUE);

	if (g_strcmp0(sending_info->transfer_info->transfer_path,
			transfer_path) != 0) {
		BT_INFO("Path mismatch, previous transfer failed! Returning");
		return FALSE;
	}

	size = sending_info->transfer_info->size;

	if (size != 0)
		percentage_progress = (int)(((gdouble)transferred /
				(gdouble)size) * 100);
	else
		percentage_progress = 0;

	sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_PROGRESS;
	sending_info->result = result;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &percentage_progress,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	BT_DBG("-");

	return TRUE;
}

gboolean _bt_obex_client_completed(const char *transfer_path, gboolean success)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;

	retv_if(sending_info == NULL, TRUE);
	retv_if(sending_info->transfer_info == NULL, TRUE);

	if (g_strcmp0(sending_info->transfer_info->transfer_path,
			transfer_path) != 0) {
		BT_INFO("Path mismatch, previous transfer failed! Returning");
		return FALSE;
	}

	result = (success == TRUE) ? BLUETOOTH_ERROR_NONE : BLUETOOTH_ERROR_CANCEL;

	sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_COMPLETED;
	sending_info->result = result;

	if (!success) { /*In case of remote device reject, we need to send BLUETOOTH_EVENT_OPC_DISCONNECTED */
		BT_DBG("completed with error");
		if (!sending_info->is_canceled) {
			_bt_send_event(BT_OPP_CLIENT_EVENT,
					BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
					DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
					DBUS_TYPE_INT32, &sending_info->request_id,
					DBUS_TYPE_INVALID);

		   __bt_free_transfer_info(sending_info->transfer_info);
		   sending_info->transfer_info = NULL;
		   /* Reset the file offset as we will cancelled remaining files also */
		   file_offset = 0;
		}

		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_DISCONNECTED,
				DBUS_TYPE_INT32, &sending_info->result,
				DBUS_TYPE_STRING, &sending_info->address,
				DBUS_TYPE_INT32, &sending_info->request_id,
				DBUS_TYPE_INVALID);

		__bt_sending_release();
		/* Sending info should not freed after sending_release it's
		 * already freed in that API and if any pending request is
		 * present then it recreate sending_info again.
		 * And if we free it here then CreateSession method call will
		 * made but RemoveSession method call will not done.
		 */
	} else {
		BT_DBG("complete success");
		/* Send the event in only error none case */
		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
				DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
				DBUS_TYPE_INT32, &sending_info->request_id,
				DBUS_TYPE_INVALID);

	   __bt_free_transfer_info(sending_info->transfer_info);
	   sending_info->transfer_info = NULL;
	}

	BT_DBG("-");

	return TRUE;
}

gboolean _bt_obex_client_started(const char *transfer_path)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	GError *error;
	DBusGConnection *g_conn;
	DBusGProxy *properties_proxy;
	DBusGProxy *transfer_proxy;

	if (sending_info == NULL || sending_info->is_canceled == TRUE) {
		result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		goto canceled;
	}

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, FALSE);

	properties_proxy = dbus_g_proxy_new_for_name(g_conn, BT_OBEXD_DBUS_NAME,
			transfer_path, BT_PROPERTIES_INTERFACE);

	retv_if(properties_proxy == NULL, FALSE);

	sending_info->transfer_info->properties_proxy = properties_proxy;

	transfer_proxy = dbus_g_proxy_new_for_name(g_conn, BT_OBEXD_DBUS_NAME,
			transfer_path, BT_OBEX_TRANSFER_INTERFACE);

	retv_if(transfer_proxy == NULL, FALSE);

	sending_info->transfer_info->proxy = transfer_proxy;

	sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_STARTED;
	sending_info->result = result;

	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
			DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	BT_DBG("-");
	return TRUE;
canceled:
	error = g_error_new(__bt_opc_error_quark(), BT_OBEX_AGENT_ERROR_CANCEL,
			"CancelledByUser");

	g_error_free(error);

	BT_DBG("-");
	return FALSE;
fail:
	result = BLUETOOTH_ERROR_INTERNAL;
	sending_info->result = result;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	_bt_opp_client_event_deinit();

	BT_DBG("-");

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

static void __bt_sending_release_cb(DBusGProxy *proxy, DBusGProxyCall *call,
					void *user_data)
{
	BT_DBG("+");
	ret_if(sending_info == NULL);

	GError *error = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	if (dbus_g_proxy_end_call(proxy, call, &error,
						G_TYPE_INVALID) == FALSE) {
		BT_ERR("%s", error->message);
		g_error_free(error);

		result = BLUETOOTH_ERROR_INTERNAL;
	} else {
		BT_DBG("Session Removed");
	}

	sending_info->result = result;
	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			DBUS_TYPE_INT32, &sending_info->result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	_bt_opp_client_event_deinit();

	/* Operate remain works */
	if (g_slist_length(transfer_list) > 0) {
		bt_sending_data_t *data = NULL;

		data = transfer_list->data;
		if (data == NULL)
			goto fail;

		transfer_list = g_slist_remove(transfer_list, data);

		BT_DBG("calling __bt_opp_client_start_sending");

		if (__bt_opp_client_start_sending(data->request_id,
				data->address, data->file_path,
				data->file_count) != BLUETOOTH_ERROR_NONE) {
			goto fail;
		}
	}

	return;
fail:
	g_slist_free_full(transfer_list,
				(GDestroyNotify)__bt_free_sending_data);
	transfer_list = NULL;

	BT_DBG("-");

	return;
}

static void _bt_remove_session()
{
	DBusGConnection *g_conn;
	DBusGProxy *session_proxy;
	DBusGProxyCall *proxy_call;

	g_conn = _bt_get_session_gconn();
	ret_if(g_conn == NULL);

	session_proxy =  dbus_g_proxy_new_for_name(g_conn, BT_OBEXD_DBUS_NAME,
						BT_OBEX_CLIENT_PATH,
						BT_OBEX_CLIENT_INTERFACE);

	ret_if(session_proxy == NULL);

	proxy_call = dbus_g_proxy_begin_call(session_proxy, "RemoveSession",
		__bt_sending_release_cb, NULL, NULL,
		DBUS_TYPE_G_OBJECT_PATH, sending_info->session_path,
		G_TYPE_INVALID);
	if (proxy_call == NULL) {
		BT_ERR("Fail to Remove session");
		g_object_unref(session_proxy);
	}

}

static gboolean __bt_sending_release()
{
	BT_DBG("+");

	retv_if(sending_info == NULL, FALSE);

	_bt_remove_session();
	BT_DBG("-");
	return TRUE;
}

void _bt_opc_disconnected(const char *session_path)
{
	BT_DBG("+");

	ret_if(sending_info == NULL);

	if (g_strcmp0(sending_info->session_path,
			session_path) != 0) {
		BT_INFO("Path mismatch, previous transfer failed! Returning");
		return;
	}

	if (sending_info->transfer_info) {
		BT_INFO("sending_info is not NULL");
		if (sending_info->transfer_info->transfer_status == BT_TRANSFER_STATUS_PROGRESS ||
				sending_info->transfer_info->transfer_status == BT_TRANSFER_STATUS_STARTED) {
			BT_INFO("Abnormal termination");

			_bt_send_event(BT_OPP_CLIENT_EVENT,
					BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
					DBUS_TYPE_INT32, &sending_info->result,
					DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
					DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
					DBUS_TYPE_INT32, &sending_info->request_id,
					DBUS_TYPE_INVALID);
			__bt_free_transfer_info(sending_info->transfer_info);
		}
	}

	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			DBUS_TYPE_INT32, &sending_info->result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);


	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	BT_DBG("-");
}

void _bt_sending_files(void)
{
	BT_DBG("+");

	DBusGConnection *g_conn;
	DBusGProxy *client_proxy;
	GError *err = NULL;
	char *path = NULL;
	GHashTable *hash = NULL;
	GValue *value = NULL;
	const char *transfer_name;
	const char *file_name;
	const char *status;
	int size;
	char *mimetype = NULL;
	char *ext = NULL;


	if (sending_info == NULL)
		return;

	if (file_offset < sending_info->file_count){
		/* Get the session bus. */
		g_conn = _bt_get_session_gconn();
		ret_if(g_conn == NULL);

		client_proxy =  dbus_g_proxy_new_for_name(g_conn,
						BT_OBEXD_DBUS_NAME,
						sending_info->session_path,
						BT_OBEX_OBJECT_PUSH_INTERFACE);

		ret_if(client_proxy == NULL);

		BT_DBG("Calling SendFile");
		ext = strrchr(sending_info->file_name_array[file_offset], '.');

		if(!strcmp(ext+1, "imy"))
			mimetype = g_strdup("audio/imelody");

		if (!dbus_g_proxy_call(client_proxy, "SendFile", &err,
				G_TYPE_STRING,
				sending_info->file_name_array[file_offset],
				G_TYPE_STRING, mimetype,
				G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH, &path,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID)) {
			if (err != NULL) {
				BT_ERR("Calling SendFile failed: [%s]\n", err->message);
				g_error_free(err);
			}
			g_free(mimetype);
			return;
		}

		g_free(mimetype);

		if (hash == NULL)
			return;

		__bt_free_transfer_info(sending_info->transfer_info);

		sending_info->transfer_info = g_malloc0(sizeof(bt_transfer_info_t));

		value = g_hash_table_lookup(hash, "Name");
		transfer_name = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Filename");
		file_name = value ? g_value_get_string(value) : NULL;

		value = g_hash_table_lookup(hash, "Size");
		size = value ? g_value_get_uint64(value) : 0;

		sending_info->transfer_info->transfer_name = g_strdup(transfer_name);
		sending_info->transfer_info->file_name = g_strdup(file_name);
		sending_info->transfer_info->size = size;
		sending_info->transfer_info->transfer_path = path;
		sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_QUEUED;
		sending_info->result = BLUETOOTH_ERROR_NONE;

		g_hash_table_destroy(hash);

		file_offset++;
	}else{
		file_offset = 0;
		__bt_sending_release();
	}

	BT_DBG("-");
}

static void __bt_create_session_cb(DBusGProxy *proxy, DBusGProxyCall *call,
					void *user_data)
{
	BT_DBG("+");

	GError *error = NULL;
	int result = BLUETOOTH_ERROR_NONE;
	char *session_path = NULL;

	if (dbus_g_proxy_end_call(proxy, call, &error,
		DBUS_TYPE_G_OBJECT_PATH, &session_path, G_TYPE_INVALID) == FALSE) {

		BT_ERR("%s", error->message);
		g_error_free(error);

		result = BLUETOOTH_ERROR_INTERNAL;
	}else{
		BT_DBG("Session created");
		if(sending_info != NULL)
			sending_info->session_path = g_strdup(session_path);
}
	g_free(session_path);
	g_object_unref(proxy);
	ret_if(sending_info == NULL);

	sending_info->sending_proxy = NULL;
	sending_info->result = result;

	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &sending_info->address,
			DBUS_TYPE_INT32, &sending_info->request_id,
			DBUS_TYPE_INVALID);

	if (result != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Calling __bt_sending_release");
		__bt_sending_release();

		__bt_free_sending_info(sending_info);
		sending_info = NULL;
	}else {
		BT_DBG("Calling sending_files");
		_bt_sending_files();
	}
	BT_DBG("-");

}

static int __bt_opp_client_start_sending(int request_id, char *address,
					char **file_name_array, int file_count)
{
	BT_DBG("+");

	GHashTable *hash;
	GValue *value;
	DBusGConnection *g_conn;
	DBusGProxy *client_proxy;
	DBusGProxyCall *proxy_call;

	int i;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(file_name_array, return);

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	client_proxy =  dbus_g_proxy_new_for_name(g_conn, BT_OBEX_SERVICE_NAME,
					BT_OBEX_CLIENT_PATH, BT_OBEX_CLIENT_INTERFACE);

	retv_if(client_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("client_proxy is not NULL");

	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
				     NULL, (GDestroyNotify)__bt_value_free);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_STRING);

	g_value_set_string(value, "OPP");
	g_hash_table_insert(hash, "Target", value);
	BT_DBG("Hash Table success");

	__bt_free_sending_info(sending_info);

	sending_info = g_malloc0(sizeof(bt_sending_info_t));
	sending_info->address = g_strdup(address);
	sending_info->request_id = request_id;

	sending_info->file_count = file_count;
	sending_info->file_offset = 0;
	sending_info->file_name_array = g_new0(char *, file_count + 1);

	for (i = 0; i < file_count; i++) {
		sending_info->file_name_array[i] = g_strdup(file_name_array[i]);
		BT_DBG("file[%d]: %s", i, sending_info->file_name_array[i]);
	}

	_bt_opp_client_event_deinit();
	_bt_opp_client_event_init();
	//_bt_obex_client_started(agent_path);

	BT_DBG("Going to call CreateSession");

	proxy_call = dbus_g_proxy_begin_call(client_proxy, "CreateSession",
			__bt_create_session_cb, NULL, NULL,
			G_TYPE_STRING, address,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			hash, G_TYPE_INVALID);

	if (proxy_call == NULL) {
			BT_ERR("Fail to Send files");
			g_hash_table_destroy(hash);
			g_object_unref(client_proxy);
			__bt_free_sending_info(sending_info);
			_bt_opp_client_event_deinit();
			sending_info = NULL;
			return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_DBG("After CreateSession");

	sending_info->sending_proxy = proxy_call;
	g_hash_table_destroy(hash);

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}


int _bt_opp_client_push_files(int request_id, DBusGMethodInvocation *context,
				bluetooth_device_address_t *remote_address,
				char **file_path, int file_count)
{
	BT_DBG("+");
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_sending_data_t *data;

	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;

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

	out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));
	out_param2 = g_array_new(FALSE, FALSE, sizeof(gchar));

	g_array_append_vals(out_param1, &request_id,
				sizeof(int));
	g_array_append_vals(out_param2, &result, sizeof(int));

	dbus_g_method_return(context, out_param1, out_param2);

	g_array_free(out_param1, TRUE);
	g_array_free(out_param2, TRUE);

	BT_DBG("-");

	return result;
}

int _bt_opp_client_cancel_push(void)
{
	BT_DBG("+");

	DBusGConnection *g_conn;
	DBusGProxy *client_proxy;
	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;

	retv_if(sending_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	sending_info->is_canceled = TRUE;
	sending_info->result = result;

	if (sending_info->transfer_info) {
		BT_DBG("calling cancel in Bluez");
		dbus_g_proxy_call_no_reply(sending_info->transfer_info->proxy,
					"Cancel", G_TYPE_INVALID,
					G_TYPE_INVALID);

		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &sending_info->transfer_info->file_name,
				DBUS_TYPE_UINT64, &sending_info->transfer_info->size,
				DBUS_TYPE_INT32, &sending_info->request_id,
				DBUS_TYPE_INVALID);

		if (result == BLUETOOTH_ERROR_CANCEL_BY_USER) {
			BT_ERR("result is not BLUETOOTH_ERROR_NONE");
			__bt_sending_release();
			file_offset = 0;
		}
	} else {
		retv_if(sending_info->sending_proxy == NULL,
					BLUETOOTH_ERROR_INTERNAL);

		g_conn = _bt_get_session_gconn();
		retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

		client_proxy =	dbus_g_proxy_new_for_name(g_conn, BT_OBEX_SERVICE_NAME,
						BT_OBEX_CLIENT_PATH, BT_OBEX_CLIENT_INTERFACE);
		retv_if(client_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		dbus_g_proxy_cancel_call(client_proxy,
					sending_info->sending_proxy);

		g_idle_add(__bt_cancel_push_cb, NULL);
	}

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_cancel_all_transfers(void)
{
	BT_DBG("+");
	if (transfer_list) {
		g_slist_free_full(transfer_list,
			(GDestroyNotify)__bt_free_sending_data);

		transfer_list = NULL;
	}

	_bt_opp_client_cancel_push();
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_is_sending(gboolean *sending)
{
	BT_CHECK_PARAMETER(sending, return);

	*sending = sending_info ? TRUE : FALSE;

	return BLUETOOTH_ERROR_NONE;
}
