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


#ifndef _BT_INTERNAL_TYPES_H_
#define _BT_INTERNAL_TYPES_H_

#include <sys/types.h>
#include <libintl.h>

#include <dlog.h>

#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
	BT_NO_SERVER,
	BT_NATIVE_SERVER,
	BT_CUSTOM_SERVER,
	BT_FTP_SERVER,
} bt_server_type_t;

typedef enum {
	BT_SYNC_REQ,
	BT_ASYNC_REQ
} bt_req_type_t;

typedef enum {
	BT_MANAGER_EVENT = 0x01,
	BT_ADAPTER_EVENT,
	BT_LE_ADAPTER_EVENT,
	BT_DEVICE_EVENT,
	BT_HID_EVENT,
	BT_NETWORK_EVENT,
	BT_HEADSET_EVENT,
	BT_AVRCP_EVENT,
	BT_OPP_CLIENT_EVENT,
	BT_OPP_SERVER_EVENT,
	BT_PBAP_CLIENT_EVENT,
	BT_RFCOMM_CLIENT_EVENT,
	BT_RFCOMM_SERVER_EVENT,
	BT_AGENT_EVENT,
	BT_OBJECT_MANAGER_EVENT,
	BT_MEDIA_TRANSFER_EVENT,
	BT_HF_AGENT_EVENT,
	BT_AVRCP_CONTROL_EVENT,
	BT_A2DP_SOURCE_EVENT,
	BT_HID_DEVICE_EVENT,
	/* Will be added */
} bt_event_type_t;

typedef enum {
	BT_OBEX_SERVER = 0x00,
	BT_RFCOMM_SERVER = 0x01,
} bt_osp_server_type_t;

typedef enum {
	BT_BLUEZ_SERVICE = 0x00,
	BT_OBEX_SERVICE,
	BT_AGENT_SERVICE,
	BT_CORE_SERVICE,
	BT_CHECK_PRIVILEGE,
} bt_service_type_t;

typedef enum {
	BT_RFCOMM_UUID = 0x00,
	BT_RFCOMM_CHANNEL,
} bt_rfcomm_connect_type_t;

typedef enum {
	BT_ADAPTER_DISABLED = 0x00,
	BT_ADAPTER_ENABLED,
} bt_adapter_status_t;

typedef enum {
	BT_ADAPTER_LE_DISABLED = 0x00,
	BT_ADAPTER_LE_ENABLED,
} bt_adapter_le_status_t;

#define BT_ADDRESS_STR_LEN 18
#define BT_DBUS_TIMEOUT_MAX 50000
#define BT_SERVER_ACCEPT_TIMEOUT 2000 /* 2 seconds */
#define BT_FILE_PATH_MAX 256
#define BT_META_DATA_MAX_LEN 512 + 1

#define BT_HFP_AUDIO_GATEWAY_UUID "0000111f-0000-1000-8000-00805f9b34fb"
#define BT_A2DP_UUID "0000110D-0000-1000-8000-00805F9B34FB"
#define BT_AVRCP_TARGET_UUID "0000110c-0000-1000-8000-00805f9b34fb"
#define BT_OPP_UUID "00001105-0000-1000-8000-00805f9b34fb"
#define BT_FTP_UUID "00001106-0000-1000-8000-00805f9b34fb"
#define BT_SPP_UUID "00001101-0000-1000-8000-00805f9b34fb"
#define BT_HID_UUID "00001124-0000-1000-8000-00805f9b34fb"
#define BT_PAN_PANU_UUID "00001115-0000-1000-8000-00805f9b34fb"
#define BT_PAN_NAP_UUID "00001116-0000-1000-8000-00805f9b34fb"
#define BT_PAN_GN_UUID "00001117-0000-1000-8000-00805f9b34fb"

#define BT_FUNC_BASE ((int)(0x0000))
#define BT_FUNC_DEVICE_BASE ((int)(BT_FUNC_BASE + 0x0050))
#define BT_FUNC_HID_BASE ((int)(BT_FUNC_DEVICE_BASE + 0x0020))
#define BT_FUNC_NETWORK_BASE ((int)(BT_FUNC_HID_BASE + 0x0020))
#define BT_FUNC_AUDIO_BASE ((int)(BT_FUNC_NETWORK_BASE + 0x0020))
#define BT_FUNC_OOB_BASE ((int)(BT_FUNC_AUDIO_BASE + 0x0020))
#define BT_FUNC_AVRCP_BASE ((int)(BT_FUNC_OOB_BASE + 0x0020))
#define BT_FUNC_OPP_BASE ((int)(BT_FUNC_AVRCP_BASE + 0x0020))
#define BT_FUNC_RFCOMM_BASE ((int)(BT_FUNC_OPP_BASE + 0x0020))
#define BT_FUNC_PBAP_BASE ((int)(BT_FUNC_RFCOMM_BASE + 0x0020))
#define BT_FUNC_HDP_BASE ((int)(BT_FUNC_PBAP_BASE + 0x0020))
#define BT_FUNC_GATT_BASE ((int)(BT_FUNC_HDP_BASE + 0x0020))
#define BT_FUNC_IPSP_BASE ((int)(BT_FUNC_GATT_BASE + 0x0020))

typedef enum {
	BT_CHECK_ADAPTER = BT_FUNC_BASE,
	BT_ENABLE_ADAPTER,
	BT_DISABLE_ADAPTER,
	BT_RECOVER_ADAPTER,
	BT_SET_DISCOVERABLE_TIME,
	BT_GET_DISCOVERABLE_TIME,
	BT_IGNORE_AUTO_PAIRING,
	BT_GET_LOCAL_ADDRESS,
	BT_GET_LOCAL_VERSION,
	BT_GET_LOCAL_NAME,
	BT_SET_LOCAL_NAME,
	BT_IS_SERVICE_USED,
	BT_GET_DISCOVERABLE_MODE,
	BT_SET_DISCOVERABLE_MODE,
	BT_START_DISCOVERY,
	BT_START_CUSTOM_DISCOVERY,
	BT_CANCEL_DISCOVERY,
	BT_START_LE_DISCOVERY,
	BT_STOP_LE_DISCOVERY,
	BT_IS_DISCOVERYING,
	BT_IS_LE_DISCOVERYING,
	BT_ENABLE_RSSI,
	BT_GET_RSSI,
	BT_IS_CONNECTABLE,
	BT_SET_CONNECTABLE,
	BT_GET_BONDED_DEVICES,
	BT_RESET_ADAPTER,
	BT_SET_ADVERTISING,
	BT_SET_CUSTOM_ADVERTISING,
	BT_SET_ADVERTISING_PARAMETERS,
	BT_GET_ADVERTISING_DATA,
	BT_SET_ADVERTISING_DATA,
	BT_SET_SCAN_PARAMETERS,
	BT_GET_SCAN_RESPONSE_DATA,
	BT_SET_SCAN_RESPONSE_DATA,
	BT_IS_ADVERTISING,
	BT_SET_MANUFACTURER_DATA,
	BT_LE_CONN_UPDATE,
	BT_LE_READ_MAXIMUM_DATA_LENGTH,
	BT_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH,
	BT_LE_READ_HOST_SUGGESTED_DATA_LENGTH,
	BT_LE_SET_DATA_LENGTH,
	BT_ADD_WHITE_LIST,
	BT_REMOVE_WHITE_LIST,
	BT_CLEAR_WHITE_LIST,
	BT_REGISTER_SCAN_FILTER,
	BT_UNREGISTER_SCAN_FILTER,
	BT_UNREGISTER_ALL_SCAN_FILTERS,
	BT_BOND_DEVICE = BT_FUNC_DEVICE_BASE,
	BT_BOND_DEVICE_BY_TYPE,
	BT_CANCEL_BONDING,
	BT_PASSKEY_REPLY,
	BT_PASSKEY_CONFIRMATION_REPLY,
	BT_UNBOND_DEVICE,
	BT_SEARCH_SERVICE,
	BT_CANCEL_SEARCH_SERVICE,
	BT_GET_BONDED_DEVICE,
	BT_SET_ALIAS,
	BT_SET_AUTHORIZATION,
	BT_UNSET_AUTHORIZATION,
	BT_IS_DEVICE_CONNECTED,
	BT_GET_CONNECTED_LINK_TYPE,
	BT_SET_PIN_CODE,
	BT_UNSET_PIN_CODE,
	BT_UPDATE_LE_CONNECTION_MODE,
	BT_HID_CONNECT = BT_FUNC_HID_BASE,
	BT_HID_DISCONNECT,
	BT_HID_DEVICE_ACTIVATE,
	BT_HID_DEVICE_DEACTIVATE,
	BT_HID_DEVICE_CONNECT,
	BT_HID_DEVICE_DISCONNECT,
	BT_HID_DEVICE_SEND_MOUSE_EVENT,
	BT_HID_DEVICE_SEND_KEY_EVENT,
	BT_HID_DEVICE_SEND_REPLY_TO_REPORT,
	BT_NETWORK_ACTIVATE = BT_FUNC_NETWORK_BASE,
	BT_NETWORK_DEACTIVATE,
	BT_NETWORK_CONNECT,
	BT_NETWORK_DISCONNECT,
	BT_NETWORK_SERVER_DISCONNECT,
	BT_AUDIO_CONNECT = BT_FUNC_AUDIO_BASE,
	BT_AUDIO_DISCONNECT,
	BT_AG_CONNECT,
	BT_AG_DISCONNECT,
	BT_AV_CONNECT,
	BT_AV_DISCONNECT,
	BT_AV_SOURCE_CONNECT,
	BT_AV_SOURCE_DISCONNECT,
	BT_HF_CONNECT,
	BT_HF_DISCONNECT,
	BT_GET_SPEAKER_GAIN,
	BT_SET_SPEAKER_GAIN,
	BT_SET_CONTENT_PROTECT,
	BT_OOB_READ_LOCAL_DATA = BT_FUNC_OOB_BASE,
	BT_OOB_ADD_REMOTE_DATA,
	BT_OOB_REMOVE_REMOTE_DATA,
	BT_AVRCP_SET_TRACK_INFO = BT_FUNC_AVRCP_BASE,
	BT_AVRCP_SET_PROPERTY,
	BT_AVRCP_SET_PROPERTIES,
	BT_AVRCP_CONTROL_CONNECT,
	BT_AVRCP_CONTROL_DISCONNECT,
	BT_AVRCP_HANDLE_CONTROL,
	BT_AVRCP_CONTROL_SET_PROPERTY,
	BT_AVRCP_CONTROL_GET_PROPERTY,
	BT_AVRCP_GET_TRACK_INFO,
	BT_OPP_PUSH_FILES = BT_FUNC_OPP_BASE,
	BT_OPP_CANCEL_PUSH,
	BT_OPP_IS_PUSHING_FILES,
	BT_OBEX_SERVER_ALLOCATE,
	BT_OBEX_SERVER_DEALLOCATE,
	BT_OBEX_SERVER_IS_ACTIVATED,
	BT_OBEX_SERVER_ACCEPT_CONNECTION,
	BT_OBEX_SERVER_REJECT_CONNECTION,
	BT_OBEX_SERVER_ACCEPT_FILE,
	BT_OBEX_SERVER_REJECT_FILE,
	BT_OBEX_SERVER_SET_PATH,
	BT_OBEX_SERVER_SET_ROOT,
	BT_OBEX_SERVER_CANCEL_TRANSFER,
	BT_OBEX_SERVER_CANCEL_ALL_TRANSFERS,
	BT_OBEX_SERVER_IS_RECEIVING,
	BT_RFCOMM_CLIENT_CONNECT = BT_FUNC_RFCOMM_BASE,
	BT_RFCOMM_CLIENT_CANCEL_CONNECT,
	BT_RFCOMM_CLIENT_IS_CONNECTED,
	BT_RFCOMM_SOCKET_DISCONNECT,
	BT_RFCOMM_SOCKET_WRITE,
	BT_RFCOMM_CREATE_SOCKET,
	BT_RFCOMM_REMOVE_SOCKET,
	BT_RFCOMM_LISTEN,
	BT_RFCOMM_IS_UUID_AVAILABLE,
	BT_RFCOMM_ACCEPT_CONNECTION,
	BT_RFCOMM_REJECT_CONNECTION,
	BT_RFCOMM_CREATE_SOCKET_EX,
	BT_RFCOMM_REMOVE_SOCKET_EX,
	BT_PBAP_CONNECT = BT_FUNC_PBAP_BASE,
	BT_PBAP_DISCONNECT,
	BT_PBAP_GET_PHONEBOOK_SIZE,
	BT_PBAP_GET_PHONEBOOK,
	BT_PBAP_GET_LIST,
	BT_PBAP_PULL_VCARD,
	BT_PBAP_PHONEBOOK_SEARCH,

	BT_ENABLE_ADAPTER_LE,
	BT_DISABLE_ADAPTER_LE,

	BT_CONNECT_LE,
	BT_DISCONNECT_LE,
	BT_SET_LE_PRIVACY,
	BT_HDP_CONNECT = BT_FUNC_HDP_BASE,
	BT_HDP_DISCONNECT,
	BT_HDP_SEND_DATA,
	BT_GATT_GET_PRIMARY_SERVICES = BT_FUNC_GATT_BASE,
	BT_GATT_DISCOVER_CHARACTERISTICS,
	BT_GATT_SET_PROPERTY_REQUEST,
	BT_GATT_READ_CHARACTERISTIC,
	BT_GATT_DISCOVER_CHARACTERISTICS_DESCRIPTOR,
	BT_GATT_WATCH_CHARACTERISTIC,
	BT_GATT_UNWATCH_CHARACTERISTIC,
	BT_LE_IPSP_INIT = BT_FUNC_IPSP_BASE,
	BT_LE_IPSP_DEINIT,
	BT_LE_IPSP_CONNECT,
	BT_LE_IPSP_DISCONNECT,
} bt_function_t;

typedef struct {
	char title[BT_META_DATA_MAX_LEN];
	char artist[BT_META_DATA_MAX_LEN];
	char album[BT_META_DATA_MAX_LEN];
	char genre[BT_META_DATA_MAX_LEN];
	unsigned int total_tracks;
	unsigned int number;
	unsigned int duration;
} media_metadata_t;

#define BT_COMMON_PKG "ug-setting-bluetooth-efl"

/* Need to convert the design ID */
#define BT_STR_NOT_SUPPORT "Not support"

#ifndef TIZEN_WEARABLE
#define BT_FILE_VISIBLE_TIME "file/private/libug-setting-bluetooth-efl/visibility_time"
#endif

#define BT_OFF_DUE_TO_FLIGHT_MODE "file/private/bt-core/flight_mode_deactivated"
#define BT_OFF_DUE_TO_POWER_SAVING_MODE "file/private/bt-core/powersaving_mode_deactivated"
#define BT_OFF_DUE_TO_TIMEOUT "file/private/bt-service/bt_off_due_to_timeout"

#define BT_EVENT_SERVICE "org.projectx.bt_event"
#define BT_HF_AGENT_SERVICE	"org.bluez.HandsfreeAgent"
#define BT_HF_SERVICE_INTERFACE "org.tizen.HfApp"
#define BT_CORE_EVENT_INTERFACE "org.projectx.bt_core_event"
#define BT_HF_LOCAL_TERM_EVENT_INTERFACE "org.projectx.bt_hf_local_term"

#define BT_ADAPTER_PATH "/org/projectx/bt/adapter"
#define BT_LE_ADAPTER_PATH "/org/projectx/bt/le/adapter"
#define BT_DEVICE_PATH "/org/projectx/bt/device"
#define BT_HID_PATH "/org/projectx/bt/hid"
#define BT_HEADSET_PATH "/org/projectx/bt/headset"
#define BT_AVRCP_PATH "/org/projectx/bt/avrcp"
#define BT_NETWORK_PATH "/org/projectx/bt/newtork"
#define BT_OPP_CLIENT_PATH "/org/projectx/bt/opp_client"
#define BT_OPP_SERVER_PATH "/org/projectx/bt/opp_server"
#define BT_PBAP_CLIENT_PATH "/org/projectx/bt/pbap_client"
#define BT_RFCOMM_CLIENT_PATH "/org/projectx/bt/rfcomm_client"
#define BT_RFCOMM_SERVER_PATH "/org/projectx/bt/rfcomm_server"
#define BT_HF_AGENT_PATH "/org/bluez/handsfree_agent"
#define BT_CORE_EVENT_PATH "/org/projectx/bt/bt_core"
#define BT_HF_LOCAL_TERM_EVENT_PATH "/org/projectx/bt/hf_local_term"
#define BT_AVRCP_CONTROL_PATH "/org/projectx/bt/avrcp_control"
#define BT_A2DP_SOURCE_PATH "/org/projectx/bt/a2dp_source"
#define BT_HID_DEVICE_PATH "/org/projectx/bt/hid_device"

#define BT_ENABLED "Enabled"
#define BT_DISABLED "Disabled"
#define BT_LE_ENABLED "LeEnabled"
#define BT_LE_DISABLED "LeDisabled"
#define BT_DISCOVERABLE_MODE_CHANGED "DiscoverableModeChanged"
#define BT_DISCOVERABLE_TIMEOUT_CHANGED "DiscoverableTimeoutChanged"
#define BT_CONNECTABLE_CHANGED "ConnectableChanged"
#define BT_ADAPTER_NAME_CHANGED "AdapterNameChanged"
#define BT_DISCOVERY_STARTED "DiscoveryStarted"
#define BT_DISCOVERY_FINISHED "DiscoveryFinished"
#define BT_DEVICE_FOUND "DeviceFound"
#define BT_LE_DISCOVERY_STARTED "LEDiscoveryStarted"
#define BT_LE_DISCOVERY_FINISHED "LEDiscoveryFinished"
#define BT_LE_DEVICE_FOUND "LEDeviceFound"
#define BT_READ_MAXIMUM_LE_DATA_LENGTH "ReadMaximumLEDataLength"
#define BT_ADVERTISING_STARTED "AdvertisingStarted"
#define BT_ADVERTISING_STOPPED "AdvertisingStopped"
#define BT_ADVERTISING_MANUFACTURER_DATA_CHANGED "AdvertisingManufacturerDataChanged"
#define BT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED "ScanResponseManufacturerDataChanged"
#define BT_MANUFACTURER_DATA_CHANGED "ManufacturerDataChanged"
#define BT_DEVICE_CONNECTED "DeviceConnected"
#define BT_DEVICE_DISCONNECTED "DeviceDisconnected"
#define BT_DEVICE_PROFILE_STATE_CHANGED "ProfileStateChanged"
#define BT_BOND_CREATED "BondCreated"
#define BT_BOND_DESTROYED "BondDestroyed"
#define BT_KBD_PASSKEY_DISPLAY_REQ_RECEIVED "KeyBoardPasskeyDisplayRequest"
#define BT_PIN_REQ_RECEIVED "PinRequest"
#define BT_PASSKEY_REQ_RECEIVED "PasskeyRequest"
#define BT_PASSKEY_CFM_REQ_RECEIVED "PasskeyConfirmRequest"
#define BT_DEVICE_AUTHORIZED "DeviceAuthorized"
#define BT_DEVICE_UNAUTHORIZED "DeviceUnauthorized"
#define BT_RSSI_MONITORING_ENABLED "RssiMonitoringEnabled"
#define BT_RSSI_ALERT "RssiMonitoringAlert"
#define BT_RAW_RSSI_EVENT "RawRssiEvent"
#define BT_SERVICE_SEARCHED "ServiceSearched"
#define BT_INPUT_CONNECTED "InputConnected"
#define BT_INPUT_DISCONNECTED "InputDisconnected"
#define BT_PBAP_CONNECTED "PbapConnected"
#define BT_PBAP_DISCONNECTED "PbapDisconnected"
#define BT_PBAP_PHONEBOOK_SIZE "PbapPhonebookSize"
#define BT_PBAP_PHONEBOOK_PULL "PbapPhonebookPull"
#define BT_PBAP_VCARD_LIST "PbapvCardList"
#define BT_PBAP_VCARD_PULL "PbapvCardPull"
#define BT_PBAP_SEARCH_PHONEBOOK "PbapSearchPhonebook"
#define BT_HEADSET_CONNECTED "HeadsetConnected"
#define BT_HEADSET_DISCONNECTED "HeadsetDisconnected"
#define BT_STEREO_HEADSET_CONNECTED "StereoHeadsetConnected"
#define BT_STEREO_HEADSET_DISCONNECTED "StereoHeadsetDisconnected"
#define BT_A2DP_SOURCE_CONNECTED "A2DPSourceConnected"
#define BT_A2DP_SOURCE_DISCONNECTED "A2DPSourceDisconnected"
#define BT_SCO_CONNECTED "ScoConnected"
#define BT_SCO_DISCONNECTED "ScoDisconnected"
#define BT_SPEAKER_GAIN "SpeakerGain"
#define BT_MICROPHONE_GAIN "MicrophoneGain"
#define BT_HF_RING "Ring"
#define BT_HF_CALL_TERMINATED "CallTerminated"
#define BT_HF_CALL_STARTED "CallStarted"
#define BT_HF_CALL_ENDED "CallEnded"
#define BT_NETWORK_CONNECTED "NetworkConnected"
#define BT_NETWORK_DISCONNECTED "NetworkDisconnected"
#define BT_NETWORK_SERVER_CONNECTED "NetworkServerConnected"
#define BT_NETWORK_SERVER_DISCONNECTED "NetworkServerDisconnected"
#define BT_OPP_CONNECTED "OppConnected"
#define BT_OPP_DISCONNECTED "OppDisconnected"
#define BT_TRANSFER_CONNECTED "TransferConnected"
#define BT_TRANSFER_DISCONNECTED "TransferDisonnected"
#define BT_TRANSFER_STARTED "TransferStarted"
#define BT_TRANSFER_PROGRESS "TransferProgress"
#define BT_TRANSFER_COMPLETED "TransferCompleted"
#define BT_TRANSFER_AUTHORIZED "TransferAuthorized"
#define BT_CONNECTION_AUTHORIZED "ConnectionAuthorized"
#define BT_RFCOMM_SERVER_REMOVED "RfcommServerRemoved"
#define BT_RFCOMM_DATA_RECEIVED "RfcommDataReceived"
#define BT_RFCOMM_CONNECTED "RfcommConnected"
#define BT_RFCOMM_DISCONNECTED "RfcommDisconnected"
#define BT_AVRCP_CONNECTED "AvrcpConnected"
#define BT_AVRCP_DISCONNECTED "AvrcpDisconnected"
#define BT_MEDIA_SHUFFLE_STATUS "MediaShuffleStatus"
#define BT_MEDIA_EQUALIZER_STATUS "MediaEqualizerStatus"
#define BT_MEDIA_REPEAT_STATUS "MediaRepeatStatus"
#define BT_MEDIA_SCAN_STATUS "MediaScanStatus"
#define BT_MEDIA_PLAY_STATUS "MediaPlayStatus"
#define BT_MEDIA_POSITION_STATUS "MediaPositionStatus"
#define BT_MEDIA_TRACK_CHANGE "MediaTrackStatus"
#define BT_NAME_OWNER_CHANGED "NameOwnerChanged"
#define BT_GATT_CONNECTED "GattConnected"
#define BT_GATT_DISCONNECTED "GattDisconnected"
#define BT_GATT_CHAR_VAL_CHANGED "GattCharValueChanged"
#define BT_HARDWARE_ERROR "HardwareError"
#define BT_TX_TIMEOUT_ERROR "TxTimeoutError"
#define BT_HF_LOCAL_TERM "HandsfreeLocalTermination"
#define BT_HID_DEVICE_CONNECTED "HIDConnected"
#define BT_HID_DEVICE_DISCONNECTED "HIDDisconnected"
#define BT_IPSP_INITIALIZED "IpspInitStateChanged"
#define BT_IPSP_CONNECTED "IpspConnected"
#define BT_IPSP_DISCONNECTED "IpspDisconnected"
#define BT_LE_DATA_LENGTH_CHANGED "LEDataLengthChanged"

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_INTERNAL_TYPES_H_*/

