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

#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"

static DBusConnection *event_conn;

int _bt_send_event(int event_type, int event, int type, ...)
{
	DBusMessage *msg;
	char *path;
	char *signal;
	va_list arguments;

	retv_if(event_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	switch (event_type) {
	case BT_ADAPTER_EVENT:
		path = BT_ADAPTER_PATH;
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
	case BT_NETWORK_EVENT:
		path = BT_NETWORK_PATH;
		break;
	case BT_OPP_CLIENT_EVENT:
		path = BT_OPP_CLIENT_PATH;
		break;
	case BT_OPP_SERVER_EVENT:
		path = BT_OPP_SERVER_PATH;
		break;
	case BT_RFCOMM_CLIENT_EVENT:
		path = BT_RFCOMM_CLIENT_PATH;
		break;
	case BT_RFCOMM_SERVER_EVENT:
		path = BT_RFCOMM_SERVER_PATH;
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
	case BLUETOOTH_EVENT_ADVERTISING_STARTED:
		signal = BT_ADVERTISING_STARTED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_STOPPED:
		signal = BT_ADVERTISING_STOPPED;
		break;
	case BLUETOOTH_EVENT_ADVERTISING_MANUFACTURER_DATA_CHANGED:
		signal = BT_ADVERTISING_MANUFACTURER_DATA_CHANGED;
		break;
	case BLUETOOTH_EVENT_BONDING_FINISHED:
		signal = BT_BOND_CREATED;
		break;
	case BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED:
		signal = BT_BOND_DESTROYED;
		break;
	case BLUETOOTH_EVENT_DEVICE_ERROR:
	case BLUETOOTH_EVENT_TX_TIMEOUT_ERROR:
		signal = BT_DEVICE_ERROR;
		break;
	case BLUETOOTH_EVENT_SERVICE_SEARCHED:
		signal = BT_SERVICE_SEARCHED;
		break;
	case BLUETOOTH_HID_CONNECTED:
		signal = BT_INPUT_CONNECTED;
		break;
	case BLUETOOTH_HID_DISCONNECTED:
		signal = BT_INPUT_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_AG_CONNECTED:
		signal = BT_HEADSET_CONNECTED;
		break;
	case BLUETOOTH_EVENT_AG_DISCONNECTED:
		signal = BT_HEADSET_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_AV_CONNECTED:
		signal = BT_STEREO_HEADSET_CONNECTED;
		break;
	case BLUETOOTH_EVENT_AV_DISCONNECTED:
		signal = BT_STEREO_HEADSET_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_AG_AUDIO_CONNECTED:
		signal = BT_SCO_CONNECTED;
		break;
	case BLUETOOTH_EVENT_AG_AUDIO_DISCONNECTED:
		signal = BT_SCO_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_AG_SPEAKER_GAIN:
		signal = BT_SPEAKER_GAIN;
		break;
	case BLUETOOTH_EVENT_AG_MIC_GAIN:
		signal = BT_MICROPHONE_GAIN;
		break;
	case BLUETOOTH_EVENT_NETWORK_CONNECTED:
		signal = BT_NETWORK_CONNECTED;
		break;
	case BLUETOOTH_EVENT_NETWORK_DISCONNECTED:
		signal = BT_NETWORK_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED:
		signal = BT_NETWORK_SERVER_CONNECTED;
		break;
	case BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED:
		signal = BT_NETWORK_SERVER_DISCONNECTED;
		break;
	case BLUETOOTH_EVENT_OPC_CONNECTED:
		signal = BT_OPP_CONNECTED;
		break;
	case BLUETOOTH_EVENT_OPC_DISCONNECTED:
		signal = BT_OPP_DISCONNECTED;
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
	case BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS:
		signal = BT_MEDIA_SHUFFLE_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS:
		signal = BT_MEDIA_EQUALIZER_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS:
		signal = BT_MEDIA_REPEAT_STATUS;
		break;
	case BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS:
		signal = BT_MEDIA_SCAN_STATUS;
		break;
	case BLUETOOTH_EVENT_GATT_CONNECTED:
		signal = BT_GATT_CONNECTED;
		break;
	case BLUETOOTH_EVENT_GATT_DISCONNECTED:
		signal = BT_GATT_DISCONNECTED;
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	msg = dbus_message_new_signal(path, BT_EVENT_SERVICE,
				signal);

	if (msg == NULL) {
		BT_ERR("Message is NULL\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (type) {
		/* Set the arguments of the dbus message */
		va_start(arguments, type);

		if (!dbus_message_append_args_valist(msg, type, arguments)) {
			dbus_message_unref(msg);
			va_end(arguments);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		va_end(arguments);
	}

	if (!dbus_connection_send(event_conn, msg, NULL)) {
		BT_ERR("send failed\n");
		dbus_message_unref(msg);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_connection_flush(event_conn);
	dbus_message_unref(msg);

	return BLUETOOTH_ERROR_NONE;
}


/* To send the event from service daemon to application*/
int _bt_init_service_event_sender(void)
{
	DBusConnection *conn;
	DBusError err;
	int ret;

	if (event_conn) {
		BT_ERR("Event handler is already exist");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, NULL);
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&err);

	ret = dbus_bus_request_name(conn, BT_EVENT_SERVICE,
				DBUS_NAME_FLAG_REPLACE_EXISTING, &err);

	if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		if (dbus_error_is_set(&err) == TRUE) {
			BT_ERR("Event init failed, %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	event_conn = conn;

	return BLUETOOTH_ERROR_NONE;
}

void _bt_deinit_service_event_sender(void)
{
	if (event_conn) {
		dbus_connection_close(event_conn);
		event_conn = NULL;
	}
}

