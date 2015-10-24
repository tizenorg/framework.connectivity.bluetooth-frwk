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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <dlog.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <net_connection.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <net_connection.h>
#include <bundle.h>
#include <eventsystem.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-agent.h"

static GDBusConnection *system_conn;
static GDBusConnection *session_conn;
static GDBusProxy *manager_proxy;
static GDBusProxy *adapter_proxy;
static void *net_conn;

static GDBusProxy *adapter_properties_proxy;

static GDBusConnection *system_gconn = NULL;

GDBusConnection *_bt_gdbus_init_system_gconn(void)
{
	GError *error = NULL;

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

//	dbus_g_thread_init();

	g_type_init();

	if (system_gconn != NULL)
		return system_gconn;

	system_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

	if (!system_gconn) {
		BT_ERR("Unable to connect to dbus: %s", error->message);
		g_clear_error(&error);
	}

	return system_gconn;
}

GDBusConnection *_bt_gdbus_get_system_gconn(void)
{
	GDBusConnection *local_system_gconn = NULL;
	GError *error = NULL;

	if (system_gconn == NULL) {
		system_gconn = _bt_gdbus_init_system_gconn();
	} else if (g_dbus_connection_is_closed(system_gconn)){

		local_system_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

		if (!local_system_gconn) {
			BT_ERR("Unable to connect to dbus: %s", error->message);
			g_clear_error(&error);
		}

		system_gconn = local_system_gconn;
	}

	return system_gconn;
}

static GDBusProxy *__bt_init_manager_proxy(void)
{
	GDBusProxy *proxy;

	g_type_init();

	if (system_conn == NULL) {
		system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
		retv_if(system_conn == NULL, NULL);
	}

	proxy = g_dbus_proxy_new_sync(system_conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								BT_MANAGER_PATH, BT_MANAGER_INTERFACE,  NULL, NULL);

	retv_if(proxy == NULL, NULL);

	manager_proxy = proxy;

	return proxy;
}

static GDBusProxy *__bt_init_adapter_proxy(void)
{
	GDBusProxy *manager_proxy;
	GDBusProxy *proxy;
	char *adapter_path = NULL;

	g_type_init();

	if (system_conn == NULL) {
		system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
		retv_if(system_conn == NULL, NULL);
	}

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, NULL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(system_conn, G_DBUS_PROXY_FLAGS_NONE,
								NULL, BT_BLUEZ_NAME,
								adapter_path, BT_ADAPTER_INTERFACE,  NULL, NULL);

	g_free(adapter_path);

	retv_if(proxy == NULL, NULL);

	adapter_proxy = proxy;

	return proxy;
}

static GDBusProxy *__bt_init_adapter_properties_proxy(void)
{
	GDBusProxy *manager_proxy;
	GDBusProxy *proxy;
	char *adapter_path = NULL;

	g_type_init();

	if (system_conn == NULL) {
		system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
		retv_if(system_conn == NULL, NULL);
	}

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, NULL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(system_conn, G_DBUS_PROXY_FLAGS_NONE,
									NULL, BT_BLUEZ_NAME,
									adapter_path, BT_PROPERTIES_INTERFACE,  NULL, NULL);

	g_free(adapter_path);

	retv_if(proxy == NULL, NULL);

	adapter_properties_proxy = proxy;

	return proxy;
}

GDBusConnection *__bt_init_system_gconn(void)
{
	g_type_init();

	if (system_conn == NULL)
		system_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

	return system_conn;
}

GDBusConnection *__bt_init_session_conn(void)
{
	if (session_conn == NULL)
		session_conn =g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	return session_conn;
}

GDBusConnection *_bt_get_session_gconn(void)
{
	return (session_conn) ? session_conn : __bt_init_session_conn();
}

GDBusConnection *_bt_get_system_gconn(void)
{
	return (system_conn) ? system_conn : __bt_init_system_gconn();
}

GDBusConnection *_bt_get_system_conn(void)
{
	GDBusConnection *g_conn;

	if (system_conn == NULL) {
		g_conn = __bt_init_system_gconn();
	} else {
		g_conn = system_conn;
	}

	retv_if(g_conn == NULL, NULL);

	return g_conn;
}

GDBusProxy *_bt_get_manager_proxy(void)
{
	if (manager_proxy) {
		const gchar *path =  g_dbus_proxy_get_object_path(manager_proxy);
		if (path == NULL) {
			BT_ERR("Already proxy released hence creating new proxy");
			return  __bt_init_manager_proxy();
		}
		return manager_proxy;
	}
	return  __bt_init_manager_proxy();
}

static void *__bt_init_net_conn(void)
{
	int result;
	connection_h connection = NULL;

	if (net_conn == NULL) {
		result = connection_create(&connection);

	if (result != CONNECTION_ERROR_NONE ||
					connection == NULL) {
		BT_DBG("connection_create() failed: %d", result);
		net_conn = NULL;
		return NULL;
	}
		net_conn = connection;
	}
	return net_conn;
}

void *_bt_get_net_conn(void)
{
	return (net_conn) ? net_conn : __bt_init_net_conn();
}

GDBusProxy *_bt_get_adapter_proxy(void)
{
	if (adapter_proxy) {
		const char *path =  g_dbus_proxy_get_object_path(adapter_proxy);
		if (path == NULL) {
			BT_ERR("Already proxy released hence creating new proxy");
			return  __bt_init_adapter_proxy();
		}

		return adapter_proxy;
	}
	return  __bt_init_adapter_proxy();

}

GDBusProxy *_bt_get_adapter_properties_proxy(void)
{
	return (adapter_properties_proxy) ? adapter_properties_proxy :
					__bt_init_adapter_properties_proxy();
}

static char *__bt_extract_adapter_path(GVariantIter *iter)
{
	char *object_path = NULL;
	GVariantIter *interface_iter;
	GVariantIter *svc_iter;
	char *interface_str = NULL;

	/* Parse the signature: oa{sa{sv}}} */
	while (g_variant_iter_loop(iter, "{&oa{sa{sv}}}", &object_path,
			&interface_iter)) {

		if (object_path == NULL)
			continue;

		while (g_variant_iter_loop(interface_iter, "{&sa{sv}}",
				&interface_str, &svc_iter)) {
			if (g_strcmp0(interface_str, "org.bluez.Adapter1") != 0)
				continue;

			BT_DBG("Object Path: %s", object_path);
			g_variant_iter_free(svc_iter);
			g_variant_iter_free(interface_iter);
			return g_strdup(object_path);
		}
	}
	return NULL;
}

char *_bt_get_adapter_path(void)
{
	GDBusConnection *conn;
	GDBusProxy *manager_proxy;
	GVariant *result = NULL;
	GVariantIter *iter = NULL;
	char *adapter_path = NULL;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, NULL);

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, NULL);

	result = g_dbus_proxy_call_sync(manager_proxy, "GetManagedObjects",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				NULL);
	if (!result) {
		BT_ERR("Can't get managed objects");
		return NULL;
	}

	/* signature of GetManagedObjects:  a{oa{sa{sv}}} */
	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);

	adapter_path = __bt_extract_adapter_path(iter);
	g_variant_iter_free(iter);
	g_variant_unref(result);
	return adapter_path;
}

void _bt_deinit_bluez_proxy(void)
{
	if (manager_proxy) {
		g_object_unref(manager_proxy);
		manager_proxy = NULL;
	}

	if (adapter_proxy) {
		g_object_unref(adapter_proxy);
		adapter_proxy = NULL;
	}
	if (adapter_properties_proxy) {
		g_object_unref(adapter_properties_proxy);
		adapter_properties_proxy = NULL;
	}
}

void _bt_deinit_proxys(void)
{
	int ret;
	_bt_deinit_bluez_proxy();

	if (system_conn) {
		g_object_unref(system_conn);
		system_conn = NULL;
	}

	if (session_conn) {
		g_object_unref(session_conn);
		session_conn = NULL;
	}

	if (net_conn) {
		ret = connection_destroy(net_conn);
		net_conn = NULL;
		if (ret != 0)
			BT_ERR("connection_destroy failed : %d", ret);
	}
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

	snprintf(address, BT_ADDRESS_STRING_SIZE,
			"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
			addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]);
}

void _bt_print_device_address_t(const bluetooth_device_address_t *addr)
{
	BT_DBG("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n", addr->addr[0], addr->addr[1], addr->addr[2],
				addr->addr[3], addr->addr[4], addr->addr[5]);
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

void _bt_free_device_info(bt_remote_dev_info_t *dev_info)
{
	int i;

	ret_if(dev_info == NULL);

	g_free(dev_info->address);
	g_free(dev_info->name);
	g_free(dev_info->manufacturer_data);

	if (dev_info->uuids) {
		for (i = 0; i < dev_info->uuid_count && dev_info->uuids[i]; i++)
			g_free(dev_info->uuids[i]);

		g_free(dev_info->uuids);
	}

	g_free(dev_info);
}

void _bt_free_le_device_info(bt_remote_le_dev_info_t *le_dev_info)
{
	ret_if(le_dev_info == NULL);

	g_free(le_dev_info->adv_data);
	g_free(le_dev_info);
}

int _bt_copy_utf8_string(char *dest, const char *src, unsigned int length)
{
	int i;
	const char *p = src;
	char *next;
	int count;

	if (dest == NULL || src == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	BT_DBG("+src : %s", src);
	BT_DBG("+dest : %s", dest);

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

int _bt_register_osp_server_in_agent(int type, char *uuid, char *path, int fd)
{
	BT_DBG("+");
	if (!_bt_agent_register_osp_server( type, uuid, path, fd))
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_osp_server_in_agent(int type, char *uuid)
{
	BT_DBG("+");
	if (!_bt_agent_unregister_osp_server( type, uuid))
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_socket_non_blocking(int socket_fd)
{
	/* Set Nonblocking */
	long arg;

	arg = fcntl(socket_fd, F_GETFL);

	if (arg < 0)
		return -errno;

	if (arg & O_NONBLOCK) {
		BT_ERR("Already Non-blocking \n");
	}

	arg |= O_NONBLOCK;

	if (fcntl(socket_fd, F_SETFL, arg) < 0)
		return -errno;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_set_non_blocking_tty(int sk)
{
	struct termios ti = {0,};
	int err;

	err = _bt_set_socket_non_blocking(sk);

	if (err < 0) {
		BT_ERR("Error in set non blocking!\n");
		return err;
	}

	tcflush(sk, TCIOFLUSH);

	/* Switch tty to RAW mode */
	cfmakeraw(&ti);
	tcsetattr(sk, TCSANOW, &ti);

	return BLUETOOTH_ERROR_NONE;
}

static char *__bt_extract_device_path(GVariantIter *iter, char *address)
{
	char *object_path = NULL;
	char device_address[BT_ADDRESS_STRING_SIZE] = { 0 };

	/* Parse the signature: oa{sa{sv}}} */
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
	char *object_path = NULL;
	GDBusConnection *conn;
	GDBusProxy *manager_proxy;
	GVariant *result = NULL;
	GVariantIter *iter = NULL;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, NULL);

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, NULL);

	result = g_dbus_proxy_call_sync(manager_proxy, "GetManagedObjects",
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				NULL);
	if (!result) {
		BT_ERR("Can't get managed objects");
		return NULL;
	}

	/* signature of GetManagedObjects:  a{oa{sa{sv}}} */
	g_variant_get(result, "(a{oa{sa{sv}}})", &iter);
	object_path = __bt_extract_device_path(iter, address);
	g_variant_iter_free(iter);
	g_variant_unref(result);
	return object_path;
}

char *_bt_get_profile_uuid128(bt_profile_type_t profile_type)
{
	switch(profile_type) {
	case BT_PROFILE_CONN_RFCOMM:
		return strdup(RFCOMM_UUID_STR);
	case BT_PROFILE_CONN_A2DP:
		return strdup(A2DP_SINK_UUID);
	case BT_PROFILE_CONN_A2DP_SINK:
		return strdup(A2DP_SOURCE_UUID);
	case BT_PROFILE_CONN_HSP:
		return strdup(HFP_HS_UUID);
	case BT_PROFILE_CONN_HID:
		return strdup(HID_UUID);
	case BT_PROFILE_CONN_NAP:
		return strdup(NAP_UUID);
	case BT_PROFILE_CONN_HFG:
		return strdup(HFP_AG_UUID);
	case BT_PROFILE_CONN_GATT:
	case BT_PROFILE_CONN_ALL: /* NULL UUID will connect to both the audio profiles*/
	default:
		return NULL;
	};
}

char *_bt_convert_error_to_string(int error)
{
	switch (error) {
	case BLUETOOTH_ERROR_CANCEL:
		return "CANCELLED";
	case BLUETOOTH_ERROR_INVALID_PARAM:
		return "INVALID_PARAMETER";
	case BLUETOOTH_ERROR_INVALID_DATA:
		return "INVALID DATA";
	case BLUETOOTH_ERROR_MEMORY_ALLOCATION:
	case BLUETOOTH_ERROR_OUT_OF_MEMORY:
		return "OUT_OF_MEMORY";
	case BLUETOOTH_ERROR_TIMEOUT:
		return "TIMEOUT";
	case BLUETOOTH_ERROR_NO_RESOURCES:
		return "NO_RESOURCES";
	case BLUETOOTH_ERROR_INTERNAL:
		return "INTERNAL";
	case BLUETOOTH_ERROR_NOT_SUPPORT:
		return "NOT_SUPPORT";
	case BLUETOOTH_ERROR_DEVICE_NOT_ENABLED:
		return "NOT_ENABLED";
	case BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED:
		return "ALREADY_ENABLED";
	case BLUETOOTH_ERROR_DEVICE_BUSY:
		return "DEVICE_BUSY";
	case BLUETOOTH_ERROR_ACCESS_DENIED:
		return "ACCESS_DENIED";
	case BLUETOOTH_ERROR_MAX_CLIENT:
		return "MAX_CLIENT";
	case BLUETOOTH_ERROR_NOT_FOUND:
		return "NOT_FOUND";
	case BLUETOOTH_ERROR_SERVICE_SEARCH_ERROR:
		return "SERVICE_SEARCH_ERROR";
	case BLUETOOTH_ERROR_PARING_FAILED:
		return "PARING_FAILED";
	case BLUETOOTH_ERROR_NOT_PAIRED:
		return "NOT_PAIRED";
	case BLUETOOTH_ERROR_SERVICE_NOT_FOUND:
		return "SERVICE_NOT_FOUND";
	case BLUETOOTH_ERROR_NOT_CONNECTED:
		return "NOT_CONNECTED";
	case BLUETOOTH_ERROR_ALREADY_CONNECT:
		return "ALREADY_CONNECT";
	case BLUETOOTH_ERROR_CONNECTION_BUSY:
		return "CONNECTION_BUSY";
	case BLUETOOTH_ERROR_CONNECTION_ERROR:
		return "CONNECTION_ERROR";
	case BLUETOOTH_ERROR_MAX_CONNECTION:
		return "MAX_CONNECTION";
	case BLUETOOTH_ERROR_NOT_IN_OPERATION:
		return "NOT_IN_OPERATION";
	case BLUETOOTH_ERROR_CANCEL_BY_USER:
		return "CANCEL_BY_USER";
	case BLUETOOTH_ERROR_REGISTRATION_FAILED:
		return "REGISTRATION_FAILED";
	case BLUETOOTH_ERROR_IN_PROGRESS:
		return "IN_PROGRESS";
	case BLUETOOTH_ERROR_AUTHENTICATION_FAILED:
		return "AUTHENTICATION_FAILED";
	case BLUETOOTH_ERROR_HOST_DOWN:
		return "HOST_DOWN";
	case BLUETOOTH_ERROR_END_OF_DEVICE_LIST:
		return "END_OF_DEVICE_LIST";
	case BLUETOOTH_ERROR_AGENT_ALREADY_EXIST:
		return "AGENT_ALREADY_EXIST";
	case BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST:
		return "AGENT_DOES_NOT_EXIST";
	case BLUETOOTH_ERROR_ALREADY_INITIALIZED:
		return "ALREADY_INITIALIZED";
	case BLUETOOTH_ERROR_PERMISSION_DEINED:
		return "PERMISSION_DEINED";
	case BLUETOOTH_ERROR_ALREADY_DEACTIVATED:
		return "ALREADY_DEACTIVATED";
	case BLUETOOTH_ERROR_NOT_INITIALIZED:
		return "NOT_INITIALIZED";
	default:
		return "UNKNOWN";
	}
}

char * _bt_convert_disc_reason_to_string(int reason)
{
	switch(reason) {
	case 1:
		return "Link loss";
	case 2:
		return "Connection terminated by local host";
	case 3:
		return "Remote user terminated connection";
	case 0:
	default:
		return "Unknown";
	}
}

void _bt_logging_connection(gboolean connect, int addr_type)
{
	static int le_conn = 0;
	static int le_disc = 0;
	static int edr_conn = 0;
	static int edr_disc = 0;

	if (connect) {
		if (addr_type)
			le_conn++;
		else
			edr_conn++;
	} else {
		if (addr_type)
			le_disc++;
		else
			edr_disc++;
	}

	BT_INFO("[PM] Number of LE conn: %d disc: %d, Number of BR/EDR conn: %d disc: %d",
			le_conn, le_disc, edr_conn, edr_disc);
}

int _bt_eventsystem_set_value(const char *event, const char *key, const char *value)
{
	int ret;
	bundle *b = NULL;

	b = bundle_create();

	bundle_add_str(b, key, value);

	ret = eventsystem_send_system_event(event, b);

	BT_DBG("eventsystem_send_system_event result: %d", ret);

	bundle_free(b);

	return ret;
}

void _bt_swap_byte_ordering(char *data, int data_len)
{
	char temp;
	int i, j;

	ret_if(data == NULL);
	/* Swap to opposite endian */
	for (i = 0, j = data_len - 1; i < data_len; i++, j--) {
		temp = data[i];
		data[i] = data[j];
		data[j] = temp;
		}
}

int _bt_byte_arr_cmp(const char *data1, const char *data2, int data_len)
{
	int i;

	retv_if(data1 == NULL, -1);
	retv_if(data2 == NULL, -1);
	for (i = 0; i < data_len; i++) {
		if (data1[i] != data2[i])
			return data1[i] - data2[i];
		}
	return 0;
}
int _bt_byte_arr_cmp_with_mask(const char *data1, const char *data2,
	const char *mask, int data_len)
{
	int i;
	char a, b;

	retv_if(data1 == NULL, -1);
	retv_if(data2 == NULL, -1);
	retv_if(mask == NULL, -1);
	for (i = 0; i < data_len; i++) {
		a = data1[i] & mask[i];
		b = data2[i] & mask[i];
		if (a != b)
			return (int)(a - b);
		}
	return 0;
}
