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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <security-server.h>
#include <sys/socket.h>


#include <gio/gunixfdlist.h>


#include "bluetooth-api.h"
#include "bluetooth-audio-api.h"
#include "bluetooth-hid-api.h"
#include "bluetooth-media-control.h"
#include "bt-internal-types.h"
#include "bluetooth-ipsp-api.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

static bt_user_info_t user_info[BT_MAX_USER_INFO];
static DBusConnection *system_conn = NULL;
static GDBusConnection *system_gdbus_conn = NULL;

static char *cookie;
static size_t cookie_size;

static guint bus_id;

static GDBusConnection *system_gconn = NULL;

#define DBUS_TIMEOUT 20 * 1000 /* 20 Seconds */

GDBusConnection *g_bus_get_private_conn(void)
{
	GError *error = NULL;
	char *address;
	GDBusConnection *private_gconn = NULL;

	address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (address == NULL) {
		if (error) {
			BT_ERR ("Failed to get bus address: %s", error->message);
			g_clear_error(&error);
		}
		return NULL;
	}

	private_gconn = g_dbus_connection_new_for_address_sync (address,
				G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
				G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
				NULL, /* GDBusAuthObserver */
				NULL,
				&error);
	if (!private_gconn) {
		if (error) {
			BT_ERR("Unable to connect to dbus: %s", error->message);
			g_clear_error(&error);
		}
		return NULL;
	}

	return private_gconn;
}

GDBusConnection *_bt_gdbus_init_system_gconn(void)
{
	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	dbus_g_thread_init();

	g_type_init();

	if (system_gconn != NULL)
		return system_gconn;

	system_gconn = g_bus_get_private_conn();

	return system_gconn;
}

GDBusConnection *_bt_gdbus_get_system_gconn(void)
{
	if (system_gconn == NULL) {
		system_gconn = _bt_gdbus_init_system_gconn();
	} else if (g_dbus_connection_is_closed(system_gconn)){
		system_gconn = g_bus_get_private_conn();
	}

	return system_gconn;
}

void _bt_print_device_address_t(const bluetooth_device_address_t *addr)
{
	BT_DBG("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n", addr->addr[0], addr->addr[1], addr->addr[2],
				addr->addr[3], addr->addr[4], addr->addr[5]);
}

void _bt_set_user_data(int type, void *callback, void *user_data)
{
	user_info[type].cb = callback;
	user_info[type].user_data = user_data;
}

bt_user_info_t *_bt_get_user_data(int type)
{
	return &user_info[type];
}

void _bt_common_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	bluetooth_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;

	if (callback)
		((bluetooth_cb_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}

void _bt_input_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	hid_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;

	if (callback)
		((hid_cb_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}

void _bt_headset_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	bt_audio_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;

	if (callback)
		((bt_audio_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}

void _bt_a2dp_source_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	bt_audio_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;
	if (callback)
		((bt_audio_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}

void _bt_hf_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	bt_hf_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;

	if (callback)
		((bt_hf_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}


void _bt_avrcp_event_cb(int event, int result, void *param,
					void *callback, void *user_data)
{
	media_event_param_t bt_event = { 0, };
	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param;

	if (callback)
		((media_cb_func_ptr)callback)(bt_event.event, &bt_event,
					user_data);
}

void _bt_divide_device_class(bluetooth_device_class_t *device_class,
				unsigned int cod)
{
	ret_if(device_class == NULL);

	device_class->major_class = (unsigned short)(cod & 0x00001F00) >> 8;
	device_class->minor_class = (unsigned short)((cod & 0x000000FC));
	device_class->service_class = (unsigned long)((cod & 0x00FF0000));

	if (cod & 0x002000) {
		device_class->service_class |=
		BLUETOOTH_DEVICE_SERVICE_CLASS_LIMITED_DISCOVERABLE_MODE;
	}
}

void _bt_convert_addr_string_to_type(unsigned char *addr,
					const char *address)
{
        int i;
        char *ptr = NULL;

	ret_if(address == NULL);
	ret_if(addr == NULL);

        for (i = 0; i < BT_ADDRESS_LENGTH_MAX; i++) {
                addr[i] = strtol(address, &ptr, 16);
                if (ptr[0] != '\0') {
                        if (ptr[0] != ':')
                                return;

                        address = ptr + 1;
                }
        }
}

void _bt_convert_addr_type_to_string(char *address,
				unsigned char *addr)
{
	ret_if(address == NULL);
	ret_if(addr == NULL);

	g_snprintf(address, BT_ADDRESS_STRING_SIZE,
			"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
			addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]);
}

int _bt_copy_utf8_string(char *dest, const char *src, unsigned int length)
{
	int i;
	const char *p = src;
	char *next;
	int count;

	if (dest == NULL || src == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	i = 0;
	while (*p != '\0' && i < length) {
		next = g_utf8_next_char(p);
		count = next - p;

		while (count > 0 && ((i + count) < length)) {
			dest[i++] = *p;
			p++;
			count --;
		}
		p = next;
	}
	return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_utf8_validate(char *name)
{
	BT_DBG("+");
	gunichar2 *u16;
	glong items_written = 0;

	if (FALSE == g_utf8_validate(name, -1, NULL))
		return FALSE;

	u16 = g_utf8_to_utf16(name, -1, NULL, &items_written, NULL);
	if (u16 == NULL)
		return FALSE;

	g_free(u16);

	if (items_written != g_utf8_strlen(name, -1))
		return FALSE;

	BT_DBG("-");
	return TRUE;
}


static GDBusProxy *profile_gproxy;
static GDBusConnection *gconn;
static int latest_id = -1;
#define BT_RFCOMM_ID_MAX 245
static gboolean id_used[BT_RFCOMM_ID_MAX];
GDBusNodeInfo *new_conn_node;

static const gchar rfcomm_agent_xml[] =
"<node name='/'>"
" <interface name='org.bluez.Profile1'>"
"     <method name='NewConnection'>"
"          <arg type='o' name='object' direction='in'/>"
"          <arg type='h' name='fd' direction='in'/>"
"          <arg type='a{sv}' name='properties' direction='in'/>"
"     </method>"
"     <method name='RequestDisconnection'>"
"          <arg type='o' name='device' direction='in'/>"
"     </method>"
"  </interface>"
"</node>";

static void __new_connection_method(GDBusConnection *connection,
					    const gchar *sender,
					    const gchar *object_path,
					    const gchar *interface_name,
					    const gchar *method_name,
					    GVariant *parameters,
					    GDBusMethodInvocation *invocation,
					    gpointer user_data)
{
	BT_DBG("method %s", method_name);
	if (g_strcmp0(method_name, "NewConnection") == 0) {
		int index;
		GDBusMessage *msg;
		GUnixFDList *fd_list;
		GVariantBuilder *properties;
		char *obj_path;
		char addr[20];
		bluetooth_device_address_t  remote_addr1;
		bt_new_connection_cb cb = user_data;
		int fd;

		g_variant_get(parameters, "(oha{sv})", &obj_path, &index,
								&properties);

		msg = g_dbus_method_invocation_get_message(invocation);
		fd_list = g_dbus_message_get_unix_fd_list(msg);
		if (fd_list == NULL) {
			GQuark quark = g_quark_from_string("rfcomm-app");
			GError *err = g_error_new(quark, 0, "No fd in message");
			g_dbus_method_invocation_return_gerror(invocation, err);
			g_error_free(err);
			return;
		}


		fd = g_unix_fd_list_get(fd_list, index, NULL);
		if (fd == -1) {
			BT_ERR("Invalid fd return");
			GQuark quark = g_quark_from_string("rfcomm-app");
			GError *err = g_error_new(quark, 0, "Invalid FD return");
			g_dbus_method_invocation_return_gerror(invocation, err);
			g_error_free(err);
			return;
		}
		BT_INFO("Object Path %s", obj_path);

		_bt_device_path_to_address(obj_path, addr);
		_bt_convert_addr_string_to_type(remote_addr1.addr, (const char *)addr);
		BT_INFO("fd: %d, address %s", fd, addr);

		g_dbus_method_invocation_return_value(invocation, NULL);

		if (cb)
			cb(object_path, fd, &remote_addr1);
	} else if (g_strcmp0(method_name, "RequestDisconnection") == 0) {
		g_dbus_method_invocation_return_value(invocation, NULL);
	}
}


static const GDBusInterfaceVTable method_table = {
	__new_connection_method,
	NULL,
	NULL,
};

void _bt_swap_addr(unsigned char *dst, const unsigned char *src)
{
	int i;

	for (i = 0; i < 6; i++)
		dst[i] = src[5-i];
}

int __rfcomm_assign_id(void)
{
	int index;

	BT_DBG("latest_id: %d", latest_id);

	index = latest_id + 1;

	if (index >= BT_RFCOMM_ID_MAX)
		index = 0;

	BT_DBG("index: %d", index);

	while (id_used[index] == TRUE) {
		if (index == latest_id) {
			/* No available ID */
			BT_ERR("All request ID is used");
			return -1;
		}

		index++;

		if (index >= BT_RFCOMM_ID_MAX)
			index = 0;
	}

	latest_id = index;
	id_used[index] = TRUE;

	BT_DBG("Assigned Id: %d", latest_id);

	return latest_id;
}

void __rfcomm_delete_id(int id)
{
	ret_if(id >= BT_RFCOMM_ID_MAX);
	ret_if(id < 0);

	id_used[id] = FALSE;

	/* Next server will use this ID */
	latest_id = id - 1;
}

static GDBusConnection *__get_gdbus_connection()
{
	if (gconn == NULL)
		gconn = g_bus_get_private_conn();

	return gconn;
}

static GDBusProxy *__bt_gdbus_get_profile_proxy(void)
{
	GDBusConnection *gconn;
	GError *err = NULL;

	if (profile_gproxy)
		return profile_gproxy;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return NULL;

	profile_gproxy = g_dbus_proxy_new_sync(gconn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_BLUEZ_NAME,
						"/org/bluez",
						"org.bluez.ProfileManager1",
						NULL, &err);
	if (err) {
		BT_ERR("Unable to create proxy: %s", err->message);
		g_clear_error(&err);
		return NULL;
	}

	return profile_gproxy;
}

static GDBusProxy *__bt_gdbus_get_device_proxy(char *object_path)
{
	GDBusConnection *gconn;
	GError *err = NULL;
	GDBusProxy *device_gproxy;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return NULL;

	device_gproxy = g_dbus_proxy_new_sync(gconn, G_DBUS_PROXY_FLAGS_NONE,
						NULL, BT_BLUEZ_NAME,
						object_path,
						BT_DEVICE_INTERFACE,
						NULL, &err);

	if (err) {
		BT_ERR("Unable to create proxy: %s", err->message);
		g_clear_error(&err);
		return NULL;
	}

	return device_gproxy;
}

void _bt_unregister_gdbus(int object_id)
{
	GDBusConnection *gconn;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return;

	g_dbus_connection_unregister_object(gconn, object_id);
}

int _bt_register_new_conn(const char *path, bt_new_connection_cb cb)
{
	GDBusConnection *gconn;
	int id;
	GError *error = NULL;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return -1;

	if (new_conn_node == NULL)
		new_conn_node = _bt_get_gdbus_node(rfcomm_agent_xml);

	if (new_conn_node == NULL)
		return -1;

	id = g_dbus_connection_register_object(gconn, path,
						new_conn_node->interfaces[0],
						&method_table,
						cb, NULL, &error);
	if (id == 0) {
		BT_ERR("Failed to register: %s", error->message);
		g_error_free(error);
		return -1;
	}

	BT_DBG("NEW CONNECTION ID %d", id);

	return id;
}

static GDBusProxy * __bt_gdbus_get_adapter_proxy()
{
	GError *err = NULL;
	GDBusProxy *manager_proxy = NULL;
	GDBusProxy *adapter_proxy = NULL;
	GDBusConnection *conn;
	GVariant *result = NULL;
	char *adapter_path = NULL;

	conn = __get_gdbus_connection();
	retv_if(conn == NULL, NULL);

	manager_proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			BT_MANAGER_PATH,
			BT_MANAGER_INTERFACE,
			NULL, &err);

	if (!manager_proxy) {
		BT_ERR("Unable to create proxy: %s", err->message);
		goto fail;
	}

	result = g_dbus_proxy_call_sync(manager_proxy, "DefaultAdapter", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
	if (!result) {
		if (err != NULL)
			BT_ERR("Fail to get DefaultAdapter (Error: %s)", err->message);
		else
			BT_ERR("Fail to get DefaultAdapter");

		goto fail;
	}

	if (g_strcmp0(g_variant_get_type_string(result), "(o)")) {
		BT_ERR("Incorrect result\n");
		goto fail;
	}

	g_variant_get(result, "(&o)", &adapter_path);

	if (adapter_path == NULL ||
		strlen(adapter_path) >= BT_ADAPTER_OBJECT_PATH_MAX) {
		BT_ERR("Adapter path is inproper\n");
		goto fail;
	}

	BT_INFO("Adapter Path %s", adapter_path);

	adapter_proxy = g_dbus_proxy_new_sync(conn,
					G_DBUS_PROXY_FLAGS_NONE, NULL,
					BT_BLUEZ_NAME,
					adapter_path,
					BT_ADAPTER_INTERFACE,
					NULL, &err);
	if (err) {
		BT_ERR("DBus Error message: [%s]", err->message);
		g_clear_error(&err);
	}

fail:
	if (manager_proxy)
		g_object_unref(manager_proxy);
	if (result)
		g_variant_unref(result);
	return adapter_proxy;
}

int _bt_register_new_conn_ex(const char *path, const char *bus_name,bt_new_connection_cb cb)
{
	GDBusConnection *gconn;
	int id;
	GError *error = NULL;

	gconn = __get_gdbus_connection();
	if (gconn == NULL)
		return -1;

	if (new_conn_node == NULL)
		new_conn_node = _bt_get_gdbus_node_ex(rfcomm_agent_xml, bus_name);

	if (new_conn_node == NULL)
		return -1;

	id = g_dbus_connection_register_object(gconn, path,
						new_conn_node->interfaces[0],
						&method_table,
						cb, NULL, &error);
	if (id == 0) {
		BT_ERR("Failed to register: %s", error->message);
		g_error_free(error);
		return -1;
	}

	BT_DBG("NEW CONNECTION ID %d", id);

	return id;
}

int _bt_register_profile(bt_register_profile_info_t *info, gboolean use_default_rfcomm)
{
	GVariantBuilder *option_builder;
	GVariant *ret;
	GDBusProxy *proxy;
	GError *err = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	proxy = __bt_gdbus_get_profile_proxy();
	if (proxy == NULL) {
		BT_ERR("Getting profile proxy failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	option_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	if (info->authentication)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthentication",
						g_variant_new_boolean(TRUE));
	if (info->authorization)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthorization",
						g_variant_new_boolean(TRUE));
	if (info->role)
		g_variant_builder_add(option_builder, "{sv}",
						"Role",
						g_variant_new_string(info->role));

	/* Setting RFCOMM channel to default value 0; would allow bluez to assign
	 * RFCOMM channels based on the availability when two services want
	 * to use the RFCOMM along with SPP. Hence bluez makes sure that no
	 * two services use the same SPP RFCOMM channel. */
	if (use_default_rfcomm)
		g_variant_builder_add(option_builder, "{sv}",
						"Channel",
						g_variant_new_uint16(RFCOMM_DEFAULT_PROFILE_CHANNEL));
	if (info->service)
		g_variant_builder_add(option_builder, "{sv}",
						"Service",
						g_variant_new_string(info->service));

	ret = g_dbus_proxy_call_sync(proxy, "RegisterProfile",
					g_variant_new("(osa{sv})", info->obj_path,
								info->uuid,
								option_builder),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &err);
	if (err) {
		g_dbus_error_strip_remote_error(err);
		BT_ERR("RegisterProfile failed: %s", err->message);

		if (g_strrstr(err->message, BT_ACCESS_DENIED_MSG))
			result = BLUETOOTH_ERROR_ACCESS_DENIED;
		else
			result = BLUETOOTH_ERROR_INTERNAL;

		g_clear_error(&err);
	}

	g_variant_builder_unref(option_builder);

	if (ret)
		g_variant_unref(ret);

	return result;
}

int _bt_register_profile_ex(bt_register_profile_info_t *info, gboolean use_default_rfcomm, const char *name, const char *path)
{
	GVariantBuilder *option_builder;
	GVariant *ret;
	GDBusProxy *proxy;
	GError *err = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	proxy = __bt_gdbus_get_profile_proxy();
	if (proxy == NULL) {
		BT_ERR("Getting profile proxy failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	option_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	if (info->authentication)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthentication",
						g_variant_new_boolean(TRUE));
	if (info->authorization)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthorization",
						g_variant_new_boolean(TRUE));
	if (info->role)
		g_variant_builder_add(option_builder, "{sv}",
						"Role",
						g_variant_new_string(info->role));

	/* Setting RFCOMM channel to default value 0; would allow bluez to assign
	 * RFCOMM channels based on the availability when two services want
	 * to use the RFCOMM along with SPP. Hence bluez makes sure that no
	 * two services use the same SPP RFCOMM channel. */
	if (use_default_rfcomm)
		g_variant_builder_add(option_builder, "{sv}",
						"Channel",
						g_variant_new_uint16(RFCOMM_DEFAULT_PROFILE_CHANNEL));
	if (info->service)
		g_variant_builder_add(option_builder, "{sv}",
						"Service",
						g_variant_new_string(info->service));

	ret = g_dbus_proxy_call_sync(proxy, "RegisterProfile2",
					g_variant_new("(osssa{sv})", info->obj_path,
								info->uuid,
								name,
								path,
								option_builder),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &err);
	if (err) {
		g_dbus_error_strip_remote_error(err);
		BT_ERR("RegisterProfile failed: %s", err->message);

		if (g_strrstr(err->message, BT_ACCESS_DENIED_MSG))
			result = BLUETOOTH_ERROR_ACCESS_DENIED;
		else
			result = BLUETOOTH_ERROR_INTERNAL;

		g_clear_error(&err);
	}

	g_variant_builder_unref(option_builder);

	if (ret)
		g_variant_unref(ret);

	return result;
}

int _bt_register_profile_platform(bt_register_profile_info_t *info, gboolean use_default_rfcomm)
{
	GVariantBuilder *option_builder;
	GVariant *ret;
	GDBusProxy *proxy;
	GError *err = NULL;
	int result = BLUETOOTH_ERROR_NONE;

	proxy = __bt_gdbus_get_profile_proxy();
	if (proxy == NULL) {
		BT_ERR("Getting profile proxy failed");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	option_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	if (info->authentication)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthentication",
						g_variant_new_boolean(TRUE));
	if (info->authorization)
		g_variant_builder_add(option_builder, "{sv}",
						"RequireAuthorization",
						g_variant_new_boolean(TRUE));
	if (info->role)
		g_variant_builder_add(option_builder, "{sv}",
						"Role",
						g_variant_new_string(info->role));

	/* Setting RFCOMM channel to default value 0; would allow bluez to assign
	 * RFCOMM channels based on the availability when two services want
	 * to use the RFCOMM along with SPP. Hence bluez makes sure that no
	 * two services use the same SPP RFCOMM channel. */
	if (use_default_rfcomm)
		g_variant_builder_add(option_builder, "{sv}",
						"Channel",
						g_variant_new_uint16(RFCOMM_DEFAULT_PROFILE_CHANNEL));
	if (info->service)
		g_variant_builder_add(option_builder, "{sv}",
						"Service",
						g_variant_new_string(info->service));

	ret = g_dbus_proxy_call_sync(proxy, "RegisterProfile1",
					g_variant_new("(osa{sv})", info->obj_path,
								info->uuid,
								option_builder),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &err);

	if (err) {
		g_dbus_error_strip_remote_error(err);
		BT_ERR("RegisterProfile failed: %s", err->message);

		if (g_strrstr(err->message, BT_ACCESS_DENIED_MSG))
			result = BLUETOOTH_ERROR_ACCESS_DENIED;
		else
			result = BLUETOOTH_ERROR_INTERNAL;

		g_clear_error(&err);
	}

	g_variant_builder_unref(option_builder);

	if (ret)
		g_variant_unref(ret);

	return result;
}


void _bt_unregister_profile(char *path)
{
	GVariant *ret;
	GDBusProxy *proxy;
	GError *err = NULL;

	proxy = __bt_gdbus_get_profile_proxy();
	if (proxy == NULL) {
		BT_ERR("Getting profile proxy failed");
		return;
	}

	ret = g_dbus_proxy_call_sync(proxy, "UnregisterProfile",
			g_variant_new("(o)", path),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &err);
	if (err) {
		BT_ERR("UnregisterProfile failed : %s", err->message);
		g_clear_error(&err);
	}

	if (ret)
		g_variant_unref(ret);

	return;
}

GDBusNodeInfo * _bt_get_gdbus_node(const gchar *xml_data)
{
	if (bus_id == 0) {
		char *name = g_strdup_printf("org.bt.frwk%d", getpid());

		bus_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
						name,
						G_BUS_NAME_OWNER_FLAGS_NONE,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL);
		BT_DBG("Got bus id %d", bus_id);
		g_free(name);
	}

	return g_dbus_node_info_new_for_xml(xml_data, NULL);
}

GDBusNodeInfo * _bt_get_gdbus_node_ex(const gchar *xml_data, const char *bus_name)
{
	if (bus_id == 0) {
		char *name = g_strdup(bus_name);
		bus_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
						name,
						G_BUS_NAME_OWNER_FLAGS_NONE,
						NULL,
						NULL,
						NULL,
						NULL,
						NULL);
		BT_DBG("Got bus id %d", bus_id);
		g_free(name);
	}

	return g_dbus_node_info_new_for_xml(xml_data, NULL);
}

int _bt_connect_profile(char *address, char *uuid, void *cb,
							gpointer func_data)
{
	GDBusProxy *proxy;
	GDBusProxy *adapter_proxy;
	char *object_path;
	GError *err = NULL;

	object_path = _bt_get_device_object_path(address);

	if (object_path == NULL) {
		GVariant *ret = NULL;
		BT_ERR("No searched device");
		adapter_proxy = __bt_gdbus_get_adapter_proxy();

		if (adapter_proxy == NULL) {
			BT_ERR("adapter proxy is NULL");
			return BLUETOOTH_ERROR_INTERNAL;
		}

		ret = g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
				g_variant_new("(s)", address),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&err);

		if (err != NULL) {
			BT_ERR("CreateDevice Failed: %s", err->message);
			g_clear_error(&err);
		}
		if (ret)
			g_variant_unref(ret);
		g_object_unref(adapter_proxy);
		object_path = _bt_get_device_object_path(address);

		if (object_path == NULL)
			return BLUETOOTH_ERROR_INTERNAL;
	}

	proxy = __bt_gdbus_get_device_proxy(object_path);
	g_free(object_path);

	if (proxy == NULL) {
		BT_ERR("Error while getting proxy");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_dbus_proxy_call(proxy, "ConnectProfile",
			g_variant_new("(s)", uuid),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			(GAsyncReadyCallback)cb,
			func_data);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_discover_services(char *address, char *uuid, void *cb,
		gpointer func_data)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusProxy *adapter_proxy;
	GError *err = NULL;
	object_path = _bt_get_device_object_path(address);
	if (object_path == NULL) {
		GVariant *ret = NULL;
		BT_ERR("No searched device");
		adapter_proxy = __bt_gdbus_get_adapter_proxy();
		retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
		ret = g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
				g_variant_new("(s)", address),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&err);
		if (err != NULL) {
			BT_ERR("CreateDevice Failed: %s", err->message);
			g_clear_error(&err);
		}
		if (ret)
			g_variant_unref(ret);

		g_object_unref(adapter_proxy);

		object_path = _bt_get_device_object_path(address);
		if (object_path == NULL)
			return BLUETOOTH_ERROR_INTERNAL;
	}
	proxy = __bt_gdbus_get_device_proxy(object_path);
	g_free(object_path);
	if (proxy == NULL) {
		BT_ERR("Error while getting proxy");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_dbus_proxy_call(proxy, "DiscoverServices",
			g_variant_new("(s)", uuid),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			(GAsyncReadyCallback)cb,
			func_data);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_cancel_discovers(char *address)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusProxy *adapter_proxy;
	GError *err = NULL;
	object_path = _bt_get_device_object_path(address);
	if (object_path == NULL) {
		GVariant *ret = NULL;
		BT_ERR("No searched device");
		adapter_proxy = __bt_gdbus_get_adapter_proxy();
		retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
		ret = g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
				g_variant_new("(s)", address),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&err);
		if (err != NULL) {
			BT_ERR("CreateDevice Failed: %s", err->message);
			g_clear_error(&err);
		}
		if (ret)
			g_variant_unref(ret);

		g_object_unref(adapter_proxy);

		object_path = _bt_get_device_object_path(address);
		if (object_path == NULL)
			return BLUETOOTH_ERROR_INTERNAL;
	}
	proxy = __bt_gdbus_get_device_proxy(object_path);
	g_free(object_path);
	g_dbus_proxy_call_sync(proxy, "CancelDiscovery",
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		DBUS_TIMEOUT, NULL,
		&err);
	if (err) {
		BT_ERR("DBus Error message: [%s]", err->message);
		g_clear_error(&err);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	if (proxy)
		g_object_unref(proxy);
	return BLUETOOTH_ERROR_NONE;
}

int _bt_discover_service_uuids(char *address, char *remote_uuid)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusConnection *gconn;
	GError *err = NULL;
	char **uuid_value = NULL;
	gsize size = 0;
	int i =0;
	GVariant *value = NULL;
	GVariant *ret = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL;
	BT_INFO("+");
	retv_if(remote_uuid == NULL, BLUETOOTH_ERROR_INTERNAL);
	gconn = __get_gdbus_connection();
	retv_if(gconn == NULL, BLUETOOTH_ERROR_INTERNAL);
	object_path = _bt_get_device_object_path(address);
	retv_if(object_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy = g_dbus_proxy_new_sync(gconn, G_DBUS_PROXY_FLAGS_NONE, NULL,
				BT_BLUEZ_NAME, object_path, BT_PROPERTIES_INTERFACE, NULL,
				&err);
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	if (err) {
		BT_ERR("DBus Error: [%s]", err->message);
		g_clear_error(&err);
	}
	ret = g_dbus_proxy_call_sync(proxy, "GetAll",
			g_variant_new("(s)", BT_DEVICE_INTERFACE),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&err);
	if (err) {
		result = BLUETOOTH_ERROR_INTERNAL;
		BT_ERR("DBus Error : %s", err->message);
		g_clear_error(&err);
		goto done;
	}
	if (ret == NULL) {
		BT_ERR("g_dbus_proxy_call_sync function return NULL");
		result = BLUETOOTH_ERROR_INTERNAL;
		goto done;
	}

	g_variant_get(ret, "(@a{sv})", &value);
	g_variant_unref(ret);
	if (value) {
		GVariant *temp_value = g_variant_lookup_value(value, "UUIDs",
			G_VARIANT_TYPE_STRING_ARRAY);

		if (temp_value) {
			size = g_variant_get_size(temp_value);
			if (size > 0) {
				uuid_value = (char **)g_variant_get_strv(temp_value, &size);
				BT_DBG("Size items %d", size);

				if (uuid_value) {
					for (i = 0; uuid_value[i] != NULL; i++) {
						BT_DBG("Remote uuids %s", uuid_value[i]);
						if (strcasecmp(uuid_value[i], remote_uuid) == 0) {
							result = BLUETOOTH_ERROR_NONE;
							g_variant_unref(temp_value);
							goto done;
						}
					}
				}
			}
			g_variant_unref(temp_value);
		}
	}
done:
	if (proxy)
		g_object_unref(proxy);
	if (value)
		g_variant_unref(value);
	if (uuid_value)
		g_free(uuid_value);

	BT_DBG("-");
	return result;
}

int _bt_get_cod_by_address(char *address, bluetooth_device_class_t *dev_class)
{
	char *object_path;
	GDBusProxy *proxy;
	GDBusConnection *gconn;
	GError *err = NULL;
	GVariant *value = NULL;
	GVariant *result = NULL;
	unsigned int  class = 0x00;
	int ret = BLUETOOTH_ERROR_INTERNAL;

	gconn = __get_gdbus_connection();
	retv_if(gconn == NULL, BLUETOOTH_ERROR_INTERNAL);
		object_path = _bt_get_device_object_path(address);

	retv_if(object_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy = g_dbus_proxy_new_sync(gconn, G_DBUS_PROXY_FLAGS_NONE, NULL,
				BT_BLUEZ_NAME, object_path, BT_PROPERTIES_INTERFACE, NULL,
				&err);
	retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
	if (err) {
		BT_ERR("DBus Error: [%s]", err->message);
		g_clear_error(&err);
	}

	result = g_dbus_proxy_call_sync(proxy, "GetAll",
			g_variant_new("(s)", BT_DEVICE_INTERFACE),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			&err);
	if (err) {
		ret = BLUETOOTH_ERROR_INTERNAL;
		BT_ERR("DBus Error : %s", err->message);
		g_clear_error(&err);
		goto done;
	}
	if (result == NULL) {
		BT_ERR("g_dbus_proxy_call_sync function return NULL");
		ret = BLUETOOTH_ERROR_INTERNAL;
		goto done;
	}
	g_variant_get(result, "(@a{sv})", &value);
	g_variant_unref(result);
	if (value) {
		GVariant *temp_value = g_variant_lookup_value(value, "Class",
			G_VARIANT_TYPE_UINT32);
		class = g_variant_get_uint32(temp_value);
		_bt_divide_device_class(dev_class, class);
		if (temp_value)
			g_variant_unref(temp_value);
	}

done:
	if (proxy)
		g_object_unref(proxy);
	if (value)
		g_variant_unref(value);

	BT_DBG("-");
	return ret;
}

int _bt_disconnect_profile(char *address, char *uuid, void *cb,
							gpointer func_data)
{
	GDBusProxy *proxy;
	char *object_path;
	GError *err = NULL;
	GDBusProxy *adapter_proxy;
	object_path = _bt_get_device_object_path(address);
	if (object_path == NULL) {
		GVariant *ret = NULL;
		BT_ERR("No searched device");
		adapter_proxy = __bt_gdbus_get_adapter_proxy();
		retv_if(adapter_proxy == NULL, BLUETOOTH_ERROR_INTERNAL);
		ret = g_dbus_proxy_call_sync(adapter_proxy, "CreateDevice",
				g_variant_new("(s)", address),
				G_DBUS_CALL_FLAGS_NONE,
				DBUS_TIMEOUT, NULL,
				&err);
		if (err != NULL) {
			BT_ERR("CreateDevice Failed: %s", err->message);
			g_error_free(err);
		}
		if (ret)
			g_variant_unref(ret);
		g_object_unref(adapter_proxy);
		object_path = _bt_get_device_object_path(address);
		if (object_path == NULL)
			return BLUETOOTH_ERROR_INTERNAL;
	}
	proxy = __bt_gdbus_get_device_proxy(object_path);
	g_free(object_path);
	if (proxy == NULL) {
		BT_ERR("Error while getting proxy");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_dbus_proxy_call(proxy, "DisconnectProfile",
			g_variant_new("(s)", uuid),
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_TIMEOUT, NULL,
			(GAsyncReadyCallback)cb,
			func_data);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_get_adapter_path(GDBusConnection *conn, char *path)
{
	GError *err = NULL;
	GDBusProxy *manager_proxy = NULL;
	GVariant *result = NULL;
	char *adapter_path = NULL;

	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	manager_proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			BT_MANAGER_PATH,
			BT_MANAGER_INTERFACE,
			NULL, &err);

	if (!manager_proxy) {
		BT_ERR("Unable to create proxy: %s", err->message);
		goto fail;
	}

	result = g_dbus_proxy_call_sync(manager_proxy, "DefaultAdapter", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
	if (!result) {
		if (err != NULL)
			BT_ERR("Fail to get DefaultAdapter (Error: %s)", err->message);
		else
			BT_ERR("Fail to get DefaultAdapter");

		goto fail;
	}

	if (g_strcmp0(g_variant_get_type_string(result), "(o)")) {
		BT_ERR("Incorrect result\n");
		goto fail;
	}

	g_variant_get(result, "(&o)", &adapter_path);

	if (adapter_path == NULL ||
		strlen(adapter_path) >= BT_ADAPTER_OBJECT_PATH_MAX) {
		BT_ERR("Adapter path is inproper\n");
		goto fail;
	}

	if (path)
		g_strlcpy(path, adapter_path, BT_ADAPTER_OBJECT_PATH_MAX);

	g_variant_unref(result);
	g_object_unref(manager_proxy);

	return BLUETOOTH_ERROR_NONE;

fail:
	g_clear_error(&err);

	if (result)
		g_variant_unref(result);

	if (manager_proxy)
		g_object_unref(manager_proxy);

	return BLUETOOTH_ERROR_INTERNAL;

}

void _bt_convert_device_path_to_address(const char *device_path,
						char *device_address)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *dev_addr;

	ret_if(device_path == NULL);
	ret_if(device_address == NULL);

	dev_addr = strstr(device_path, "dev_");
	if (dev_addr != NULL) {
		char *pos = NULL;
		dev_addr += 4;
		g_strlcpy(address, dev_addr, sizeof(address));

		while ((pos = strchr(address, '_')) != NULL) {
			*pos = ':';
		}

		g_strlcpy(device_address, address, BT_ADDRESS_STRING_SIZE);
	}
}

static char *__bt_extract_device_path(GVariantIter *iter, char *address)
{
	char *object_path = NULL;
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	/* Parse the signature:  oa{sa{sv}}} */
	while (g_variant_iter_loop(iter, "{&oa{sa{sv}}}", &object_path,
			NULL)) {
		retv_if(object_path == NULL, NULL);
		_bt_convert_device_path_to_address(object_path, device_address);

		if (g_strcmp0(address, device_address) == 0) {
			return g_strdup(object_path);
		}
	}
	return NULL;
}

char *_bt_get_device_object_path(char *address)
{
	GError *err = NULL;
	GDBusProxy *proxy = NULL;
	GVariant *result = NULL;
	GVariantIter *iter = NULL;
	GDBusConnection *conn = NULL;
	char *object_path = NULL;

	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, NULL);

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			BT_MANAGER_PATH,
			BT_MANAGER_INTERFACE,
			NULL, &err);

	if (!proxy) {
		BT_ERR("Unable to create proxy: %s", err->message);
		goto fail;
	}

	result = g_dbus_proxy_call_sync(proxy, "GetManagedObjects", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
	if (!result) {
		if (err != NULL)
			BT_ERR("Fail to get GetManagedObjects (Error: %s)", err->message);
		else
			BT_ERR("Fail to get GetManagedObjects");

		goto fail;
	}

	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
	object_path = __bt_extract_device_path(iter, address);

	g_variant_unref(result);
	g_object_unref(proxy);
	g_variant_iter_free(iter);
	return object_path;

fail:
	g_clear_error(&err);

	if (proxy)
		g_object_unref(proxy);

	return object_path;
}


DBusGProxy *_bt_get_adapter_proxy(DBusGConnection *conn)
{
	GError *err = NULL;
	DBusGProxy *manager_proxy = NULL;
	DBusGProxy *adapter_proxy = NULL;
	char *adapter_path = NULL;

	retv_if(conn == NULL, NULL);

	manager_proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				BT_MANAGER_PATH, BT_MANAGER_INTERFACE);

	retv_if(manager_proxy == NULL, NULL);

	if (!dbus_g_proxy_call(manager_proxy, "DefaultAdapter", &err,
				G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
				&adapter_path,
				G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting DefaultAdapter failed: [%s]\n", err->message);
			g_error_free(err);
		}
		g_object_unref(manager_proxy);
		return NULL;
	}

	if (adapter_path == NULL || strlen(adapter_path) >= BT_ADAPTER_OBJECT_PATH_MAX) {
		BT_ERR("Adapter path is inproper\n");
		g_free(adapter_path);
		g_object_unref(manager_proxy);
		return NULL;
	}

	adapter_proxy = dbus_g_proxy_new_for_name(conn,
					BT_BLUEZ_NAME,
					adapter_path,
					BT_ADAPTER_INTERFACE);
	g_free(adapter_path);
	g_object_unref(manager_proxy);

	return adapter_proxy;
}

void _bt_device_path_to_address(const char *device_path, char *device_address)
{
	char address[BT_ADDRESS_STRING_SIZE] = { 0 };
	char *dev_addr = NULL;

	if (!device_path || !device_address)
		return;

	dev_addr = strstr(device_path, "dev_");
	if (dev_addr != NULL) {
		char *pos = NULL;
		dev_addr += 4;
		g_strlcpy(address, dev_addr, sizeof(address));

		while ((pos = strchr(address, '_')) != NULL) {
			*pos = ':';
		}

		g_strlcpy(device_address, address, BT_ADDRESS_STRING_SIZE);
	}
}

DBusConnection *__bt_init_system_conn(void)
{
	if (system_conn == NULL)
		system_conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, NULL);

	if (system_conn) {
		dbus_connection_setup_with_g_main(system_conn, NULL);
		dbus_connection_set_exit_on_disconnect(system_conn, FALSE);
	}

	return system_conn;
}

DBusConnection *_bt_get_system_conn(void)
{
	DBusConnection *conn = NULL;

	if (system_conn == NULL) {
		conn = __bt_init_system_conn();
	} else {
		conn = system_conn;
	}

	return conn;
}

void _bt_generate_cookie(void)
{
	int retval;

	ret_if(cookie != NULL);

	cookie_size = security_server_get_cookie_size();

	cookie = g_malloc0((cookie_size*sizeof(char))+1);

	retval = security_server_request_cookie(cookie, cookie_size);
	if(retval < 0) {
		BT_ERR("Fail to get cookie: %d", retval);
	}
}

void _bt_destroy_cookie(void)
{
	g_free(cookie);
	cookie = NULL;
	cookie_size = 0;
}

char *_bt_get_cookie(void)
{
	return cookie;
}

int _bt_get_cookie_size(void)
{
	return cookie_size;
}

int _bt_register_osp_server_in_agent(int type, char *uuid, char *path, int fd)
{
	int ret;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX] = { 0, };
	char path_str[BLUETOOTH_PATH_STRING] = { 0, };

	BT_DBG("+");
	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &type, sizeof(int));
	g_strlcpy(uuid_str, uuid, sizeof(uuid_str));
	g_array_append_vals(in_param2, &uuid_str, BLUETOOTH_UUID_STRING_MAX);
	g_strlcpy(path_str, path, sizeof(path_str));
	g_array_append_vals(in_param3, &path_str, BLUETOOTH_PATH_STRING);
	g_array_append_vals(in_param4, &fd, sizeof(int));

	ret =  _bt_send_request(BT_AGENT_SERVICE, BT_SET_AUTHORIZATION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	BT_DBG("-");
	return ret;
}

int _bt_unregister_osp_server_in_agent(int type, char *uuid)
{
	int ret;
	char uuid_str[BLUETOOTH_UUID_STRING_MAX] = { 0, };

	BT_DBG("+");
	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &type, sizeof(int));
	g_strlcpy(uuid_str, uuid, sizeof(uuid_str));
	g_array_append_vals(in_param2, &uuid_str, BLUETOOTH_UUID_STRING_MAX);

	ret =  _bt_send_request(BT_AGENT_SERVICE, BT_UNSET_AUTHORIZATION,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);
	BT_DBG("-");
	return ret;
}

int _bt_check_privilege(int service_type, int service_function)
{
	int result;

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(service_type, service_function,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

GVariant *_bt_get_managed_objects(void)
{
	GDBusConnection *g_conn;
	GDBusProxy *manager_proxy = NULL;
	GVariant *result = NULL;
	GError *error = NULL;

	BT_DBG("+");

	g_conn = _bt_gdbus_get_system_gconn();
	retv_if(g_conn == NULL, NULL);

	manager_proxy = g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME,
			BT_MANAGER_PATH,
			BT_MANAGER_INTERFACE,
			NULL, &error);

	if (error) {
		BT_ERR("Unable to create proxy: %s", error->message);
		g_clear_error(&error);
		return NULL;
	}

	result = g_dbus_proxy_call_sync (manager_proxy,
			"GetManagedObjects", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &error);

	if (error) {
		BT_ERR("Fail to get ManagedObjects (Error: %s)", error->message);
		g_clear_error(&error);
	}

	g_object_unref(manager_proxy);

	BT_DBG("-");
	return result;
}

BT_EXPORT_API int bluetooth_is_supported(void)
{
	int is_supported = 0;
	int len = 0;
	int fd = -1;
	rfkill_event event;

	fd = open(RFKILL_NODE, O_RDONLY);
	if (fd < 0) {
		BT_ERR("Fail to open RFKILL node");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		BT_ERR("Fail to set RFKILL node to non-blocking");
		close(fd);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	while (1) {
		len = read(fd, &event, sizeof(event));
		if (len < 0) {
			BT_ERR("Fail to read events");
			break;
		}

		if (len != RFKILL_EVENT_SIZE) {
			BT_ERR("The size is wrong\n");
			continue;
		}

		if (event.type == RFKILL_TYPE_BLUETOOTH) {
			is_supported = 1;
			break;
		}
	}

	close(fd);

	BT_DBG("supported: %d", is_supported);

	return is_supported;
}

BT_EXPORT_API int bluetooth_register_callback(bluetooth_cb_func_ptr callback_ptr, void *user_data)
{
	int ret;

	_bt_gdbus_init_system_gconn();
	__bt_init_system_conn();

	ret = _bt_init_event_handler();
	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to init the event handler");
		return ret;
	}

	_bt_generate_cookie();

	_bt_set_user_data(BT_COMMON, (void *)callback_ptr, user_data);

	/* Register All events */
	ret = _bt_register_event(BT_ADAPTER_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
	ret = _bt_register_event(BT_DEVICE_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
	ret = _bt_register_event(BT_NETWORK_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
	ret = _bt_register_event(BT_RFCOMM_CLIENT_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
	ret = _bt_register_event(BT_RFCOMM_SERVER_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
#ifdef GATT_NO_RELAY
	ret = _bt_register_event(BT_GATT_BLUEZ_EVENT, (void *)callback_ptr, user_data);
	if (ret != BLUETOOTH_ERROR_NONE)
		goto fail;
#endif

	_bt_register_name_owner_changed();

	return BLUETOOTH_ERROR_NONE;
fail:
	BT_ERR("Fail to do _bt_register_event()");
	bluetooth_unregister_callback();
	return ret;
}

BT_EXPORT_API int bluetooth_unregister_callback(void)
{
	int ret;

	_bt_destroy_cookie();

	ret = _bt_deinit_event_handler();
	if (ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to deinit the event handler");
	}

	_bt_unregister_name_owner_changed();

	_bt_set_user_data(BT_COMMON, NULL, NULL);

	if (system_conn) {
		dbus_connection_flush(system_conn);
		dbus_connection_close(system_conn);
		dbus_connection_unref(system_conn);
		system_conn = NULL;
	}
	if (system_gconn) {
		g_object_unref(system_gconn);
		system_gconn = NULL;
	}
	_bt_gdbus_deinit_proxys();
	return BLUETOOTH_ERROR_NONE;
}

