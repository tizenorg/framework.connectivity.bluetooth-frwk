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
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_FRWK_SERVICE"

#define BT_DBG(fmt, args...) \
        SLOGD(fmt, ##args)
#define BT_INFO(fmt, args...) \
	SLOGI(fmt, ##args)
#define BT_ERR(fmt, args...) \
        SLOGE(fmt, ##args)

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
			BT_ERR("INVALID PARAMETER"); \
			func BLUETOOTH_ERROR_INVALID_PARAM; \
		} \
	} while (0)

#define BT_CHANNEL_LENGTH_MAX 5
#define BT_ADDRESS_LENGTH_MAX 6
#define BT_ADDRESS_STRING_SIZE 18
#define BT_RFCOMM_BUFFER_MAX 1024
#define BT_LOWER_ADDRESS_LENGTH 9

#define BT_AGENT_AUTO_PAIR_BLACKLIST_FILE (APP_SYSCONFDIR"/auto-pair-blacklist")
#define BT_AGENT_NEW_LINE "\r\n"

#define BT_MAX_DBUS_TIMEOUT 45000
#define BT_ENABLE_TIMEOUT 30000 /* 30 seconds */
#define BT_DISCOVERY_FINISHED_DELAY 200

#define MANAGER_EVENT_MATCH_RULE \
			"type='signal'," \
			"interface='%s'," \
			"path='%s'"

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
#define BT_AGENT_NAME "org.bluez.frwk_agent"
#define BT_AGENT_PATH "/org/bluez/agent/frwk_agent"
#define BT_DEVICE_AGENT_PATH "/org/tizen/device_agent"
#define BT_ADAPTER_AGENT_PATH "/org/tizen/adapter_agent"
#define BT_MANAGER_PATH "/"
#define BT_MANAGER_INTERFACE "org.bluez.Manager"
#define BT_ADAPTER_INTERFACE "org.bluez.Adapter"
#define BT_AGENT_INTERFACE "org.bluez.Agent"
#define BT_DEVICE_INTERFACE "org.bluez.Device"
#define BT_INPUT_INTERFACE "org.bluez.Input"
#define BT_NETWORK_INTERFACE "org.bluez.Network"
#define BT_NETWORK_SERVER_INTERFACE "org.bluez.NetworkServer"
#define BT_NETWORK_CLIENT_INTERFACE "org.bluez.Network"
#define BT_SERIAL_INTERFACE "org.bluez.Serial"
#define BT_SERIAL_MANAGER_INTERFACE "org.bluez.SerialProxyManager"
#define BT_SERIAL_PROXY_INTERFACE "org.bluez.SerialProxy"
#define BT_HEADSET_INTERFACE "org.bluez.Headset"
#define BT_SINK_INTERFACE "org.bluez.AudioSink"
#define BT_AUDIO_INTERFACE "org.bluez.Audio"
#define BT_OOB_INTERFACE "org.bluez.OutOfBand"
#define BT_MEDIA_INTERFACE "org.bluez.Media"
#define BT_MEDIA_PLAYER_INTERFACE "org.bluez.MediaPlayer"
#define BT_HANDSFREE_GATEWAY_INTERFACE "org.bluez.HandsfreeGateway"
#define BT_OBEXD_INTERFACE "org.openobex"
#define BT_OBEXD_MANAGER_INTERFACE "org.openobex.Manager"
#define BT_OBEXD_TRANSFER_INTERFACE "org.openobex.Transfer"

#define BT_OBEX_SERVICE_NAME "org.openobex.client"

#define BT_OBEX_CLIENT_PATH "/"
#define BT_OBEX_CLIENT_INTERFACE "org.openobex.Client"
#define BT_OBEX_TRANSFER_INTERFACE "org.openobex.Transfer"
#define BT_OBEX_AGENT_INTERFACE "org.openobex.Agent"

#define BT_FREEDESKTOP_INTERFACE "org.freedesktop.DBus"
#define BT_FREEDESKTOP_PATH "/org/freedesktop/DBus"

/* Privilege */
#define BT_PRIVILEGE_MANAGER "bt-service::manager"
#define BT_PRIVILEGE_ADMIN "bt-service::admin"
#define BT_PRIVILEGE_GAP "bt-service::gap"
#define BT_PRIVILEGE_OPP "bt-service::opp"
#define BT_PRIVILEGE_SPP "bt-service::spp"
#define BT_PRIVILEGE_HEALTH "bt-service::health"

typedef enum {
	BT_OBEX_SERVER = 0x00,
	BT_RFCOMM_SERVER = 0x01,
} bt_osp_server_type_t;

typedef struct {
	int rssi;
	int class;
	char *address;
	char *name;
	char **uuids;
	int uuid_count;
	gboolean paired;
	gboolean connected;
	gboolean trust;
} bt_remote_dev_info_t;

/* RFCOMM client /server will use this structure*/
typedef struct {
	int fd;
	GIOChannel *io_channel;
	guint io_event;
	char *dev_node;
	char *address;
	char *uuid;
#ifdef GIOOUT_Q_WRITE
	GQueue *tx_queue;
	char *current_tx_buf;
	char *tx_buf;
	int current_tx_len;
	int write_id;
#endif
} bt_rfcomm_info_t;

typedef struct {
	int req_id;
	char *address;
} bt_function_data_t;

DBusConnection *_bt_get_system_conn(void);

DBusGConnection *_bt_get_system_gconn(void);

DBusGConnection *_bt_get_session_gconn(void);

DBusGProxy *_bt_get_manager_proxy(void);

DBusGProxy *_bt_get_adapter_proxy(void);

char *_bt_get_adapter_path(void);

void _bt_deinit_proxys(void);

void _bt_convert_device_path_to_address(const char *device_path,
						char *device_address);

void _bt_convert_addr_string_to_type(unsigned char *addr,
					const char *address);

void _bt_convert_addr_type_to_string(char *address,
				unsigned char *addr);

void _bt_print_device_address_t(const bluetooth_device_address_t *addr);

void _bt_divide_device_class(bluetooth_device_class_t *device_class,
				unsigned int cod);

void _bt_free_device_info(bt_remote_dev_info_t *dev_info);

int _bt_register_osp_server_in_agent(int type, char *uuid);

int _bt_unregister_osp_server_in_agent(int type, char *uuid);

int _bt_set_socket_non_blocking(int socket_fd);

int _bt_set_non_blocking_tty(int sk);

gboolean _bt_is_headset_class(int dev_class);

void _bt_deinit_bluez_proxy(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_COMMON_H_*/

