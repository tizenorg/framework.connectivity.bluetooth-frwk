/*
 * Bluetooth-frwk low energy
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hocheol Seo <hocheol.seo@samsung.com>
 *	    Girishashok Joshi <girish.joshi@samsung.com>
 *	    Chanyeol Park <chanyeol.park@samsung.com>
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
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

#include "bluetooth-api.h"
#include "bt-common.h"
#include "bt-internal-types.h"


#define BLUEZ_CHAR_INTERFACE "org.bluez.Characteristic"

#define GATT_OBJECT_PATH  "/org/bluez/gatt_attrib"

typedef struct {
	GObject parent;
} BluetoothGattService;

typedef struct {
	GObjectClass parent;
} BluetoothGattServiceClass;

typedef struct {
	char *char_uuid;
	char **handle;
} char_pty_req_t;

GType bluetooth_gatt_service_get_type(void);

#define BLUETOOTH_GATT_TYPE_SERVICE (bluetooth_gatt_service_get_type())

#define BLUETOOTH_GATT_SERVICE(object)	\
		(G_TYPE_CHECK_INSTANCE_CAST((object), \
		BLUETOOTH_GATT_TYPE_SERVICE, BluetoothGattService))

#define BLUETOOTH_GATT_SERVICE_CLASS(klass)	\
		(G_TYPE_CHECK_CLASS_CAST((klass), \
		BLUETOOTH_GATT_TYPE_SERVICE, BluetoothGattServiceClass))

#define BLUETOOTH_GATT_IS_SERVICE(object)	\
		(G_TYPE_CHECK_INSTANCE_TYPE((object), BLUETOOTH_GATT_TYPE_SERVICE))

#define BLUETOOTH_GATT_IS_SERVICE_CLASS(klass)	\
		(G_TYPE_CHECK_CLASS_TYPE((klass), BLUETOOTH_GATT_TYPE_SERVICE))

#define BLUETOOTH_GATT_SERVICE_GET_CLASS(obj)	\
		(G_TYPE_INSTANCE_GET_CLASS((obj), \
		BLUETOOTH_GATT_TYPE_SERVICE, BluetoothGattServiceClass))

G_DEFINE_TYPE(BluetoothGattService, bluetooth_gatt_service, G_TYPE_OBJECT)

static gboolean bluetooth_gatt_value_changed(BluetoothGattService *agent,
					gchar *obj_path,
					GArray *byte_array,
					DBusGMethodInvocation *context);

#include "bt-gatt-glue.h"

static void bluetooth_gatt_service_init(BluetoothGattService *obj)
{
	g_assert(obj != NULL);
}

static void bluetooth_gatt_service_finalize(GObject *obj)
{
	G_OBJECT_CLASS(bluetooth_gatt_service_parent_class)->finalize(obj);
}

static void bluetooth_gatt_service_class_init(BluetoothGattServiceClass *klass)
{
	GObjectClass *object_class = (GObjectClass *)klass;

	g_assert(klass != NULL);

	object_class->finalize = bluetooth_gatt_service_finalize;

	dbus_g_object_type_install_info(BLUETOOTH_GATT_TYPE_SERVICE,
					&dbus_glib_bluetooth_gatt_object_info);
}

static gboolean bluetooth_gatt_value_changed(BluetoothGattService *agent,
					gchar *obj_path,
					GArray *byte_array,
					DBusGMethodInvocation *context)
{
	bt_gatt_char_value_t char_val;
	bt_user_info_t *user_info;
	BT_DBG("+");

	char_val.char_handle = obj_path;
	char_val.char_value = &g_array_index(byte_array, guint8, 0);
	char_val.val_len = byte_array->len;
	BT_DBG("Byte array length = %d", char_val.val_len);

	user_info = _bt_get_user_data(BT_COMMON);

	if (user_info->cb) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
				BLUETOOTH_ERROR_NONE, &char_val,
				user_info->cb, user_info->user_data);
	}

	BT_DBG("-");

	return TRUE;
}

static void __add_value_changed_method(DBusGConnection *conn)
{
	static gboolean method_added = FALSE;
	BluetoothGattService *bluetooth_gatt_obj = NULL;

	if (method_added) {
		BT_ERR("Method already added. \n");
		return;
	}

	method_added = TRUE;

	bluetooth_gatt_obj = g_object_new(BLUETOOTH_GATT_TYPE_SERVICE, NULL);

	dbus_g_connection_register_g_object(conn, GATT_OBJECT_PATH,
			G_OBJECT(bluetooth_gatt_obj));

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

static void __bluetooth_internal_get_char_cb(DBusGProxy *proxy,
					DBusGProxyCall *call,
					gpointer user_data)
{
	GError *error = NULL;
	GPtrArray *gp_array = NULL;
	bt_gatt_discovered_char_t svc_char = { 0, };
	bt_user_info_t *user_info;

	svc_char.service_handle = user_data;

	user_info = _bt_get_user_data(BT_COMMON);

	if (!dbus_g_proxy_end_call(proxy, call, &error,
		dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH),
		&gp_array, G_TYPE_INVALID)) {
		BT_ERR("Error : %s \n", error->message);
		g_error_free(error);

		if (user_info->cb) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
					BLUETOOTH_ERROR_NONE, &svc_char,
					user_info->cb, user_info->user_data);
		}
		g_free(svc_char.service_handle);
		g_object_unref(proxy);
		return;
	}

	if (NULL != gp_array) {
		svc_char.handle_info.count = gp_array->len;
		svc_char.handle_info.handle = __get_string_array_from_gptr_array(gp_array);
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
				BLUETOOTH_ERROR_NONE, &svc_char,
				user_info->cb, user_info->user_data);
	}

	g_ptr_array_free(gp_array, TRUE);
	g_free(svc_char.service_handle);
	g_free(svc_char.handle_info.handle);
	g_object_unref(proxy);
}

static void __bluetooth_internal_read_cb(GObject *source_object,
					GAsyncResult *res,
					gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	bt_gatt_char_value_t rsp =  { 0, };
	GDBusConnection *system_gconn = NULL;
	GVariant *return_data;
	GByteArray *gp_byte_array = NULL;
	GVariantIter *iter;
	guint8 g_byte;

	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_init_system_gdbus_conn();
	return_data = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		if(return_data)
			g_variant_unref(return_data);
		g_clear_error(&error);
		return;
	}

	gp_byte_array = g_byte_array_new();
	g_variant_get(return_data, "(ay)", &iter);

	while (g_variant_iter_loop(iter, "y",  &g_byte)) {
		g_byte_array_append(gp_byte_array, &g_byte, 1);
	}

	if (NULL != gp_byte_array) {
		rsp.val_len = gp_byte_array->len;
		rsp.char_value = gp_byte_array->data;
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_CHAR,
				BLUETOOTH_ERROR_NONE, &rsp,
				user_info->cb, user_info->user_data);
	}

	g_byte_array_free(gp_byte_array, TRUE);
	g_variant_unref(return_data);
	g_variant_iter_free(iter);
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
	if (user_info->cb == NULL) {
		__free_char_req(char_req);
		return FALSE;
	}

	char_pty = g_new0(bt_gatt_char_property_t, 1);

	while (char_req->handle[i] != NULL) {
		BT_DBG("char_pty[%d] = %s", i, char_req->handle[i]);
		ret = bluetooth_gatt_get_characteristics_property(char_req->handle[i],
									char_pty);
		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("get char property failed");
			goto done;
		}

		if (char_pty->uuid  && g_strstr_len(char_pty->uuid, -1,
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

static void __disc_char_from_uuid_cb(DBusGProxy *proxy,
					DBusGProxyCall *call,
					gpointer user_data)
{
	GError *error = NULL;
	GPtrArray *gp_array = NULL;
	bt_user_info_t *user_info;
	char_pty_req_t *char_req = user_data;

	user_info = _bt_get_user_data(BT_COMMON);
	if (!user_info->cb) {
		__free_char_req(char_req);
		return;
	}

	if (!dbus_g_proxy_end_call(proxy, call, &error,
		dbus_g_type_get_collection("GPtrArray", DBUS_TYPE_G_OBJECT_PATH),
		&gp_array, G_TYPE_INVALID)) {
		BT_ERR("Error : %s \n", error->message);
		g_error_free(error);

		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
					BLUETOOTH_ERROR_INTERNAL, NULL,
					user_info->cb, user_info->user_data);

		__free_char_req(char_req);
		g_object_unref(proxy);
		return;
	}

	if (gp_array == NULL) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
					BLUETOOTH_ERROR_NOT_FOUND, NULL,
					user_info->cb, user_info->user_data);

		__free_char_req(char_req);
		g_object_unref(proxy);

		return;
	}

	char_req->handle = __get_string_array_from_gptr_array(gp_array);

	__filter_chars_with_uuid(char_req);

	g_ptr_array_free(gp_array, TRUE);
	g_object_unref(proxy);
}


static int __discover_char_from_uuid(const char *service_handle,
							const char *char_uuid){
	DBusGProxy *service_proxy = NULL;
	DBusGConnection *conn;
	char_pty_req_t *char_req;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	service_proxy = dbus_g_proxy_new_for_name(conn,
						BT_BLUEZ_NAME, service_handle,
						BLUEZ_CHAR_INTERFACE);
	retv_if(service_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	char_req = g_new0(char_pty_req_t, 1);

	char_req->char_uuid = g_strdup(char_uuid);
	BT_DBG("Char uuid %s ", char_uuid);

	if (!dbus_g_proxy_begin_call(service_proxy, "DiscoverCharacteristics",
			(DBusGProxyCallNotify)__disc_char_from_uuid_cb,
			char_req, NULL, G_TYPE_INVALID)) {
		__free_char_req(char_req);
		g_object_unref(service_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_free_primary_services(bt_gatt_handle_info_t *prim_svc)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(prim_svc, return);

	g_strfreev(prim_svc->handle);

	memset(prim_svc, 0, sizeof(bt_gatt_handle_info_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_free_service_property(bt_gatt_service_property_t *svc_pty)
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

BT_EXPORT_API int bluetooth_gatt_free_char_property(bt_gatt_char_property_t *char_pty)
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

BT_EXPORT_API int bluetooth_gatt_get_primary_services(const bluetooth_device_address_t *address,
								bt_gatt_handle_info_t *prim_svc)
{
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	gchar *device_path = NULL;
	GError *error = NULL;
	DBusGProxy *device_proxy = NULL;
	GHashTable *hash = NULL;
	GValue *value = NULL;
	GPtrArray *gp_array  = NULL;
	DBusGProxy *adapter_proxy;
	DBusGConnection *conn;
	int ret = BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("+");

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(prim_svc, return);

	BT_CHECK_ENABLED(return);

	_bt_convert_addr_type_to_string(device_address,
				(unsigned char *)address->addr);

	BT_DBG("bluetooth address [%s]\n", device_address);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_proxy = _bt_get_adapter_proxy(conn);
	retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(adapter_proxy, "FindDevice", &error,
		G_TYPE_STRING, device_address, G_TYPE_INVALID,
		DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID);
	if (error) {
		BT_ERR("FindDevice Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(adapter_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(adapter_proxy);

	retv_if(device_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	device_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				      device_path, BT_DEVICE_INTERFACE);
	g_free(device_path);
	retv_if(device_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(device_proxy, "GetProperties", &error, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error) {
		BT_ERR("GetProperties Call Error %s[%s]", error->message, device_address);
		g_error_free(error);
		g_object_unref(device_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(device_proxy);

	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	value = g_hash_table_lookup(hash, "Services");
	if (value == NULL) {
		BT_ERR("value == NULL");
		goto done;
	}

	gp_array = g_value_get_boxed(value);
	if (gp_array == NULL) {
		BT_ERR("gp_array == NULL");
		goto done;
	}

	prim_svc->count = gp_array->len;
	prim_svc->handle = __get_string_array_from_gptr_array(gp_array);

	ret = BLUETOOTH_ERROR_NONE;
done:
	g_hash_table_destroy(hash);
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_discover_service_characteristics(const char *service_handle)
{
	DBusGProxy *service_proxy = NULL;
	char *handle;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	service_proxy = dbus_g_proxy_new_for_name(conn,
						BT_BLUEZ_NAME, service_handle,
						BLUEZ_CHAR_INTERFACE);

	retv_if(service_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(service_handle);
	BT_DBG("Requested characteristic handle:%s \n ", handle);

	if (!dbus_g_proxy_begin_call(service_proxy, "DiscoverCharacteristics",
			(DBusGProxyCallNotify)__bluetooth_internal_get_char_cb,
			handle, NULL, G_TYPE_INVALID)) {
		g_free(handle);
		g_object_unref(service_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_service_property(const char *service_handle,
						bt_gatt_service_property_t *service)
{
	DBusGProxy *service_proxy = NULL;
	GHashTable *hash = NULL;
	GError *error = NULL;
	GValue *value = NULL;
	GPtrArray *gp_array  = NULL ;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_PARAMETER(service, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	service_proxy = dbus_g_proxy_new_for_name(conn,
						BT_BLUEZ_NAME, service_handle,
						BLUEZ_CHAR_INTERFACE);

	retv_if(service_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(service_proxy, "GetProperties", &error, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error != NULL) {
		BT_ERR("GetProperties Call Error %s\n", error->message);
		g_error_free(error);
		g_object_unref(service_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_object_unref(service_proxy);

	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	memset(service, 0, sizeof(bt_gatt_service_property_t));

	value = g_hash_table_lookup(hash, "UUID");
	service->uuid = value ? g_value_dup_string(value) : NULL;
	if (service->uuid) {
		BT_DBG("svc_pty.uuid = [%s] \n", service->uuid);
	}

	value = g_hash_table_lookup(hash, "Characteristics");
	gp_array = value ? g_value_get_boxed(value) : NULL;
	if (gp_array) {
		service->handle_info.count = gp_array->len;
		service->handle_info.handle = __get_string_array_from_gptr_array(gp_array);
	}

	service->handle = g_strdup(service_handle);
	g_hash_table_destroy(hash);
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_watch_characteristics(const char *service_handle)
{
	DBusGProxy *watch_proxy = NULL;
	GError *error = NULL;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);

	BT_DBG("Entered service handle:%s \n ", service_handle);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	watch_proxy = dbus_g_proxy_new_for_name(conn,
				BT_BLUEZ_NAME, service_handle,
				BLUEZ_CHAR_INTERFACE);

	retv_if(watch_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	__add_value_changed_method(conn);

	dbus_g_proxy_call(watch_proxy, "RegisterCharacteristicsWatcher", &error,
				DBUS_TYPE_G_OBJECT_PATH, GATT_OBJECT_PATH,
				G_TYPE_INVALID, G_TYPE_INVALID);
	if (error) {
		BT_ERR("Method call  Fail: %s", error->message);
		g_error_free(error);
		g_object_unref(watch_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(watch_proxy);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_unwatch_characteristics(const char *service_handle)
{
	DBusGProxy *watch_proxy = NULL;
	GError *error = NULL;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(service_handle, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	watch_proxy = dbus_g_proxy_new_for_name(conn,
				BT_BLUEZ_NAME, service_handle,
				BLUEZ_CHAR_INTERFACE);

	retv_if(watch_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(watch_proxy, "UnregisterCharacteristicsWatcher", &error,
				DBUS_TYPE_G_OBJECT_PATH, GATT_OBJECT_PATH,
				G_TYPE_INVALID, G_TYPE_INVALID);
	if (error) {
		BT_ERR("Method call  Fail: %s", error->message);
		g_error_free(error);
		g_object_unref(watch_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_object_unref(watch_proxy);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_characteristics_property(const char *char_handle,
						bt_gatt_char_property_t *characteristic)
{
	DBusGProxy *characteristic_proxy = NULL;
	GHashTable *hash = NULL;
	GError *error = NULL;
	GValue *value = NULL;
	GByteArray *gb_array = NULL;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(characteristic, return);

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	characteristic_proxy = dbus_g_proxy_new_for_name(conn,
						BT_BLUEZ_NAME, char_handle,
						BLUEZ_CHAR_INTERFACE);

	retv_if(characteristic_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_g_proxy_call(characteristic_proxy, "GetProperties", &error, G_TYPE_INVALID,
				dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				&hash, G_TYPE_INVALID);
	if (error != NULL) {
		BT_ERR("GetProperties Call Error %s\n", error->message);
		g_error_free(error);
		g_object_unref(characteristic_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_object_unref(characteristic_proxy);

	retv_if(hash == NULL, BLUETOOTH_ERROR_INTERNAL);

	memset(characteristic, 0, sizeof(bt_gatt_char_property_t));

	value = g_hash_table_lookup(hash, "UUID");
	characteristic->uuid = value ? g_value_dup_string(value) : NULL;
	if (characteristic->uuid) {
		BT_DBG("characteristic->uuid = [%s] \n", characteristic->uuid);
	}

	value = g_hash_table_lookup(hash, "Name");
	characteristic->name = value ? g_value_dup_string(value) : NULL;
	if (characteristic->name) {
		DBG_SECURE("characteristic->name = [%s] \n", characteristic->name);
	}

	value = g_hash_table_lookup(hash, "Description");
	characteristic->description = value ? g_value_dup_string(value) : NULL;
	if (characteristic->description) {
		BT_DBG("characteristic->description = [%s] \n", characteristic->description);
	}

	value = g_hash_table_lookup(hash, "Value");

	gb_array = value ? g_value_get_boxed(value) : NULL;
	if (gb_array) {
		if (gb_array->len) {
			BT_DBG("gb_array->len  = %d \n", gb_array->len);
			characteristic->val_len = gb_array->len;

			characteristic->val = g_malloc0(gb_array->len * sizeof(unsigned char));
			memcpy(characteristic->val, gb_array->data, gb_array->len);
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
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristics_value(const char *char_handle,
						const guint8 *value, int length)
{
	DBusGProxy *characteristic_proxy = NULL;
	GValue *val;
	GByteArray *gbarray;
	GError *error = NULL;
	DBusGConnection *conn;

	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);

	BT_CHECK_ENABLED(return);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("Requested characteristic handle:%s \n ", char_handle);

	characteristic_proxy = dbus_g_proxy_new_for_name(conn,
							BT_BLUEZ_NAME, char_handle,
							BLUEZ_CHAR_INTERFACE);

	retv_if(characteristic_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	gbarray = g_byte_array_new();
	g_byte_array_append(gbarray, (guint8 *)value, length);

	val = g_new0(GValue, 1);
	g_value_init(val, DBUS_TYPE_G_UCHAR_ARRAY);
	g_value_take_boxed(val, gbarray);

	dbus_g_proxy_call(characteristic_proxy, "SetProperty",
		&error, G_TYPE_STRING, "Value",
		G_TYPE_VALUE, val, G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref(characteristic_proxy);
	g_free(val);

	if (error) {
		BT_ERR("Set value Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristics_value_request(
		const char *char_handle, const guint8 *value, int length)
{
	GVariant *val;
	GError *error = NULL;
	GDBusConnection *conn;
	char *handle;
	GVariantBuilder *builder;
	int i;

	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);

	BT_CHECK_ENABLED(return);

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", value[i]);
	}

	val = g_variant_new("ay", builder);

	handle = g_strdup(char_handle);

	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				char_handle,
				BLUEZ_CHAR_INTERFACE,
				"SetPropertyRequest",
				g_variant_new("(sv)",
					"Value", val),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback)
				__bluetooth_internal_write_cb,
				handle);

	if (error) {
		BT_ERR("Set value Fail: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_builder_unref(builder);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_service_from_uuid(bluetooth_device_address_t *address,
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

	return ret;
}

BT_EXPORT_API int bluetooth_gatt_get_char_from_uuid(const char *service_handle,
						const char *char_uuid)
{
	char **char_handles;
	char_pty_req_t *char_pty;
	int i;
	bt_gatt_service_property_t svc_pty;

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

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_read_characteristic_value(const char *characteristic)
{
        GDBusConnection *conn;
	char *handle;

	BT_CHECK_PARAMETER(characteristic, return);
	BT_CHECK_ENABLED(return);

	conn = _bt_init_system_gdbus_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(characteristic);
	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				characteristic,
				BLUEZ_CHAR_INTERFACE,
				"ReadCharacteristic",
				NULL,
				G_VARIANT_TYPE("(ay)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback)
				__bluetooth_internal_read_cb,
				handle);

	return BLUETOOTH_ERROR_NONE;
}

