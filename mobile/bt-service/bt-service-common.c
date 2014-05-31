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

#include <stdio.h>
#include <string.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-agent.h"

static DBusGConnection *system_conn;
static DBusGConnection *session_conn;
static DBusGProxy *manager_proxy;
static DBusGProxy *adapter_proxy;

static DBusGProxy *__bt_init_manager_proxy(void)
{
	DBusGProxy *proxy;

	g_type_init();

	if (system_conn == NULL) {
		system_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
		retv_if(system_conn == NULL, NULL);
	}

	proxy = dbus_g_proxy_new_for_name(system_conn, BT_BLUEZ_NAME,
			BT_MANAGER_PATH, BT_MANAGER_INTERFACE);

	retv_if(proxy == NULL, NULL);

	manager_proxy = proxy;

	return proxy;
}

static DBusGProxy *__bt_init_adapter_proxy(void)
{
	DBusGProxy *manager_proxy;
	DBusGProxy *proxy;
	char *adapter_path = NULL;

	g_type_init();

	if (system_conn == NULL) {
		system_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
		retv_if(system_conn == NULL, NULL);
	}

	manager_proxy = _bt_get_manager_proxy();
	retv_if(manager_proxy == NULL, NULL);

	if (!dbus_g_proxy_call(manager_proxy, "DefaultAdapter", NULL,
			G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
			&adapter_path, G_TYPE_INVALID)) {
		BT_ERR("Fail to get DefaultAdapter");
		return NULL;
	}

	adapter_path = g_strdup(adapter_path);
	retv_if(adapter_path == NULL, NULL);

	proxy = dbus_g_proxy_new_for_name(system_conn, BT_BLUEZ_NAME,
				adapter_path, BT_ADAPTER_INTERFACE);

	g_free(adapter_path);

	retv_if(proxy == NULL, NULL);

	adapter_proxy = proxy;

	return proxy;
}

DBusGConnection *__bt_init_system_gconn(void)
{
	g_type_init();

	if (system_conn == NULL)
		system_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);

	return system_conn;
}

DBusGConnection *__bt_init_session_conn(void)
{
	if (session_conn == NULL)
		session_conn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);

	return session_conn;
}

DBusGConnection *_bt_get_session_gconn(void)
{
	return (session_conn) ? session_conn : __bt_init_session_conn();
}

DBusGConnection *_bt_get_system_gconn(void)
{
	return (system_conn) ? system_conn : __bt_init_system_gconn();
}

DBusConnection *_bt_get_system_conn(void)
{
	DBusGConnection *g_conn;

	if (system_conn == NULL) {
		g_conn = __bt_init_system_gconn();
	} else {
		g_conn = system_conn;
	}

	retv_if(g_conn == NULL, NULL);

	return dbus_g_connection_get_connection(g_conn);
}

DBusGProxy *_bt_get_manager_proxy(void)
{
	return (manager_proxy) ? manager_proxy : __bt_init_manager_proxy();
}

DBusGProxy *_bt_get_adapter_proxy(void)
{
	return (adapter_proxy) ? adapter_proxy : __bt_init_adapter_proxy();
}

char *_bt_get_adapter_path(void)
{
	char *adapter_path = NULL;
	DBusGProxy *proxy;
	GError *err = NULL;

	proxy = _bt_get_manager_proxy();

	if (!dbus_g_proxy_call(proxy, "DefaultAdapter", &err,
			       G_TYPE_INVALID, DBUS_TYPE_G_OBJECT_PATH,
			       &adapter_path, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting DefaultAdapter failed: [%s]\n",
							err->message);
			g_error_free(err);
		}
		return NULL;
	}

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
}

void _bt_deinit_proxys(void)
{

	_bt_deinit_bluez_proxy();

	if (system_conn) {
		dbus_g_connection_unref(system_conn);
		system_conn = NULL;
	}

	if (session_conn) {
		dbus_g_connection_unref(session_conn);
		session_conn = NULL;
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

	if (dev_info->uuids) {
		for (i = 0; dev_info->uuids[i] != NULL; i++)
			g_free(dev_info->uuids[i]);

		g_free(dev_info->uuids);
	}

	g_free(dev_info);
}

int _bt_register_osp_server_in_agent(int type, char *uuid)
{
	if (!_bt_agent_register_osp_server( type, uuid))
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_osp_server_in_agent(int type, char *uuid)
{
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

gboolean _bt_is_headset_class(int dev_class)
{
	gboolean is_headset = FALSE;

	switch ((dev_class & 0x1f00) >> 8) {
	case 0x04:
		switch ((dev_class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			/* Headset */
			is_headset = TRUE;
			break;
		case 0x06:
			/* Headphone */
			is_headset = TRUE;
			break;
		case 0x0b:	/* VCR */
		case 0x0c:	/* Video Camera */
		case 0x0d:	/* Camcorder */
			break;
		default:
			/* Other audio device */
			is_headset = TRUE;
			break;
		}
		break;
	}

	return is_headset;
}

