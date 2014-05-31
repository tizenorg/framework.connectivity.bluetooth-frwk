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

#ifndef _BLUETOOTH_API_H_
#define _BLUETOOTH_API_H_

#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */

/**
 * @defgroup BLUETOOTHFW BluetoothFW
 *
 * A base library for bluetooth framework
 *
 * @addtogroup BLUETOOTHFW
 * @{
 */

#define BLUETOOTH_ADDRESS_LENGTH            6 /**< This specifies bluetooth device address length */
#define BLUETOOTH_INTERFACE_NAME_LENGTH        16
#define BLUETOOTH_DEVICE_NAME_LENGTH_MAX       248 /**< This specifies maximum device name length */

#define BLUETOOTH_MAX_SERVICES_FOR_DEVICE       40  /**< This specifies maximum number of services
							a device can support */

#define BLUETOOTH_UUID_STRING_MAX 50

#define BLUETOOTH_OOB_DATA_LENGTH		16
/**
 * This is Bluetooth Connected event role
 */
#define RFCOMM_ROLE_SERVER 1
#define RFCOMM_ROLE_CLIENT 2

/**
 * This is Bluetooth error code
 */
#define BLUETOOTH_ERROR_BASE                   ((int)0)		/**< Error code base */

#define BLUETOOTH_ERROR_NONE                   ((int)0)		/**< No error #0 */
#define BLUETOOTH_ERROR_CANCEL                 ((int)BLUETOOTH_ERROR_BASE - 0x01)
								/**< cancelled */
#define BLUETOOTH_ERROR_INVALID_CALLBACK       ((int)BLUETOOTH_ERROR_BASE - 0x02)
								/**< Callback error */
#define BLUETOOTH_ERROR_INVALID_PARAM          ((int)BLUETOOTH_ERROR_BASE - 0x03)
								/**< invalid paramerror */
#define BLUETOOTH_ERROR_INVALID_DATA           ((int)BLUETOOTH_ERROR_BASE - 0x04)
								/**< invalid data error */
#define BLUETOOTH_ERROR_MEMORY_ALLOCATION      ((int)BLUETOOTH_ERROR_BASE - 0x05)
								/**< Memory allocation error */
#define BLUETOOTH_ERROR_OUT_OF_MEMORY          ((int)BLUETOOTH_ERROR_BASE - 0x06)
								/**< out of memory error */
#define BLUETOOTH_ERROR_TIMEOUT                ((int)BLUETOOTH_ERROR_BASE - 0x07)
								/**< timeout error */
#define BLUETOOTH_ERROR_NO_RESOURCES           ((int)BLUETOOTH_ERROR_BASE - 0x08)
								/**< No resource error */
#define BLUETOOTH_ERROR_INTERNAL               ((int)BLUETOOTH_ERROR_BASE - 0x09)
								/**< internal error */
#define BLUETOOTH_ERROR_NOT_SUPPORT            ((int)BLUETOOTH_ERROR_BASE - 0x0a)
								/**< Not supported error */
#define BLUETOOTH_ERROR_DEVICE_NOT_ENABLED     ((int)BLUETOOTH_ERROR_BASE - 0x0b)
								/**< Operation is failed because
								of not enabled BT Adapter */
#define BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED	((int)BLUETOOTH_ERROR_BASE - 0x0c)
								/**< Enabling is failed because of
								already enabled BT Adapter */
#define BLUETOOTH_ERROR_DEVICE_BUSY            ((int)BLUETOOTH_ERROR_BASE - 0x0d)
								/**< Operation is failed because of
								other on going operation */
#define BLUETOOTH_ERROR_ACCESS_DENIED          ((int)BLUETOOTH_ERROR_BASE - 0x0e)
								/**< access denied error */
#define BLUETOOTH_ERROR_MAX_CLIENT             ((int)BLUETOOTH_ERROR_BASE - 0x0f)
								/**< max client error */
#define BLUETOOTH_ERROR_NOT_FOUND              ((int)BLUETOOTH_ERROR_BASE - 0x10)
								/**< not found error */
#define BLUETOOTH_ERROR_SERVICE_SEARCH_ERROR   ((int)BLUETOOTH_ERROR_BASE - 0x11)
								/**< service search fail */
#define BLUETOOTH_ERROR_PARING_FAILED          ((int)BLUETOOTH_ERROR_BASE - 0x12)
								/**< pairing failed error */
#define BLUETOOTH_ERROR_NOT_PAIRED             ((int)BLUETOOTH_ERROR_BASE - 0x13)
								/**< Not paired error */
#define BLUETOOTH_ERROR_SERVICE_NOT_FOUND      ((int)BLUETOOTH_ERROR_BASE - 0x14)
								/**< no service error */
#define BLUETOOTH_ERROR_NOT_CONNECTED          ((int)BLUETOOTH_ERROR_BASE - 0x15)
								/**< no connection error */
#define BLUETOOTH_ERROR_ALREADY_CONNECT        ((int)BLUETOOTH_ERROR_BASE - 0x16)
								/**< alread connected error */
#define BLUETOOTH_ERROR_CONNECTION_BUSY        ((int)BLUETOOTH_ERROR_BASE - 0x17)
								/**< connection busy error */
#define BLUETOOTH_ERROR_CONNECTION_ERROR       ((int)BLUETOOTH_ERROR_BASE - 0x18)
								/**< connection error */
#define BLUETOOTH_ERROR_MAX_CONNECTION         ((int)BLUETOOTH_ERROR_BASE - 0x19)
								/**< max connection error*/
#define BLUETOOTH_ERROR_NOT_IN_OPERATION       ((int)BLUETOOTH_ERROR_BASE - 0x1a)
								/**< Not in operation */
#define BLUETOOTH_ERROR_CANCEL_BY_USER         ((int)BLUETOOTH_ERROR_BASE - 0x1b)
								/**< Cancelled by user */
#define BLUETOOTH_ERROR_REGISTRATION_FAILED    ((int)BLUETOOTH_ERROR_BASE - 0x1c)
								/**< Service record registration failed */
#define BLUETOOTH_ERROR_IN_PROGRESS            ((int)BLUETOOTH_ERROR_BASE - 0x1d)
								/**< Operation in progress */
#define BLUETOOTH_ERROR_AUTHENTICATION_FAILED  ((int)BLUETOOTH_ERROR_BASE - 0x1e)
								/**< authentication failed error when paring*/
#define BLUETOOTH_ERROR_HOST_DOWN              ((int)BLUETOOTH_ERROR_BASE - 0x1f)
								/**< Remote host is down */
#define BLUETOOTH_ERROR_END_OF_DEVICE_LIST     ((int)BLUETOOTH_ERROR_BASE - 0x20)
								/**< End of device list */

#define BLUETOOTH_ERROR_AGENT_ALREADY_EXIST      ((int)BLUETOOTH_ERROR_BASE - 0x21)
								/**< Obex agent already exists */
#define BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST    ((int)BLUETOOTH_ERROR_BASE - 0x22)
								/**< Obex agent does not exist */

#define BLUETOOTH_ERROR_ALREADY_INITIALIZED    ((int)BLUETOOTH_ERROR_BASE - 0x23)
								/**< Already initialized */

#define BLUETOOTH_ERROR_PERMISSION_DEINED    ((int)BLUETOOTH_ERROR_BASE - 0x24)
								/**< Permission deined */


/**
 * This is Bluetooth device address type, fixed to 6 bytes ##:##:##:##:##:##
 */
typedef struct {
	unsigned char addr[BLUETOOTH_ADDRESS_LENGTH];
} bluetooth_device_address_t;

/**
 * This is Bluetooth device name type, maximum size of Bluetooth device name is 248 bytes
 */
typedef struct {
	char name[BLUETOOTH_DEVICE_NAME_LENGTH_MAX + 1];
} bluetooth_device_name_t;

/**
 * Adapter state
 */
typedef enum {
	BLUETOOTH_ADAPTER_DISABLED,	    /**< Bluetooth adapter is disabled */
	BLUETOOTH_ADAPTER_ENABLED,	    /**< Bluetooth adapter is enabled */
	BLUETOOTH_ADAPTER_CHANGING_ENABLE,  /**< Bluetooth adapter is currently enabling */
	BLUETOOTH_ADAPTER_CHANGING_DISABLE, /**< Bluetooth adapter is currently disabling */
} bluetooth_adapter_state_t;

/**
 * Discoverable mode
 */
typedef enum {
	BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE,	 /**< Non discoverable mode */
	/*Changed the order to make it compatable with old method */
	BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE,/**< Discoverable mode */
	BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE,
							 /**< Discoverable mode with time limit
							After specific timeout, it is changed
								to non discoverable mode */
} bluetooth_discoverable_mode_t;

/**
 * Network connect mode
 */
typedef enum {
	BLUETOOTH_NETWORK_PANU_ROLE,
				 /**< PAN user */
	BLUETOOTH_NETWORK_NAP_ROLE,/**< Network Access Point */
	BLUETOOTH_NETWORK_GN_ROLE,  /**< Group ad-hoc Network */
	BLUETOOTH_NETWORK_CUSTOM_UUID, /**< Custom role */
} bluetooth_network_role_t;

/**
 * Service type
 */
typedef enum {
	BLUETOOTH_RFCOMM_SERVICE = 0x01,
	BLUETOOTH_A2DP_SERVICE = 0x02,
	BLUETOOTH_HSP_SERVICE = 0x04,
	BLUETOOTH_HID_SERVICE = 0x08,
	BLUETOOTH_NAP_SERVICE = 0x10,
} bluetooth_service_type_t;

#define BLUETOOTH_EVENT_BASE            ((int)(0x0000))		/**< No event */
#define BLUETOOTH_EVENT_GAP_BASE        ((int)(BLUETOOTH_EVENT_BASE + 0x0010))
								/**< Base ID for GAP Event */
#define BLUETOOTH_EVENT_SDP_BASE        ((int)(BLUETOOTH_EVENT_GAP_BASE + 0x0020))
								/**< Base ID for SDP events */
#define BLUETOOTH_EVENT_RFCOMM_BASE     ((int)(BLUETOOTH_EVENT_SDP_BASE + 0x0020))
								/**< Base ID for RFCOMM events */
#define BLUETOOTH_EVENT_NETWORK_BASE     ((int)(BLUETOOTH_EVENT_RFCOMM_BASE + 0x0020))
								/**< Base ID for NETWORK events */
#define BLUETOOTH_EVENT_HDP_BASE     ((int)(BLUETOOTH_EVENT_NETWORK_BASE + 0x0020))
								/**< Base ID for HDP events */
#define BLUETOOTH_EVENT_OPC_BASE  ((int)(BLUETOOTH_EVENT_HDP_BASE + 0x0020))
								/**< Base ID for OPC events */
#define BLUETOOTH_EVENT_OBEX_SERVER_BASE ((int)(BLUETOOTH_EVENT_OPC_BASE + 0x0020))
								/**< Base ID for Obex Server events */
#define BLUETOOTH_EVENT_GATT_BASE ((int)(BLUETOOTH_EVENT_OBEX_SERVER_BASE + 0x0020))
								/**< Base ID for GATT events */

#define BLUETOOTH_EVENT_AUDIO_BASE ((int)(BLUETOOTH_EVENT_GATT_BASE + 0x0020))
								/**< Base ID for Audio events */
#define BLUETOOTH_EVENT_HID_BASE ((int)(BLUETOOTH_EVENT_AUDIO_BASE + 0x0020))
								/**< Base ID for HID events */

/**
 * Bluetooth event type
 */
typedef enum {
	BLUETOOTH_EVENT_NONE = BLUETOOTH_EVENT_BASE,/**< No event */

	BLUETOOTH_EVENT_ENABLED,		    /**< Bluetooth event adpater enabled */
	BLUETOOTH_EVENT_DISABLED,		    /**< Bluetooth event adpater disabled */
	BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,	    /**< Bluetooth event local name changed*/
	BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_REQUESTED,
					/**< Bluetooth event Discoverable timeout requested*/
	BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,  /**< Bluetooth event mode changed */
	BLUETOOTH_EVENT_DISCOVERY_OPTION_REQUESTED, /**< Bluetooth event discovery option */
	BLUETOOTH_EVENT_DISCOVERY_STARTED,	    /**< Bluetooth event discovery started */
	BLUETOOTH_EVENT_DISCOVERY_FINISHED,	    /**< Bluetooth event discovery finished */

	BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND,	    /**< Bluetooth event remote deice found */
	BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED,/**< Bluetooth event remote device name updated*/
	BLUETOOTH_EVENT_BONDING_FINISHED,	    /**< Bluetooth event bonding completed */
	BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED,	    /**< Bluetooth event bonding removed */
	BLUETOOTH_EVENT_BONDED_DEVICE_FOUND,	    /**< Bluetooth event paired device found */
	BLUETOOTH_EVENT_REMOTE_DEVICE_READ,	    /**< Bluetooth event read remote device */
	BLUETOOTH_EVENT_DEVICE_AUTHORIZED,	    /**< Bluetooth event authorize device */
	BLUETOOTH_EVENT_DEVICE_UNAUTHORIZED,	    /**< Bluetooth event unauthorize device */
	BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED,  /**< Bluetooth event mode changed */

	BLUETOOTH_EVENT_SERVICE_SEARCHED = BLUETOOTH_EVENT_SDP_BASE,
						    /**< Bluetooth event serice search base id */
	BLUETOOTH_EVENT_SERVICE_SEARCH_CANCELLED,   /**< Bluetooth event service search cancelled */
	BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED = BLUETOOTH_EVENT_RFCOMM_BASE,
							/**< RFCOMM data receive event */
	BLUETOOTH_EVENT_RFCOMM_CONNECTED,		/**< Rfcomm server incomming connection */
	BLUETOOTH_EVENT_RFCOMM_DISCONNECTED,		/**< Rfcomm server/client disconnect */

	BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,

	BLUETOOTH_EVENT_DEVICE_CONNECTED,	    /**< Bluetooth event device connected */
	BLUETOOTH_EVENT_DEVICE_DISCONNECTED,	    /**< Bluetooth event device disconnected */

	BLUETOOTH_EVENT_RFCOMM_SERVER_REMOVED,

	BLUETOOTH_EVENT_NETWORK_SERVER_ACTIVATED = BLUETOOTH_EVENT_NETWORK_BASE,
								/**< Bluetooth Network event */
	BLUETOOTH_EVENT_NETWORK_SERVER_DEACTIVATED, /**< Network server deactivated */
	BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED,     /**< Network connected event in server */
	BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED,
						   /**< Network disconnected evnet in server */

	BLUETOOTH_EVENT_NETWORK_CONNECTED,		/**< Network connected event in client*/
	BLUETOOTH_EVENT_NETWORK_DISCONNECTED,		/**< Network disconnected evnet in client*/

	BLUETOOTH_EVENT_HDP_CONNECTED
			= BLUETOOTH_EVENT_HDP_BASE,		   /**<HDP Connect>*/
	BLUETOOTH_EVENT_HDP_DISCONNECTED,	   /**<HDP Disconnect>*/
	BLUETOOTH_EVENT_HDP_DATA_RECEIVED,	   /**<HDP Data Indication>*/

	BLUETOOTH_EVENT_OPC_CONNECTED = BLUETOOTH_EVENT_OPC_BASE,
								/* OPC Connected event */
	BLUETOOTH_EVENT_OPC_DISCONNECTED, 		/* OPC Disonnected event */
	BLUETOOTH_EVENT_OPC_TRANSFER_STARTED,	/* OPC Transfer started event */
	BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS,	/* OPC Transfer progress event */
	BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE,	/* OPC Transfer Complete event */

	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE = BLUETOOTH_EVENT_OBEX_SERVER_BASE,
								/* Obex server authorize event*/
	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED,	/* Obex Server transfer started event*/
	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS,/* Obex Server transfer progress event*/
	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED,/* Obex Server transfer complete event*/
	BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED, /* OPC Transfer connected event */
	BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_DISCONNECTED, /* OPC Transfer disconnected event */

	BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED = BLUETOOTH_EVENT_GATT_BASE,
				/**<Discovered GATT service characteristics event*/
	BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
				/**<Remote GATT charateristic value changed event*/
	BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID,
	BLUETOOTH_EVENT_GATT_CONNECTED,/**<Gatt connected event */
	BLUETOOTH_EVENT_GATT_DISCONNECTED, /**<Gatt Disconnected event */
	BLUETOOTH_EVENT_GATT_RSSI, /**<Get RSSI value for remote device */
	BLUETOOTH_EVENT_GATT_READ_CHAR, /**<Gatt Read Characteristic Value */
	BLUETOOTH_EVENT_GATT_WRITE_CHAR, /**<Gatt Write Characteristic Value */

	BLUETOOTH_EVENT_AG_CONNECTED = BLUETOOTH_EVENT_AUDIO_BASE, /**<AG service connected event*/
	BLUETOOTH_EVENT_AG_DISCONNECTED, /**<AG service disconnected event*/
	BLUETOOTH_EVENT_AG_SPEAKER_GAIN, /**<Speaker gain request event*/
	BLUETOOTH_EVENT_AG_MIC_GAIN, /**<Mic gain request event*/
	BLUETOOTH_EVENT_AG_AUDIO_CONNECTED, /**<AV & AG service connected event*/
	BLUETOOTH_EVENT_AG_AUDIO_DISCONNECTED,  /**<AV & AG service disconnected event*/
	BLUETOOTH_EVENT_AV_CONNECTED, /**<AV service connected event*/
	BLUETOOTH_EVENT_AV_DISCONNECTED, /**<AV service disconnected event*/
	BLUETOOTH_EVENT_AVRCP_CONNECTED, /**<AVRCP service connected event*/
	BLUETOOTH_EVENT_AVRCP_DISCONNECTED, /**<AVRCP service disconnected event*/
	BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS, /**<AVRCP service player suffle  status event*/
	BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS, /**<AVRCP service player equalizer status event*/
	BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS, /**<AVRCP service player repeat status event*/
	BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS, /**<AVRCP service player scan status event*/

	BLUETOOTH_HID_CONNECTED = BLUETOOTH_EVENT_HID_BASE, /**< Input connectd event*/
	BLUETOOTH_HID_DISCONNECTED, /**< Input disconnectd event*/
	BLUETOOTH_EVENT_MAX, /**< Bluetooth event Max value */
} bluetooth_event_type_t;

 /**
 * This bt_service_uuid_list_t  enum  indicates service uuid list .
 * This values is stored the service_list_array in bt_sdp_info_t and bluetooth_device_info_t.
 */

typedef enum {
	BLUETOOTH_SPP_PROFILE_UUID = ((unsigned short)0x1101),			/**<SPP*/
	BLUETOOTH_LAP_PROFILE_UUID = ((unsigned short)0x1102),			/**<LAP*/
	BLUETOOTH_DUN_PROFILE_UUID = ((unsigned short)0x1103),			/**<DUN*/
	BLUETOOTH_OBEX_IR_MC_SYNC_SERVICE_UUID = ((unsigned short)0x1104),	/**<OBEX IR MC SYNC*/
	BLUETOOTH_OBEX_OBJECT_PUSH_SERVICE_UUID = ((unsigned short)0x1105),	/**<OPP*/
	BLUETOOTH_OBEX_FILE_TRANSFER_UUID = ((unsigned short)0x1106),		/**<FTP*/
	BLUETOOTH_IRMC_SYNC_COMMAND_UUID = ((unsigned short)0x1107),		/**<IRMC SYNC COMMAND*/
	BLUETOOTH_HS_PROFILE_UUID = ((unsigned short)0x1108),			/**<HS*/
	BLUETOOTH_CTP_PROFILE_UUID = ((unsigned short)0x1109),			/**<CTP*/
	BLUETOOTH_AUDIO_SOURCE_UUID = ((unsigned short)0x110A),			/**<AUDIO SOURCE*/
	BLUETOOTH_AUDIO_SINK_UUID = ((unsigned short)0x110B),			/**<AUDIO SINK*/
	BLUETOOTH_AV_REMOTE_CONTROL_TARGET_UUID = ((unsigned short)0x110C),	/**<AV REMOTE CONTROL
										TARGET*/
	BLUETOOTH_ADVANCED_AUDIO_PROFILE_UUID = ((unsigned short)0x110D),	/**<A2DP*/
	BLUETOOTH_AV_REMOTE_CONTROL_UUID = ((unsigned short)0x110E),		/**<AV REMOTE CONTROL UUID*/
	BLUETOOTH_AV_REMOTE_CONTROL_CONTROLLER_UUID = ((unsigned short)0x110F),	/**<AV REMOTE CONTROLLER UUID*/
	BLUETOOTH_ICP_PROFILE_UUID = ((unsigned short)0x1110),			/**<ICP*/
	BLUETOOTH_FAX_PROFILE_UUID = ((unsigned short)0x1111),			/**<FAX*/
	BLUETOOTH_HEADSET_AG_SERVICE_UUID = ((unsigned short)0x1112),		/**<HS AG */
	BLUETOOTH_PAN_PANU_PROFILE_UUID = ((unsigned short)0x1115),		/**<PAN*/
	BLUETOOTH_PAN_NAP_PROFILE_UUID = ((unsigned short)0x1116),		/**<PAN*/
	BLUETOOTH_PAN_GN_PROFILE_UUID = ((unsigned short)0x1117),		/**<PAN*/
	BLUETOOTH_DIRECT_PRINTING = ((unsigned short)0x1118),
	BLUETOOTH_OBEX_BPPS_PROFILE_UUID = ((unsigned short)0x1118),		/**<OBEX BPPS*/ /* Will be removed */
	BLUETOOTH_REFERENCE_PRINTING = ((unsigned short)0x1119),
	BLUETOOTH_OBEX_IMAGING_UUID = ((unsigned short)0x111A),			/**<OBEX_IMAGING*/
	BLUETOOTH_OBEX_IMAGING_RESPONDER_UUID = ((unsigned short)0x111B),	/**<OBEX_IMAGING
										RESPONDER*/
	BLUETOOTH_IMAGING_AUTOMATIC_ARCHIVE_UUID = ((unsigned short)0x111C),	/**<IMAGING AUTOMATIC ARCHIVE*/
	BLUETOOTH_IMAGING_REFERENCED_OBJECTS_UUID = ((unsigned short)0x111D),	/**<IMAGING REFERENCED OBJECTS*/
	BLUETOOTH_HF_PROFILE_UUID = ((unsigned short)0x111E),			/**<HF*/
	BLUETOOTH_HFG_PROFILE_UUID = ((unsigned short)0x111F),			/**<HFG*/
	BLUETOOTH_DIRECT_PRINTING_REFERENCE_OBJ_UUID = ((unsigned short)0x1120),
									/**<DIRECT PRINTING*/
	BLUETOOTH_REFLECTED_UI = ((unsigned short)0x1121),		/**<REFLECTED UI*/
	BLUETOOTH_BASIC_PRINTING = ((unsigned short)0x1122),		/**<BASIC PRINTING*/
	BLUETOOTH_PRINTING_STATUS = ((unsigned short)0x1123),		/**<PRINTING  STATUS*/
	BLUETOOTH_OBEX_PRINTING_STATUS_UUID = ((unsigned short)0x1123),	/**<OBEX PRINTING STATUS*/ /* Will be removed */
	BLUETOOTH_HID_PROFILE_UUID = ((unsigned short)0x1124),		/**<HID*/
	BLUETOOTH_HCR_PROFILE_UUID = ((unsigned short)0x1125),		/**<HCRP*/
	BLUETOOTH_HCR_PRINT_UUID = ((unsigned short)0x1126),		/**<HCR PRINT*/
	BLUETOOTH_HCR_SCAN_UUID = ((unsigned short)0x1127),		/**<HCR SCAN*/
	BLUETOOTH_SIM_ACCESS_PROFILE_UUID = ((unsigned short)0x112D),	/**<SIM ACCESS PROFILE*/
	BLUETOOTH_PBAP_PCE_UUID = ((unsigned short)0x112E),		/**<PBAP - PCE*/
	BLUETOOTH_PBAP_PSE_UUID = ((unsigned short)0x112F),		/**<OBEX PBA*/
	BLUETOOTH_OBEX_PBA_PROFILE_UUID = ((unsigned short)0x112F),	/**<OBEX PBA*/ /* Will be removed */
	BLUETOOTH_OBEX_PBAP_UUID = ((unsigned short)0x1130),		/**<OBEX PBA*/
	BLUETOOTH_HEADSET_HS_UUID = ((unsigned short)0x1131),		/**<HEADSET HS*/
	BLUETOOTH_MESSAGE_ACCESS_SERVER_UUID = ((unsigned short)0x1132),/**<MESSAGE ACCESS SERVER*/
	BLUETOOTH_MESSAGE_NOTIFICATION_SERVER_UUID = ((unsigned short)0x1133),/**<MESSAGE NOTIFICATION SERVER*/
	BLUETOOTH_MESSAGE_ACCESS_PROFILE_UUID = ((unsigned short)0x1134),/**<MESSAGE ACCESS PROFILE*/
	BLUETOOTH_PNP_INFORMATION_UUID = ((unsigned short)0x1200),	/**<PNP*/
	BLUETOOTH_GENERIC_NETWORKING_UUID = ((unsigned short)0x1201),	/**<GENERIC NETWORKING*/
	BLUETOOTH_GENERIC_FILE_TRANSFER_UUID = ((unsigned short)0x1202),/**<GENERIC FILE TRANSFER*/
	BLUETOOTH_GENERIC_AUDIO_UUID = ((unsigned short)0x1203),	/**<GENERIC AUDIO*/
	BLUETOOTH_GENERIC_TELEPHONY_UUID = ((unsigned short)0x1204),	/**<GENERIC TELEPHONY*/
	BLUETOOTH_VIDEO_SOURCE_UUID = ((unsigned short)0x1303), 	/**<VEDIO SOURCE*/
	BLUETOOTH_VIDEO_SINK_UUID = ((unsigned short)0x1304),		/**<VEDIO SINK*/
	BLUETOOTH_VIDEO_DISTRIBUTION_UUID = ((unsigned short)0x1305),	/**<VEDIO DISTRIBUTION*/
	BLUETOOTH_HDP_UUID = ((unsigned short)0x1400),			/**<HDP*/
	BLUETOOTH_HDP_SOURCE_UUID = ((unsigned short)0x1401),		/**<HDP SOURCE*/
	BLUETOOTH_HDP_SINK_UUID = ((unsigned short)0x1402),		/**<HDP SINK*/
	BLUETOOTH_OBEX_SYNCML_TRANSFER_UUID = ((unsigned short)0x0000)	/**<OBEX_SYNC*/ /* Will be removed */
} bluetooth_service_uuid_list_t;

/**
* Service class part of class of device returned from device discovery
*/
typedef enum {
	BLUETOOTH_DEVICE_SERVICE_CLASS_LIMITED_DISCOVERABLE_MODE = 0x002000,
	BLUETOOTH_DEVICE_SERVICE_CLASS_POSITIONING = 0x010000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_NETWORKING = 0x020000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_RENDERING = 0x040000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_CAPTURING = 0x080000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_OBJECT_TRANSFER = 0x100000,		/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_AUDIO = 0x200000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_TELEPHONY = 0x400000,			/**<  */
	BLUETOOTH_DEVICE_SERVICE_CLASS_INFORMATION = 0x800000,			/**<  */
} bluetooth_device_service_class_t;


/**
 * Major device mask (For device discovery)
 */
typedef enum {
	BLUETOOTH_DEVICE_MAJOR_MASK_MISC = 0x00,
	BLUETOOTH_DEVICE_MAJOR_MASK_COMPUTER = 0x0001,
	BLUETOOTH_DEVICE_MAJOR_MASK_PHONE = 0x0002,
	BLUETOOTH_DEVICE_MAJOR_MASK_LAN_ACCESS_POINT = 0x0004,
	BLUETOOTH_DEVICE_MAJOR_MASK_AUDIO = 0x0008,
	BLUETOOTH_DEVICE_MAJOR_MASK_PERIPHERAL = 0x0010,
	BLUETOOTH_DEVICE_MAJOR_MASK_IMAGING = 0x0020,
	BLUETOOTH_DEVICE_MAJOR_MASK_WEARABLE = 0x0040,
	BLUETOOTH_DEVICE_MAJOR_MASK_TOY = 0x0080,
	BLUETOOTH_DEVICE_MAJOR_MASK_HEALTH = 0x0100,
} bluetooth_device_major_mask_t;


/**
 * Major device class (part of Class of Device)
 */
typedef enum {
	BLUETOOTH_DEVICE_MAJOR_CLASS_MISC = 0x00,	/**< Miscellaneous major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_COMPUTER = 0x01,		/**< Computer major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_PHONE = 0x02,		/**< Phone major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_LAN_ACCESS_POINT = 0x03,	/**< LAN major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_AUDIO = 0x04,		/**< AUDIO major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_PERIPHERAL = 0x05,		/**< Peripheral major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_IMAGING = 0x06,		/**< Imaging major device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_WEARABLE = 0x07,		/**< Wearable device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_TOY = 0x08,		/**< Toy device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_HEALTH = 0x09,		/**< Health device class*/
	BLUETOOTH_DEVICE_MAJOR_CLASS_UNCLASSIFIED = 0x1F	/**< Unknown major device class*/
} bluetooth_device_major_class_t;

typedef enum {
	BLUETOOTH_DEVICE_MINOR_CLASS_UNCLASSIFIED = 0x00,	/**< unclassified minor class */

	/* About Computer Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_DESKTOP_WORKSTATION = 0x04,	/**< desktop workstation
									minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_SERVER_CLASS_COMPUTER = 0x08,	/**< server minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_LAPTOP = 0x0C,			/**< laptop minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HANDHELD_PC_OR_PDA = 0x10,		/**< PDA minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PALM_SIZED_PC_OR_PDA = 0x14,	/**< PALM minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_WEARABLE_COMPUTER = 0x18,	/**< Wearable PC minor class */

	/* About Phone Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CELLULAR = 0x04,			/**< Cellular minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CORDLESS = 0x08,			/**< cordless minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_SMART_PHONE = 0x0C,	/**< smart phone minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_WIRED_MODEM_OR_VOICE_GATEWAY = 0x10,
								/**< voice gateway minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_COMMON_ISDN_ACCESS = 0x14,		/**< ISDN minor class */

	/* About LAN/Network Access Point Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_FULLY_AVAILABLE = 0x04,		/**< Fully available minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_1_TO_17_PERCENT_UTILIZED = 0x20,	/**< 1-17% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_17_TO_33_PERCENT_UTILIZED = 0x40,	/**< 17-33% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_33_TO_50_PERCENT_UTILIZED = 0x60,	/**< 33-50% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_50_to_67_PERCENT_UTILIZED = 0x80,	/**< 50-67% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_67_TO_83_PERCENT_UTILIZED = 0xA0,	/**< 67-83% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_83_TO_99_PERCENT_UTILIZED = 0xC0,	/**< 83-99% utilized minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_NO_SERVICE_AVAILABLE = 0xE0,		/**< No service available minor class */

	/* About Audio/Video Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HEADSET_PROFILE = 0x04,		/**< Headset minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HANDSFREE = 0x08,			/**< Handsfree minor class*/

	BLUETOOTH_DEVICE_MINOR_CLASS_MICROPHONE = 0x10,		/**< Microphone minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_LOUD_SPEAKER = 0x14,	/**< Loud Speaker minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HEADPHONES = 0x18,		/**< Headphones minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PORTABLE_AUDIO = 0x1C,	/**< Portable Audio minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CAR_AUDIO = 0x20,		 /**< Car Audio minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_SET_TOP_BOX = 0x24,	/**< Set top box minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HIFI_AUDIO_DEVICE = 0x28,	/**< Hifi minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VCR = 0x2C,		/**< VCR minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VIDEO_CAMERA = 0x30,	/**< Video Camera minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CAM_CORDER = 0x34,		/**< CAM Corder minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VIDEO_MONITOR = 0x38,	/**<Video Monitor minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VIDEO_DISPLAY_AND_LOUD_SPEAKER = 0x3C,
									/**< Video Display and Loud
									Speaker minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VIDEO_CONFERENCING = 0x40,	/**< Video Conferencing minor
								class */

	BLUETOOTH_DEVICE_MINOR_CLASS_GAMING_OR_TOY = 0x48,	/**< Gaming or toy minor class */

	/* About Peripheral Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_KEY_BOARD = 0x40,		/**< Key board minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_POINTING_DEVICE = 0x80,	/**< Pointing Device minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_COMBO_KEYBOARD_OR_POINTING_DEVICE = 0xC0,
								/**< Combo Keyboard or pointing
								device minorclass */

	BLUETOOTH_DEVICE_MINOR_CLASS_JOYSTICK = 0x04,		/**< JoyStick minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_GAME_PAD = 0x08,		/**< Game Pad minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_REMOTE_CONTROL = 0x0C,	/**< Remote Control minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_SENSING_DEVICE = 0x10,	/**< Sensing Device minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_DIGITIZER_TABLET = 0x14,	/**< Digitizer minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CARD_READER = 0x18,	/**< Card Reader minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_DIGITAL_PEN = 0x1C,	/**< Digital pen minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HANDHELD_SCANNER = 0x20,	/**< Handheld scanner for bar-codes, RFID minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HANDHELD_GESTURAL_INPUT_DEVICE = 0x24,	/**< Handheld gestural input device minor class */

	/* About Imaging Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_DISPLAY = 0x10,		/**< Display minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CAMERA = 0x20,		/**< Camera minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_SCANNER = 0x40,		/**< Scanner minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PRINTER = 0x80,		/**< Printer minor class */

	/* About Wearable Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_WRIST_WATCH = 0x04,	/**< Wrist watch minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PAGER = 0x08,		/**< Pager minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_JACKET = 0x0C,		/**< Jacket minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HELMET = 0x10,		/**< Helmet minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_GLASSES = 0x14,		/**< Glasses minor class */

	/* About Toy Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_ROBOT = 0x04,		/**< Robot minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_VEHICLE = 0x08,		/**< Vehicle minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_DOLL_OR_ACTION = 0x0C,	/**< Doll or Action minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_CONTROLLER = 0x10,		/**< Controller minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_GAME = 0x14,		/**< Game minor class */

	/* About Health Major class */
	BLUETOOTH_DEVICE_MINOR_CLASS_BLOOD_PRESSURE_MONITOR = 0x04,	/**< Blood Pressure minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_THERMOMETER = 0x08,		/**< Thermometer minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_WEIGHING_SCALE = 0x0C,		/**< Weighing Scale minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_GLUCOSE_METER = 0x10,		/**< Glucose minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PULSE_OXIMETER = 0x14,		/**< Pulse Oximeter minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_HEART_OR_PULSE_RATE_MONITOR = 0x18,/**< Heart or pulse rate monitor minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_MEDICAL_DATA_DISPLAY = 0x1C,	/**< Medical minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_STEP_COUNTER = 0x20,		/**< Step Counter minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_BODY_COMPOSITION_ANALYZER = 0x24,	/**< Body composition analyzer minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_PEAK_FLOW_MONITOR = 0x28,	/**< Peak flow monitor minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_MEDICATION_MONITOR = 0x2C,	/**< Medication monitor minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_KNEE_PROSTHESIS = 0x30,	/**< Knee prosthesis minor class */
	BLUETOOTH_DEVICE_MINOR_CLASS_ANKLE_PROSTHESIS = 0x34,	/**< Ankle prosthesis minor class */
} bluetooth_device_minor_class_t;

/**
 * structure to hold the device information
 */
typedef struct {
	bluetooth_device_major_class_t major_class; /**< major device class */
	bluetooth_device_minor_class_t minor_class; /**< minor device class */
	bluetooth_device_service_class_t service_class;
						    /**< service device class */
} bluetooth_device_class_t;

/**
 * Discovery Role types
 */
typedef enum {
	DISCOVERY_ROLE_BREDR = 0x1,
	DISCOVERY_ROLE_LE,
	DISCOVERY_ROLE_LE_BREDR
} bt_discovery_role_type_t;

/**
* structure to hold the device information
*/
typedef struct {
	bluetooth_device_address_t device_address;	/**< device address */
	bluetooth_device_name_t device_name;	/**< device name */
	bluetooth_device_class_t device_class;	/**< device class */
	char uuids[BLUETOOTH_MAX_SERVICES_FOR_DEVICE][BLUETOOTH_UUID_STRING_MAX];
	unsigned int service_list_array[BLUETOOTH_MAX_SERVICES_FOR_DEVICE]; /**< Use enum values in bt_service_uuid_list_t */
	int service_index;
	int rssi;			/**< received strength signal*/
	gboolean paired;		/**< paired flag */
	gboolean connected;	/**< connected flag */
	gboolean trust;		/**< connected flag */
	unsigned char device_type;
} bluetooth_device_info_t;

/**
 * structure to hold the paired device information
 */
typedef struct {
	bluetooth_device_address_t device_address;  /**< paired device address */
	bluetooth_device_name_t device_name;	    /**< device name */
	bluetooth_device_class_t device_class;	    /**< device class */
} bluetooth_paired_device_info_t;

/**
* structure to hold the paired device information
*/
typedef struct {
	bluetooth_device_address_t device_address;
					       /**< device address */
	char interface_name[BLUETOOTH_INTERFACE_NAME_LENGTH + 1];
							  /**< network interface name */
} bluetooth_network_device_info_t;

/**
* Stucture to hold discovery option
*/
typedef struct {
	unsigned short max_response;	/**< the number of maximum response */
	unsigned short discovery_duration;
					/**< duration of discovery (seconds) */
	unsigned int classOfDeviceMask;	/**<  mask for values of class of device. to be used with
					classOfDevice variable */
} bluetooth_discovery_option_t;

/**
 * Stucture to hold event information
 */
typedef struct {
	int event;	/**< event type */
	int result;	/**< Success or error value */
	void *param_data;
			/**<parameter data pointer */
	void *user_data;
} bluetooth_event_param_t;

typedef struct {
	bluetooth_device_address_t device_addr;
	char uuids[BLUETOOTH_MAX_SERVICES_FOR_DEVICE][BLUETOOTH_UUID_STRING_MAX];
	unsigned int service_list_array[BLUETOOTH_MAX_SERVICES_FOR_DEVICE]; /**< Use enum values in bt_service_uuid_list_t */
	unsigned int service_name_array[BLUETOOTH_MAX_SERVICES_FOR_DEVICE];
	int service_index;
} bt_sdp_info_t;

/**
 * Stucture to rfcomm receive data
 */

typedef struct {
	int socket_fd;
		/**< the socket fd */
	int buffer_size;/**< the length of the receive buffer */
	char *buffer;
		/**< the receive data buffer */
} bluetooth_rfcomm_received_data_t;

/**
* Stucture to rfcomm connection
*/

typedef struct {
	int socket_fd;
		/**< the socket fd */
	int device_role;/** < Device role - RFCOMM_ROLE_SERVER or RFCOMM_ROLE_CLIENT */
	bluetooth_device_address_t device_addr;
					      /**< device address */
	char uuid[BLUETOOTH_UUID_STRING_MAX];
} bluetooth_rfcomm_connection_t;

/**
 * Stucture to rfcomm disconnection
 */
typedef struct {
	int socket_fd;
		/**< the socket fd */
	int device_role;/** < Device role - RFCOMM_ROLE_SERVER or RFCOMM_ROLE_CLIENT */
	bluetooth_device_address_t device_addr;
					      /**< device address */
	char uuid[BLUETOOTH_UUID_STRING_MAX];
} bluetooth_rfcomm_disconnection_t;

typedef struct {
	int socket_fd;
		/**< the socket fd */
	bluetooth_device_address_t device_addr;
					      /**< device address */
} bluetooth_rfcomm_connection_request_t;

/**
 * HDP QOS types
 */
typedef enum {
	HDP_QOS_RELIABLE,
	HDP_QOS_STREAMING,
	HDP_QOS_ANY
}bt_hdp_qos_type_t;

/**
 * HDP Role types
 */
typedef enum {
	HDP_ROLE_SOURCE = 0x0,
	HDP_ROLE_SINK
}bt_hdp_role_type_t;


/**
 * Stucture to HDP connected
 */
typedef struct {
	const char *app_handle;	/**< the application handle */
	unsigned int channel_id;	/**< the channel id */
	bt_hdp_qos_type_t type;	/**< the QOS type */
	bluetooth_device_address_t device_address; /**< the remote address */
} bt_hdp_connected_t;

/**
 * Stucture to HDP disconnected
 */
typedef struct {
	unsigned int channel_id; /**< the channel id */
	bluetooth_device_address_t device_address; /**< the remote address */
} bt_hdp_disconnected_t;

/**
 * Stucture to HDP data indication
 */
typedef struct {
	unsigned int channel_id;	 /**< the channel id */
	const char *buffer;	 /**< the RX data buffer */
	unsigned int size;	 /**< the RX data size */
} bt_hdp_data_ind_t;

/**
 * Stucture to OPP client transfer information
 */
typedef struct {
	char *filename;
	unsigned long size;
	int percentage;
}bt_opc_transfer_info_t;

/* Obex Server transfer type */
#define TRANSFER_PUT "PUT"
#define TRANSFER_GET "GET"
/**
 * Stucture to OPP/FTP Server authorize information
 */
typedef struct {
	char *filename;
	int length;
} bt_obex_server_authorize_into_t;

/**
 * Server type
 */
typedef enum {
	OPP_SERVER = 0x0,
	FTP_SERVER
} bt_obex_server_type_t;

/**
 * Stucture to OPP/FTP server transfer information
 */
typedef struct {
	char *filename;
	char *device_name;
	char *file_path;
	char *type;
	int transfer_id;
	unsigned long file_size;
	int percentage;
	bt_obex_server_type_t server_type;
} bt_obex_server_transfer_info_t;

/**
 * Stucture to OOB data
 */

typedef struct {
	unsigned char hash[BLUETOOTH_OOB_DATA_LENGTH];
	unsigned char randomizer[BLUETOOTH_OOB_DATA_LENGTH];
	unsigned int hash_len;
	unsigned int randomizer_len;
} bt_oob_data_t;

/**
 * Structure to GATT attribute handle data
 */

typedef struct {
	int count;
	char **handle;
} bt_gatt_handle_info_t;

/**
 * Structure to GATT Remote service data
 */

typedef struct {
	char *uuid;
	char *handle;
	bt_gatt_handle_info_t handle_info;
} bt_gatt_service_property_t;

/**
 * Structure to GATT Remote characteristic data
 */

typedef struct {
	char *service_handle;
	bt_gatt_handle_info_t handle_info;
} bt_gatt_discovered_char_t;

/**
 * Structure to format of GATT Characteristic Value
 */

typedef struct {
	unsigned char format;
	unsigned char exponent;
	unsigned short unit;
	unsigned char name_space;
	unsigned short description;
} bt_gatt_char_format_t;

/**
 * Structure to GATT Characteristic property
 */

typedef struct {
	char *handle;
	char *uuid;
	char *name;
	char *description;
	bt_gatt_char_format_t format;
	unsigned char *val;
	unsigned int val_len;
	char *representation;
} bt_gatt_char_property_t;

/**
 * Structure to GATT Characteristic value
 */

typedef struct {
	char *char_handle;
	guint8 *char_value;
	guint32 val_len;
} bt_gatt_char_value_t;

/**
 * Callback pointer type
 */
typedef void (*bluetooth_cb_func_ptr) (int, bluetooth_event_param_t *, void *);

/**
 * @fn int bluetooth_register_callback(bluetooth_cb_func_ptr callback_ptr, void *user_data)
 * @brief Set the callback function pointer for bluetooth event
 *
 *
 * This API will register the callback function, when any response and event are received from
 * bluetooth framework. @n
 * this registered callback function will be get called with appropriate event and data structures.
 * This function is a synchronous call. An application developer can call
 * bluetooth_register_callback() function to register a callback function of bluetooth_cb_func_ptr
 * type. This registered function will receive events of bluetooth_event_type_t type along with
 * data any.
 *
 *
 * @param[in]   callback_ptr    A pointer to the callback function
 * @param[in]   user_data    A pointer to user data
 * @return      BLUETOOTH_ERROR_NONE - Success
 * @remark      None
 * @see         None
@code
void bt_event_callback(int event, bluetooth_event_param_t *param, void *user_data)
{
	GMainLoop *main_loop = (GMainLoop*) user_data;

	switch(event)
	{
		// Code for each event
		default:
			g_main_loop_quit(main_loop);
			break;
	}
}

int main()
{
	GMainLoop *main_loop = NULL;
	int ret = 0;
	g_type_init();
	main_loop = g_main_loop_new(NULL, FALSE);
	ret = bluetooth_register_callback(bt_event_callback, (void*)main_loop);
	if (ret >= BLUETOOTH_ERROR_NONE)
	{
		// bluetooth_register_callback returned Success
	}
	else
	{
		// bluetooth_register_callback returned failiure
	}
	g_main_loop_run(main_loop);
}
@endcode
 */
int bluetooth_register_callback(bluetooth_cb_func_ptr callback_ptr, void *user_data);

/**
 * @fn int bluetooth_deregister_callback(bluetooth_cb_func_ptr callback_ptr)
 * @brief Set the callback function pointer for bluetooth event
 *
 *
 * This API will register the callback function, when any response and event are received from
 * bluetooth framework. @n
 * this registered callback function will be get called with appropriate event and data structures.
 * This function is a synchronous call. An application developer can call
 * bluetooth_register_callback() function to register a callback function of bluetooth_cb_func_ptr
 * type. This registered function will receive events of bluetooth_event_type_t type along with
 * data any.
 *
 *
 * @param[in]   none
 * @return      BLUETOOTH_ERROR_NONE - Success
 * @remark      None
 * @see         None
@code
void bt_event_callback(int event, bluetooth_event_param_t *param, void *user_data)
{
	GMainLoop *main_loop = (GMainLoop*) user_data;

	switch(event)
	{
		// Code for each event
		default:
			g_main_loop_quit(main_loop);
			break;
	}
}

int main()
{
	GMainLoop *main_loop = NULL;
	int ret = 0;
	g_type_init();
	main_loop = g_main_loop_new(NULL, FALSE);
	ret = bluetooth_register_callback(bt_event_callback, (void*)main_loop);
	if (ret >= BLUETOOTH_ERROR_NONE)
	{
		// bluetooth_register_callback returned Success
	}
	else
	{
		// bluetooth_register_callback returned failiure
	}
	ret = bluetooth_deregister_callback(void);
	g_main_loop_run(main_loop);
}
@endcode
 */
int bluetooth_unregister_callback(void);

/**
 * @fn int bluetooth_enable_adapter(void)
 * @brief Enable the Bluetooth H/W
 *
 *
 * This API can be used to activate Bluetooth. It initializes Bluetooth protocol stack for use and
 * send request to bluetooth chip for activation.
 * This function is typically called at startup or when Bluetooth services are required.  This
 * function must be called before calling any other API of Bluetooth operations.
 *
 * Before performing any operations like Device discover, service search etc.., the adapter must be
 * enabled.
 *
 * This function is a asynchronous call.
 * If the call is success then the application will receive BLUETOOTH_EVENT_ENABLED event
 * through registered callback function with appropriate result code
 *			BLUETOOTH_CHANGE_STATUS_TIMEOUT - Timeout has happen \n
 *			BLUETOOTH_ERROR_NONE - Success \n
 *
 * If the adpter is not enabled with in 30 seconds then BLUETOOTH_EVENT_ENABLED with result code
 * BLUETOOTH_CHANGE_STATUS_TIMEOUT will come
 *
 * @return      BLUETOOTH_ERROR_NONE - Success\n
 *		BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED - Adapter already enabled\n
 *		BLUETOOTH_ERROR_IN_PROGRESS - Adapter is activating or deactivating\n
 * @exception   BLUETOOTH_ERROR_INTERNAL - Dbus proxy call is fail
 * @remark      None
 * @see         bluetooth_check_adapter, bluetooth_disable_adapter
@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_ENABLED:
			if (param->result == BLUETOOTH_ERROR_NONE)
			{
				// Successfully Enabled
			}
			else
			{
				// Failed
			}
			break;
	}
}

...

int ret = 0;
ret = bluetooth_enable_adapter();

@endcode
 */
int bluetooth_enable_adapter(void);

/**
 * @fn int bluetooth_disable_adapter(void)
 * @brief Disable the Bluetooth H/W
 *
 *
 * This function disables Bluetooth protocol stack and hardware. This function is called when
 * Bluetooth is no longer used. It will internally free all resources and power off the RF radio.
 *
 * Bluetooth adapter should be disabled to switch off Bluetooth chip (and thereby saving power).
 * bluetooth_disable_adapter() API will do that job for you. After switching off Bluetooth,
 * BLUETOOTH_EVENT_DISABLED will be sent by SDK to application for confirmation with appropriate
 * error code.
 * The various error codes are BLUETOOTH_ERROR_NONE for success and BLUETOOTH_ERROR_INTERNAL for
 * internal error.
 *
 * This function is a asynchronous call.
 * If this call is success then the applications will receive BLUETOOTH_EVENT_DISABLED event
 * through registered callback function.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success\n
 *		BLUETOOTH_ERROR_IN_PROGRESS - Adapter is activating or deactivating\n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth adapter is not enabled\n
 * @exception   BLUETOOTH_ERROR_INTERNAL - Dbus proxy call is fail
 * @remark      None
 * @see         bluetooth_check_adapter, bluetooth_enable_adapter
@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_DISABLED:
			if (param->result == BLUETOOTH_ERROR_NONE)
			{
				// Successfully disabled
			}
			else
			{
				// Failed
			}
			break;
	}
}

...

int ret = 0;
ret = bluetooth_disable_adapter();
@endcode
 */
int bluetooth_disable_adapter(void);

/**
 * @fn int bluetooth_reset_adapter(void)
 * @brief Reset the Bluetooth H/W and values
 *
 *
 * This function resets Bluetooth protocol stack and hardware. This function is called when
 * an user want to initialize Bluetooth environment.
 *
 * The various error codes are BLUETOOTH_ERROR_NONE for success and BLUETOOTH_ERROR_INTERNAL for
 * internal error.
 *
 * This function is a synchronous call.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success\n
 * @exception   BLUETOOTH_ERROR_INTERNAL - Dbus proxy call is fail
 * @remark      None
 * @see         bluetooth_check_adapter, bluetooth_enable_adapter
@code
...

int ret = 0;
ret = bluetooth_reset_adapter();
@endcode
 */
int bluetooth_reset_adapter(void);

/**
 * @fn int bluetooth_is_supported(void)
 * @brief Check if the bluetooth is supported or not by the current target
 *
 * This API checks whether the bluetooth is supported or not.
 * This API only run by root permission.
 *
 * This function is a synchronous call.
 *
 * @return	0 - if bluetooth is not supported\n
 *		1 - if bluetooth is supported\n
 *		BLUETOOTH_ERROR_INTERNAL - Error in API internal
 * @remark      None
@code

int ret = 0;
ret = bluetooth_is_supported();
@endcode
 */
int bluetooth_is_supported(void);


/**
 * @fn int bluetooth_check_adapter(void)
 * @brief Check the current status of the Bluetooth adapter
 *
 *
 * This API checks whether the Bluetooth adapter is enabled or not. Before performing any operations
 * the bluetooth adapter should be enabled. This API helps to find out the current state of the
 * bluetooth adapter.
 * This API get the adapter internal data structure and check current adapter status.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ADAPTER_DISABLED - if bluetooth adapter is disabled\n
 *		BLUETOOTH_ADAPTER_ENABLED - if bluetooth adapter is enabled\n
 * @remark      None
 * @see         bluetooth_enable_adapter, bluetooth_disable_adapter
@code

int ret = 0;
ret = bluetooth_check_adapter();
@endcode
 */
int bluetooth_check_adapter(void);

/**
 * @fn int bluetooth_get_local_address(bluetooth_device_address_t *local_address)
 * @brief Get the local adapter bluetooth address
 *
 *
 * This API is used, get the device address of the local bluetooth adapter. Before calling this API,
 * the adapter should be enabled.
 * In its output parameter, you can receive bluetooth_device_address_t type of pointer which will
 * contain Bluetooth address.
 * Since its inconvenient for user to remember the address, Bluetooth provides a method to have a
 * friendly name for each device.
 * There is no event callback for this API.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Succeess \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -  Invalid parameter (NULL buffer) \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED -  Adapter is disabled \n
 * @param[out]  local_address   a device address of local bluetooth adapter
 * @remark      None
 * @see         None
@code

bluetooth_device_address_t local_address={0,};
int ret = 0;

ret = bluetooth_get_local_address(&local_address);
@endcode
 */
int bluetooth_get_local_address(bluetooth_device_address_t *local_address);

/**
 * @fn int bluetooth_get_local_name(bluetooth_device_name_t* local_name)
 * @brief Get the local device name
 *
 *
 * This function is used, get the local device name. Since its difficult to remember the Adapter
 * address, the friendly name can be assigned to the adapter and we can get it using this API. This
 * friendly name is retrived by the remote device and displaying.
 * Before calling this API, the adapter should be enabled. There is no event callback for this API.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter (NULL buffer)\n
 * @param[out]  local_name      a local device name
 * @remark      None
 * @see         None
@code
bluetooth_device_name_t local_name={0,}
int ret = 0;
ret = bluetooth_get_local_name (&local_name);
@endcode
 */
int bluetooth_get_local_name(bluetooth_device_name_t *local_name);

/**
 * @fn int bluetooth_set_local_name(const bluetooth_device_name_t *local_name)
 * @brief Set the local device name
 *
 *
 * This function is used to set the local device name. This is a human friendly name whose
 * length can be BLUETOOTH_DEVICE_NAME_LENGTH_MAX maximum
 *
 * This function is a synchronous call.
 *
 * @param[in]   local_name   bluetooth device name to set local device
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Bluetooth name parameter is incorrect \n
 *		BLUETOOTH_ERROR_INVALID_DATA - Device address provided is incorrect \n
 *		BLUETOOTH_ERROR_NO_RESOURCES - Pre-allocated memory error \n
 *		BLUETOOTH_ERROR_INTERNAL - The dbus method call is fail \n
 *
 * @remark      None

@code
bluetooth_device_name_t local_name={0,}
int ret = 0;
ret = bluetooth_set_local_name (&local_name);
@endcode
 */
int bluetooth_set_local_name(const bluetooth_device_name_t *local_name);


/**
 * @fn int bluetooth_is_service_used(const char *service_uuid, gboolean *used)
 * @brief Check if the uuid is used or not
 *
 * This function is used to check if the uuid is used or not.
 *
 * This function is a synchronous call.
 *
 * @param[in]   service_uuid   service uuid (UUID 128 bit as string)
 * @param[out] used  if the uuid is used or not
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Bluetooth name parameter is incorrect \n
 *		BLUETOOTH_ERROR_INTERNAL - The dbus method call is fail \n
 *
 * @remark      None
 *
@code
gboolean used = FALSE;
const char *uuid ="00001101-0000-1000-8000-00805F9B34FB";
ret = bluetooth_is_service_used(uuid, &used);
@endcode
 */
int bluetooth_is_service_used(const char *service_uuid, gboolean *used);

/**
 * @fn int bluetooth_is_device_connected(const bluetooth_device_address_t *device_address, bluetooth_service_type_t type, gboolean *is_connected)
 * @brief Check if the device is connected to the specific service
 *
 * This function is used to check if if the device is connected to the specific service.
 *
 * This function is a synchronous call.
 *
 * @param[in]   local_address   a device address of remote bluetooth device
 * @param[in]   type the service type
 * @param[out] is_connected  if the device is connected or not
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Bluetooth name parameter is incorrect \n
 *		BLUETOOTH_ERROR_INTERNAL - The dbus method call is fail \n
 *
 * @remark      None
 *
@code
gboolean is_connected = FALSE;
bluetooth_device_address_t device_address={{0x00,0x0D,0xFD,0x24,0x5E,0xFF}};
ret = bluetooth_is_device_connected(&device_address, BLUETOOTH_HSP_SERVICE, &used);
@endcode
 */
int bluetooth_is_device_connected(const bluetooth_device_address_t *device_address,
				bluetooth_service_type_t type,
				gboolean *is_connected);

/**
 * @fn int bluetooth_get_discoverable_mode(bluetooth_discoverable_mode_t *discoverable_mode_ptr)
 * @brief Get the visibility mode
 *
 *
 * This function is used to get the discoverable mode (Visibility option). Depending upon the
 * visibity mode, the property of the device is determined whether it can be discoverable, non
 * discoverable, connectable etc. Before calling this API make sure that the adapter is enabled.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_DATA - Invalid data \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal IPC error \n
 * @param[out]  discoverable_mode   current bluetooth discoverable mode
 * @remark      None
 * @see		bluetooth_set_discoverable_mode
 @code
 bluetooth_discoverable_mode_t discoverable_mode_ptr;
 int ret = 0;
 ret = bluetooth_get_discoverable_mode (&discoverable_mode_ptr);
 @endcode
 */
int bluetooth_get_discoverable_mode(bluetooth_discoverable_mode_t *discoverable_mode_ptr);

/**
 * @fn int bluetooth_set_discoverable_mode(bluetooth_discoverable_mode_t discoverable_mode,
 *						int timeout)
 * @brief Set the visibility mode
 *
 *
 * This function is used to set the discoverable mode (Visibility option).
 *
 * Many times user may want to keep his device discoverable so that when peer device is performing
 * device search, he/she can find user's device. Application programmer can keep the mode as
 * BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE or
 * BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE for the same purpose. However, all these
 * modes cause bluetooth adapter to consume more battery. Hence developer should generally
 * keep discoverable mode as BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE.
 *
 * This function is a synchronous call.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_NOT_SUPPORT - Requested mode is not supported \n
 *
 * @param[in]  discoverable_mode   the bluetooth discoverable mode to set
 * @param[in]  timeout   discoverable time in only limited discoverable mode (second), default: 0
 * @remark      None
 * @see         bluetooth_get_discoverable_mode

@code

bluetooth_discoverable_mode_t mode;
int ret = 0;
mode= BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE;
ret = bluetooth_set_discoverable_mode (mode, 180);

@endcode
 */
int bluetooth_set_discoverable_mode(bluetooth_discoverable_mode_t discoverable_mode,
					    int timeout);


/**
 * @fn int bluetooth_get_timeout_value(int *timeout)
 * @brief Get the visibility timeout value
 *
 *
 * This function is used to get the visibility timeout
 * Before calling this API make sure that the adapter is enabled.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_DATA - Invalid data \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal IPC error \n
 * @param[out]  timeout   remain visibility timeout value
 * @remark      None
 * @see		bluetooth_set_discoverable_mode
 @code
 int timeout;
 int ret = 0;
 ret = bluetooth_get_timeout_value (&timeout);
 @endcode
 */
int bluetooth_get_timeout_value(int *timeout);


/**
 * @fn int bluetooth_start_discovery(unsigned short max_response, unsigned short discovery_duration,
 *					unsigned int  classOfDeviceMask)
 * @brief Start the generic device discovery which finds both the BR/EDR and LE devices.
 *
 * To connect connect to peer bluetooth device, you will need to know its bluetooth address and its
 * name. You can search for Bluetooth devices in vicinity by bluetooth_start_discovery() API. It
 * first performs an inquiry and try to find both the BREDR and LE devices. For each device found
 * from the inquiry it gets the remote name of the device. Bluetooth device address and name are
 * given to Application via BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND event. In param_data of
 * bluetooth_event_param_t, you will receive a pointer to a structure of bluetooth_device_info_t type.
 * You will receive device address, device name, device class, rssi (received signal strength indicator).
 * please see bluetooth_device_info_t for more details.
 *
 *
 * This API provides searching options like max responses, discovery duration in seconds and class
 * of device mask to filter device search. some times there may be too many bluetooth devices in
 * vicinity of your device.in such scenario, application can request to reduce number of responces
 * (BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND event) with help of max_response parameter. However if you
 * pass zero, bluetooth adapter will not restrict number of responses. you can also specify duration
 * of the seach in discovery_duration. bluetooth adapter will automatically stop device search after
 * application defined time. please note that discovery_duration should be mentioned in seconds.
 * Also note that search will end after 180 seconds automatically if you pass 0 in discovery
 * duration.
 *
 * Sometimes user may want to search for a particular kind of device. for ex, mobile or pc. in such
 * case, you can use classOfDeviceMask parameter. please see bluetooth_device_service_class_t,
 * bluetooth_device_major_class_t and bluetooth_device_minor_class_t enums
 *
 * This function is a asynchronous call.
 * If the call is success then the application will receive BLUETOOTH_EVENT_DISCOVERY_STARTED event
 * through registered callback function.
 *
 * The discovery is responded by an BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND event for each device it
 * found and a BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED event for its name updation.
 *
 * The completion or cancellation of the discovery is indicated by an
 * BLUETOOTH_EVENT_DISCOVERY_FINISHED event.
 *
 * The device discovery can be cancelled by calling bluetooth_stop_discovery().
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth adapter is not enabled \n
 *		BLUETOOTH_ERROR_DEVICE_BUSY - Bluetooth adapter is busy doing some operation \n
 *		BLUETOOTH_ERROR_INTERNAL - System error like heap full has occured or bluetooth
						agent is not running \n
 *
 * @param[in] max_response		define the maximum response of the number of founded devices
					(0 means unlimited)
 * @param[in] discovery_duration	define bluetooth discovery duration (0 means 180s )
 * @param[in] classOfDeviceMask		define classes of the device mask which user wants
					(refer to class of device)
 * @remark      None
 * @see         bluetooth_start_custom_discovery(), bluetooth_cancel_discovery, bluetooth_device_info_t

@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND:
		{
			bluetooth_device_info_t *device_info = NULL;
			printf("BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND, result [0x%04x]",
					param->result);
			device_info  = (bluetooth_device_info_t *)param->param_data;
			memcpy(&searched_device, &device_info->device_address,
						sizeof(bluetooth_device_address_t));
			printf("dev [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]",
				device_info->device_address.addr[0],
				device_info->device_address.addr[1],
				device_info->device_address.addr[2],
				device_info->device_address.addr[3],
				device_info->device_address.addr[4],
				device_info->device_address.addr[5]);
			break;
		}
		case BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED:
		{
			bluetooth_device_info_t *device_info = NULL;
			printf("BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED, result [0x%04x]",
										param->result);
			device_info  = (bluetooth_device_info_t *)param->param_data;
			memcpy(&searched_device, &device_info->device_address,
								sizeof(bluetooth_device_address_t));
			printf("dev [%s] [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]",
							device_info->device_name.name,
							device_info->device_address.addr[0],
							device_info->device_address.addr[1],
							device_info->device_address.addr[2],
							device_info->device_address.addr[3],
							device_info->device_address.addr[4],
							device_info->device_address.addr[5]);
			break;
		}

		case BLUETOOTH_EVENT_DISCOVERY_FINISHED:
			printf("BLUETOOTH_EVENT_DISCOVERY_FINISHED, result[0x%04x]", param->result);
			break;
	}
}

unsigned short max_response;
unsigned short discovery_duration;
unsigned classOfDeviceMask;
int ret = 0;

max_response =0;
discovery_duration =0;
classOfDeviceMask =0;

ret = bluetooth_start_discovery(max_response,discovery_duration,classOfDeviceMask);

@endcode
 *
 */

int bluetooth_start_discovery(unsigned short max_response,
				      unsigned short discovery_duration,
				      unsigned int classOfDeviceMask);

/**
 * @fn int bluetooth_start_custom_discovery(bt_discovery_role_type_t role
 *					unsigned short max_response,
 *					unsigned short discovery_duration,
 *					unsigned int  classOfDeviceMask)
 * @brief Start the custom device discovery with specific type such as BR/EDR, LE, LE+BR/EDR etc.
 *
 * Sometimes user may want to search for a particular kind of device. for ex, LE only or BR/EDR only.
 * In such case, you can use type parameter. This API is similar to that of bluetooth_start_discovery().
 * Please see bluetooth_start_discovery() for other parameters description.
 *
 * This function is a asynchronous call.
 * If the call is success then the application will receive BLUETOOTH_EVENT_DISCOVERY_STARTED event
 * through registered callback function.
 *
 * The discovery is responded by an BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND event for each device it
 * found and a BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED event for its name updation.
 *
 * The completion or cancellation of the discovery is indicated by an
 * BLUETOOTH_EVENT_DISCOVERY_FINISHED event.
 *
 * The device discovery can be cancelled by calling bluetooth_stop_discovery().
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth adapter is not enabled \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *		BLUETOOTH_ERROR_IN_PROGRESS - Bluetooth adapter is busy with another discovery \n
 *		BLUETOOTH_ERROR_INTERNAL - System error like heap full has occured or bluetooth
						agent is not running \n
 *
 * @param[in] role		define the role type of devices to be discovered. See enum bt_discovery_role_type_t.
					(DISCOVERY_ROLE_BREDR means BREDR only, DISCOVERY_ROLE_LE mean Low Energy only,
					DISCOVERY_ROLE_LE_BREDR means LE & BREDR - same as bluetooth_start_discovery())
 * @param[in] max_response		define the maximum response of the number of founded devices
					(0 means unlimited)
 * @param[in] discovery_duration	define bluetooth discovery duration (0 means 180s )
 * @param[in] classOfDeviceMask		define classes of the device mask which user wants
					(refer to class of device)
 * @remark      None
 * @see         bluetooth_start_discovery(), bluetooth_cancel_discovery, bluetooth_device_info_t

@code
unsigned short type;
unsigned short max_response;
unsigned short discovery_duration;
unsigned classOfDeviceMask;
int ret = 0;

type = 1;
max_response =0;
discovery_duration =0;
classOfDeviceMask =0;

ret = bluetooth_start_custom_discovery(type, max_response, discovery_duration, classOfDeviceMask);

@endcode
 *
 */
int bluetooth_start_custom_discovery(bt_discovery_role_type_t role,
						unsigned short max_response,
						unsigned short discovery_duration,
						unsigned int classOfDeviceMask);

/**
 * @fn int bluetooth_cancel_discovery (void)
 * @brief Cancel the on-going device discovery operation
 *
 *
 * This function stops the on-going device discovery operation. This API has to be called after the
 * bluetooth_start_discovery API and before the BLUETOOTH_EVENT_DISCOVERY_FINISHED event comes of
 * the bluetooth_start_discovery API
 *
 * Normally the device discovery takes a more time (~10.24 seconds) to get all the devices in its
 * vicinity and it recevies as BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND event. This API helps us to
 * cancel the discover request once the user received the device to which he wish to connect.
 *
 * This function is a asynchronous call.
 * If the call is success to cancel discovey then the application will receive
 * BLUETOOTH_EVENT_DISCOVERY_FINISHED event through registered callback function
 * with an error code BLUETOOTH_ERROR_CANCEL. In the case of failure the error code will be
 * BLUETOOTH_ERROR_NONE
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_NOT_IN_OPERATION - No Discovery operation in progess to cancel \n
 *		BLUETOOTH_ERROR_ACCESS_DENIED - Currently in discovery but it is requested from
						other application \n
 *		BLUETOOTH_ERROR_INTERNAL - Internel IPC error \n
 * @remark      None
 * @see		bluetooth_start_discovery
@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_DISCOVERY_FINISHED:
			TC_PRT("BLUETOOTH_EVENT_DISCOVERY_FINISHED, result[0x%04x]", param->result);
			break;
	}
}

..

int ret = 0;

ret = bluetooth_cancel_discovery();
@endcode
 */
int bluetooth_cancel_discovery(void);

/**
 * @fn int bluetooth_is_discovering(void)
 * @brief Check for the device discovery is in-progress or not.
 *
 * This API is used to check the current status of the Discovery operation.If discovery is in\
 * progress normally other operations are not allowed.
 * If a device discovery is in progress, we have to either cancel the discovery operation or wait
 * for the BLUETOOTH_EVENT_DISCOVERY_FINISHED
 * event before performing other operations. This API is used to get for the current discovery
 * operation status and using bluetooth_cancel_discovery()
 * we can cancell the ongoing discovery process.
 * Before calling this API, make sure that the adapter is enabled. There is no callback event for
 * this API.
 *
 * This function checks whether the device discovery is started or not.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Discovery is not in progress \n
 *		BLUETOOTH_ERROR_NONE+1 - Discovery in progress \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *
 * @remark      None
 * @see         bluetooth_start_discovery, bluetooth_cancel_discovery

@code
int ret = 0;
ret = bluetooth_is_discovering ();
@endcode
 */
int bluetooth_is_discovering(void);

/**
 * @fn int bluetooth_bond_device(const bluetooth_device_address_t *device_address)
 * @brief Initiate a bonding process
 *
 *
 * This function initiates a bonding procedure with a peer device.  The bonding procedure
 * enables authentication and optionally encryption on the Bluetooth link.
 *
 * Bonding is applied to the discovered device to which we need a secure connection. We cannot
 * inititate the bonding request to the devices already in the paired list.
 *
 * Usually we call this API after the device discovery.
 * This function is a asynchronous call.
 *
 * Response will be received through BLUETOOTH_EVENT_BONDING_FINISHED event. It can any of the below
 * mentioed result code
 * BLUETOOTH_ERROR_PARING_FAILED - Pairing faied \n
 * BLUETOOTH_ERROR_ACCESS_DENIED - Authetication rejected \n
 * BLUETOOTH_ERROR_CANCEL_BY_USER - Cancelled by the user \n
 * BLUETOOTH_ERROR_PARING_FAILED - Pairing failed \n
 * BLUETOOTH_ERROR_TIMEOUT - Timeout has haapened \n
 *
 * If the remote user is not responding with in a specific time(60 seconds), then a timeout happens
 * and BLUETOOTH_EVENT_BONDING_FINISHED callback event is called with and BLUETOOTH_ERROR_TIMEOUT
 * result code
 *
 * The bonding operation can be cancelled by calling bluetooth_cancel_bonding().
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 *		BLUETOOTH_ERROR_DEVICE_BUSY - Adapter is busy or Discovery is in Progress \n
 *		BLUETOOTH_ERROR_INVALID_DATA - Invalid BD address \n
 * @exception   None
 * @param[in]   device_address   This indicates an address of the device with which the pairing
 *					should be initiated
 * @remark      None
 * @see		bluetooth_cancel_bonding
 @code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_BONDING_FINISHED:
		{
			TC_PRT("BLUETOOTH_EVENT_BONDING_FINISHED, result [0x%04x]", param->result);
			if (param->result >= BLUETOOTH_ERROR_NONE)
			{
				bluetooth_device_info_t *device_info = NULL;
				device_info  = (bluetooth_device_info_t *)param->param_data;
				printf("dev [%s] [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]
							mjr[%#x] min[%#x] srv[%#x]",
							device_info->device_name.name,
							device_info->device_address.addr[0],
							device_info->device_address.addr[1],
							device_info->device_address.addr[2],
							device_info->device_address.addr[3],
							device_info->device_address.addr[4],
							device_info->device_address.addr[5],
							device_info->device_class.major_class,
							device_info->device_class.minor_class,
							device_info->device_class.service_class);
			}
			else
			{
				//bonding failed
			}
			break;
		}
	}
}

...

int ret = 0;
bluetooth_device_address_t device_address={{0}};

ret = bluetooth_bond_device(&device_address);

@endcode
 */
int bluetooth_bond_device(const bluetooth_device_address_t *device_address);

/**
 * @fn int bluetooth_cancel_bonding(void)
 * @brief Cancel the on-going bonding process
 *
 * This API is called to cancel the on-going bonding procedure. It should be called before the
 * BLUETOOTH_EVENT_BONDING_FINISHED event comes.
 * This API is useful when the remote device is not responding to the bond request or we wish to
 * cancel the bonding request. In this case we need not wait for the timeout to happen.
 *
 * This function is a synchronous call.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_NOT_IN_OPERATION - No bonding request in progress \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is not enabled \n
 * @exception   None
 * @remark      None
 * @see		bluetooth_bond_device
@code
...

int ret = 0;

ret = bluetooth_cancel_bonding();
@endcode
 */
int bluetooth_cancel_bonding(void);

/**
 * @fn int bluetooth_unbond_device(const bluetooth_device_address_t *device_address)
 * @brief Remove bonding
 *
 *
 * To communicate with remote device over bluetooth link, user should bond with peer device.
 * After bonding is over, peer device is added to list of bonded devices. bluetooth_unbond_device()
 * API is used to remove peer device from the list. Please note that after removing the device
 * from bonded device list, you cannot communication with peer device until bonding happens again.
 *
 * User can call this function by passing bluetooth device address of any bonded device. Please note
 * that after successful return of this function, any bluetooth application running on your device
 * will not be able to communicate with unbonded device until bond operation happens again using
 * bluetooth_bond_device()
 *
 *
 * This function is a asynchronous call. The request to remove the specified device from the bonded
 * list is responded by an BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED event. if the operation is success,
 * you will receive BLUETOOTH_ERROR_NONE. BLUETOOTH_ERROR_NOT_PAIRED may be received in result code
 * in case if there is a problem in locating given bluetooth device address in bonded devices list
 *
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Device address is not valid \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth adapter is not enabled \n
 *		BLUETOOTH_ERROR_DEVICE_BUSY	- Bluetooth adapter is busy doing some operation \n
 *		BLUETOOTH_ERROR_INVALID_DATA - Device address provided is incorrect \n
 *		BLUETOOTH_ERROR_INTERNAL - System error like heap full has occured or bluetooth
 *						agent is not running \n
 *		BLUETOOTH_ERROR_NOT_PAIRED - Device address mentioned in the argument is not a
 *						bonded device \n
 *
 * @param[in]   device_address   This indicates an address of the device to remove bonding
 *
 * @remark      None
 *
 * @see		bluetooth_bond_device, bluetooth_cancel_bonding
 *
@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event) {
		case BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED: {
			bluetooth_device_address_t *dev_addr = NULL;

			if (param->result == BLUETOOTH_ERROR_NONE) {
				dev_addr = (bluetooth_device_address_t *)param->param_data;
				//Unbound scuccess
				printf("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
					dev_addr->addr[0], dev_addr->addr[1]
					dev_addr->addr[2], dev_addr->addr[3]
					dev_addr->addr[4], dev_addr->addr[5]
			} else {
				//unbound failure
			}
		}
	}
}

...

int ret = 0;
bluetooth_device_address_t *device_address;

// copy valid device address in device_address

ret = bluetooth_unbond_device(device_address);
@endcode
 */
int bluetooth_unbond_device(const bluetooth_device_address_t *device_address);

/**
 * @fn int bluetooth_get_bonded_device_list(GPtrArray **dev_list)
 * @brief Get bonded(paired) device list
 *
 *
 * This API gets all bonded device list.
 * The devices in the bonded device list further can be used to perform the authorization by calling
 * bluetooth_authorize_device API.
 *
 * This function is a synchronous call.
 * Information for bonded devices can be obtained when result code is BLUETOOTH_ERROR_NONE. If not,
 * there is no valid information in the dev_list.
 * The len field in the dev_list represents the number of bonded devices. The data structure for
 * bonded device information is bluetooth_device_info_t.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_NOT_SUPPORT - Opreation not supported \n
 * @remark      None
 * @see		bluetooth_bond_device, bluetooth_unbond_device, bluetooth_authorize_device
 @code
void bt_get_bonded_devices(void)
{
...
	int i;
	GPtrArray *devinfo = NULL;
	bluetooth_device_info_t *ptr;

	// allocate the g_pointer_array
	devinfo = g_ptr_array_new();

	ret = bluetooth_get_bonded_device_list(&devinfo);
	if (ret != BLUETOOTH_ERROR_NONE)
	{
		printf("bluetooth_get_bonded_device_list failed with [%d]",ret);
	}
	else
	{
		printf("g pointer arrary count : [%d]", devinfo->len);
		for (i=0; i<devinfo->len; i++)
		{
			ptr = g_ptr_array_index(devinfo, i);
			if (ptr != NULL)
			{
				printf("Name [%s]\n", ptr->device_name.name);
				printf("Major Class [%d]\n", ptr->device_class.major_class);
				printf("Minor Class [%d]\n", ptr->device_class.minor_class);
				printf("Service Class [%d]\n", ptr->device_class.service_class);
				printf("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
								ptr->device_address.addr[0],
				ptr->device_address.addr[1], ptr->device_address.addr[2],
								ptr->device_address.addr[3],
				ptr->device_address.addr[4], ptr->device_address.addr[5]);

				// handle
				...
			}
		}
	}
	// free g_pointer_array
	g_ptr_array_free(devinfo, TRUE);
}

@endcode
 */
int bluetooth_get_bonded_device_list(GPtrArray **dev_list);

/**
 * @fn int bluetooth_get_bonded_device(const bluetooth_device_address_t *device_address,
 *					bluetooth_device_info_t *dev_info)
 * @brief Get a bonded(paired) device
 *
 *
 * This API gets a bonded device.
 *
 * This function is a synchronous call.
 * Information for bonded devices can be obtained when result code is BLUETOOTH_ERROR_NONE. If not,
 * there is no valid information in the dev_info.
 * The data structure for bonded device information is bluetooth_device_info_t.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_NOT_SUPPORT - Opreation not supported \n
 * @remark      None
 * @see		bluetooth_bond_device, bluetooth_unbond_device, bluetooth_authorize_device
 @code
void bt_get_bonded_device(void)
{
...
	int i;
	bluetooth_device_info_t devinfo = {0};
	bluetooth_device_address_t device_address={{0x00,0x1C,0x43,0x2B,0x1A,0xE5}};

	ret = bluetooth_get_bonded_device(&device_address, &devinfo);
	if (ret != BLUETOOTH_ERROR_NONE)
	{
		printf("bluetooth_get_bonded_device failed with [%d]",ret);
	}
	else
	{
		printf("Name [%s]\n", devinfo.device_name.name);
		printf("Major Class [%d]\n", devinfo.device_class.major_class);
		printf("Minor Class [%d]\n", devinfo.device_class.minor_class);
		printf("Service Class [%d]\n", devinfo.device_class.service_class);
		printf("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n", devinfo.device_address.addr[0],
		devinfo.device_address.addr[1], devinfo.device_address.addr[2],
		devinfo.device_address.addr[3], devinfo.device_address.addr[4],
		devinfo.device_address.addr[5]);

		// handle
		...
	}
}

@endcode
 */
int bluetooth_get_bonded_device(const bluetooth_device_address_t *device_address,
					bluetooth_device_info_t *dev_info);

/**
 * @fn int bluetooth_set_alias(const bluetooth_device_address_t *device_address, const char *alias)
 * @brief set alias for bonded device
 *
 *
 * This function set alias for bonded device.
 *
 * This function is a synchronous call.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 * @param[in]   device_address   This indicates an address of the remote device
 * @param[in]   alias			This indicates an alias to set
 * @remark      None
 * @see		None
@code
int ret = 0;
ret = bluetooth_set_alias(&remote_address);
@endcode
 */
int bluetooth_set_alias(const bluetooth_device_address_t *device_address,
				const char *alias);

/**
 * @fn int bluetooth_get_remote_device(const bluetooth_device_address_t *device_address)
 * @brief Get remote deivice
 *
 *
 * This function gets specific remote device.
 *
 * This function is a asynchronous call.
 * This API is responded by an BLUETOOTH_EVENT_REMOTE_DEVICE_READ event.
 *
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_NOT_SUPPORT - Operation not supported \n
 * @param[in]   device_address   This indicates an address of the remote device
 * @remark      None
 * @see		None
@code
int ret = 0;
ret = bluetooth_get_remote_device(&remote_address);
@endcode
 */
int bluetooth_get_remote_device(const bluetooth_device_address_t *device_address);

/**
 * @fn int bluetooth_authorize_device(const bluetooth_device_address_t *device_address,
 *					gboolean authorized)
 * @brief Authorize/Unauthorize a bonded device
 *
 *
 * This function authorizes/unauthorize a bonded device. It decides the device to connect
 * with/without user confirmation.
 *
 * If we select a paired device and make it authorized by calling this API with the authorized
 * parameter to TRUE, then it will not ask for the user conformation before connecting. Similarly
 * if we unauthorize the paired device by calling this API with the authorized parameter to FALSE,
 * then it will ask for the user conformation before the connection.
 *
 * This API supposed to be called on the paired devices. Which means we have to use this API only
 * after successful pairing.
 *
 * This function is a asynchronous call.
 * Response will be received through BLUETOOTH_EVENT_DEVICE_AUTHORIZED event.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 * @exception   BLUETOOTH_ERROR_INTERNAL - Cannot get the interal DBUS proxy \n
 * @param[in]   device_address   This indicates an address of the device to authorize \n
 * @param[in]	authorized	TRUE : authorize FALSE: unauthorize
 * @remark      None
 * @see		bluetooth_get_bonded_device_list
@code
void bt_event_callback(int event, bluetooth_event_param_t *param)
{
	switch(event)
	{
		case BLUETOOTH_EVENT_DEVICE_AUTHORIZED :
		{
			if (param->result == BLUETOOTH_ERROR_NONE)
			{
				//Device authorized
			}
			//device authorization failed failure
		}
	}
}

...

int ret = 0;
bluetooth_device_address_t device_address={{0}};
gboolean authorized;

authorized =TRUE;

ret = bluetooth_authorize_device(&device_address,authorized);
@endcode
 */
int bluetooth_authorize_device(const bluetooth_device_address_t *device_address,
				       gboolean authorized);

/**
 * @fn int bluetooth_search_service(const bluetooth_device_address_t *device_address)
 * @brief Get all services supported by remote device
 *
 *
 * This API call initiates the search for the services supported by the specified device. Normally
 * the service search will take a couple of seconds to get it completed. Before calling this API
 * make sure that the Adapter is enabled. We have to give the device address of the remote device to
 * perform the service search. We can get the device address by doing a device discovery operation.
 *
 *
 * This function is a asynchronous call.
 * The service search request is responded by BLUETOOTH_EVENT_SERVICE_SEARCHED event.
 *
 * There is a timeout associated with the service search. The default time out is 40 seconds. If the
 * remove device did not respond with in the time out period the BLUETOOTH_EVENT_SERVICE_SEARCHED
 * event is generated with appropriate result code.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_SERVICE_SEARCH_ERROR - Service search error (NULL device address) \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal IPC error \n
 * @param[in]   device_address   This indicates an address of the device
 *                               whose services need to be found
 * @remark      None
 * @see		bluetooth_cancel_service_search
 */
int bluetooth_search_service(const bluetooth_device_address_t *device_address);

/**
 * @fn int bluetooth_cancel_service_search(void)
 * @brief Cancel the ongoing service search operation
 *
 *
 * This function cancel the ongoing service search operation. This API is usually calling after the
 * bluetooth_search_service API.
 * Normally service search will take a more time (> 5 seconds) to complete. This API will be called
 * if the user wish to cancel the Ongoing service search operation.
 *
 * This API should be called just after the bluetooth_search_service API and before the
 * BLUETOOTH_EVENT_SERVICE_SEARCHED event
 *
 * This function is a synchronous call.
 *
 * @return	BLUETOOTH_ERROR_NONE - Success \n
 *		BLUETOOTH_ERROR_CANCEL - Error in service search cancel operation \n
 *		BLUETOOTH_ERROR_NOT_PAIRED - Not paired device \n
 *		BLUETOOTH_ERROR_NOT_IN_OPERATION - Searching service is not in operation \n
 *
 * @remark      None
 * @see		bluetooth_search_service
@code
...

int ret = 0;
ret = bluetooth_cancel_service_search();
@endcode
*/
int bluetooth_cancel_service_search(void);

/**
 * @fn int bluetooth_rfcomm_create_socket(const char *uuid)
 * @brief Register rfcomm socket with a specific uuid
 *
 *
 * This API register rfcomm socket with the given UUID. The return value of this API is the socket
 * descriptor of the server.
 * This is the first API which is called to create the server. Once we created the server socket,
 * we will listen on that return socket.
 * So a bluetooth_rfcomm_listen_and_accept should follow this API call. This is a synchronous call.
 *
 *
 * @return  socket FD on Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal error\n
 *              BLUETOOTH_ERROR_MAX_CONNECTION - Maximum connection reached\n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 * @param[in]   UUID (128 bits)
 *
 * @remark      None
 * @see       bluetooth_rfcomm_listen_and_accept, bluetooth_rfcomm_remove_socket
 *
 @code

  const char *rfcomm_test_uuid="00001101-0000-1000-8000-00805F9B34FB";
  fd  = bluetooth_rfcomm_create_socket(rfcomm_test_uuid);

 @endcode
 */
int bluetooth_rfcomm_create_socket(const char *uuid);

/**
 * @fn int bluetooth_rfcomm_remove_socket(int socket_fd, const char *uuid)
 * @brief De-register the rfcomm socket
 *
 *
 * This API deregister rfcomm socket with the given socket fd and  UUID. If the remote device is
 * already connected then we will receive the BLUETOOTH_EVENT_RFCOMM_DISCONNECTED with socket
 * descriptor else no event will come. We will call this API only after the
 * bluetooth_rfcomm_listen_and_accept.
 * This is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE - Success \n
 *               BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *               BLUETOOTH_ERROR_NOT_FOUND - Cannot find the proxy\n
 *               BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 * @param[in]  int socket_fd
 *
 * @remark      None
 * @see       bluetooth_rfcomm_create_socket, bluetooth_rfcomm_listen_and_accept
 *
 @code
 void bt_event_callback(int event, bluetooth_event_param_t *param)
 {
	switch(event)
	{
		case BLUETOOTH_EVENT_RFCOMM_DISCONNECTED:
		{
			bluetooth_rfcomm_connection_t *discon_ind =
					(bluetooth_rfcomm_connection_t *)param->param_data;

			printf("\nDisconnected from FD %d",  discon_ind->socket_fd);
		}
	}
 }

 ...

 int ret = 0;
 fd  = bluetooth_rfcomm_create_socket(rfcomm_test_uuid);
 ret = bluetooth_rfcomm_listen_and_accept(fd, 1);
 ....
 ret = bluetooth_rfcomm_remove_socket(fd);
 @endcode
 */
int bluetooth_rfcomm_remove_socket(int socket_fd);

/**
 * @fn int bluetooth_rfcomm_server_disconnect(int socket_fd)
 * @brief Disconnect rfcomm connection
 *
 *
 * Disconnect a specific(device node fd)  RFCOMM connection. This is a Synchronous call and there
 * is no cabbback events for this API. We have to provice the valid client fd to disconnect from the
 * remote server.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_NOT_CONNECTED - Not connected \n
 * @param[in]  int socket_fd the sending socket fd
 *
 * @remark      None
 *
 @code

  ret = bluetooth_rfcomm_server_disconnect(g_ret_fd);
  if (ret < 0)
	printf("Disconnection failed");
  else
  printf("Disconnection Success");

 @endcode
 */
int bluetooth_rfcomm_server_disconnect(int socket_fd);

/**
 * @fn int bluetooth_rfcomm_listen_and_accept(int socket_fd,int max_pending_connection)
 * @brief Rfcomm socket listen
 *
 *
 * This API make rfcomm socket listen and accept with socket. We will call this API immediatly
 * after the bluetooth_rfcomm_create_socket API.
 * This API listen for the incomming connection and once it receives a connection, it will give
 * BLUETOOTH_EVENT_RFCOMM_CONNECTED
 * event to the application. This is an Asynchronous API call.
 *
 *
 * @return  BLUETOOTH_ERROR_NONE - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_CONNECTION_ERROR - Listen failed \n

 * @param[in]  int socket_fd
 * @param[in]  max pending connection.
 *
 * @remark      None
 * @see       bluetooth_rfcomm_create_socket
 *
  @code
  void bt_event_callback(int event, bluetooth_event_param_t* param)
 {
	switch(event)
	{
		case BLUETOOTH_EVENT_RFCOMM_CONNECTED:
		{
			bluetooth_rfcomm_connection_t *conn_ind =
						(bluetooth_rfcomm_connection_t *)param->param_data;

			printf("\nConnected from FD %d",  conn_ind->socket_fd);
		}
	}
 }

 ...

 int ret = 0;
 fd  = bluetooth_rfcomm_create_socket(rfcomm_test_uuid);
 ret = bluetooth_rfcomm_listen_and_accept(fd, 1);

 @endcode
 */
int bluetooth_rfcomm_listen_and_accept(int socket_fd, int max_pending_connection);

/**
 * @fn int bluetooth_rfcomm_listen(int socket_fd,int max_pending_connection)
 * @brief Rfcomm socket listen
 *
 *
 * This API make rfcomm socket listen and accept with socket. We will call this API immediatly
 * after the bluetooth_rfcomm_create_socket API.
 * This API listen for the incomming connection and once it receives a connection, it will give
 * BLUETOOTH_EVENT_RFCOMM_AUTHORIZE
 * event to the application. This is an Asynchronous API call.
 *
 *
 * @return  BLUETOOTH_ERROR_NONE - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_CONNECTION_ERROR - Listen failed \n

 * @param[in]  int socket_fd
 * @param[in]  max pending connection.
 *
 * @remark      None
 * @see       bluetooth_rfcomm_create_socket
 *
  @code
  void bt_event_callback(int event, bluetooth_event_param_t* param)
 {
	switch(event)
	{
		case BLUETOOTH_EVENT_RFCOMM_AUTHORIZE:
		{
			char *name = (char *)param->param_data;

			printf("\nConnected from %s",  name);

			bluetooth_rfcomm_accept_connection();
		}
	}
 }

 ...

 int ret = 0;
 fd  = bluetooth_rfcomm_create_socket(rfcomm_test_uuid);
 ret = bluetooth_rfcomm_listen(fd, 1);

 @endcode
 */
int bluetooth_rfcomm_listen(int socket_fd, int max_pending_connection);

/**
 * @fn int bluetooth_rfcomm_accept_connection()
 * @brief Accepts the authorization request indicated by the event
  * BLUETOOTH_EVENT_RFCOMM_AUTHORIZE.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal error \n
 *
 * @param[in]  the socket fd of the server
 * @param[out]  the socket fd of the client
 *
 * @exception   None
 * @remark       None
 * @see    	  bluetooth_rfcomm_reject_connection
 */
int bluetooth_rfcomm_accept_connection(int server_fd, int *client_fd);

/**
 * @fn int bluetooth_rfcomm_reject_connection()
 * @brief Rejects the authorization request indicated by the event
  * BLUETOOTH_EVENT_RFCOMM_AUTHORIZE.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal error \n
 *
 * @param[in]  the socket fd of the server
 *
 * @exception   None
 * @remark       None
 * @see    	  bluetooth_rfcomm_accept_connection
 */
int bluetooth_rfcomm_reject_connection(int server_fd);

/**
 * @fn gboolean bluetooth_rfcomm_is_server_uuid_available(const char *uuid)
 * @brief Informs whether rfcomm server uuid is available or not.
 *
 * This function is a synchronous call.
 *
 * @return   TRUE  - RFCOMM uuid is available \n
 *              FALSE - RFCOMM uuid is not available \n
 *
 * @param[in]  uuid UUID string
 *
 * @exception   None
 *
 * @remark       None
 */
gboolean bluetooth_rfcomm_is_server_uuid_available(const char *uuid);

/**
 * @fn int bluetooth_rfcomm_connect(const bluetooth_device_address_t  *remote_bt_address,
 *									const char *remote_uuid)
 * @brief Connect to the remote device rfcomm *
 *
 * Connect to a specific RFCOMM based service on a remote device UUID. This is a Async call. Once
 * the connection is successful callback BLUETOOTH_EVENT_RFCOMM_CONNECTED events is generated,
 * which contains the socket_fd, device role (RFCOMM_ROLE_SERVER/RFCOMM_ROLE_CLIENT), device addess
 * etc. The socket_fd can be further used to send the data. It better to do a sevice search before
 * initiating a connection.
 *
 * @return  BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_CONNECTION_BUSY - Connection in progress \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error\n
 * @param[in]  bluetooth_device_address_t remote bt_address
 * @param[in]  char remote uuid
 * @remark      None
 * @see         bluetooth_rfcomm_disconnect, bluetooth_rfcomm_write, bluetooth_search_service
 *
 @code

 void bt_event_callback(int event, bluetooth_event_param_t *param)
 {
	switch(event)
	{
		case BLUETOOTH_EVENT_SERVICE_SEARCHED:
		{
			if (param->result >= BLUETOOTH_ERROR_NONE)
			{
				bt_sdp_info_t *bt_sdp_info=param->param_data;

				printf("Dev add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
					bt_sdp_info->device_addr.addr[0],
					bt_sdp_info->device_addr.addr[1],
					bt_sdp_info->device_addr.addr[2],
					bt_sdp_info->device_addr.addr[3],
					bt_sdp_info->device_addr.addr[4],
					bt_sdp_info->device_addr.addr[5]);

					printf("Supported service list:\n");
					for(i=0; i<bt_sdp_info->service_index; i++)
						printf("[%#x]\n",
							bt_sdp_info->service_list_array[i]);

				//Alternate method
				//ret = bluetooth_rfcomm_connect(bt_sdp_info->device_addr,
										//rfcomm_test_uuid);
			}
			break;
		}
		case BLUETOOTH_EVENT_RFCOMM_CONNECTED:
		{
			bluetooth_rfcomm_connection_t *conn_ind =
						(bluetooth_rfcomm_connection_t *)param->param_data;

			printf("\nConnected from FD %d, Role = %s",  conn_ind->socket_fd,
						(conn_ind->device_role == RFCOMM_ROLE_SERVER) ?
									"SERVER" : "CLIENT");
		}
	}
 }

  bluetooth_device_address_t remote_address = {{0},};
  remote_address.addr[0] = 0x0; remote_address.addr[1] = 0x0A; remote_address.addr[2] = 0x3A;
  remote_address.addr[3]= 0x54; remote_address.addr[4] = 0x19;  remote_address.addr[5]= 0x36;
  ret = bluetooth_search_service(&remote_address);
 if (ret < 0)
	printf("Seach failed, Reason = %d", ret);
  else
	 printf("Search Success, Ret = %d", ret);

  ret = bluetooth_rfcomm_connect(&remote_address, rfcomm_test_uuid);
  if (ret < 0)
	printf("Connection failed, Reason = %d", ret);
  else
	 printf("Connection Success, Ret = %d", ret);

  @endcode
  */
int bluetooth_rfcomm_connect(const bluetooth_device_address_t *remote_bt_address,
				     const char *remote_uuid);

/**
 * @fn int bluetooth_rfcomm_disconnect(int socket_fd)
 * @brief Disconnect rfcomm connection
 *
 *
 * Disconnect a specific(device node fd)  RFCOMM connection. This is a Synchronous call and there
 * is no cabbback events for this API. We have to provice the valid client fd to disconnect from the
 * remote server.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_NOT_CONNECTED - Not connected \n
 * @param[in]  char remote bt_address
 *
 * @remark      None
 * @see         bluetooth_rfcomm_connect
 *
 @code

  ret = bluetooth_rfcomm_disconnect(g_ret_fd);
  if (ret < 0)
	printf("Disconnection failed");
  else
  printf("Disconnection Success");

 @endcode
 */

int bluetooth_rfcomm_disconnect(int socket_fd);

/**
 * @fn int bluetooth_rfcomm_write (int fd, const char *buff, int length)
 * @brief Write to rfcomm connection
 *
 *
 * This API is used to send the data over the rfcomm connection. This is a synchronous API. The same
 * API is used to send the data for server and the client.
 *
 * @return  BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_NOT_IN_OPERATION - The Fd is currently not in operation\n
 * @param[in]  int fd
 * @param[in]  const char *buff  Data buffer to send
 * @param[in]  int length Length of the data
 *
 * @remark      None
 * @see         bluetooth_rfcomm_connect
 *
  @code
  char *buff = "Test data 123456789"
  ret =  bluetooth_rfcomm_write(g_ret_fd, buff, 15);
  if (ret < 0)
	printf("Send failed");
  else
   printf("Send success");

 @endcode
 */
int bluetooth_rfcomm_write(int fd, const char *buf, int length);

/**
 * @fn gboolean bluetooth_rfcomm_is_client_connected(void)
 * @brief Informs whether rfcomm client is connected.
 *
 * This function is a synchronous call.
 *
 * @return   TRUE  - RFCOMM client is connected \n
 *              FALSE - RFCOMM client is not connected \n
 *
 * @exception   None
 *
 * @remark       None
 */
gboolean bluetooth_rfcomm_is_client_connected(void);

/**
 * @fn int bluetooth_network_activate_server(void)
 * @brief Activate the NAP (Network Access Point) service
 *
 * This function is a asynchronous call.
 * The activate server request is responded by BLUETOOTH_EVENT_NETWORK_SERVER_ACTIVATED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *
 * @remark      None
  * @see	bluetooth_network_deactivate_server
 *
 */
int bluetooth_network_activate_server(void);

/**
 * @fn int bluetooth_network_deactivate_server(void)
 * @brief Deactivate the NAP (Network Access Point) service
 *
 * This function is a asynchronous call.
 * The deactivate server request is responded by BLUETOOTH_EVENT_NETWORK_SERVER_DEACTIVATED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *
 * @remark      None
 * @see		bluetooth_network_activate_server
 *
 */
int bluetooth_network_deactivate_server(void);

/**
 * @fn int bluetooth_network_connect(const bluetooth_device_address_t *device_address,
 *					bluetooth_network_role_t role,
 *					char  custom_uuid)
 * @brief Connect the network server in the peer
 *
 * This function is a asynchronous call.
 * The network connect request is responded by BLUETOOTH_EVENT_NETWORK_CONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *
 * @exception   None
 * @param[in]  device_address   This indicates an address of the device with which the pairing
 *				should be initiated
 * @param[in]  role   The role to connect. PANU / GN / NAP / CUSTOM. If use the CUSTOM value,
 *			need to use the third parameter.
 * @param[in]  custom_uuid   If use the CUSTOM value in second parameter, use this parameter to
 *				connect. UUID string
 * @remark      None
 * @see		bluetooth_network_disconnect
 */
int bluetooth_network_connect(const bluetooth_device_address_t *device_address,
				      bluetooth_network_role_t role, char *custom_uuid);

/**
 * @fn int bluetooth_network_disconnect(const bluetooth_device_address_t *device_address,
 *							bluetooth_network_role_t role,
 *							  char *custom_uuid)
 * @brief Connect the network server in the peer
 *
 * This function is a asynchronous call.
 * The network disconnect request is responded by BLUETOOTH_EVENT_NETWORK_CONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *
 * @exception   None
 * @param[in]   device_address   This indicates an address of the device with which the pairing
 *					should be initiated
 * @remark       None
 * @see		bluetooth_network_connect
 */
int bluetooth_network_disconnect(const bluetooth_device_address_t *device_address);

/**
 * @fn int bluetooth_network_server_disconnect(const bluetooth_device_address_t *device_address)
 * @brief Disconnect the device from the network
 *
 * This function is an asynchronous call.
 * The network server disconnect request is responded by
 * BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *
 * @exception   None
 * @param[in]   device_address   This indicates an address of the connected client device
 * @remark       None
 * @see		bluetooth_network_activate_server
 */
int bluetooth_network_server_disconnect(const bluetooth_device_address_t *device_address);

/*HDP - API's*/

/**
 * @fn int bluetooth_hdp_activate(unsigned short  data_type,
 *					bt_hdp_role_type_t role,
 *					bt_hdp_qos_type_t channel_type,
 *					char **app_handle)
 * @brief Activate the HDP service for a particular data type
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *
 * @exception   None
 * @param[in]  data_type   The data type against corresponding service
 * @param[in]  role   The role of HDP. HDP_ROLE_SOURCE/HDP_ROLE_SINK.
 * @param[in]  channel_type   The QOS type for the channel.
 *				HDP_QOS_RELIABLE/HDP_QOS_STREAMING/HDP_QOS_ANY.
 *				For role = HDP_ROLE_SINK, the channel_type
 *				should be HDP_QOS_ANY.
 * @param[out]  app_handle    The application handler against corresponding service
 * @remark       None
 * @see    	   bluetooth_hdp_deactivate
 */
int bluetooth_hdp_activate(unsigned short  data_type,
				bt_hdp_role_type_t role,
				bt_hdp_qos_type_t channel_type,
				char **app_handle);
/**
 * @fn int bluetooth_hdp_deactivate(const char *app_handle)
 * @brief Deactivate the HDP service for a particular service using the handler
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *
 * @exception   None
 * @param[in]  app_handle   The application handler against corresponding service
 * @remark       None
 * @see    	   bluetooth_hdp_deactivate
 */
int bluetooth_hdp_deactivate(const char *app_handle);

/**
 * @fn int bluetooth_hdp_send_data(unsigned int channel_id,
 *					const char *buffer, unsigned int size)
 * @brief Send data to the remote HDP device
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *             BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *             BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *             BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *             BLUETOOTH_ERROR_NOT_IN_OPERATION - FD is invalid  \n
 *
 * @exception   None
 * @param[in]  channel_id   The channel id for the connection.
 * @param[in]  buffer   The pdu buffer.
 * @param[in]  size   Size of the buffer.
 * @remark       None
 * @see    	   bluetooth_hdp_connect
 */
int bluetooth_hdp_send_data(unsigned int channel_id,
				const char *buffer, unsigned int size);
/**
 * @fn int bluetooth_hdp_connect(const char *app_handle,
 *				bt_hdp_qos_type_t channel_type,
 *				const bluetooth_device_address_t *device_address)
 * @brief Connect to the remote device(Mainly used by source)
 *
 * This function is a asynchronous call.
 * The HDP activate is responded by BLUETOOTH_EVENT_HDP_CONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_MEMORY_ALLOCATION -Memory allocation failed \n
 *
 * @exception   None
 * @param[in]  app_handle   The application handler against corresponding service
 * @param[in]  channel_type   The QOS type for the channel.
 *				HDP_QOS_RELIABLE/HDP_QOS_STREAMING.
 * @param[in]  device_address   The remote device Bd address.
 *
 * @remark       None
 * @see    	   bluetooth_hdp_disconnect
 */
int bluetooth_hdp_connect(const char *app_handle,
			bt_hdp_qos_type_t channel_type,
			const bluetooth_device_address_t *device_address);
/**
 * @fn int bluetooth_hdp_disconnect(unsigned int channel_id,
 *			const bluetooth_device_address_t *device_address)
 * @brief Disconnect from the remote device(Mainly used by source)
 *
 * This function is a asynchronous call.
 * The HDP activate is responded by BLUETOOTH_EVENT_HDP_DISCONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
  *              BLUETOOTH_ERROR_MEMORY_ALLOCATION -Memory allocation failed \n
 *
 * @exception   None
 * @param[in]  channel_id    The channel id for the connection.
 * @param[in]  device_address   The remote device Bd address.
 *
 * @remark       None
 * @see    	   bluetooth_hdp_connect
 */
int bluetooth_hdp_disconnect(unsigned int channel_id,
			const bluetooth_device_address_t  *device_address);


/**
 * @fn int bluetooth_opc_init(void)
 * @brief Initialize OPP client.
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
  *             BLUETOOTH_ERROR_ACCESS_DENIED -Memory allocation failed \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	  bluetooth_opc_deinit
 */
int bluetooth_opc_init(void);

/**
 * @fn int bluetooth_opc_deinit(void)
 * @brief Deinitialize OPP client.
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_ACCESS_DENIED -Memory allocation failed \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	  bluetooth_opc_init
 */

 int bluetooth_opc_deinit(void);

/**
 * @fn int bluetooth_opc_push_files(bluetooth_device_address_t *remote_address,
					char **file_name_array)
 * @brief Send multiple files to a remote device.
 *
 * This function is a asynchronous call.
 * This api  is responded by BLUETOOTH_EVENT_OPC_CONNECTED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_IN_PROGRESS -Already one push in progress \n
 *
 * @exception   None
 * @param[in]  device_address   The remote device Bd address.
 * @param[in]  file_name_array  Array of filepaths to be sent.
 *
 * @remark       None
 * @see    	 bluetooth_opc_cancel_push
 */

int bluetooth_opc_push_files(bluetooth_device_address_t *remote_address,
		   		char **file_name_array);

/**
 * @fn int bluetooth_opc_cancel_push(void)
 * @brief Cancels the ongoing file push.
 *
 * This function is a asynchronous call.
 * This api is responded with either BLUETOOTH_EVENT_OPC_CONNECTED or
 * BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETED event.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_ACCESS_DENIED - No push in progress \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	  bluetooth_opc_push_files
 */

int bluetooth_opc_cancel_push(void);

/**
 * @fn gboolean bluetooth_opc_session_is_exist(void)
 * @brief Informs whether opc session exists or not.
 *
 * This function is a synchronous call.
 *
 * @return   TRUE  - OPC session exists \n
 *              FALSE - OPC session does not exist \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	 None
 */

gboolean bluetooth_opc_session_is_exist(void);

/**
 * @fn int bluetooth_opc_is_sending(gboolean *is_sending)
 * @brief Informs whether opc session exists or not.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *               BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *               BLUETOOTH_ERROR_INTERNAL \n
 *
 * @exception   None
 * @param[out] is_sending The sending status: (@c TRUE = in sending , @c  false = not in sending)
 *
 * @remark       None
 * @see            None
 */
int bluetooth_opc_is_sending(gboolean *is_sending);

/**
 * @fn int bluetooth_obex_server_init(const char *dst_path)
 * @brief Initialize OPP and FTP server.
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_AGENT_ALREADY_EXIST - Obex agent already registered \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *
 * @exception   None
 * @param[in]  dst_path   OPS destination file path.
 * @remark       None
 * @see    	  bluetooth_obex_server_deinit
 */
int bluetooth_obex_server_init(const char *dst_path);


/**
 * @fn int bluetooth_obex_server_deinit(void)
 * @brief Deinitialize OPP and FTP server.
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	  bluetooth_obex_server_init
 */
int bluetooth_obex_server_deinit(void);


/**
 * @fn gboolean bluetooth_obex_server_is_activated(void)
 * @brief Informs whether obex server is activated or not.
 *
 * This function is a synchronous call.
 *
 * @return   TRUE  - OBEX server is activated \n
 *              FALSE - OBEX server is not activated \n
 *
 * @exception   None
 *
 * @remark       None
 */

gboolean bluetooth_obex_server_is_activated(void);


/**
 * @fn int bluetooth_obex_server_init_without_agent(const char *dst_path)
 * @brief Initialize OPP and FTP server without the conneciton authorization of the agent
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_AGENT_ALREADY_EXIST - Obex agent already registered \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *
 *
 * @exception   None
 * @param[in]  dst_path   OPS destination file path.
 * @remark       None
 * @see    	  bluetooth_obex_server_deinit_without_agent
 */
int bluetooth_obex_server_init_without_agent(const char *dst_path);


/**
 * @fn int bluetooth_obex_server_deinit_without_agent(void)
 * @brief Deinitialize OPP and FTP server without the conneciton authorization of the agent
 *
 * This function is a synchronous call.
 * No event corresponding to this api
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *
 * @exception   None
 *
 * @remark       None
 * @see    	  bluetooth_obex_server_init_without_agent
 */
int bluetooth_obex_server_deinit_without_agent(void);


/**
 * @fn int bluetooth_obex_server_accept_connection(void)
 * @brief Accepts the authorization request indicated by the event
  * BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal error \n
 *
 * @exception   None
 * @remark       None
 * @see    	  bluetooth_obex_server_reject_authorize
 */
int bluetooth_obex_server_accept_connection(void);


/**
 * @fn int bluetooth_obex_server_reject_connection(void)
 * @brief Rejects the authorization request indicated by the event
  * BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal error \n
 *
 * @exception   None
 * @remark       None
 * @see    	  bluetooth_obex_server_reject_authorize
 */
int bluetooth_obex_server_reject_connection(void);


/**
 * @fn int bluetooth_obex_server_accept_authorize(const char *filename)
 * @brief Accepts the authorization request indicated by the event
  * BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE.
 *
 * This function is a asynchronous call.
 * This api will be responded with the event BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *
 * @exception   None
 * @param[in]  filename   Authorized filename.

 * @remark       None
 * @see    	  bluetooth_obex_server_reject_authorize
 */

int bluetooth_obex_server_accept_authorize(const char *filename);

/**
 * @fn int bluetooth_obex_server_reject_authorize(void)
 * @brief Reject the authorization request indicated by the event
  * BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE.
 *
 * This function is a asynchronous call.
 * No event for this api..
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *
 * @exception   None

 * @remark       None
 * @see    	  bluetooth_obex_server_accept_authorize
 */

int bluetooth_obex_server_reject_authorize(void);

/**
 * @fn int bluetooth_obex_server_set_destination_path(const char *dst_path)
 * @brief Set the OPS destination file path..
 *
 * This function is a asynchronous call.
 * No event for this api..
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid Param \n
 *
 * @exception   None
 * @param[in]  dst_path   OPS destination file path.

 * @remark       None
 * @see    	 None
 */

int bluetooth_obex_server_set_destination_path(const char *dst_path);

/**
 * @fn int bluetooth_obex_server_set_root(const char *root)
 * @brief Set the FTS root folder..
 *
 * This function is a asynchronous call.
 * No event for this api..
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *              BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Device is not enabled \n
 *              BLUETOOTH_ERROR_NO_RESOURCES - Not resource available \n
 *              BLUETOOTH_ERROR_ACCESS_DENIED - Operation not allowed \n
 *              BLUETOOTH_ERROR_INVALID_PARAM - Invalid parameter \n
 *              BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *
 * @exception  None
 * @param[in]   root   FTS root folder.

 * @remark       None
 * @see    	 None
 */

int bluetooth_obex_server_set_root(const char *root);

/**
 * @fn int bluetooth_obex_server_cancel_transfer(int transfer_id)
 * @brief Cancel the transfer on server
 *
 * This function is an asynchronous call.
 * If the function call that cancels transfer is successful, the application would receive
 * BLUETOOTH_EVENT_TRANSFER_COMPLETED event through registered callback
 * function with an error code BLUETOOTH_ERROR_CANCEL. In the case of failure
 * the error code will be BLUETOOTH_ERROR_NONE
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *               BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *               BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *               BLUETOOTH_ERROR_INTERNAL - internal error (proxy does not exist) \n
 *               BLUETOOTH_ERROR_NOT_FOUND - The transfer is not found \n
 *
 * @exception None
 * @param[in] transfer_id transfer ID

 * @remark       None
 * @see    	 None
 */
int bluetooth_obex_server_cancel_transfer(int transfer_id);


/**
 * @fn int bluetooth_obex_server_cancel_all_transfers(void)
 * @brief Cancel the transfer on server
 *
 * This function is an asynchronous call.
 * If the function call that cancels transfer is successful, the application would receive
 * BLUETOOTH_EVENT_TRANSFER_COMPLETED event through registered callback
 * function with an error code BLUETOOTH_ERROR_CANCEL. In the case of failure
 * the error code will be BLUETOOTH_ERROR_NONE
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *               BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *               BLUETOOTH_ERROR_AGENT_DOES_NOT_EXIST -Obex agent not registered \n
 *               BLUETOOTH_ERROR_INTERNAL - internal error (proxy does not exist) \n
 *               BLUETOOTH_ERROR_NOT_FOUND - The transfer is not found \n
 *
 * @exception None
 *
 * @remark       None
 * @see    	 None
 */
int bluetooth_obex_server_cancel_all_transfers(void);


/**
 * @fn int bluetooth_obex_server_is_receiving(gboolean *is_receiving)
 * @brief Informs whether obex server is receiving or not.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *               BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Not enabled \n
 *               BLUETOOTH_ERROR_INTERNAL \n
 *
 * @exception   None
 * @param[out] is_receiving The receiving status: (@c TRUE = in receiving , @c  false = not in receiving)
 *
 * @remark       None
 * @see            None
 */
int bluetooth_obex_server_is_receiving(gboolean *is_receiving);


/**
 * @fn int bluetooth_oob_read_local_data(bt_oob_data_t *local_oob_data)
 * @brief Read the local Hash and Randmizer.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *           BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *
 * @exception  None
 * @param[in]  None.
 * @param[out] local_oob_data - Pointer to the local OOB data
 *
 * @remark       None
 * @see    	 None
 */

int bluetooth_oob_read_local_data(bt_oob_data_t *local_oob_data);


/**
 * @fn int bluetooth_oob_add_remote_data(
 *			const bluetooth_device_address_t *remote_device_address,
 *			bt_oob_data_t *oob_data)
 * @brief Add/updated the remote device  Hash and Randmizer.
 *
 * This function is a synchronous call.
 * No event for this api..
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *           BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *
 * @exception	None
 * @param[in] remote_device_address - Remote device address
 *	      remote_oob_data - Ponter to Hash and Randomizer oob data structure
 *
 * @remark	None
 * @see		None
 */

int bluetooth_oob_add_remote_data(
		   const bluetooth_device_address_t *remote_device_address,
		   bt_oob_data_t *remote_oob_data);


/**
 * @fn int bluetooth_oob_remove_remote_data(
 *			const bluetooth_device_address_t *remote_device_address)
 * @brief Delete the Hash and Randomizer w.r.t the remote device address.
 *
 * This function is a synchronous call.
 * No event for this api..
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *           BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *
 * @exception  None
 * @param[in] remote_device_address - Remote device address
 *
 * @remark       None
 * @see    	 None
 */

int bluetooth_oob_remove_remote_data(
			const bluetooth_device_address_t *remote_device_address);

/**
 * @fn int bluetooth_gatt_get_primary_services(const bluetooth_device_address_t *address,
 *						bt_gatt_handle_info_t *prim_svc);
 *
 * @brief Gets the GATT based primary services handle supported by remote device
 *
 * This function is a synchronous call.
 * The output parameter needs to be freed by calling bluetooth_gatt_free_primary_services()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	 None
 * @param[in]	 address - Remote device address
 * @param[out] prim_svc - Structure containing remote service count and handle list.
 *
 * @remark	None
 * @see		bluetooth_gatt_free_primary_services()
 */
int bluetooth_gatt_get_primary_services(const bluetooth_device_address_t *address,
						bt_gatt_handle_info_t *prim_svc);

/**
 * @fn int bluetooth_gatt_discover_service_characteristics(const char *service_handle)
 *
 * @brief Discovers the characteristics of GATT based service of remote device
 *
 * This function is an asynchronous call.
 * This API is responded with BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	service_handle - Handle for remote service.
 *
 * @remark	None
 * @see		None
 */
int bluetooth_gatt_discover_service_characteristics(const char *service_handle);

/**
 * @fn int bluetooth_gatt_get_service_property(const char *service_handle,
 *						bt_gatt_service_property_t *service);
 *
 * @brief Gets the properties of GATT based service of remote device
 *
 * This function is a synchronous call.
 * The output parameter needs to be freed by calling bluetooth_gatt_free_primary_services()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	service_handle - Handle for remote service.
 * @param[out]	service - Structure containing remote service property.
 *
 * @remark	None
 * @see		bluetooth_gatt_free_service_property()
 */
int bluetooth_gatt_get_service_property(const char *service_handle,
						bt_gatt_service_property_t *service);

/**
 * @fn int bluetooth_gatt_watch_characteristics(const char *service_handle)
 *
 * @brief Register to GATT based service to receive value change notification/indication.
 *
 * This function is a synchronous call.
 * No event for this api.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	service_handle - Handle for remote service.
 *
 * @remark	None
 * @see    	None
 */
int bluetooth_gatt_watch_characteristics(const char *service_handle);

/**
 * @fn int bluetooth_gatt_unwatch_characteristics(const char *service_handle)
 *
 * @brief Unregister GATT based service to receive value change notification/indication.
 *
 * This function is a synchronous call.
 * No event for this api.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	service_handle - Handle for remote service.
 *
 * @remark	None
 * @see		None
 */
int bluetooth_gatt_unwatch_characteristics(const char *service_handle);

/**
 * @fn int bluetooth_gatt_get_characteristics_property(const char *char_handle,
 *						bt_gatt_char_property_t *characteristic);
 *
 * @brief Provides characteristic value along with properties.
 *
 * This function is a synchronous call.
 * The output parameter needs to be freed by calling bluetooth_gatt_free_char_property()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	char_handle - Handle for Characteristic property.
 * @param[out] characteristic - Structure containing remote characteristic property.
 *
 * @remark	None
 * @see		bluetooth_gatt_free_char_property()
 */
int bluetooth_gatt_get_characteristics_property(const char *char_handle,
						bt_gatt_char_property_t *characteristic);

/**
 * @fn int bluetooth_gatt_set_characteristics_value(const char *char_handle,
 *						const guint8 *value, int lengthi,
 *						guint8 request)
 *
 * @brief Set characteristic value.
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	char_handle - Handle for Characteristic property.
 * @param[in]	value - New value to set for characteristic property.
 * @param[in]	length - Length of the value to be set.
 * @param[in]   request - TRUE for request and FALSE for command.
 *
 * @remark	None
 * @see		None
 */
int bluetooth_gatt_set_characteristics_value(const char *char_handle,
						const guint8 *value, int length,
						guint8 request);

int bluetooth_gatt_read_characteristic_value(const char *char_handle);

/**
 * @fn int bluetooth_gatt_get_service_from_uuid(bluetooth_device_address_t *address,
 *					const char *service_uuid,
 *					bt_gatt_service_property_t *service)
 *
 * @brief Gets the service property from a device based on a particular uuid
 *
 * This function is a synchronous call.
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	address - BD address of the remote device
 * @param[in]	service_uuid - uuid of the service.
 * @param[out] service - Structure containing the service property.
 *
 * @remark	None
 * @see		None
 */
 int bluetooth_gatt_get_service_from_uuid(bluetooth_device_address_t *address,
					const char *service_uuid,
					bt_gatt_service_property_t *service);

/**
 * @fn int bluetooth_gatt_get_char_from_uuid(const char *service_handle,
 *							const char *char_uuid)
 *
 * @brief Gets the characteristic property from a service based on a particular char uuid
 *
 * This function is an asynchronous call.
 * This API is responded with BLUETOOTH_EVENT_GATT_GET_CHAR_FROM_UUID
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INTERNAL - Internal Error \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Adapter is disabled \n
 *
 * @exception	None
 * @param[in]	service_handle - Service handle.
 * @param[in]	uuid - Characteristic uuid.
 *
 * @remark	None
 * @see		None
 */
int bluetooth_gatt_get_char_from_uuid(const char *service_handle,
						const char *char_uuid);

/**
 * @fn int bluetooth_gatt_free_primary_services(bt_gatt_handle_info_t *prim_svc);
 *
 * @brief Releases the memory allocated by bluetooth_gatt_get_primary_services()
 *
 * This function is a synchronous call.
 * The input parameter is obtained by calling bluetooth_gatt_get_primary_services()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *
 * @exception	None
 * @param[in]	prim_svc - GATT handle info structure
 *
 * @remark	None
 * @see		bluetooth_gatt_get_primary_services()
 */
int bluetooth_gatt_free_primary_services(bt_gatt_handle_info_t *prim_svc);

/**
 * @fn int bluetooth_gatt_free_service_property(bt_gatt_service_property_t *svc_pty);
 *
 * @brief  Releases the memory allocated by bluetooth_gatt_get_service_property()
 *
 * This function is a synchronous call.
 * The input parameter is obtained by calling bluetooth_gatt_get_service_property()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *
 * @exception	None
 * @param[in]	svc_pty - GATT service property structure.
 *
 * @remark	None
 * @see		bluetooth_gatt_get_service_property()
 */
int bluetooth_gatt_free_service_property(bt_gatt_service_property_t *svc_pty);

/**
 * @fn int bluetooth_gatt_free_char_property(bt_gatt_char_property_t *char_pty);
 *
 * @brief Provides characteristic value along with properties.
 *
 * This function is a synchronous call.
 * The input parameter is obtained by calling bluetooth_gatt_get_characteristics_property()
 *
 * @return   BLUETOOTH_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *
 * @exception	None
 * @param[in]	char_pty - GATT characteristics property structure.
 *
 * @remark	None
 * @see		bluetooth_gatt_get_characteristics_property()
 */
 int bluetooth_gatt_free_char_property(bt_gatt_char_property_t *char_pty);

int bluetooth_connect_le(const bluetooth_device_address_t *device_address);

int bluetooth_disconnect_le(const bluetooth_device_address_t *device_address);

int bluetooth_read_rssi(const bluetooth_device_address_t *device_address);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _BLUETOOTH_API_H_*/
