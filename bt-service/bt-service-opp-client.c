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

#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <mime_type.h>

#include <glib.h>
#include <gio/gio.h>

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

#define DBUS_TIEMOUT 20 * 1000  /* 20 Seconds */
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

static gboolean __bt_cancel_push_cb(gpointer data)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;
	GVariant *param = NULL;
	retv_if(sending_info == NULL, FALSE);
	sending_info->result = result;

	param = g_variant_new("(isi)", result,
				sending_info->address,
				sending_info->request_id);
	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			param);
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
	GVariant *param = NULL;
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
	param = g_variant_new("(istii)", result,
				sending_info->transfer_info->file_name,
				sending_info->transfer_info->size,
				percentage_progress,
				sending_info->request_id);
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,
			param);
	BT_DBG("-");

	return TRUE;
}

gboolean _bt_obex_client_completed(const char *transfer_path, gboolean success)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	GVariant *param = NULL;
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
			param = g_variant_new("(isti)", result,
						sending_info->transfer_info->file_name,
						sending_info->transfer_info->size,
						sending_info->request_id);
			_bt_send_event(BT_OPP_CLIENT_EVENT,
					BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
					param);
		   __bt_free_transfer_info(sending_info->transfer_info);
		   sending_info->transfer_info = NULL;
		   /* Reset the file offset as we will cancelled remaining files also */
		   file_offset = 0;
		}
		param = g_variant_new("(isi)", sending_info->result,
					sending_info->address,
					sending_info->request_id);
		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_DISCONNECTED,
				param);
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
		param = g_variant_new("(isti)", result,
					sending_info->transfer_info->file_name,
					sending_info->transfer_info->size,
					sending_info->request_id);
		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				param);
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
	GError *error = NULL;
	GVariant *param = NULL;
	GDBusConnection *g_conn;
	GDBusProxy *properties_proxy;
	GDBusProxy *transfer_proxy;

	if (sending_info == NULL || sending_info->is_canceled == TRUE) {
		result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		goto canceled;
	}

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, FALSE);
	properties_proxy = g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL,BT_OBEXD_DBUS_NAME,
						transfer_path, BT_PROPERTIES_INTERFACE,
						NULL, &error);

	retv_if(properties_proxy == NULL, FALSE);

	sending_info->transfer_info->properties_proxy = properties_proxy;

	transfer_proxy = g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_OBEXD_DBUS_NAME,
						transfer_path, BT_OBEX_TRANSFER_INTERFACE,
						NULL, &error);

	retv_if(transfer_proxy == NULL, FALSE);

	sending_info->transfer_info->proxy = transfer_proxy;

	sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_STARTED;
	sending_info->result = result;
	param = g_variant_new("(isti)", result,
				sending_info->transfer_info->file_name,
				sending_info->transfer_info->size,
				sending_info->request_id);
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,
			param);

	BT_DBG("-");
	return TRUE;
canceled:
	error = g_error_new(__bt_opc_error_quark(), BT_OBEX_AGENT_ERROR_CANCEL,
			"CancelledByUser");

	g_error_free(error);

	BT_DBG("-");
	return FALSE;
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

static void __bt_sending_release_cb(GDBusProxy *proxy,
				GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	ret_if(sending_info == NULL);

	GError *error = NULL;
	int result = BLUETOOTH_ERROR_NONE;
	GVariant *param = NULL;
	g_dbus_proxy_call_finish(proxy, res, &error);
	if (proxy)
		g_object_unref(proxy);

	if (error) {
		BT_ERR("%s", error->message);
		g_error_free(error);

		result = BLUETOOTH_ERROR_INTERNAL;
	} else {
		file_offset = 0;
		BT_DBG("Session Removed");
	}

	sending_info->result = result;
	param = g_variant_new("(isi)", sending_info->result,
				sending_info->address,
				sending_info->request_id);
	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			param);

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
	GDBusConnection *g_conn;
	GDBusProxy *session_proxy;
	GError *err = NULL;

	g_conn = _bt_get_session_gconn();
	ret_if(g_conn == NULL);
	ret_if(sending_info->session_path == NULL);

	session_proxy = g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_OBEXD_DBUS_NAME,
						BT_OBEX_CLIENT_PATH,
						BT_OBEX_CLIENT_INTERFACE,
						NULL, &err);

	ret_if(session_proxy == NULL);

	g_dbus_proxy_call(session_proxy, "RemoveSession",
		g_variant_new("(o)", sending_info->session_path),
		G_DBUS_CALL_FLAGS_NONE,
		DBUS_TIEMOUT, NULL,
		(GAsyncReadyCallback)__bt_sending_release_cb,
		NULL);

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
	GVariant *param = NULL;
	ret_if(sending_info == NULL);

	if (g_strcmp0(sending_info->session_path,
			session_path) != 0) {
		BT_INFO("Path mismatch, previous transfer failed! Returning");
		return;
	}

	if (sending_info->transfer_info) {
		if (sending_info->transfer_info->transfer_status == BT_TRANSFER_STATUS_PROGRESS ||
				sending_info->transfer_info->transfer_status == BT_TRANSFER_STATUS_STARTED) {
			BT_INFO("Abnormal termination");
			param = g_variant_new("(isti)", sending_info->result,
						sending_info->transfer_info->file_name,
						sending_info->transfer_info->size,
						sending_info->request_id);
			_bt_send_event(BT_OPP_CLIENT_EVENT,
					BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
					param);
			__bt_free_transfer_info(sending_info->transfer_info);
		}
	}
	param = g_variant_new("(isi)", sending_info->result,
				sending_info->address,
				sending_info->request_id);
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_DISCONNECTED,
			param);

	__bt_free_sending_info(sending_info);
	sending_info = NULL;

	BT_DBG("-");
}

static void __bt_send_file_cb(GDBusProxy *proxy,
				GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GVariant *value = NULL;
	GError *error = NULL;
	char *session_path = NULL;
	const char *transfer_name = NULL;
	const char *file_name = NULL;
	int size = 0;
	GVariantIter *iter = NULL;
	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (error) {
		BT_ERR("%s", error->message);
		g_error_free(error);
		if (proxy)
			g_object_unref(proxy);
		return;
	}
	if (proxy)
		g_object_unref(proxy);

	if (value) {
		g_variant_get(value, "(oa{sv})", &session_path, &iter);
		g_variant_unref(value);
	}

	__bt_free_transfer_info(sending_info->transfer_info);

	sending_info->transfer_info = g_malloc0(sizeof(bt_transfer_info_t));

	if (iter) {
		const gchar *key;
		GVariant *val;
		gsize len = 0;
		while (g_variant_iter_loop(iter, "{sv}", &key, &val)) {
			if (g_strcmp0(key, "Name") == 0) {
				transfer_name = g_variant_dup_string(val,&len);
			} else if (g_strcmp0(key, "Filename") == 0) {
				file_name = g_variant_dup_string(val, &len);
			} else if (g_strcmp0(key, "Size") == 0) {
				size = g_variant_get_uint64(val);
			}
		}
		g_variant_iter_free(iter);
	}

	sending_info->transfer_info->transfer_name = g_strdup(transfer_name);
	sending_info->transfer_info->file_name = g_strdup(file_name);
	sending_info->transfer_info->size = size;
	sending_info->transfer_info->transfer_path = session_path;
	sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_QUEUED;
	sending_info->result = BLUETOOTH_ERROR_NONE;
	file_offset++;

}

void _bt_sending_files(void)
{
	BT_DBG("+");

	GError *err = NULL;
	GDBusConnection *g_conn;
	GDBusProxy *client_proxy;
	char *mimetype = NULL;
	char *ext = NULL;

	if (sending_info == NULL)
		return;
	if (file_offset < sending_info->file_count){
		/* Get the session bus. */
		g_conn = _bt_get_session_gconn();
		ret_if(g_conn == NULL);

		client_proxy = g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_OBEXD_DBUS_NAME,
						sending_info->session_path,
						BT_OBEX_OBJECT_PUSH_INTERFACE,
						NULL, &err);
		ret_if(client_proxy == NULL);

		BT_DBG("Calling SendFile");
		ext = strrchr(sending_info->file_name_array[file_offset], '.');

		if (ext != NULL && (!strcmp(ext, ".imy")))
			mimetype = g_strdup("audio/imelody");
		g_dbus_proxy_call(client_proxy, "SendFile",
				g_variant_new("(ss)", sending_info->file_name_array[file_offset],
								mimetype),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIEMOUT, NULL,
				(GAsyncReadyCallback)__bt_send_file_cb,
				sending_info);
		if (err != NULL) {
			BT_ERR("Calling SendFile failed: [%s]\n", err->message);
			g_clear_error(&err);
			g_free(mimetype);
			return;
		}

		g_free(mimetype);
	}else{
		file_offset = 0;
		__bt_sending_release();
	}

	BT_DBG("-");
}

static void __bt_create_session_cb(GDBusProxy *proxy,
				GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");

	GError *error = NULL;
	GVariant *value;
	int result = BLUETOOTH_ERROR_NONE;
	char *session_path = NULL;
	GVariant *param = NULL;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (proxy)
		g_object_unref(proxy);

	if (value) {
		g_variant_get(value, "(o)", &session_path);
		g_variant_unref(value);
	}
	if (error) {

		BT_ERR("%s", error->message);
		g_clear_error(&error);

		result = BLUETOOTH_ERROR_INTERNAL;
	}else{
		BT_DBG("Session created");
		if(sending_info != NULL)
			sending_info->session_path = g_strdup(session_path);
	}
	g_free(session_path);
	ret_if(sending_info == NULL);

	sending_info->result = result;
	param = g_variant_new("(isi)", result,
				sending_info->address,
				sending_info->request_id);
	/* Send the event in only error none case */
	_bt_send_event(BT_OPP_CLIENT_EVENT,
			BLUETOOTH_EVENT_OPC_CONNECTED,
			param);

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
	GVariantBuilder *builder;
	int i;
	GDBusConnection *g_conn;
	GDBusProxy *client_proxy;
	GError *error = NULL;
	BT_DBG("+");

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(file_name_array, return);

	/* Get the session bus. */
	g_conn = _bt_get_session_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	client_proxy =  g_dbus_proxy_new_sync(g_conn, G_DBUS_PROXY_FLAGS_NONE,
					NULL, BT_OBEX_SERVICE_NAME,
					BT_OBEX_CLIENT_PATH,
					BT_OBEX_CLIENT_INTERFACE,
					NULL, &error);

	if (error) {
		BT_ERR("Unable to create client proxy: %s", error->message);
		g_clear_error(&error);
	}

	retv_if(client_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(
				G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(builder, "{sv}", "Target",
		g_variant_new_string("OPP"));

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

	g_dbus_proxy_call(client_proxy, "CreateSession",
						g_variant_new("(sa{sv})", address, builder),
						G_DBUS_CALL_FLAGS_NONE,
						DBUS_TIEMOUT, NULL,
						(GAsyncReadyCallback)__bt_create_session_cb,
						NULL);
	g_variant_builder_unref(builder);

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_opp_client_push_files(int request_id, GDBusMethodInvocation *context,
				bluetooth_device_address_t *remote_address,
				char **file_path, int file_count)
{
	BT_DBG("+");
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	bt_sending_data_t *data;

	GVariant *out_param1 = NULL;

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
		/* Fix : NULL_RETURNS */
		if (data == NULL)
			return BLUETOOTH_ERROR_MEMORY_ALLOCATION;

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

	out_param1 = g_variant_new_from_data((const GVariantType *)"ay",
							&request_id, sizeof(int),
							TRUE, NULL, NULL);


	g_dbus_method_invocation_return_value(context,
			g_variant_new("(iv)", result, out_param1));

	BT_DBG("-");

	return result;
}

int _bt_opp_client_cancel_push(void)
{
	BT_DBG("+");

	GError *err = NULL;
	int result = BLUETOOTH_ERROR_CANCEL_BY_USER;
	GVariant *param = NULL;
	retv_if(sending_info == NULL, BLUETOOTH_ERROR_NOT_IN_OPERATION);

	sending_info->is_canceled = TRUE;
	sending_info->result = result;

	if (sending_info->transfer_info) {

		g_dbus_proxy_call_sync(sending_info->transfer_info->proxy,
					"Cancel", NULL,
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &err);
		param = g_variant_new("(isti)", result,
					sending_info->transfer_info->file_name,
					sending_info->transfer_info->size,
					sending_info->request_id);
		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
				param);

		if (result == BLUETOOTH_ERROR_CANCEL_BY_USER) {
			BT_ERR("result is not BLUETOOTH_ERROR_NONE");
			__bt_sending_release();
			file_offset = 0;
		}

	} else {
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

void _bt_opp_client_check_pending_transfer(const char *address)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_CANCEL;
	GVariant *param = NULL;
	ret_if(sending_info == NULL);
	ret_if(sending_info->transfer_info == NULL);

	if (g_strcmp0(sending_info->address, address) == 0) {
		BT_INFO("Address Match.Cancel current transfer");
		sending_info->transfer_info->transfer_status = BT_TRANSFER_STATUS_COMPLETED;
		sending_info->result = result;

		if (!sending_info->is_canceled) {
			param = g_variant_new("(isti)", result,
						sending_info->transfer_info->file_name,
						sending_info->transfer_info->size,
						sending_info->request_id);
			_bt_send_event(BT_OPP_CLIENT_EVENT,
					BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,
					param);
			__bt_free_transfer_info(sending_info->transfer_info);
			sending_info->transfer_info = NULL;
			/* Reset the file offset as we will cancelled remaining files also */
			file_offset = 0;
		}
		param = g_variant_new("(isi)", sending_info->result,
					sending_info->address,
					sending_info->request_id);
		_bt_send_event(BT_OPP_CLIENT_EVENT,
				BLUETOOTH_EVENT_OPC_DISCONNECTED,
				param);

		__bt_sending_release();
	}
	BT_DBG("-");
}
