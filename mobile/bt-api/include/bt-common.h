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


#ifndef _BT_COMMON_H_
#define _BT_COMMON_H_

#include <sys/types.h>
#include <libintl.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dlog.h>
#include <glib.h>
#include <gio/gio.h>

#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_FRWK"

#ifndef BT_EXPORT_API
#define BT_EXPORT_API __attribute__((visibility("default")))
#endif

#define BT_DBG(fmt, args...) \
        SLOGD(fmt, ##args)
#define BT_ERR(fmt, args...) \
        SLOGE(fmt, ##args)

#define DBG_SECURE(fmt, args...) SECURE_SLOGD(fmt, ##args)
#define ERR_SECURE(fmt, args...) SECURE_SLOGE(fmt, ##args)

#ifdef FUNCTION_TRACE
#define	FN_START BT_DBG("[ENTER FUNC]")
#define	FN_END BT_DBG("[EXIT FUNC]")
#else
#define	FN_START
#define	FN_END
#endif

#define ret_if(expr) \
	do { \
		if (expr) { \
			BT_ERR("(%s) return", #expr); \
			return; \
		} \
	} while (0)

#define retv_if(expr, val) \
	do { \
		if (expr) { \
			BT_ERR("(%s) return", #expr); \
			return (val); \
		} \
	} while (0)

#define BT_INIT_PARAMS() \
	GArray *in_param1 = NULL; \
	GArray *in_param2 = NULL; \
	GArray *in_param3 = NULL; \
	GArray *in_param4 = NULL; \
	GArray *out_param = NULL;

#define BT_FREE_PARAMS(IP1,IP2,IP3,IP4,OP) \
	do { \
		if (IP1) \
			g_array_free(IP1, TRUE); \
		if (IP2) \
			g_array_free(IP2, TRUE); \
		if (IP3) \
			g_array_free(IP3, TRUE); \
		if (IP4) \
			g_array_free(IP4, TRUE); \
		if (OP) \
			g_array_free(OP, TRUE); \
	} while (0)

#define BT_ALLOC_PARAMS(IP1,IP2,IP3,IP4,OP ) \
	do { \
	        IP1 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP2 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP3 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP4 = g_array_new(TRUE, TRUE, sizeof(gchar)); \
	} while (0)

#define BT_INIT_AGENT_PARAMS() \
	GArray *in_param = NULL; \
	GArray *out_param = NULL;

#define BT_FREE_AGENT_PARAMS(IP,OP) \
	do { \
		if (IP) \
			g_array_free(IP, TRUE); \
		if (OP) \
			g_array_free(OP, TRUE); \
	} while (0)

#define BT_ALLOC_AGENT_PARAMS(IP,OP) \
	do { \
	        IP = g_array_new(FALSE, FALSE, sizeof(gchar));	\
	} while (0)

#define BT_CHECK_PARAMETER(arg, func) \
	do { \
		if (arg == NULL) \
		{ \
			BT_ERR("INVALID PARAMETER"); \
			func BLUETOOTH_ERROR_INVALID_PARAM; \
		} \
	} while (0)

#define BT_CHECK_ENABLED(func) \
	do { \
		if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED) \
		{ \
			BT_ERR("BT is not enabled"); \
			func BLUETOOTH_ERROR_DEVICE_NOT_ENABLED; \
		} \
	} while (0)

#define BT_ADDRESS_LENGTH_MAX 6
#define BT_ADDRESS_STRING_SIZE 18
#define BT_ADAPTER_OBJECT_PATH_MAX 50

#define BT_EVENT_FREEDESKTOP "org.freedesktop.DBus"
#define BT_FREEDESKTOP_PATH "/org/freedesktop/DBus"

#define BT_EVENT_MANAGER "org.bluez.Manager"
#define BT_MANAGER_PATH "/"
#define BT_MANAGER_INTERFACE "org.bluez.Manager"
#define BT_ADAPTER_INTERFACE "org.bluez.Adapter"
#define BT_DEVICE_INTERFACE "org.bluez.Device"

#define BT_BLUEZ_NAME "org.bluez"
#define BT_DBUS_NAME "org.projectx.bt"
#define BT_SERVICE_PATH "/org/projectx/bt_service"

#define BT_AGENT_NAME "org.bluez.frwk_agent"
#define BT_AGENT_PATH "/org/bluez/agent/frwk_agent"
#define BT_AGENT_INTERFACE "org.bluez.Agent"

#define BT_MAX_USER_INFO 5
#define RFKILL_EVENT_SIZE 8
#define RFKILL_NODE "/dev/rfkill"

typedef enum {
        RFKILL_TYPE_ALL = 0,
        RFKILL_TYPE_WLAN,
        RFKILL_TYPE_BLUETOOTH,
        RFKILL_TYPE_UWB,
        RFKILL_TYPE_WIMAX,
        RFKILL_TYPE_WWAN,
        RFKILL_TYPE_GPS,
        RFKILL_TYPE_FM,
        NUM_RFKILL_TYPES,
} rfkill_type;

typedef struct {
        unsigned int idx;
        unsigned char type;
        unsigned char op;
        unsigned char soft;
        unsigned char hard;
} rfkill_event;

typedef enum {
	BT_COMMON = 0x00,
	BT_HID,
	BT_AUDIO,
	BT_AVRCP,
} bt_user_info_type_t;

typedef struct {
	void *cb;
	void *user_data;
} bt_user_info_t;

void _bt_set_user_data(int type, void *callback, void *user_data);

void _bt_print_device_address_t(const bluetooth_device_address_t *addr);

bt_user_info_t* _bt_get_user_data(int type);

void _bt_common_event_cb(int event, int result, void *param,
				void *callback, void *user_data);

void _bt_input_event_cb(int event, int result, void *param,
					void *callback, void *user_data);

void _bt_headset_event_cb(int event, int result, void *param,
					void *callback, void *user_data);

void _bt_avrcp_event_cb(int event, int result, void *param,
					void *callback, void *user_data);

void _bt_opp_client_event_cb(int event, int result, void *param,
					void *callback, void *user_data);

void _bt_divide_device_class(bluetooth_device_class_t *device_class,
				unsigned int cod);

void _bt_convert_addr_string_to_type(unsigned char *addr,
					const char *address);

void _bt_convert_addr_type_to_string(char *address,
				unsigned char *addr);

int _bt_copy_utf8_string(char *dest, const char *src, unsigned int length);

gboolean _bt_utf8_validate(char *name);

int _bt_get_adapter_path(DBusGConnection *conn, char *path);

int _bt_get_adapter_path_dbus(DBusConnection *conn, char *path);

DBusGProxy *_bt_get_adapter_proxy(DBusGConnection *conn);

void _bt_device_path_to_address(const char *device_path, char *device_address);

DBusGConnection *__bt_init_system_gconn(void);

DBusGConnection *_bt_get_system_gconn(void);

DBusConnection *_bt_get_system_conn(void);

GDBusConnection *_bt_init_system_gdbus_conn(void);

char *_bt_get_cookie(void);

int _bt_get_cookie_size(void);
#ifdef __ENABLE_GDBUS__
GDBusConnection *_bt_gdbus_get_system_gconn(void);

int _bt_gdbus_get_adapter_path(GDBusConnection *conn, char *path);

GDBusProxy *_bt_gdbus_get_adapter_proxy(GDBusConnection *conn);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_COMMON_H_*/

