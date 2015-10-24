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

#include <glib.h>
#include <dlog.h>
#include <gio/gio.h>
#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"

static GDBusConnection *event_conn;
static GDBusConnection *hf_local_term_event_conn;

#ifdef HPS_FEATURE
int _bt_send_to_hps(void)
{
	gboolean ret = FALSE;
	GError *error = NULL;

	BT_DBG(" ");

	retv_if(event_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	ret = g_dbus_connection_emit_signal(event_conn, NULL,
					"/org/projectx/httpproxy",
					"org.projectx.httpproxy_service",
					BT_LE_ENABLED,
					NULL, &error);
	if (!ret) {
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], \
					message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}
#endif

int _bt_send_event(int event_type, int event, GVariant *param)
{
	BT_DBG("+");
	char *path;
	char *signal;
	GDBusMessage *msg1 = NULL;

	retv_if(event_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("event_type [%d], event [%d]", event_type, event);

	switch (event_type) {
	case BT_ADAPTER_EVENT:
		path = BT_ADAPTER_PATH;
		break;
	case BT_LE_ADAPTER_EVENT:
		path = BT_LE_ADAPTER_PATH;
		break;
	case BT_DEVICE_EVENT:
		path = BT_DEVICE_PATH;
		break;
	case BT_HID_EVENT:
		path = BT_HID_PATH;
		break;
	case BT_HEADSET_EVENT:
		path = BT_HEADSET_PATH;
		break;
	case BT_AVRCP_EVENT:
		path = BT_AVRCP_PATH;
		break;
	case BT_AVRCP_CONTROL_EVENT:
		path = BT_AVRCP_CONTROL_PATH;
		break;
	case BT_NETWORK_EVENT:
		path = BT_NETWORK_PATH;
		break;
	case BT_OPP_CLIENT_EVENT:
		path = BT_OPP_CLIENT_PATH;
		break;
	case BT_OPP_SERVER_EVENT:
		path = BT_OPP_SERVER_PATH;
		break;
	case BT_PBAP_CLIENT_EVENT:
		path = BT_PBAP_CLIENT_PATH;
		break;
	case BT_RFCOMM_CLIENT_EVENT:
		path = BT_RFCOMM_CLIENT_PATH;
		break;
	case BT_RFCOMM_SERVER_EVENT:
		path = BT_RFCOMM_SERVER_PATH;
		break;
        case BT_A2DP_SOURCE_EVENT:
                path = BT_A2DP_SOURCE_PATH;
                break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	switch (event) {
	case BLUETOOTH_EVENT_ENABLED:
		signal = BT_ENABLED;
		break;
	case BLUETOOTH_EVENT_DISABLED:
		signal = BT_DISABLED;
		break;
	case BLUETOOTH_EVENT_LE_ENABLED:
		signal = BT_LE_ENABLED;
		break;
	case BLUETOOTH_EVENT_LE_DISABLED:
		signal = BT_LE_DISABLED;
		break;
	case BLUETOOTH_EVENT_LOCAL_NAME_CHANGED:
		signal = BT_ADAPTER_NAME_CHANGED;
		break;
	case BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED:
		signal = BT_DISCOVERABLE_MODE_CHANGED;
		break;
	case BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED:
		signal = BT_DISCOVERABLE_TIMEOUT_CHANGED;
		break;
	case BLUETOOTH_EVENT_CONNECTABLE_CHANGED:
		signal = BT_CONNECTABLE_CHANGED;
		break;
	case BLUETOOTH_EVENT_DISCOVERY_STARTED:
		signal = BT_DISCOVERY_STARTED;
		break;
	case BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND:
		signal = BT_DEVICE_FOUND;
		break;
	case BLUETOOTH_EVENT_DISCOVERY_FINISHED:
		signal = BT_DISCOVERY_FINISHED;
		break;
	case BLUETOOTH_EVENT_LE_DISCOVERY_STARTED:
		signal = BT_LE_DISCOVERY_STARTED;
		break;
	case BLUETOOTH_EVENT_REMOTE_LE_DEVICE_FOUND:
		signal = BT_LE_DEVICE_FOUND;
		break;
	case BLUETOOTH_EVENT_LE_DISCOVERY_FINISHED:
		signal = BT_LE_DISCOVERY_FINISHED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_STARTED:
		signal = BT_ADVERTISING_STARTED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_STOPPED:
		signal = BT_ADVERTISING_STOPPED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_MANUFACTURER_DATA_CHANGED:
		signal = BT_ADVERTISING_MANUFACTURER_DATA_CHANGED;
		break;
	case BLUETOOTH_EVENT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED:
		signal = BT_SCAN_RESPONSE_MANUFACTURER_DATA_CHANGED;
		break;
	case BLUETOOTH_EVENT_MANUFACTURER_DATA_CHANGED:
		signal = BT_MANUFACTURER_DATA_CHANGED;
		break;
	case BLUETOOTH_EVENT_BONDING_FINISHED:
		signal = BT_BOND_CREATED;
		break;
	case BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED:
		signal = BT_BOND_DESTROYED;
		break;
	case BLUETOOTH_EVENT_DEVICE_AUTHORIZED:
		signal = BT_DEVICE_AUTHORIZED;
		break;
	case BLUETOOTH_EVENT_DEVICE_UNAUTHORIZED:
		signal = BT_DEVICE_UNAUTHORIZED;
		break;
	case BLUETOOTH_EVENT_RSSI_ENABLED:
		signal = BT_RSSI_MONITORING_ENABLED;
		break;
	case BLUETOOTH_EVENT_RSSI_ALERT:
		signal = BT_RSSI_ALERT;
		break;
	case BLUETOOTH_EVENT_RAW_RSSI:
		signal = BT_RAW_RSSI_EVENT;
		break;
	case BLUETOOTH_EVENT_KEYBOARD_PASSKEY_DISPLAY:
		signal = BT_KBD_PASSKEY_DISPLAY_REQ_RECEIVED;
		break;
	case BLUETOOTH_EVENT_PIN_REQUEST:
		signal = BT_PIN_REQ_RECEIVED;
		break;
	case BLUETOOTH_EVENT_PASSKEY_REQUEST:
		signal = BT_PASSKEY_REQ_RECEIVED;
		break;
	case BLUETOOTH_EVENT_PASSKEY_CONFIRM_REQUEST:
		signal = BT_PASSKEY_CFM_REQ_RECEIVED;
		break;
	case BLUETOOTH_EVENT_SERVICE_SEARCHED:
		signal = BT_SERVICE_SEARCHED;
		break;
	case BLUETOOTH_HID_CONNECTED:
		signal = BT_INPUT_CONNECTED;
		BT_INFO_C("Connected [HID]");
		break;
	case BLUETOOTH_HID_DISCONNECTED:
		signal = BT_INPUT_DISCONNECTED;
		BT_INFO_C("Disconnected [HID]");
		break;
	case BLUETOOTH_PBAP_CONNECTED:
		signal = BT_PBAP_CONNECTED;
		BT_INFO_C("Connected [PBAP Client]");
		break;
	case BLUETOOTH_PBAP_DISCONNECTED:
		signal = BT_PBAP_DISCONNECTED;
		BT_INFO_C("Disconnected [PBAP Client]");
		break;
	case BLUETOOTH_PBAP_PHONEBOOK_SIZE:
		signal = BT_PBAP_PHONEBOOK_SIZE;
		break;
	case BLUETOOTH_PBAP_PHONEBOOK_PULL:
		signal = BT_PBAP_PHONEBOOK_PULL;
		break;
	case BLUETOOTH_PBAP_VCARD_LIST:
		signal = BT_PBAP_VCARD_LIST;
		break;
	case BLUETOOTH_PBAP_VCARD_PULL:
		signal = BT_PBAP_VCARD_PULL;
		break;
	case BLUETOOTH_PBAP_PHONEBOOK_SEARCH:
		signal = BT_PBAP_SEARCH_PHONEBOOK;
		break;
	case BLUETOOTH_EVENT_AG_CONNECTED:
		signal = BT_HEADSET_CONNECTED;
		BT_INFO_C("Connected [HSP/HFP]");
		break;
	case BLUETOOTH_EVENT_AG_DISCONNECTED:
		signal = BT_HEADSET_DISCONNECTED;
		BT_INFO_C("Disconnected [HSP/HFP]");
		break;
	case BLUETOOTH_EVENT_AV_CONNECTED:
		signal = BT_STEREO_HEADSET_CONNECTED;
		BT_INFO_C("Connected [A2DP]");
		break;
	case BLUETOOTH_EVENT_AV_DISCONNECTED:
		signal = BT_STEREO_HEADSET_DISCONNECTED;
		BT_INFO_C("Disconnected [A2DP]");
		break;
	case BLUETOOTH_EVENT_AG_AUDIO_CONNECTED:
		signal = BT_SCO_CONNECTED;
		BT_INFO_C("Connected [SCO]");
		break;
	case BLUETOOTH_EVENT_AG_AUDIO_DISCONNECTED:
		signal = BT_SCO_DISCONNECTED;
		BT_INFO_C("Disonnected [SCO]");
		break;
	case BLUETOOTH_EVENT_AG_SPEAKER_GAIN:
		signal = BT_SPEAKER_GAIN;
		break;
	case BLUETOOTH_EVENT_AG_MIC_GAIN:
		signal = BT_MICROPHONE_GAIN;
		break;
	case BLUETOOTH_EVENT_NETWORK_CONNECTED:
		signal = BT_NETWORK_CONNECTED;
		BT_INFO_C("Connected [Newwork]");
		break;
	case BLUETOOTH_EVENT_NETWORK_DISCONNECTED:
		signal = BT_NETWORK_DISCONNECTED;
		BT_INFO_C("Disconnected [Newwork]");
		break;
	case BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED:
		signal = BT_NETWORK_SERVER_CONNECTED;
		BT_INFO_C("Connected [Network Server]");
		break;
	case BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED:
		signal = BT_NETWORK_SERVER_DISCONNECTED;
		BT_INFO_C("Disconnected [Network Server]");
		break;
	case BLUETOOTH_EVENT_OPC_CONNECTED:
		signal = BT_OPP_CONNECTED;
		BT_INFO_C("Connected [OPP]");
		break;
	case BLUETOOTH_EVENT_OPC_DISCONNECTED:
		signal = BT_OPP_DISCONNECTED;
		BT_INFO_C("Disconnected [OPP]");
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_CONNECTED:
		signal = BT_TRANSFER_CONNECTED;
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_DISCONNECTED:
		signal = BT_TRANSFER_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_OPC_TRANSFER_STARTED:
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED:
		signal = BT_TRANSFER_STARTED;
		break;
	case BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS:
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS:
		signal = BT_TRANSFER_PROGRESS;
		break;
	case BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE:
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED:
		signal = BT_TRANSFER_COMPLETED;
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE:
		signal = BT_TRANSFER_AUTHORIZED;
		break;
	case BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE:
	case BLUETOOTH_EVENT_RFCOMM_AUTHORIZE:
		signal = BT_CONNECTION_AUTHORIZED;
		break;
	case BLUETOOTH_EVENT_RFCOMM_CONNECTED:
		signal = BT_RFCOMM_CONNECTED;
		break;
	case BLUETOOTH_EVENT_RFCOMM_DISCONNECTED:
		signal = BT_RFCOMM_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED:
		signal = BT_RFCOMM_DATA_RECEIVED;
		break;
	case BLUETOOTH_EVENT_RFCOMM_SERVER_REMOVED:
		signal = BT_RFCOMM_SERVER_REMOVED;
		break;
	case BLUETOOTH_EVENT_DEVICE_CONNECTED:
		signal = BT_DEVICE_CONNECTED;
		break;
	case BLUETOOTH_EVENT_DEVICE_DISCONNECTED:
		signal = BT_DEVICE_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_AV_SOURCE_CONNECTED:
		signal = BT_A2DP_SOURCE_CONNECTED;
		BT_INFO_C("Connected [A2DP Source]");
		break;
    case BLUETOOTH_EVENT_AV_SOURCE_DISCONNECTED:
        signal = BT_A2DP_SOURCE_DISCONNECTED;
        BT_INFO_C("Disconnected [A2DP Source]");
        break;
	case BLUETOOTH_EVENT_AVRCP_CONNECTED:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_CONNECTED:
		signal = BT_AVRCP_CONNECTED;
		BT_INFO_C("Connected [AVRCP]");
		break;
	case BLUETOOTH_EVENT_AVRCP_DISCONNECTED:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_DISCONNECTED:
		signal = BT_AVRCP_DISCONNECTED;
		BT_INFO_C("Disconnected [AVRCP]");
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_SHUFFLE_STATUS:
		signal = BT_MEDIA_SHUFFLE_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_EQUALIZER_STATUS:
		signal = BT_MEDIA_EQUALIZER_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_REPEAT_STATUS:
		signal = BT_MEDIA_REPEAT_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS:
	case BLUETOOTH_EVENT_AVRCP_CONTROL_SCAN_STATUS:
		signal = BT_MEDIA_SCAN_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SONG_POSITION_STATUS:
		signal = BT_MEDIA_POSITION_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_PLAY_STATUS_CHANGED:
		signal = BT_MEDIA_PLAY_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_TRACK_CHANGED:
		signal = BT_MEDIA_TRACK_CHANGE;
		break;
	case BLUETOOTH_EVENT_GATT_CONNECTED:
		signal = BT_GATT_CONNECTED;
		break;
	case BLUETOOTH_EVENT_GATT_DISCONNECTED:
		signal = BT_GATT_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_IPSP_INIT_STATE_CHANGED:
		signal = BT_IPSP_INITIALIZED;
		break;
	case BLUETOOTH_EVENT_IPSP_CONNECTED:
		signal = BT_IPSP_CONNECTED;
		break;
	case BLUETOOTH_EVENT_IPSP_DISCONNECTED:
		signal = BT_IPSP_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_LE_DATA_LENGTH_CHANGED:
		signal = BT_LE_DATA_LENGTH_CHANGED;
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_DBG("Path : %s", path);
	BT_INFO_C("Signal : %s", signal);

	msg1 = g_dbus_message_new_signal(path, BT_EVENT_SERVICE, signal);
	g_dbus_message_set_body(msg1, param);
	if (!g_dbus_connection_send_message(event_conn, msg1,G_DBUS_SEND_MESSAGE_FLAGS_NONE, 0, NULL)) {
		BT_ERR("Error while sending");
	}

	g_object_unref(msg1);

#ifdef HPS_FEATURE
	if (g_strcmp0(signal, BT_LE_ENABLED) == 0)
		_bt_send_to_hps();
#endif

	return BLUETOOTH_ERROR_NONE;
}

int _bt_send_event_to_dest(const char* dest, int event_type,
		int event, GVariant *param)
{
	BT_DBG("+");
	char *path;
	char *signal;
	GError *error = NULL;

	retv_if(event_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_DBG("dest : %s", dest);
	BT_DBG("event_type [%d], event [%d]", event_type, event);

	switch (event_type) {
	case BT_ADAPTER_EVENT:
		path = BT_ADAPTER_PATH;
		break;
	case BT_LE_ADAPTER_EVENT:
		path = BT_LE_ADAPTER_PATH;
		break;
	case BT_DEVICE_EVENT:
                path = BT_DEVICE_PATH;
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	switch (event) {
	case BLUETOOTH_EVENT_ADVERTISING_STARTED:
		signal = BT_ADVERTISING_STARTED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_STOPPED:
		signal = BT_ADVERTISING_STOPPED;
		break;
	case BLUETOOTH_EVENT_LE_DISCOVERY_STARTED:
		signal = BT_LE_DISCOVERY_STARTED;
		break;
	case BLUETOOTH_EVENT_REMOTE_LE_DEVICE_FOUND:
		signal = BT_LE_DEVICE_FOUND;
		break;
	case BLUETOOTH_EVENT_LE_DISCOVERY_FINISHED:
		signal = BT_LE_DISCOVERY_FINISHED;
		break;
	case BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED:
		signal = BT_GATT_CHAR_VAL_CHANGED;
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_DBG("Path : %s", path);
	BT_INFO_C("Signal : %s", signal);

	if (!g_dbus_connection_emit_signal(event_conn, dest, path, BT_EVENT_SERVICE,
			signal, param, &error)) {
		BT_ERR("Error while sending Signal: %s", signal);
		if (error) {
			BT_ERR("Error Code [%d], Error Message [%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
	}

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_send_hf_local_term_event(char *address)
{
	GError *error = NULL;

	retv_if(hf_local_term_event_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!g_dbus_connection_emit_signal(hf_local_term_event_conn, NULL,
			BT_HF_LOCAL_TERM_EVENT_PATH,
			BT_HF_LOCAL_TERM_EVENT_INTERFACE,
			BT_HF_LOCAL_TERM, g_variant_new("s", address),
			&error)) {
		BT_ERR("Error while sending Signal: %s", signal);
		if (error) {
			BT_ERR("Error Code [%d], Error Message [%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
	}

	return BLUETOOTH_ERROR_NONE;
}

/* To send the event from service daemon to application*/
int _bt_init_service_event_sender(void)
{
	GDBusConnection *conn;
	GError *err = NULL;

	if (event_conn) {
		BT_ERR("Event handler is already exist");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	if (conn == NULL) {
		BT_ERR("conn == NULL");
		if (err) {
			BT_ERR("Code[%d], Message[%s]",
					err->code, err->message);
			g_clear_error(&err);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	event_conn = conn;
	return BLUETOOTH_ERROR_NONE;
}

void _bt_deinit_service_event_sender(void)
{
	if (event_conn) {
		g_object_unref(event_conn);
		event_conn = NULL;
	}
}

int _bt_init_hf_local_term_event_sender(void)
{
	GDBusConnection *conn;
	GError *err = NULL;

	if (hf_local_term_event_conn) {
		BT_ERR("Event handler is already exist");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	if (conn == NULL) {
		BT_ERR("conn == NULL");
		if (err) {
			BT_ERR("Code[%d], Message[%s]",
					err->code, err->message);
			g_clear_error(&err);
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	hf_local_term_event_conn = conn;

	return BLUETOOTH_ERROR_NONE;
}

void _bt_deinit_hf_local_term_event_sender(void)
{
	if (hf_local_term_event_conn) {
		g_object_unref(hf_local_term_event_conn);
		hf_local_term_event_conn = NULL;
	}
}
