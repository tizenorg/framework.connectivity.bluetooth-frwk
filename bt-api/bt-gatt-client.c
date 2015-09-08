/*
 * Bluetooth-frwk low energy (GATT Client)
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hocheol Seo <hocheol.seo@samsung.com>
 *		    Chanyeol Park <chanyeol.park@samsung.com>
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
#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "bt-common.h"

#define GATT_SERV_INTERFACE		"org.bluez.GattService1"
#define GATT_CHAR_INTERFACE		"org.bluez.GattCharacteristic1"
#define GATT_DESC_INTERFACE		"org.bluez.GattDescriptor1"

#define GATT_USER_DESC_UUID		 "00002901-0000-1000-8000-00805f9b34fb"
#define GATT_CHAR_CLIENT_CONF		"00002902-0000-1000-8000-00805f9b34fb"
#define GATT_CHAR_SERVER_CONF		 "00002903-0000-1000-8000-00805f9b34fb"
#define GATT_CHAR_FORMAT		"00002904-0000-1000-8000-00805f9b34fb"

typedef enum {
	TYPE_NONE,
	USER_DESC,
	CLIENT_CONF,
	SERVER_CONF,
	CHAR_FORMAT
}char_descriptor_type_t;

BT_EXPORT_API int bluetooth_gatt_free_service_property(bt_gatt_service_property_t *svc_pty)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(svc_pty, return);

	g_free(svc_pty->uuid);
	g_free(svc_pty->handle);
	g_strfreev(svc_pty->include_handles.handle);
	g_strfreev(svc_pty->char_handle.handle);

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
	g_strfreev(char_pty->char_desc_handle.handle);

	memset(char_pty, 0, sizeof(bt_gatt_char_property_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_free_desc_property(bt_gatt_char_descriptor_property_t *desc_pty)
{
	BT_DBG("+");

	BT_CHECK_PARAMETER(desc_pty, return);

	g_free(desc_pty->uuid);
	g_free(desc_pty->val);
	g_free(desc_pty->handle);

	memset(desc_pty, 0, sizeof(bt_gatt_char_descriptor_property_t));

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static char **__get_string_array_from_gptr_array(GPtrArray *gp)
{
	gchar *gp_path = NULL;
	char **path = NULL;
	int i;

	if (gp->len == 0)
		return NULL;

	path = g_malloc0((gp->len + 1) * sizeof(char *));

	/* Fix : NULL_RETURNS */
	if (path == NULL)
		return NULL;

	for (i = 0; i < gp->len; i++) {
		gp_path = g_ptr_array_index(gp, i);
		path[i] = g_strdup(gp_path);
		BT_DBG("path[%d] : [%s]", i, path[i]);
	}

	return path;
}

BT_EXPORT_API int bluetooth_gatt_get_service_property(const char *service_handle,
						bt_gatt_service_property_t *service)
{
	GDBusProxy *properties_proxy = NULL;
	GError *error = NULL;
	GVariant *result = NULL;
	GDBusConnection *g_conn;
	gsize len;
	char *char_handle = NULL;
	GPtrArray *gp_array  = NULL ;
	GVariantIter *property_iter, *char_iter;
	const gchar *key;
	GVariant *value;

	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_PARAMETER(service, return);
	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			service_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(properties_proxy,
				"GetAll",
				g_variant_new("(s)", GATT_SERV_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Fail to get properties (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Fail to get properties");
		g_object_unref(properties_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(a{sv})", &property_iter);

	memset(service, 0, sizeof(bt_gatt_service_property_t));

	while (g_variant_iter_loop(property_iter, "{sv}", &key, &value)) {
		if (!g_strcmp0(key,"UUID")) {
			service->uuid = g_variant_dup_string(value,&len);

		} else if(!g_strcmp0(key, "Primary")) {
			service->primary = g_variant_get_boolean(value);

		} else if (!g_strcmp0(key, "Includes")) {
			g_variant_get(value, "ao", &char_iter);
			gp_array = g_ptr_array_new();
			while (g_variant_iter_loop(char_iter, "&o", &char_handle)) {
				g_ptr_array_add(gp_array, (gpointer)char_handle);
			}
			if (gp_array->len != 0) {
				service->include_handles.count = gp_array->len;
				service->include_handles.handle =
						__get_string_array_from_gptr_array(gp_array);
			}
			g_ptr_array_free(gp_array, TRUE);
		} else if (!g_strcmp0(key, "Characteristics")) {
			g_variant_get(value, "ao", &char_iter);
			gp_array = g_ptr_array_new();
			while (g_variant_iter_loop(char_iter, "&o", &char_handle)) {
				g_ptr_array_add(gp_array, (gpointer)char_handle);
			}
			if (gp_array->len != 0) {
				service->char_handle.count = gp_array->len;
				service->char_handle.handle =
						__get_string_array_from_gptr_array(gp_array);
			}
			g_ptr_array_free(gp_array, TRUE);
		}
	}

	service->handle = g_strdup(service_handle);

	g_variant_iter_free(property_iter);
	g_variant_unref(result);
	g_object_unref(properties_proxy);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_primary_services(
		const bluetooth_device_address_t *address,
		bt_gatt_handle_info_t *prim_svc)
{
	GVariant *result = NULL;
	GVariantIter *iter;
	GVariantIter *svc_iter;
	GVariantIter *interface_iter;
	char *object_path = NULL;
	char *interface_str = NULL;
	const gchar *key = NULL;
	GVariant *value = NULL;
	GPtrArray *gp_array  = NULL;
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char temp_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	int ret = BLUETOOTH_ERROR_INTERNAL;

	BT_INFO("+");
	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(prim_svc, return);
	BT_CHECK_ENABLED_ANY(return);

	result = _bt_get_managed_objects();
	if (result == NULL)
		return ret;

	_bt_convert_addr_type_to_string(device_address,
			(unsigned char *)address->addr);

	gp_array = g_ptr_array_new();
	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);

	while (g_variant_iter_loop(iter, "{&oa{sa{sv}}}", &object_path,
			&interface_iter)) {
		if (object_path == NULL)
			continue;

		_bt_convert_device_path_to_address(object_path, temp_address);

		if (g_strcmp0(temp_address, device_address) != 0)
			continue;

		while (g_variant_iter_loop(interface_iter, "{sa{sv}}",
				&interface_str, &svc_iter)) {
			if (g_strcmp0(interface_str, GATT_SERV_INTERFACE) != 0)
				continue;

			BT_DBG("Object Path: %s", object_path);
			while (g_variant_iter_loop(svc_iter, "{sv}", &key, &value)) {
				if (g_strcmp0(key, "Primary") == 0) {
					if (g_variant_get_boolean(value))
						g_ptr_array_add(gp_array, (gpointer)object_path);
				}
			}
		}
	}

	if (gp_array->len == 0) {
		BT_ERR("gp_array is NULL");
		ret = BLUETOOTH_ERROR_NOT_FOUND;
	} else {
		ret = BLUETOOTH_ERROR_NONE;
		prim_svc->count = gp_array->len;
		prim_svc->handle = __get_string_array_from_gptr_array(gp_array);
	}

	g_ptr_array_free(gp_array, TRUE);
	g_variant_iter_free(iter);
	g_variant_unref(result);
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_get_service_from_uuid(bluetooth_device_address_t *address,
			const char *service_uuid,
			bt_gatt_service_property_t *service)
{
	GVariant *result = NULL;
	GVariantIter *iter;
	GVariantIter *svc_iter;
	GVariantIter *interface_iter;
	char *object_path = NULL;
	char *interface_str = NULL;
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char temp_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	int ret = BLUETOOTH_ERROR_INTERNAL;

	BT_CHECK_PARAMETER(address, return);
	BT_CHECK_PARAMETER(service_uuid, return);
	BT_CHECK_PARAMETER(service, return);
	BT_CHECK_ENABLED_ANY(return);

	result = _bt_get_managed_objects();
	if (result == NULL)
		return ret;

	_bt_convert_addr_type_to_string(device_address,
				(unsigned char *)address->addr);

	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);

	while (g_variant_iter_loop(iter, "{oa{sa{sv}}}", &object_path,
			&interface_iter)) {
		if (object_path == NULL)
			continue;

		_bt_convert_device_path_to_address(object_path,
				temp_address);

		if (g_strcmp0(temp_address, device_address) != 0)
			continue;

		while (g_variant_iter_loop(interface_iter, "{sa{sv}}",
				&interface_str, &svc_iter)) {
			if (g_strcmp0(interface_str, GATT_SERV_INTERFACE) != 0)
				continue;

			BT_DBG("Object Path: %s", object_path);
			ret = bluetooth_gatt_get_service_property(object_path,
					service);

			if (ret != BLUETOOTH_ERROR_NONE) {
				BT_ERR("Get service property failed(0x%08x)", ret);
			} else {
				if (service->primary == TRUE &&
						g_strstr_len(service->uuid, -1,
								service_uuid)) {
					ret = BLUETOOTH_ERROR_NONE;
					goto done;
				}
			}
			bluetooth_gatt_free_service_property(service);
		}
	}

done:
	g_variant_iter_free(iter);
	g_variant_unref(result);

	return ret;
}

static void __bluetooth_internal_get_char_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	GVariant *value;
	GVariant *char_value;
	GVariantIter *char_iter;
	GPtrArray *gp_array = NULL;
	bt_gatt_discovered_char_t svc_char = { 0, };
	int i;
	char *char_handle;
	GError *error = NULL;
	bt_user_info_t *user_info;

	BT_DBG("+");

	user_info = _bt_get_user_data(BT_COMMON);
	svc_char.service_handle = user_data;

	value = g_dbus_proxy_call_finish(proxy, res, &error);

	if (value == NULL) {
		if (error != NULL) {
			BT_ERR("Get service characteristics failed\n errCode[%x],"
					"message[%s]\n", error->code, error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("Get service characteristics failed\n");
		}
		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
				BLUETOOTH_ERROR_INTERNAL, NULL,
				user_info->cb, user_info->user_data);
		}
		g_free(svc_char.service_handle);
		g_object_unref(proxy);
		return;
	}

	g_variant_get(value, "(v)", &char_value);
	g_variant_get(char_value, "ao", &char_iter);

	int len = g_variant_get_size((GVariant *)char_iter);
	if (len > 0) {
		gp_array = g_ptr_array_new();
		for (i = 0; i < len; i++) {
			g_variant_iter_loop(char_iter, "&o",  &char_handle);
			g_ptr_array_add(gp_array, (gpointer)char_handle);
		}
		if (gp_array->len != 0) {
			svc_char.handle_info.count = gp_array->len;
			svc_char.handle_info.handle =
				__get_string_array_from_gptr_array(gp_array);
		}
		g_ptr_array_free(gp_array, TRUE);
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED,
			BLUETOOTH_ERROR_NONE, &svc_char,
			user_info->cb, user_info->user_data);
	}

	g_strfreev(svc_char.handle_info.handle);
	g_free(svc_char.service_handle);
	g_variant_iter_free(char_iter);
	g_object_unref(proxy);
}

BT_EXPORT_API int bluetooth_gatt_discover_service_characteristics(
			const char *service_handle)
{
	GDBusProxy *properties_proxy = NULL;
	GDBusConnection *g_conn;
	GError *error = NULL;
	char *handle;

	BT_DBG("+");

	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			service_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(service_handle);
	g_dbus_proxy_call(properties_proxy,
			"Get",
			g_variant_new("(ss)",
				GATT_SERV_INTERFACE, "Characteristics"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			(GAsyncReadyCallback)__bluetooth_internal_get_char_cb,
			(gpointer)handle);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}


static int __get_permission_flag(char *permission)
{
	int ret = 0;

	retv_if(permission == NULL, ret);

	BT_DBG("permission = %s",permission);

	if (!g_strcmp0(permission,"broadcast")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_BROADCAST;
	} else if (!g_strcmp0(permission,"read")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ;
	} else if (!g_strcmp0(permission,"write-without-response")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE_NO_RESPONSE;
	} else if (!g_strcmp0(permission,"write")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	} else if (!g_strcmp0(permission,"notify")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_NOTIFY;
	} else if (!g_strcmp0(permission,"indicate")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_INDICATE;
	} else if (!g_strcmp0(permission,"authenticated-signed-writes")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_SIGNED_WRITE;
	} else if (!g_strcmp0(permission,"reliable-write")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_EXTENDED_PROPS;
	} else if (!g_strcmp0(permission,"writable-auxiliaries")) {
		ret = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_EXTENDED_PROPS;
	}

	return ret;
}

BT_EXPORT_API int bluetooth_gatt_get_characteristics_property(
		const char *char_handle, bt_gatt_char_property_t *characteristic)
{
	GDBusProxy *properties_proxy = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	GByteArray *gb_array = NULL;
	GPtrArray *gp_array  = NULL ;
	GDBusConnection *g_conn;
	guint8 char_value;
	const gchar *key;
	gchar* permission;
	char *char_desc_handle = NULL;
	gsize len;
	GVariantIter *property_iter;
	GVariantIter *char_value_iter;
	GVariantIter *char_perm_iter;
	GVariantIter *char_desc_iter;

	BT_DBG("+");
	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(characteristic, return);

	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			char_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(properties_proxy,
				"GetAll",
					g_variant_new("(s)", GATT_CHAR_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Fail to get properties (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Fail to get properties");
		g_object_unref(properties_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(a{sv})", &property_iter);

	memset(characteristic, 0, sizeof(bt_gatt_char_property_t));
	characteristic->handle = g_strdup(char_handle);

	while (g_variant_iter_loop(property_iter, "{sv}", &key, &value)) {
		BT_DBG("property");
		if (!g_strcmp0(key,"UUID")) {
			characteristic->uuid = g_variant_dup_string(value,&len);
			BT_DBG("UUID of the char = %s",characteristic->uuid);
		} else if(!g_strcmp0(key, "Value")) {
			gb_array = g_byte_array_new();
			g_variant_get(value, "ay", &char_value_iter);
			while(g_variant_iter_loop(char_value_iter, "y",  &char_value)) {
				BT_DBG("value of char = %d",char_value);
				g_byte_array_append(gb_array, &char_value, 1);
			}
			if (gb_array->len != 0) {
				characteristic->val = g_malloc0(gb_array->len *
								sizeof(unsigned char));
				memcpy(characteristic->val, gb_array->data, gb_array->len);
			}
			characteristic->val_len = gb_array->len;
			g_byte_array_free(gb_array, TRUE);
		} else if(!g_strcmp0(key, "Flags")) {
			g_variant_get(value, "as", &char_perm_iter);
			characteristic->permission = 0x00;
			while (g_variant_iter_loop(char_perm_iter, "s", &permission)) {
				BT_DBG("permission = %s",permission);
				characteristic->permission |= __get_permission_flag(permission);
				BT_DBG("permission check = %d",characteristic->permission);
			}
			g_variant_iter_free(char_perm_iter);
		} else if (!g_strcmp0(key, "Descriptors")) {
			g_variant_get(value, "ao", &char_desc_iter);
			gp_array = g_ptr_array_new();
			while (g_variant_iter_loop(char_desc_iter, "&o", &char_desc_handle)) {
				g_ptr_array_add(gp_array, (gpointer)char_desc_handle);
			}
			if (gp_array->len != 0) {
				characteristic->char_desc_handle.count = gp_array->len;
				characteristic->char_desc_handle.handle =
						__get_string_array_from_gptr_array(gp_array);
			}
			g_ptr_array_free(gp_array, TRUE);
		}
	}

	g_variant_iter_free(property_iter);
	g_variant_unref(result);
	g_object_unref(properties_proxy);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

void bluetooth_gatt_get_char_from_uuid_cb(GDBusProxy *proxy,
			GAsyncResult *res, gpointer user_data)
{
	GVariant *value;
	GVariantIter *char_iter;
	int i, len;
	char *char_handle;
	GError *error = NULL;
	bt_user_info_t *user_info;
	int ret = BLUETOOTH_ERROR_INTERNAL;
	bt_gatt_char_property_t characteristic = {0, };

	user_info = _bt_get_user_data(BT_COMMON);

	value = g_dbus_proxy_call_finish(proxy, res, &error);

	if (value == NULL) {
		if (error != NULL) {
			BT_ERR("Get service characteristics failed\n errCode[%x],"
					"message[%s]\n", error->code, error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("Get service characteristics failed\n");
		}
		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
				BLUETOOTH_ERROR_INTERNAL, NULL,
				user_info->cb, user_info->user_data);
		}
		g_object_unref(proxy);
		g_free(user_data);
		return;
	}

	g_variant_get(value, "(ao)", &char_iter);

	len = g_variant_get_size((GVariant *)char_iter);

	for (i = 0; i < len; i++) {
		g_variant_iter_loop(char_iter, "o",  &char_handle);
		if (!char_handle)
			continue;
		ret = bluetooth_gatt_get_characteristics_property(char_handle,
				&characteristic);

		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("Get characteristic property failed(0x%08x)", ret);
		} else {
			if (g_strstr_len(characteristic.uuid, -1, user_data)) {
				ret = BLUETOOTH_ERROR_NONE;
				break;
			}
		}
		bluetooth_gatt_free_char_property(&characteristic);
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID, ret,
				&characteristic, user_info->cb, user_info->user_data);
	}

	bluetooth_gatt_free_char_property(&characteristic);
	g_variant_iter_free(char_iter);
	g_free(user_data);
}

BT_EXPORT_API int bluetooth_gatt_get_char_from_uuid(const char *service_handle,
						const char *char_uuid)
{
	GDBusProxy *properties_proxy = NULL;
	GDBusConnection *g_conn;
	GError *error = NULL;
	char *uuid;

	BT_CHECK_PARAMETER(service_handle, return);
	BT_CHECK_PARAMETER(char_uuid, return);
	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			service_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	uuid = g_strdup(char_uuid);
	g_dbus_proxy_call(properties_proxy,
			"Get",
			g_variant_new("(ss)",
				GATT_SERV_INTERFACE, "Characteristics"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			(GAsyncReadyCallback)bluetooth_gatt_get_char_from_uuid_cb,
			(gpointer)uuid);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_get_char_descriptor_property(
		const char *descriptor_handle, bt_gatt_char_descriptor_property_t *descriptor)
{
	GDBusProxy *properties_proxy = NULL;
	GError *error = NULL;
	GDBusConnection *g_conn;
	GVariant *result = NULL;
	GVariantIter *property_iter;
	const gchar *key;
	guint8 char_value;
	gsize len;
	GVariant *value = NULL;
	GByteArray *gb_array = NULL;
	GVariantIter *desc_value_iter;

	BT_DBG("+");
	BT_CHECK_PARAMETER(descriptor_handle, return);
	BT_CHECK_PARAMETER(descriptor, return);

	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			descriptor_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(properties_proxy,
				"GetAll",
					g_variant_new("(s)", GATT_DESC_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Fail to get properties (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Fail to get properties");
		g_object_unref(properties_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(result, "(a{sv})", &property_iter);

	memset(descriptor, 0, sizeof(bt_gatt_char_descriptor_property_t));
	descriptor->handle = g_strdup(descriptor_handle);

	while (g_variant_iter_loop(property_iter, "{sv}", &key, &value)) {
		BT_DBG("property");
		if (!g_strcmp0(key,"UUID")) {
			descriptor->uuid = g_variant_dup_string(value,&len);
			BT_DBG("UUID of the char_desc = %s",descriptor->uuid);
		} else if(!g_strcmp0(key, "Value")) {
			gb_array = g_byte_array_new();
			g_variant_get(value, "ay", &desc_value_iter);
			while(g_variant_iter_loop(desc_value_iter, "y",  &char_value)) {
				BT_DBG("value of descriptor = %d",char_value);
				g_byte_array_append(gb_array, &char_value, 1);
			}
			if (gb_array->len != 0) {
				descriptor->val = g_malloc0(gb_array->len *
								sizeof(unsigned char));
				memcpy(descriptor->val, gb_array->data, gb_array->len);
			}
			descriptor->val_len = gb_array->len;
			g_byte_array_free(gb_array, TRUE);
		}
	}

	g_variant_iter_free(property_iter);
	g_variant_unref(result);
	g_object_unref(properties_proxy);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static void __bluetooth_internal_read_cb(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	bt_gatt_char_value_t char_value =  { 0, };
	GDBusConnection *system_gconn = NULL;
	GVariant *value;
	GByteArray *gp_byte_array = NULL;
	GVariantIter *iter;
	guint8 g_byte;

	BT_DBG("+");
	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_gdbus_get_system_gconn();
	value = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_CHAR,
				BLUETOOTH_ERROR_INTERNAL, NULL,
				user_info->cb, user_info->user_data);
		}
		g_free(user_data);
		return;
	}

	char_value.char_handle = user_data;
	gp_byte_array = g_byte_array_new();
	g_variant_get(value, "(ay)", &iter);

	while (g_variant_iter_loop(iter, "y", &g_byte)) {
		g_byte_array_append(gp_byte_array, &g_byte, 1);
	}

	if (gp_byte_array->len != 0) {
		char_value.val_len = gp_byte_array->len;
		char_value.char_value = gp_byte_array->data;
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_CHAR,
				BLUETOOTH_ERROR_NONE, &char_value,
				user_info->cb, user_info->user_data);
	}

	g_free(char_value.char_handle);
	g_byte_array_free(gp_byte_array, TRUE);
	g_variant_unref(value);
	g_variant_iter_free(iter);

	BT_DBG("-");
}

BT_EXPORT_API int bluetooth_gatt_read_characteristic_value(const char *characteristic)
{
	GDBusConnection *conn;
	char *handle;

	BT_CHECK_PARAMETER(characteristic, return);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(characteristic);

	g_dbus_connection_call(conn,
			BT_BLUEZ_NAME,
			characteristic,
			GATT_CHAR_INTERFACE,
			"ReadValue",
			NULL,
			G_VARIANT_TYPE("(ay)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			(GAsyncReadyCallback)__bluetooth_internal_read_cb,
			(gpointer)handle);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristics_value(
		const char *char_handle, const guint8 *value, int length)
{
	GVariant *val;
	GVariantBuilder *builder;
	GError *error = NULL;
	GDBusConnection *conn;
	int i = 0;

	BT_DBG("+");
	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", value[i]);
	}

	val = g_variant_new("(ay)", builder);

	g_dbus_connection_call_sync(conn,
			BT_BLUEZ_NAME,
			char_handle,
			GATT_CHAR_INTERFACE,
			"WriteValue",
			val,
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, &error);

	if (error) {
		BT_ERR("Set value Failed: %s", error->message);
		g_clear_error(&error);
		g_variant_builder_unref(builder);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_builder_unref(builder);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static void __bluetooth_internal_write_cb(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	bt_user_info_t *user_info;
	GDBusConnection *system_gconn = NULL;
	GVariant *value;
	int result = BLUETOOTH_ERROR_NONE;

	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_gdbus_get_system_gconn();
	value = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
		result = BLUETOOTH_ERROR_INTERNAL;
	}
	if (user_info) {
		BT_DBG("result = %d", result);
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_WRITE_CHAR,
				result, NULL,
				user_info->cb, user_info->user_data);
	}

	if (value)
		g_variant_unref(value);
	BT_DBG("-");
	return;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristics_value_by_type(
		const char *char_handle, const guint8 *value, int length, guint8 write_type)
{
	GVariant *val;
	GVariantBuilder *builder;
	GError *error = NULL;
	GDBusConnection *conn;
	int i = 0;
	int ret = BLUETOOTH_ERROR_NONE;

	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", value[i]);
	}

	val = g_variant_new("ay", builder);

	if (write_type ==
		BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE_NO_RESPONSE) {
		g_dbus_connection_call_sync(conn,
				BT_BLUEZ_NAME,
				char_handle,
				GATT_CHAR_INTERFACE,
				"WriteValuebyType",
				g_variant_new("(y@ay)", write_type, val),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error);

		if (error) {
			BT_ERR("Set value Failed: %s", error->message);
			g_clear_error(&error);
			ret = BLUETOOTH_ERROR_INTERNAL;
		}
	} else if (write_type ==
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE) {
		g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				char_handle,
				GATT_CHAR_INTERFACE,
				"WriteValuebyType",
				g_variant_new("(y@ay)", write_type, val),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL,
				(GAsyncReadyCallback)__bluetooth_internal_write_cb,
				NULL);
	} else
		ret = BLUETOOTH_ERROR_INVALID_PARAM;


	g_variant_builder_unref(builder);
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristics_value_request(
			const char *char_handle, const guint8 *value, int length)
{
	GVariant *val;
	GDBusConnection *conn;
	GVariantBuilder *builder;
	int i;

	BT_DBG("+");
	BT_CHECK_PARAMETER(char_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", value[i]);
		BT_DBG("value [] = %d", value[i]);
	}

	val = g_variant_new("(ay)", builder);

	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				char_handle,
				GATT_CHAR_INTERFACE,
				"WriteValue",
				val,
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL,
				(GAsyncReadyCallback)__bluetooth_internal_write_cb,
				NULL);

	g_variant_builder_unref(builder);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static int __bluetooth_gatt_descriptor_iter(const char *char_handle,
			bt_gatt_char_property_t *characteristic)
{
	BT_DBG("+");
	GDBusProxy *properties_proxy = NULL;
	GError *error = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	GDBusConnection *g_conn;
	int i, ret = BLUETOOTH_ERROR_NONE;
	const char *uuid = NULL;
	gsize len = 0;
	GVariantIter *desc_value_iter, *property_iter;
	const gchar *key;
	char_descriptor_type_t desc_type = TYPE_NONE;

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			char_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	result = g_dbus_proxy_call_sync(properties_proxy,
				"GetAll",
				g_variant_new("(s)", GATT_DESC_INTERFACE),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (!result) {
		if (error != NULL) {
			BT_ERR("Fail to get properties (Error: %s)", error->message);
			g_clear_error(&error);
		} else
			BT_ERR("Fail to get properties");
		g_object_unref(properties_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_variant_get(result, "(a{sv})", &property_iter);
	while (g_variant_iter_loop(property_iter, "{sv}", &key, &value)) {
		if (!g_strcmp0(key,"UUID")) {
			uuid = g_variant_get_string(value, &len);
			if (g_strcmp0(uuid, GATT_USER_DESC_UUID) == 0) {
				BT_DBG("GATT_USER_DESC_UUID");
				desc_type = USER_DESC;
			} else if (g_strcmp0(uuid, GATT_CHAR_FORMAT) == 0) {
				BT_DBG("GATT_CHAR_FORMAT");
				desc_type = CHAR_FORMAT;
			} else if (g_strcmp0(uuid, GATT_CHAR_CLIENT_CONF) == 0) {
				BT_DBG("GATT_CHAR_CLIENT_CONF");
				desc_type = CLIENT_CONF;
			} else if (g_strcmp0(uuid, GATT_CHAR_SERVER_CONF) == 0) {
				BT_DBG("GATT_CHAR_SERVER_CONF");
				desc_type = SERVER_CONF;
			} else {
				BT_DBG("descriptor uuid = %s", uuid);
			}
		} else if (!g_strcmp0(key, "Value")) {
			switch(desc_type) {
				case CHAR_FORMAT :
					BT_DBG("Format descriptor");
					g_variant_get(value, "(yyqyq)",
							&(characteristic->format.format),
							&(characteristic->format.exponent),
							&(characteristic->format.unit),
							&(characteristic->format.name_space),
							&(characteristic->format.description));
					break;
				case USER_DESC:
					BT_DBG("User descriptor");
					g_variant_get(value, "ay", &desc_value_iter);
					len = g_variant_get_size((GVariant *)desc_value_iter);

					if (len > 0) {
						characteristic->description = (char *)g_malloc0(len + 1);
						if (!characteristic->description) {
							ret = BLUETOOTH_ERROR_OUT_OF_MEMORY;
							goto done;
						}
					}
					for (i = 0; i < len; i++) {
						g_variant_iter_loop(desc_value_iter, "y",
							&characteristic->description[i]);
						BT_DBG("description = %s", characteristic->description);
					}
					break;
				case CLIENT_CONF :
					BT_DBG(" CLIENT_CONF");
					break;
				case SERVER_CONF :
					BT_DBG(" SERVER_CONF");
					break;
				default:break;
			}
		}
	}

done:
	g_variant_iter_free(property_iter);
	g_variant_unref(result);
	g_object_unref(properties_proxy);

	BT_DBG("-");
	return ret;
}


static void bluetooth_gatt_get_char_desc_cb(GDBusProxy *proxy,
			GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GVariant *value;
	GVariant	*char_value;
	GVariantIter *char_iter;
	int i;
	char *char_handle;
	GError *error = NULL;
	bt_user_info_t *user_info;
	bt_gatt_char_property_t characteristic = {0, };
	int ret = BLUETOOTH_ERROR_INTERNAL;

	user_info = _bt_get_user_data(BT_COMMON);

	value = g_dbus_proxy_call_finish(proxy, res, &error);

	if (value == NULL) {
		if (error != NULL) {
			BT_ERR("Get characteristic descriptor failed\n errCode[%x],"
					"message[%s]\n", error->code, error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("Get characteristic descriptor failed\n");
		}
		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DESC_DISCOVERED,
				BLUETOOTH_ERROR_INTERNAL, NULL,
				user_info->cb, user_info->user_data);
		}
		g_free(user_data);
		g_object_unref(proxy);
		return;
	}

	g_variant_get(value, "(v)", &char_value);
	g_variant_get(char_value, "ao", &char_iter);

	int len = g_variant_get_size((GVariant *)char_iter);
	if (len > 0) {
		for (i = 0; i < len; i++) {
			g_variant_iter_loop(char_iter, "o",  &char_handle);
			BT_DBG("object path of descriptor = %s",char_handle);
			if(char_handle) {
				ret = __bluetooth_gatt_descriptor_iter(char_handle,
							&characteristic);
				BT_DBG("Descriptor read status [%d]",ret);
			}
		}
	}

	characteristic.handle = user_data;
	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_SVC_CHAR_DESC_DISCOVERED,
				ret, &characteristic, user_info->cb, user_info->user_data);
	}
	bluetooth_gatt_free_char_property(&characteristic);

	g_variant_iter_free(char_iter);
	BT_DBG("-");
}

BT_EXPORT_API int bluetooth_gatt_discover_characteristic_descriptor(
			const char *characteristic_handle)
{
	GDBusProxy *properties_proxy = NULL;
	GDBusConnection *g_conn;
	char *handle;
	GError *error = NULL;

	BT_CHECK_PARAMETER(characteristic_handle, return);
	BT_CHECK_ENABLED_ANY(return);

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	properties_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			characteristic_handle,
			BT_PROPERTIES_INTERFACE,
			NULL, &error);

	retv_if(properties_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(characteristic_handle);
	g_dbus_proxy_call(properties_proxy,
			"Get",
			g_variant_new("(ss)",
				GATT_CHAR_INTERFACE, "Descriptors"),
			G_DBUS_CALL_FLAGS_NONE,
			-1, NULL,
			(GAsyncReadyCallback)bluetooth_gatt_get_char_desc_cb,
			(gpointer)handle);

	return BLUETOOTH_ERROR_NONE;
}

static void __bluetooth_internal_read_desc_cb(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	bt_gatt_char_property_t char_value =  { 0, };
	GDBusConnection *system_gconn = NULL;
	GVariant *value;
	GByteArray *gp_byte_array = NULL;
	GVariantIter *iter;
	guint8 g_byte;

	BT_DBG("+");
	user_info = _bt_get_user_data(BT_COMMON);
	system_gconn = _bt_gdbus_get_system_gconn();

	char_value.handle = user_data;
	value = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
		if (user_info) {
			_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_DESC,
					BLUETOOTH_ERROR_INTERNAL, NULL,
					user_info->cb, user_info->user_data);
		}
		g_free(char_value.handle);
		return;
	}

	gp_byte_array = g_byte_array_new();
	g_variant_get(value, "(ay)", &iter);

	while(g_variant_iter_loop(iter, "y",  &g_byte)) {
		g_byte_array_append(gp_byte_array, &g_byte, 1);
	}

	if (gp_byte_array->len != 0) {
		char_value.val_len = (unsigned int )gp_byte_array->len;
		char_value.description= (char *)gp_byte_array->data;
	}

	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_READ_DESC,
				BLUETOOTH_ERROR_NONE, &char_value,
				user_info->cb, user_info->user_data);
	}

	g_byte_array_free(gp_byte_array, TRUE);
	g_free(char_value.handle);
	g_variant_unref(value);
	g_variant_iter_free(iter);

	BT_DBG("-");
}

BT_EXPORT_API int bluetooth_gatt_read_descriptor_value(const char *char_descriptor)
{
	GDBusConnection *conn;
	char *handle;

	BT_DBG("+");
	BT_CHECK_PARAMETER(char_descriptor, return);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	handle = g_strdup(char_descriptor);

	g_dbus_connection_call(conn,
			BT_BLUEZ_NAME,
			char_descriptor,
			GATT_DESC_INTERFACE,
			"ReadValue",
			NULL,
			G_VARIANT_TYPE("(ay)"),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			(GAsyncReadyCallback)__bluetooth_internal_read_desc_cb,
			(gpointer)handle);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static void __bluetooth_internal_write_desc_cb(GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	GError *error = NULL;
	bt_user_info_t *user_info;
	GDBusConnection *system_gconn = NULL;
	GVariant *value;
	int result = BLUETOOTH_ERROR_NONE;

	BT_DBG("+");
	user_info = _bt_get_user_data(BT_COMMON);

	system_gconn = _bt_gdbus_get_system_gconn();
	value = g_dbus_connection_call_finish(system_gconn, res, &error);

	if (error) {
		BT_ERR("Error : %s \n", error->message);
		g_clear_error(&error);
		result = BLUETOOTH_ERROR_INTERNAL;
	}
	if (user_info) {
		_bt_common_event_cb(BLUETOOTH_EVENT_GATT_WRITE_DESC,
				result, NULL,
				user_info->cb, user_info->user_data);
	}

	if(value)
		g_variant_unref(value);

	BT_DBG("-");
}

BT_EXPORT_API int bluetooth_gatt_write_descriptor_value(
			const char *desc_handle, const guint8 *value, int length)
{
	GVariant *val;
	GDBusConnection *conn;
	GVariantBuilder *builder;
	int i;

	BT_DBG("+");
	BT_CHECK_PARAMETER(desc_handle, return);
	BT_CHECK_PARAMETER(value, return);
	retv_if(length == 0, BLUETOOTH_ERROR_INVALID_PARAM);
	BT_CHECK_ENABLED_ANY(return);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("ay"));

	for (i = 0; i < length; i++) {
		g_variant_builder_add(builder, "y", value[i]);
	}

	val = g_variant_new("(ay)", builder);

	g_dbus_connection_call(conn,
				BT_BLUEZ_NAME,
				desc_handle,
				GATT_DESC_INTERFACE,
				"WriteValue",
				val,
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL,
				(GAsyncReadyCallback)__bluetooth_internal_write_desc_cb,
				NULL);

	g_variant_builder_unref(builder);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_watch_characteristics(const char *char_handle)
{

	GDBusConnection *conn;
	GError *error = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	BT_DBG("+");
	BT_CHECK_PARAMETER(char_handle, return);

	BT_CHECK_ENABLED_ANY(return);

	BT_DBG("Entered characteristic handle:%s \n ", char_handle);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_connection_call_sync(conn,
			BT_BLUEZ_NAME,
			char_handle,
			GATT_CHAR_INTERFACE,
			"StartNotify",
			NULL,
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, &error);

	if (error) {
		BT_ERR("Watch Failed: %s", error->message);
		if (g_strrstr(error->message, "Already notifying"))
			ret = BLUETOOTH_ERROR_NONE;
		else if (g_strrstr(error->message, "In Progress"))
			ret = BLUETOOTH_ERROR_IN_PROGRESS;
		else if (g_strrstr(error->message, "Operation is not supported"))
			ret = BLUETOOTH_ERROR_NOT_SUPPORT;
		else
			ret = BLUETOOTH_ERROR_INTERNAL;

		g_clear_error(&error);
	}
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_gatt_unwatch_characteristics(const char *char_handle)
{

	GDBusConnection *conn;
	GError *error = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	BT_DBG("+");
	BT_CHECK_PARAMETER(char_handle, return);

	BT_CHECK_ENABLED_ANY(return);

	BT_DBG("Entered characteristic handle:%s \n ", char_handle);

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	g_dbus_connection_call_sync(conn,
			BT_BLUEZ_NAME,
			char_handle,
			GATT_CHAR_INTERFACE,
			"StopNotify",
			NULL,
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, &error);

	if (error) {
		BT_ERR("Watch Failed: %s", error->message);
		g_clear_error(&error);
		ret =  BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return ret;
}
