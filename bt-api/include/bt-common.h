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
#define LOG_TAG "BLUETOOTH_FRWK_API"

#ifndef BT_EXPORT_API
#define BT_EXPORT_API __attribute__((visibility("default")))
#endif

#define LOG_COLOR_RESET    "\033[0m"
#define LOG_COLOR_RED      "\033[31m"
#define LOG_COLOR_YELLOW   "\033[33m"
#define LOG_COLOR_GREEN         "\033[32m"
#define LOG_COLOR_BLUE          "\033[36m"
#define LOG_COLOR_PURPLE   "\033[35m"

#define BT_DBG(fmt, args...) \
        SLOGD(fmt, ##args)
#define BT_INFO(fmt, args...) \
        SLOGI(fmt, ##args)
#define BT_ERR(fmt, args...) \
        SLOGE(fmt, ##args)

#define BT_DBG_UUID(uuids, len, i) \
	BT_DBG("***UUIDs***"); \
	for (i = 0; i < len; i++) { \
		BT_DBG("%s", uuids[i]); \
	}

#define BT_INFO_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_GREEN" "fmt" "LOG_COLOR_RESET, ##arg)
#define BT_ERR_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_RED" "fmt" "LOG_COLOR_RESET, ##arg)

#define DBG_SECURE(fmt, args...) SECURE_SLOGD(fmt, ##args)
#define ERR_SECURE(fmt, args...) SECURE_SLOGE(fmt, ##args)

//#define FUNCTION_TRACE
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
			BT_ERR("%s is NULL", #arg); \
			func BLUETOOTH_ERROR_INVALID_PARAM; \
		} \
	} while (0)

#define BT_CHECK_ENABLED(func) \
	do { \
		if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED) \
		{ \
			BT_ERR("BT BREDR is not enabled"); \
			func BLUETOOTH_ERROR_DEVICE_NOT_ENABLED; \
		} \
	} while (0)

#define BT_CHECK_ENABLED_LE(func) \
	do { \
		if (bluetooth_check_adapter_le() == BLUETOOTH_ADAPTER_LE_DISABLED) \
		{ \
			BT_ERR("BT LE is not enabled"); \
			func BLUETOOTH_ERROR_DEVICE_NOT_ENABLED; \
		} \
	} while (0)

#define BT_CHECK_ENABLED_ANY(func) \
	do { \
		if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED && \
			bluetooth_check_adapter_le() == BLUETOOTH_ADAPTER_LE_DISABLED) \
		{ \
			BT_ERR("BT is not enabled"); \
			func BLUETOOTH_ERROR_DEVICE_NOT_ENABLED; \
		} \
	} while (0)

#define BT_ADDRESS_LENGTH_MAX 6
#define BT_ADDRESS_STRING_SIZE 18
#define BT_ADAPTER_OBJECT_PATH_MAX 50
#define BT_RFCOMM_BUFFER_LEN 1024

#define BT_ACCESS_DENIED_MSG "Rejected send message"

#define BT_EVENT_FREEDESKTOP "org.freedesktop.DBus"
#define BT_FREEDESKTOP_PATH "/org/freedesktop/DBus"

#define BT_EVENT_MANAGER "org.bluez.Manager"
#define BT_MANAGER_PATH "/"


#define BT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"
#define BT_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BT_DEVICE_INTERFACE "org.bluez.Device1"
#define BT_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"


#define BT_SERIAL_INTERFACE "org.bluez.Serial"

#define BT_BLUEZ_NAME "org.bluez"
#define BT_DBUS_NAME "org.projectx.bt"
#define BT_SERVICE_PATH "/org/projectx/bt_service"

#define BT_AGENT_NAME "org.bluez.frwk_agent"
#define BT_AGENT_PATH "/org/bluez/agent/frwk_agent"

#define BT_AGENT_INTERFACE "org.bluez.Agent1"


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
	BT_HF,
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

void _bt_a2dp_source_event_cb(int event, int result, void *param,
					void *callback, void *user_data);

void _bt_hf_event_cb(int event, int result, void *param,
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

typedef struct {
	char *obj_path;
	char *uuid;
	gboolean authentication;
	gboolean authorization;
	char *role;

	char *service;

} bt_register_profile_info_t;

int _bt_get_adapter_path(GDBusConnection *conn, char *path);
char *_bt_get_device_object_path(char *address);
int _bt_connect_profile(char *address, char *uuid, void *cb,
							gpointer func_data);
int _bt_disconnect_profile(char *address, char *uuid, void *cb,
							gpointer func_data);

int _bt_cancel_discovers(char *address);
int _bt_discover_services(char *address, char *uuid, void *cb,
		gpointer func_data);
int _bt_discover_service_uuids(char *address, char *remote_uuid);
int _bt_get_cod_by_address(char *address, bluetooth_device_class_t *dev_class);

void _bt_set_le_scan_status(gboolean mode);

int _bt_register_profile(bt_register_profile_info_t *info, gboolean use_default_rfcomm);
int _bt_register_profile_platform(bt_register_profile_info_t *info, gboolean use_default_rfcomm);
int _bt_register_profile_ex(bt_register_profile_info_t *info, gboolean use_default_rfcomm, const char *name, const char *path);

void _bt_unregister_profile(char *path);
GDBusNodeInfo * _bt_get_gdbus_node(const gchar *xml_data);
GDBusNodeInfo * _bt_get_gdbus_node_ex(const gchar *xml_data, const char *bus_name);
int __rfcomm_assign_id(void);
void __rfcomm_delete_id(int id);
void _bt_unregister_gdbus(int object_id);
typedef int (*bt_new_connection_cb) (const char *path, int fd,
					bluetooth_device_address_t *address);
int _bt_register_new_conn(const char *path, bt_new_connection_cb cb);
int _bt_register_new_conn_ex(const char *path, const char *bus_name,bt_new_connection_cb cb);
void _bt_swap_addr(unsigned char *dst, const unsigned char *src);

DBusGProxy *_bt_get_adapter_proxy(DBusGConnection *conn);

void _bt_device_path_to_address(const char *device_path, char *device_address);

GDBusConnection *g_bus_get_private_conn(void);

DBusConnection *__bt_init_system_conn(void);

DBusConnection *_bt_get_system_conn(void);

char *_bt_get_cookie(void);

int _bt_get_cookie_size(void);

int _bt_register_osp_server_in_agent(int type, char *uuid, char *path, int fd);
int _bt_unregister_osp_server_in_agent(int type, char *uuid);

void _bt_generate_cookie(void);

void _bt_destroy_cookie(void);

int _bt_check_privilege(int service_type, int service_function);

GDBusConnection *_bt_gdbus_init_system_gconn(void);

GDBusConnection *_bt_gdbus_get_system_gconn(void);

GVariant *_bt_get_managed_objects(void);

void _bt_convert_device_path_to_address(const char *device_path,
				char *device_address);

#ifdef RFCOMM_DIRECT
void _bt_rfcomm_server_free_all();

gboolean _check_uuid_path(char *path, char *uuid);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_COMMON_H_*/

