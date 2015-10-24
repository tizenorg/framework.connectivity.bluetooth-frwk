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


#ifndef _BT_SERVICE_COMMON_H_
#define _BT_SERVICE_COMMON_H_

#include <sys/types.h>
#include <dlog.h>
#include <glib.h>
#include <gio/gio.h>

#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_FRWK_SERVICE"

#ifdef FUNCTION_TRACE
#define	FN_START BT_DBG("[ENTER FUNC]")
#define	FN_END BT_DBG("[EXIT FUNC]")
#else
#define	FN_START
#define	FN_END
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

#define BT_INFO_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_GREEN" "fmt" "LOG_COLOR_RESET, ##arg)
#define BT_ERR_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_RED" "fmt" "LOG_COLOR_RESET, ##arg)

#define DBG_SECURE(fmt, args...) SECURE_SLOGD(fmt, ##args)
#define ERR_SECURE(fmt, args...) SECURE_SLOGE(fmt, ##args)

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

#define BT_CHECK_PARAMETER(arg, func) \
	do { \
		if (arg == NULL) \
		{ \
			BT_ERR("%s is NULL", #arg); \
			func BLUETOOTH_ERROR_INVALID_PARAM; \
		} \
	} while (0)


#define BT_ADDRESS_LENGTH_MAX 6
#define BT_ADDRESS_STRING_SIZE 18
#define BT_RFCOMM_BUFFER_MAX 1024
#define BT_LOWER_ADDRESS_LENGTH 9

#define BT_AGENT_AUTO_PAIR_BLACKLIST_FILE (APP_SYSCONFDIR"/auto-pair-blacklist")
#define BT_AGENT_NEW_LINE "\r\n"

#define BT_MAX_DBUS_TIMEOUT 45000
#define BT_ENABLE_TIMEOUT 20000 /* 20 seconds */
#define BT_DISCOVERY_FINISHED_DELAY 200

#define MANAGER_EVENT_MATCH_RULE \
			"type='signal'," \
			"interface='%s'," \
			"member='%s'"

#define EVENT_MATCH_RULE \
			"type='signal'," \
			"interface='%s',"

#define BT_TEMINATING_WAIT_TIME 200

#define BT_TIMEOUT_MESSAGE "Did not receive a reply. Possible causes include: " \
			"the remote application did not send a reply, " \
			"the message bus security policy blocked the reply, " \
			"the reply timeout expired, or the network connection " \
			"was broken."

#define BT_BLUEZ_NAME "org.bluez"

#define BT_BLUEZ_PATH "/org/bluez"
#define BT_BLUEZ_HCI_PATH "/org/bluez/hci0"
#define BT_AGENT_NAME "org.bluez.frwk_agent"
#define BT_AGENT_PATH "/org/bluez/agent/frwk_agent"
#define BT_DEVICE_AGENT_PATH "/org/tizen/device_agent"
#define BT_ADAPTER_AGENT_PATH "/org/tizen/adapter_agent"
#define BT_MANAGER_PATH "/"


#define BT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"
#define BT_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BT_AGENT_INTERFACE "org.bluez.Agent1"
#define BT_AGENT_MANAGER_INTERFACE "org.bluez.AgentManager1"
#define BT_DEVICE_INTERFACE "org.bluez.Device1"
#define BT_NETWORK_SERVER_INTERFACE "org.bluez.NetworkServer1"
#define BT_MEDIA_INTERFACE "org.bluez.Media1"
#define BT_MEDIA_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define BT_MEDIATRANSPORT_INTERFACE "org.bluez.MediaTransport1"
#define BT_MEDIA_CONTROL_INTERFACE "org.bluez.MediaControl1"
#define BT_PLAYER_CONTROL_INTERFACE "org.bluez.MediaPlayer1"
#define BT_GATT_CHAR_INTERFACE "org.bluez.GattCharacteristic1"

#define BT_INPUT_INTERFACE "org.bluez.Input1"
#define BT_NETWORK_INTERFACE "org.bluez.Network"
#define BT_NETWORK_CLIENT_INTERFACE "org.bluez.Network1"
#define BT_SERIAL_INTERFACE "org.bluez.Serial"
#define BT_SERIAL_MANAGER_INTERFACE "org.bluez.SerialProxyManager"
#define BT_SERIAL_PROXY_INTERFACE "org.bluez.SerialProxy"
#define BT_SINK_INTERFACE "org.bluez.AudioSink"
#define BT_AUDIO_INTERFACE "org.bluez.Audio"
#define BT_HEADSET_INTERFACE "org.bluez.Headset"
#define BT_OOB_INTERFACE "org.bluez.OutOfBand"
#define BT_HANDSFREE_GATEWAY_INTERFACE "org.bluez.HandsfreeGateway"
#define BT_OBEXD_INTERFACE "org.openobex"
#define BT_OBEXD_MANAGER_INTERFACE "org.openobex.Manager"
#define BT_OBEXD_TRANSFER_INTERFACE "org.openobex.Transfer"
#define BT_A2DP_SOURCE_INTERFACE "org.bluez.AudioSource"

#define BT_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"


#define BT_OBEX_SERVICE_NAME "org.bluez.obex"
#define BT_OBEX_CLIENT_PATH "/org/bluez/obex"
#define BT_OBEX_CLIENT_INTERFACE "org.bluez.obex.Client1"



#define BT_OBEX_TRANSFER_INTERFACE "org.bluez.obex.Transfer1"
#define BT_OBEX_AGENT_INTERFACE "org.bluez.obex.Agent1"



#define BT_OBEXD_DBUS_NAME "org.bluez.obex"
#define BT_OBEX_OBJECT_PUSH_INTERFACE "org.bluez.obex.ObjectPush1"


#define BT_FREEDESKTOP_INTERFACE "org.freedesktop.DBus"
#define BT_FREEDESKTOP_PATH "/org/freedesktop/DBus"


#define BT_INTERFACES_ADDED "InterfacesAdded"
#define BT_INTERFACES_REMOVED "InterfacesRemoved"
#define BT_NAME_OWNER_CHANGED "NameOwnerChanged"
#define BT_PROPERTIES_CHANGED "PropertiesChanged"



#define BT_SESSION_BASEPATH_SERVER "/org/bluez/obex/server"
#define BT_SESSION_BASEPATH_CLIENT "/org/bluez/obex/client"

#define BT_SERVICE_ERR_MSG_NOT_SUPPORTED "Operation is not supported"

/* UUID */
#define GENERIC_AUDIO_UUID      "00001203-0000-1000-8000-00805f9b34fb"

#define OBEX_OPP_UUID		"00001105-0000-1000-8000-00805f9b34fb"

#define HSP_HS_UUID             "00001108-0000-1000-8000-00805f9b34fb"
#define HSP_AG_UUID             "00001112-0000-1000-8000-00805f9b34fb"

#define HFP_HS_UUID             "0000111e-0000-1000-8000-00805f9b34fb"
#define HFP_AG_UUID             "0000111f-0000-1000-8000-00805f9b34fb"

#define ADVANCED_AUDIO_UUID     "0000110d-0000-1000-8000-00805f9b34fb"

#define A2DP_SOURCE_UUID        "0000110a-0000-1000-8000-00805f9b34fb"
#define A2DP_SINK_UUID          "0000110b-0000-1000-8000-00805f9b34fb"

#define AVRCP_REMOTE_UUID       "0000110e-0000-1000-8000-00805f9b34fb"
#define AVRCP_TARGET_UUID       "0000110c-0000-1000-8000-00805f9b34fb"

#define HID_UUID                "00001124-0000-1000-8000-00805f9b34fb"
#define PNP_UUID                "00001200-0000-1000-8000-00805f9b34fb"
#define RFCOMM_UUID_STR		"00000003-0000-1000-8000-00805f9b34fb"
#define PANU_UUID		"00001115-0000-1000-8000-00805f9b34fb"
#define NAP_UUID		"00001116-0000-1000-8000-00805f9b34fb"
#define OBEX_PSE_UUID		"0000112f-0000-1000-8000-00805f9b34fb"
#define GATT_UUID		"00001801-0000-1000-8000-00805f9b34fb"

/* Privilege */
#define BT_PRIVILEGE_PUBLIC "bt-service::public"
#define BT_PRIVILEGE_PLATFORM "bt-service::platform"

/* BD Address type */
#define BDADDR_BREDR           0x00
#define BDADDR_LE_PUBLIC       0x01
#define BDADDR_LE_RANDOM       0x02

/* Advertising report event types */
#define BT_LE_ADV_IND		0x00
#define BT_LE_ADV_DIRECT_IND	0x01
#define BT_LE_ADV_SCAN_IND		0x02
#define BT_LE_ADV_NONCONN_IND	0x03
#define BT_LE_ADV_SCAN_RSP		0x04

/* Profile states matched to btd_service_state_t of bluez service.h */
typedef enum {
	BT_PROFILE_STATE_UNAVAILABLE,
	BT_PROFILE_STATE_DISCONNECTED,
	BT_PROFILE_STATE_CONNECTING,
	BT_PROFILE_STATE_CONNECTED,
	BT_PROFILE_STATE_DISCONNECTING,
} bt_profile_state_t;

typedef enum {
	BT_ADV_IND_INFO = 0x00,
	BT_SCAN_RSP_INFO = 0x01,
} bt_le_device_info_type_t;

typedef enum {
	BT_PROFILE_CONN_RFCOMM= 0x01,
	BT_PROFILE_CONN_A2DP= 0x02,
	BT_PROFILE_CONN_HSP= 0x04,
	BT_PROFILE_CONN_HID= 0x08,
	BT_PROFILE_CONN_NAP= 0x10,
	BT_PROFILE_CONN_HFG= 0x20,
	BT_PROFILE_CONN_GATT= 0x40,
	BT_PROGILE_CONN_NAP = 0x80,
	BT_PROFILE_CONN_A2DP_SINK= 0x100,
	BT_PROFILE_CONN_ALL= 0xffffffff,
} bt_profile_type_t;

typedef struct {
	char *address;
	int addr_type;
	int rssi;
	int adv_type;
	bt_le_device_info_type_t dev_type;
	int adv_data_len;
	char *adv_data;
} bt_remote_le_dev_info_t;

typedef struct {
	int rssi;
	int class;
	char *address;
	char *name;
	char **uuids;
	unsigned int uuid_count;
	gboolean paired;
	bluetooth_connected_link_t connected;
	gboolean trust;
	char *manufacturer_data;
	int manufacturer_data_len;
	guchar addr_type;
#if 0 /* Should match with bt_dev_info_t in bluetooth-api.h */
	bt_remote_le_dev_info_t le_dev_info;
#endif
} bt_remote_dev_info_t;

/* RFCOMM client /server will use this structure*/
typedef struct {
	int fd;
	GIOChannel *io_channel;
	guint io_event;
	char *dev_node;
	char *address;
	char *uuid;
} bt_rfcomm_info_t;

typedef struct {
	int req_id;
	char *address;
} bt_function_data_t;

GDBusConnection *_bt_get_system_conn(void);

GDBusConnection *_bt_get_system_gconn(void);

GDBusConnection *_bt_get_session_gconn(void);

void *_bt_get_net_conn(void);

GDBusProxy *_bt_get_manager_proxy(void);

GDBusProxy *_bt_get_adapter_proxy(void);

GDBusProxy *_bt_get_adapter_properties_proxy(void);

char *_bt_get_device_object_path(char *address);

char *_bt_get_profile_uuid128(bt_profile_type_t profile_type);

char *_bt_convert_error_to_string(int error);

char * _bt_convert_disc_reason_to_string(int reason);

void _bt_logging_connection(gboolean connect, int addr_type);

char *_bt_get_adapter_path(void);

void _bt_deinit_proxys(void);

void _bt_convert_device_path_to_address(const char *device_path,
						char *device_address);

void _bt_convert_addr_string_to_type(unsigned char *addr,
					const char *address);

void _bt_convert_addr_type_to_string(char *address,
				unsigned char *addr);

void _bt_swap_byte_ordering(char *data, int data_len);

int _bt_byte_arr_cmp(const char *data1, const char *data2, int data_len);

int _bt_byte_arr_cmp_with_mask(const char *data1, const char *data2,
				const char *mask, int data_len);

void _bt_print_device_address_t(const bluetooth_device_address_t *addr);

void _bt_divide_device_class(bluetooth_device_class_t *device_class,
				unsigned int cod);

void _bt_free_device_info(bt_remote_dev_info_t *dev_info);

void _bt_free_le_device_info(bt_remote_le_dev_info_t *le_dev_info);

int _bt_copy_utf8_string(char *dest, const char *src, unsigned int length);

gboolean _bt_utf8_validate(char *name);

int _bt_register_osp_server_in_agent(int type, char *uuid, char *path, int fd);

int _bt_unregister_osp_server_in_agent(int type, char *uuid);

int _bt_set_socket_non_blocking(int socket_fd);

int _bt_set_non_blocking_tty(int sk);

void _bt_deinit_bluez_proxy(void);

int _bt_eventsystem_set_value(const char *event, const char *key, const char *value);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_COMMON_H_*/

