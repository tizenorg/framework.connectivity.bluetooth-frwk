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

#include <string.h>
#include "bluetooth-api.h"
#include "bt-common.h"
#include "bt-internal-types.h"


#define BLUEZ_CHAR_INTERFACE "org.bluez.Characteristic"

#define GATT_OBJECT_PATH  "/org/bluez/gatt_attrib"

typedef struct {
	char *char_uuid;
	char **handle;
} char_pty_req_t;

#include "bt-gatt-glue.h"

OrgBluezWatcher *bluetooth_gatt_obj;
GDBusConnection *gdbus_conn;
int owner_id;

static GArray *__bt_variant_to_garray(GVariant *variant)
{
	gchar element;
	guint size = 0;
	GArray *out_param;
	GVariantIter *iter = NULL;

	retv_if(variant == NULL, NULL);

	g_variant_get(variant, "ay", &iter);

	retv_if(iter == NULL, NULL);

	size = g_variant_iter_n_children(iter);
	retv_if(size <= 0, NULL);

	out_param = g_array_new(FALSE, FALSE, sizeof(gchar));

	while (g_variant_iter_loop(iter, "y", &element)) {
		g_array_append_vals(out_param, &element, sizeof(gchar));
	}

	g_variant_iter_free(iter);

	return out_param;
}

static char **__get_string_array_from_gptr_array(GPtrArray *gp)
{
	gchar *gp_path = NULL;
	char **path = NULL;
	int i;

	if (gp->len == 0)
		return NULL;

	path = g_malloc0((gp->len + 1) * sizeof(char *));

	for (i = 0; i < gp->len; i++) {
		gp_path = g_ptr_array_index(gp, i);
		path[i] = g_strdup(gp_path);
		BT_DBG("path[%d] : [%s]", i, path[i]);
	}

	return path;
}

static gboolean bluetooth_gatt_value_changed(OrgBluezWatcher *agent,
					GDBusMethodInvocation *context,
					gchar *obj_path,
					GVariant *value,
					gpointer user_data)
{
	bt_gatt_char_value_t char_val;
	bt_user_info_t *user_info;
	GArray *byte_array = NULL;
	BT_DBG("+");

	char_val.char_handle = obj_path;

	retv_if(value == NULL, FALSE);

	byte_array = __bt_variant_to_garray(value);
	retv_if(byte_array == NULL, FALSE);

	char_val.char_handle = obj_path;
	char_val.char_value = &g_array_index(byte_array, guint8, 0);
	char_val.val_len = byte_array->len;
	BT_DBG("Byte array length = %d", char_val.val_len);

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
				BLUETOOTH_ERROR_NONE, &char_val,
				user_info->cb, user_info->user_data);
	}

	BT_DBG("-");

	g_array_free(byte_array, TRUE);

	return TRUE;
}

static void __bt_gatt_gdbus_init(void)
{
	GDBusConnection *g_conn;
	GError *g_err = NULL;

	BT_DBG("+");

	ret_if(bluetooth_gatt_obj != NULL);

	g_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
	ret_if(g_conn == NULL);

	bluetooth_gatt_obj = org_bluez_watcher_skeleton_new();

	g_signal_connect(bluetooth_gatt_obj,
			"handle-value-changed",
			G_CALLBACK(bluetooth_gatt_value_changed),
			NULL);

	if (g_dbus_interface_skeleton_export(
			G_DBUS_INTERFACE_SKELETON(bluetooth_gatt_obj),
			g_conn,
			"/",
			&g_err) == FALSE) {
		if (g_err) {
			BT_ERR("Export error: %s", g_err->message);
			g_clear_error(&g_err);
		}
		g_object_unref(bluetooth_gatt_obj);
		bluetooth_gatt_obj = NULL;
		g_dbus_connection_close_sync(gdbus_conn, NULL, NULL);
		gdbus_conn = NULL;
		return;
	}

	gdbus_conn = g_conn;

	BT_DBG("-");
}

static void __on_bus_acquired(GDBusConnection *connection,
				const gchar *path,
				gpointer user_data)
{
	BT_DBG("bus path : %s", path);

	__bt_gatt_gdbus_init();
}

static void __on_name_acquired(GDBusConnection *connection,
			const gchar *name,
			gpointer user_data)
{
	BT_DBG("name : %s", name);
}

static void __on_name_lost(GDBusConnection *connnection,
			const gchar *name,
			gpointer user_data)
{
	BT_DBG("name : %s", name);
}

static void __add_value_changed_method(GDBusConnection *conn)
{
	int id;

	BT_DBG("+");

	id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
		"org.bluez.Watcher",
		G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
		G_BUS_NAME_OWNER_FLAGS_REPLACE,
		__on_bus_acquired,
		__on_name_acquired,
		__on_name_lost,
		NULL,
		NULL);

	ret_if(id <= 0);

	owner_id = id;

	BT_DBG("-");
}


static void __bluetooth_internal_get_char_cb(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GError *error = NULL;
	GPtrArray *gp_array = NULL;
	bt_gatt_discovered_char_t svc_char = { 0, };
	bt_user_info_t *user_info;
	GDBusConnection *system_gconn = NULL;
	GVariant *return_data;
	GVariant *value;
	GVariantIter *iter;
	gchar *g_str;
	svc_char.service_handle = user_data;
	BT_DBG("+");
	user_info = _bt_get_user_data(BT_COMMON);
	system_gconn = _bt_init_system_gdbus_conn();
	return_data = g_dbus_connection_call_finish(system_gconn, res, &error);
	BT_DBG("result data received..");
	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
		if (user_info) {
			_bt_common_event_cb(
				BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
					BLUETOOTH_ERROR_NONE, &svc_char,
					user_info->cb, user_info->user_data);
		}
		g_free(svc_char.service_handle);
		return;
	}
	gp_array = g_ptr_array_new();
	g_variant_get(return_data, "(a(o))", &iter);
	while ((value = g_variant_iter_next_value(iter))) {
		g_variant_get(value, "(o)", &g_str);
		g_ptr_array_add(gp_array, (gpointer)g_str);
	}

	if (NULL != gp_array) {
		svc_char.handle_info.count = gp_array->len;
		svc_char.handle_info.handle = __get_string_array_from_gptr_array
								(gp_array);
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
				BLUETOOTH_ERROR_NONE, &svc_char,
				user_info->cb, user_info->user_data);
	}

	g_ptr_array_free(gp_array, TRUE);
	g_free(svc_char.service_handle);
	g_free(svc_char.handle_info.handle);
	g_variant_unref(return_data);
	g_variant_unref(value);
	g_variant_iter_free(iter);
	BT_DBG("-");
}

static void __bluetooth_internal_read_cb(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	struct read_resp *rsp = user_data;
	GDBusConnection *system_gconn = NULL;
	GVariant *return_data;

	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_init_system_gdbus_conn();
	return_data = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
	}
	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_CHAR,
				BLUETOOTH_ERROR_NONE, rsp,
				user_info->cb, user_info->user_data);
	}
	return;
}

static void __bluetooth_internal_write_cb(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	GDBusConnection *system_gconn = NULL;
	GVariant *return_data;

	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_init_system_gdbus_conn();
	return_data = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
	}
	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_WRITE_CHAR,
				BLUETOOTH_ERROR_NONE, NULL,
				user_info->cb, user_info->user_data);
	}

	return;
}

static void __free_char_req(char_pty_req_t *char_req)
{
	g_free(char_req->char_uuid);
	g_strfreev(char_req->handle);
	g_free(char_req);
}

static gboolean __filter_chars_with_uuid(gpointer data)
{
	int i = 0;
	bt_gatt_char_property_t *char_pty;
	char_pty_req_t *char_req = data;
	bt_user_info_t *user_info;
	int ret;

	user_info = _bt_get_user_data(BT_COMMON);
	if (user_info == NULL) {
		__free_char_req(char_req);
		return FALSE;
	}

	char_pty = g_new0(bt_gatt_char_property_t, 1);

	while (char_req->handle[i] != NULL) {
		BT_DBG("char_pty[%d] = %s", i, char_req->handle[i]);
		ret = bluetooth_gatt_get_characteristics_property(
						char_req->handle[i],
							char_pty);
		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("get char property failed");
			goto done;
		}

		if (char_pty->uuid && g_strstr_len(char_pty->uuid, -1,
						char_req->char_uuid) != NULL) {
			BT_DBG("Requested Char recieved");
			ret = BLUETOOTH_ERROR_NONE;
			break;
		}

		bluetooth_gatt_free_char_property(char_pty);

		i++;
	}

done:
	if (char_req->handle[i] == NULL)
		ret = BLUETOOTH_ERROR_NOT_FOUND;

	_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID, ret,
				char_pty, user_info->cb, user_info->user_data);

	g_free(char_pty);
	__free_char_req(char_req);

	return FALSE;
}

static void __disc_char_from_uuid_cb(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GError *error = NULL;
	GPtrArray *gp_array = NULL;
	bt_user_info_t *user_info;
	GDBusConnection *system_gconn = NULL;
	GVariant *result;
	GVariantIter iter;
	GVariant *value;
	gchar *g_str;
	char_pty_req_t *char_req = user_data;

	user_info = _bt_get_user_data(BT_COMMON);
	if (!user_info) {
		__free_char_req(char_req);
		return;
	}

	system_gconn = _bt_init_system_gdbus_conn();
	result = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);

		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
					BLUETOOTH_ERROR_INTERNAL, NULL,
					user_info->cb, user_info->user_data);

		__free_char_req(char_req);
		return;
	}
	gp_array = g_ptr_array_new();
	g_variant_iter_init(&iter, result);
	while ((value = g_variant_iter_next_value(&iter))) {
		g_variant_get(value, "(o)", &g_str);
		g_ptr_array_add(gp_array, (gpointer)g_str);
	}

	if (gp_array == NULL) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
					BLUETOOTH_ERROR_NOT_FOUND, NULL,
					user_info->cb, user_info->user_data);

		__free_char_req(char_req);
		return;
	}

	char_req->handle = __get_string_array_from_gptr_array(gp_array);

	__filter_chars_with_uuid(char_req);

	g_ptr_array_free(gp_array, TRUE);
	g_variant_unref(result);
	g_variant_unref(value);
	g_variant_iter_free(&iter);
}

static int __discover_char_from_uuid(const char *service_handle,
					const char *char_uuid){
	GDBusConnection *conn;
	char_pty_req_t *char_req;

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	char_req = g_new0(char_pty_req_t, 1);

	char_req->char_uuid = g_strdup(char_uuid);
	BT_DBG("Char uuid %s ", char_uuid);
	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				service_handle,
				BLUEZ_CHAR_INTERFACE,
				"DiscoverCharacteristics",
				NULL,
				G_VARIANT_TYPE("a(o)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback) __disc_char_from_uuid_cb,
				char_req);
	return BLUETOOTH_ERROR_NONE;
}


BT_EXPORT_API int bluetooth_gatt_free_primary_services(
					bt_gatt_handle_info_t *prim_svc)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(prim_svc, return);

	g_strfreev(prim_svc->handle);

	memset(prim_svc, 0, sizeof(bt_gatt_handle_info_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_free_service_property(
					bt_gatt_service_property_t *svc_pty)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(svc_pty, return);

	g_free(svc_pty->uuid);
	g_free(svc_pty->handle);
	g_strfreev(svc_pty->handle_info.handle);

	memset(svc_pty, 0, sizeof(bt_gatt_service_property_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_free_char_property(
					bt_gatt_char_property_t *char_pty)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(char_pty, return);

	g_free(char_pty->uuid);
	g_free(char_pty->name);
	g_free(char_pty->description);
	g_free(char_pty->val);
	g_free(char_pty->handle);

	memset(char_pty, 0, sizeof(bt_gatt_char_property_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_primary_services(
				const bluetooth_device_address_t *address,
					bt_gatt_handle_info_t *prim_svc)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *device_path = NULL;
	GError *error = NULL;
	GHashTable *hash = NULL;
	GVariant *result;
	char *key;
	GVariant *value;
	GVariantIter iter;
	GVariantIter array_iter;
	GPtrArray *gp_array  = NULL;
	GDBusConnection *conn;
	int ret = BLUETOOTH_ERROR_INTERNAL;
	gchar *g_str;
	char *adapter_path = NULL;
	BT_DBG("+");

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(prim_svc, return);
	BT_CHECK_ENABLED(return);

	_bt_convert_addr_type_to_string(device_address,
					(unsigned char *)address->addr);

	BT_DBG("bluetooth address [%s]\n", device_address);
	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					BT_MANAGER_PATH,
					BT_MANAGER_INTERFACE,
					"DefaultAdapter",
					NULL,
					G_VARIANT_TYPE("(o)"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error) {
		BT_ERR("adapter path error %s[%s]", error->message,
							device_address);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_get(result, "(o)", &adapter_path);
	BT_DBG("adapter path is [%s]\n", adapter_path);
	g_variant_unref(result);
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					adapter_path,
					BT_ADAPTER_INTERFACE,
					"FindDevice",
					g_variant_new("(s)",
						device_address),
					G_VARIANT_TYPE("(o)"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]", error->message,
							 device_address);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_free(adapter_path);
	g_variant_get(result, "(o)", &device_path);
	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					device_path,
					BT_DEVICE_INTERFACE,
					"GetProperties",
					NULL,
					G_VARIANT_TYPE("(a{sv})"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error) {
		BT_ERR("GetProperties Call Error %s[%s]", error->message,
							 device_address);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	hash = g_hash_table_new_full(g_str_hash,
					g_str_equal,
					NULL,
					NULL);

	g_variant_get(result, "(a{sv})", &iter);
	while (g_variant_iter_next(&iter, "{sv}", &key, &value))
		g_hash_table_insert(hash, key, value);

	g_free(device_path);
	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	value = g_hash_table_lookup(hash, "Services");
	if (value == NULL) {
		BT_ERR("value == NULL");
		goto done;
	}
	gp_array = g_ptr_array_new();
	g_variant_iter_init(&array_iter, value);
	while ((value = g_variant_iter_next_value(&array_iter))) {
		g_variant_get(value, "s", &g_str);
		g_ptr_array_add(gp_array, (gpointer)g_str);
	}

	if (gp_array == NULL) {
		BT_ERR("gp_array == NULL");
		goto done;
	}

	prim_svc->count = gp_array->len;
	prim_svc->handle = __get_string_array_from_gptr_array(gp_array);

	g_ptr_array_free(gp_array, TRUE);
	ret = BLUETOOTH_ERROR_NONE;
done:
	g_hash_table_destroy(hash);
	g_variant_unref(result);
	g_variant_iter_free(&iter);
	g_variant_iter_free(&array_iter);

	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_discover_service_characteristics(
						const char *service_handle)
{
	char *handle;
	GDBusConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	handle = g_strdup(service_handle);
	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				service_handle,
				BLUEZ_CHAR_INTERFACE,
				"DiscoverCharacteristics",
				NULL,
				G_VARIANT_TYPE("a(o)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback)
				__bluetooth_internal_get_char_cb,
				handle);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_service_property(
						const char *service_handle,
				       bt_gatt_service_property_t *service)
{
	GHashTable *hash = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	GPtrArray *gp_array  = NULL ;
	GDBusConnection *conn;
	GVariantIter iter;
	GVariantIter  array_iter;
	gchar *g_str;
	char *key;

	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_PARAMETER(service, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	result = g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					service_handle,
					BLUEZ_CHAR_INTERFACE,
					"GetProperties",
					NULL,
					G_VARIANT_TYPE("(a{sv})"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error != NULL) {
		BT_ERR("GetProperties Call Error %s\n", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	hash = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				NULL,
				NULL);
	g_variant_get(result, "(a{sv})", &iter);
	while (g_variant_iter_next(&iter, "{sv}", &key, &value))
		g_hash_table_insert(hash, key, value);

	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	memset(service, 0, sizeof(bt_gatt_service_property_t));

	value = g_hash_table_lookup(hash, "UUID");
	g_variant_get(value, "s", &service->uuid);
	if (service->uuid) {
		BT_DBG("svc_pty.uuid = [%s] \n", service->uuid);
	}

	value = g_hash_table_lookup(hash, "Characteristics");
	gp_array = g_ptr_array_new();
	g_variant_iter_init(&array_iter, value);
	while ((value = g_variant_iter_next_value(&array_iter))) {
		g_variant_get(value, "s", &g_str);
		g_ptr_array_add(gp_array, (gpointer)g_str);
	}

	if (gp_array) {
		service->handle_info.count = gp_array->len;
		service->handle_info.handle =
				__get_string_array_from_gptr_array(gp_array);
	}

	g_ptr_array_free(gp_array, TRUE);
	service->handle = g_strdup(service_handle);
	g_hash_table_destroy(hash);
	g_variant_iter_free(&iter);
	g_variant_iter_free(&array_iter);
	g_variant_unref(result);
	g_variant_unref(value);
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_watch_characteristics(
						const char *service_handle)
{
	GError *error = NULL;
	GDBusConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);

	BT_DBG("Entered service handle:%s \n ", service_handle);
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	__add_value_changed_method(conn);
	BT_DBG("GATT_OBJECT_PATH is %s \n", GATT_OBJECT_PATH);
	g_dbus_connection_call_sync(conn,
				BT_BLUEZ_NAME,
				service_handle,
				BLUEZ_CHAR_INTERFACE,
				"RegisterCharacteristicsWatcher",
				g_variant_new("(o)",
					GATT_OBJECT_PATH),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);
	if (error) {
		BT_ERR("Method call  Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_unwatch_characteristics(
						const char *service_handle)
{
	GError *error = NULL;
	GDBusConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	g_dbus_connection_call_sync(conn,
				BT_BLUEZ_NAME,
				service_handle,
				BLUEZ_CHAR_INTERFACE,
				"UnregisterCharacteristicsWatcher",
				g_variant_new("(o)",
					GATT_OBJECT_PATH),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error) {
		BT_ERR("Method call  Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_characteristics_property(
				const char *char_handle,
				bt_gatt_char_property_t *characteristic)
{
	GHashTable *hash = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	GByteArray *gb_array = NULL;
	GDBusConnection *conn;
	GVariantIter iter;
	GVariantIter array_iter;
	guchar *g_str;
	char *key;

	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(characteristic, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	result = g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					char_handle,
					BLUEZ_CHAR_INTERFACE,
					"GetProperties",
					NULL,
					G_VARIANT_TYPE("(a{sv})"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error != NULL) {
		BT_ERR("GetProperties Call Error %s\n", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	hash = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				NULL,
				NULL);
	g_variant_get(result, "(a{sv})", &iter);
	while (g_variant_iter_next(&iter, "{sv}", &key, &value))
		g_hash_table_insert(hash, key, value);
	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);
	memset(characteristic, 0, sizeof(bt_gatt_char_property_t));
	value = g_hash_table_lookup(hash, "UUID");
	g_variant_get(value, "s", &characteristic->uuid);
	if (characteristic->uuid) {
		BT_DBG("characteristic->uuid = [%s] \n", characteristic->uuid);
	}

	value = g_hash_table_lookup(hash, "Name");
	g_variant_get(value, "s", &characteristic->name);

	if (characteristic->name) {
		BT_DBG("characteristic->name = [%s] \n", characteristic->name);
	}

	value = g_hash_table_lookup(hash, "Description");
	g_variant_get(value, "s", &characteristic->description);

	if (characteristic->description) {
		BT_DBG("characteristic->description = [%s] \n",
						characteristic->description);
	}

	value = g_hash_table_lookup(hash, "Value");
	gb_array = g_byte_array_new();
	g_variant_iter_init(&array_iter, value);
	while ((value = g_variant_iter_next_value(&array_iter))) {
		g_variant_get(value, "y", &g_str);
		g_byte_array_append(gb_array, (guchar *)g_str,
					sizeof(unsigned char));
	}
	if (gb_array) {
		if (gb_array->len) {
			BT_DBG("gb_array->len  = %d \n", gb_array->len);
			characteristic->val_len = gb_array->len;
			characteristic->val = g_malloc0(gb_array->len *
							sizeof(unsigned char));
			memcpy(characteristic->val, gb_array->data,
								gb_array->len);
		} else {
			characteristic->val = NULL;
			characteristic->val_len = 0;
		}
	} else {
		characteristic->val = NULL;
		characteristic->val_len = 0;
	}
	characteristic->handle = g_strdup(char_handle);
	g_hash_table_destroy(hash);
	g_variant_iter_free(&iter);
	g_variant_iter_free(&array_iter);
	g_variant_unref(result);
	g_variant_unref(value);
	return BLUETOOTH_ERROR_NONE;
}


BT_EXPORT_API int bluetooth_gatt_set_characteristics_value(
		const char *char_handle, const guint8 *value, int length,
		guint8 request)
{
	GValue *val;
	GByteArray *gbarray;
	GError *error = NULL;
	GDBusConnection *conn;
	char *handle;


	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);

	BT_CHECK_ENABLED(return);

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	gbarray = g_byte_array_new();
	g_byte_array_append(gbarray, (guint8 *)value, length);

	val = g_new0(GValue, 1);
	g_value_init(val, G_TYPE_STRING);
	g_value_take_boxed(val, gbarray);

	if(request) {
		handle = g_strdup(char_handle);

		g_dbus_connection_call(conn,
					BT_BLUEZ_NAME,
					char_handle,
					BLUEZ_CHAR_INTERFACE,
					"SetProperty",
					g_variant_new("(svy)",
						"Value", val, request),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					(GAsyncReadyCallback)
					__bluetooth_internal_write_cb,
					handle);

	} else {
		g_dbus_connection_call_sync(conn,
					BT_BLUEZ_NAME,
					char_handle,
					BLUEZ_CHAR_INTERFACE,
					"SetProperty",
					g_variant_new("(svy)",
						"Value", val, request),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	}

	if (error) {
		BT_ERR("Set value Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_free(val);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_service_from_uuid(
					bluetooth_device_address_t *address,
					const char *service_uuid,
					bt_gatt_service_property_t *service)
{
	int i;
	int ret;
	bt_gatt_handle_info_t prim_svc;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(service_uuid, return);
	BT_CHECK_PARAMETER(service, return);

	BT_CHECK_ENABLED(return);
	BT_DBG("+");
	ret = bluetooth_gatt_get_primary_services(address, &prim_svc);
	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Get primary service failed ");
		return ret;
	}

	for (i = 0; i < prim_svc.count; i++) {

		BT_DBG("prim_svc [%d] = %s", i, prim_svc.handle[i]);

		ret = bluetooth_gatt_get_service_property(prim_svc.handle[i],
								service);
		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("Get service property failed ");
			bluetooth_gatt_free_primary_services(&prim_svc);
			return ret;
		}

		BT_DBG("Service uuid %s", service->uuid);

		if (g_strstr_len(service->uuid, -1, service_uuid)) {
			BT_DBG("Found requested service");
			ret = BLUETOOTH_ERROR_NONE;
			break;
		}

		bluetooth_gatt_free_service_property(service);
	}

	if (i == prim_svc.count)
		ret = BLUETOOTH_ERROR_NOT_FOUND;

	bluetooth_gatt_free_primary_services(&prim_svc);
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_get_char_from_uuid(const char *service_handle,
							const char *char_uuid)
{
	char **char_handles;
	char_pty_req_t *char_pty;
	int i;
	bt_gatt_service_property_t svc_pty;
	BT_DBG("+");
	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_PARAMETER(char_uuid, return);

	BT_CHECK_ENABLED(return);

	if (bluetooth_gatt_get_service_property(service_handle, &svc_pty) !=
							BLUETOOTH_ERROR_NONE) {
		BT_ERR("Invalid service");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	char_handles = svc_pty.handle_info.handle;

	if (char_handles == NULL)
		return __discover_char_from_uuid(svc_pty.handle, char_uuid);

	char_pty = g_new0(char_pty_req_t, 1);

	char_pty->handle = g_malloc0((svc_pty.handle_info.count + 1) *
							sizeof(char *));
	for (i = 0; i < svc_pty.handle_info.count; i++) {
		char_pty->handle[i] = char_handles[i];
		BT_DBG("char_path[%d] : [%s]", i, char_pty->handle[i]);
	}
	char_pty->char_uuid = g_strdup(char_uuid);

	g_idle_add(__filter_chars_with_uuid, char_pty);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_read_characteristic_value(const char *characteristic)
{
        GDBusConnection *conn;
	char *handle;

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(characteristic);
	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				characteristic,
				BLUEZ_CHAR_INTERFACE,
				"ReadCharacteristic",
				NULL,
				G_VARIANT_TYPE("qa(y)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback)
				__bluetooth_internal_read_cb,
				handle);

	return BLUETOOTH_ERROR_NONE;
}

