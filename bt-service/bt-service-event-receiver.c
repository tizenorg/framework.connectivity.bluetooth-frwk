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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <string.h>
#include <dlog.h>
#include <vconf.h>
#include <vconf-internal-bt-keys.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-adapter.h"
#include "bt-service-device.h"
#include "bt-service-obex-server.h"
#include "bt-service-rfcomm-server.h"

static DBusGConnection *manager_conn;
static DBusGConnection *obexd_conn;

static guint event_id;

static gboolean __bt_parse_device_properties(DBusMessageIter *item_iter,
						bt_remote_dev_info_t *dev_info)
{
	DBusMessageIter value_iter;
	char *value;

	if (dbus_message_iter_get_arg_type(item_iter) != DBUS_TYPE_ARRAY)
		return FALSE;

	dbus_message_iter_recurse(item_iter, &value_iter);

	while (dbus_message_iter_get_arg_type(&value_iter) ==
						DBUS_TYPE_DICT_ENTRY) {
		char *key;
		DBusMessageIter dict_entry;
		DBusMessageIter iter_dict_val;

		dbus_message_iter_recurse(&value_iter, &dict_entry);

		dbus_message_iter_get_basic(&dict_entry, &key);
		if (key == NULL) {
			dbus_message_iter_next(&value_iter);
			continue;
		}

		if (!dbus_message_iter_next(&dict_entry)) {
			dbus_message_iter_next(&value_iter);
			continue;
		}
		dbus_message_iter_recurse(&dict_entry, &iter_dict_val);
		if (strcasecmp(key, "Class") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val, &dev_info->class);
		} else if (strcasecmp(key, "name") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val, &value);
			if (dev_info->name == NULL)
				dev_info->name = g_strdup(value);
		} else if (strcasecmp(key, "Connected") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->connected);
		} else if (strcasecmp(key, "paired") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->paired);
		} else if (strcasecmp(key, "Trusted") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->trust);
		} else if (strcasecmp(key, "RSSI") == 0) {
			dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->rssi);
		} else if (strcasecmp(key, "UUIDs") == 0) {
			DBusMessageIter uuid_iter;
			DBusMessageIter tmp_iter;
			int i = 0;

			dbus_message_iter_recurse(&iter_dict_val, &uuid_iter);

			tmp_iter = uuid_iter;

			/* Store the uuid count */
			while (dbus_message_iter_get_arg_type(&tmp_iter) != DBUS_TYPE_INVALID) {
				dbus_message_iter_get_basic(&tmp_iter,
							&value);

				dev_info->uuid_count++;
				if (!dbus_message_iter_next(&tmp_iter))
					break;
			}

			/* Store the uuids */
			if (dev_info->uuid_count > 0) {
				dev_info->uuids = g_new0(char *,
						dev_info->uuid_count + 1);
			} else {
				continue;
			}

			while (dbus_message_iter_get_arg_type(&uuid_iter) != DBUS_TYPE_INVALID) {
				dbus_message_iter_get_basic(&uuid_iter,
							&value);
				dev_info->uuids[i] = g_strdup(value);
				i++;
				if (!dbus_message_iter_next(&uuid_iter)) {
					break;
				}
			}

		}

		dbus_message_iter_next(&value_iter);
	}

	return TRUE;
}

char *__bt_get_headset_name(char *address)
{
	bluetooth_device_address_t device_address = { {0} };
	bluetooth_device_info_t dev_info;

	retv_if(address == NULL, strdup(""));

	_bt_convert_addr_string_to_type(device_address.addr, address);

	memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));

	_bt_get_bonded_device_info(&device_address, &dev_info);

	return g_strdup(dev_info.device_name.name);
}

static int __bt_get_owner_info(DBusMessage *msg, char **name,
				char **previous, char **current)
{
	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, name);

	retv_if(*name == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, previous);

	retv_if(*previous == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, current);

	retv_if(*current == NULL, BLUETOOTH_ERROR_INTERNAL);

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_get_agent_signal_info(DBusMessage *msg, char **address,
				char **name, char **uuid)
{
	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, address);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, name);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, uuid);

	return BLUETOOTH_ERROR_NONE;
}

gboolean _bt_discovery_finished_cb(gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	event_id = 0;

	if (_bt_get_discoverying_property() == FALSE) {
		if (_bt_get_cancel_by_user() == TRUE) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		}

		_bt_set_cancel_by_user(FALSE);
		_bt_set_discovery_status(FALSE);
		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_DISCOVERY_FINISHED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID);
	}

	return FALSE;
}

void _bt_handle_adapter_event(DBusMessage *msg)
{
	int mode = 0;
	int result = BLUETOOTH_ERROR_NONE;
	DBusGProxy *adapter_proxy;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	GValue timeout = { 0 };
	const char *member = dbus_message_get_member(msg);
	const char *property = NULL;

	ret_if(member == NULL);

	if (strcasecmp(member, "PropertyChanged") == 0) {
		dbus_message_iter_init(msg, &item_iter);

		if (dbus_message_iter_get_arg_type(&item_iter)
						!= DBUS_TYPE_STRING) {
			BT_ERR("This is bad format dbus\n");
			return;
		}

		dbus_message_iter_get_basic(&item_iter, &property);

		ret_if(property == NULL);

		if (strcasecmp(property, "Discovering") == 0) {
			gboolean discovering = FALSE;
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &discovering);

			/* Send event to application */
			if (discovering == TRUE) {
				_bt_set_discovery_status(TRUE);
				_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_DISCOVERY_STARTED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_INVALID);
			} else {
				ret_if(event_id > 0);

				adapter_proxy = _bt_get_adapter_proxy();
				ret_if(adapter_proxy == NULL);

				/* Need to stop searching */
				dbus_g_proxy_call(adapter_proxy,
							"StopDiscovery",
							NULL,
							G_TYPE_INVALID,
							G_TYPE_INVALID);

				event_id = g_timeout_add(BT_DISCOVERY_FINISHED_DELAY,
					      (GSourceFunc)_bt_discovery_finished_cb, NULL);
			}
		} else if (strcasecmp(property, "Name") == 0) {
			char *name = NULL;
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &name);

			/* Send event to application */
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &name,
					DBUS_TYPE_INVALID);
		} else if (strcasecmp(property, "Discoverable") == 0) {
			gboolean discoverable = FALSE;
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &discoverable);

			if (discoverable == FALSE) {
				if (_bt_get_discoverable_timeout_property() > 0) {
					g_value_init(&timeout, G_TYPE_UINT);
					g_value_set_uint(&timeout, 0);

					adapter_proxy = _bt_get_adapter_proxy();
					ret_if(adapter_proxy == NULL);

					dbus_g_proxy_call_no_reply(adapter_proxy, "SetProperty",
								   G_TYPE_STRING, "DiscoverableTimeout",
								   G_TYPE_VALUE, &timeout,
								   G_TYPE_INVALID);

					g_value_unset(&timeout);
				}

				mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;

				/* Send event to application */
				_bt_send_event(BT_ADAPTER_EVENT,
						BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
						DBUS_TYPE_INT32, &result,
						DBUS_TYPE_INT16, &mode,
						DBUS_TYPE_INVALID);
			} else {
				_bt_get_discoverable_mode(&mode);

				/* Event will be sent by "DiscoverableTimeout" signal */
				ret_if(mode != BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE);

				/* Send event to application */
				_bt_send_event(BT_ADAPTER_EVENT,
						BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
						DBUS_TYPE_INT32, &result,
						DBUS_TYPE_INT16, &mode,
						DBUS_TYPE_INVALID);
			}
		} else if (strcasecmp(property, "DiscoverableTimeout") == 0) {
			_bt_get_discoverable_mode(&mode);

			/* Event was already sent by "Discoverable" signal */
			ret_if(mode == BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE);

			/* Send event to application */
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_INT16, &mode,
					DBUS_TYPE_INVALID);
		}
	} else if (strcasecmp(member, "DeviceFound") == 0) {
		const char *bdaddr;
		bt_remote_dev_info_t *dev_info;

		ret_if(_bt_is_discovering() == FALSE);

		dev_info = g_malloc0(sizeof(bt_remote_dev_info_t));

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &bdaddr);
		dbus_message_iter_next(&item_iter);

		dev_info->address = g_strdup(bdaddr);

		if (__bt_parse_device_properties(&item_iter, dev_info) == FALSE) {
			BT_ERR("Fail to parse the properies");
			_bt_free_device_info(dev_info);
			return;
		}

		if (dev_info->name == NULL)
			dev_info->name = g_strdup("");

		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &dev_info->address,
			DBUS_TYPE_UINT32, &dev_info->class,
			DBUS_TYPE_INT16, &dev_info->rssi,
			DBUS_TYPE_STRING, &dev_info->name,
			DBUS_TYPE_BOOLEAN, &dev_info->paired,
			DBUS_TYPE_BOOLEAN, &dev_info->connected,
			DBUS_TYPE_BOOLEAN, &dev_info->trust,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&dev_info->uuids, dev_info->uuid_count,
			DBUS_TYPE_INVALID);

		_bt_free_device_info(dev_info);
	} else if (strcasecmp(member, "DeviceCreated") == 0) {
		const char *object_path = NULL;
		char *address;
		bt_remote_dev_info_t *remote_dev_info;

		ret_if(_bt_is_device_creating() == FALSE);

		/* Bonding from remote device */
		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &object_path);
		dbus_message_iter_next(&item_iter);

		_bt_convert_device_path_to_address(object_path, address);

		remote_dev_info = _bt_get_remote_device_info(address);
		if (remote_dev_info == NULL) {
			g_free(address);
			return;
		}

		_bt_free_device_info(remote_dev_info);
		g_free(address);
	} else if (strcasecmp(member, "DeviceRemoved") == 0) {
		const char *object_path = NULL;
		char *address;

		/* Bonding from remote device */
		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &object_path);
		dbus_message_iter_next(&item_iter);

		_bt_convert_device_path_to_address(object_path, address);

		_bt_send_event(BT_ADAPTER_EVENT,
			BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
}

void _bt_handle_input_event(DBusMessage *msg)
{
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	gboolean property_flag = FALSE;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		char *address;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		event = (property_flag == TRUE) ?
				BLUETOOTH_HID_CONNECTED :
				BLUETOOTH_HID_DISCONNECTED;

		_bt_send_event(BT_HID_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
 }

void _bt_handle_network_server_event(DBusMessage *msg)
{
	int result = BLUETOOTH_ERROR_NONE;
	char *address = NULL;
	char *device = NULL;
	const char *member = dbus_message_get_member(msg);

	ret_if(member == NULL);

	if (strcasecmp(member, "PeerConnected") == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return;
		}

		_bt_send_event(BT_NETWORK_EVENT, BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);
	} else if (strcasecmp(member, "PeerDisconnected") == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return;
		}

		_bt_send_event(BT_NETWORK_EVENT, BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);
	}
}

void _bt_handle_network_client_event(DBusMessage *msg)
{
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	gboolean property_flag = FALSE;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		char *address;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		if (property_flag == TRUE) {
			event = BLUETOOTH_EVENT_NETWORK_CONNECTED;
		} else {
			event = BLUETOOTH_EVENT_NETWORK_DISCONNECTED;
		}

		_bt_send_event(BT_NETWORK_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
}

void _bt_handle_device_event(DBusMessage *msg)
{
	int event;
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	char *address;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(path == NULL);
	ret_if(member == NULL);

	if (strcasecmp(member, "PropertyChanged") == 0) {
		dbus_message_iter_init(msg, &item_iter);

		if (dbus_message_iter_get_arg_type(&item_iter)
						!= DBUS_TYPE_STRING) {
			BT_ERR("This is bad format dbus\n");
			return;
		}

		dbus_message_iter_get_basic(&item_iter, &property);

		ret_if(property == NULL);

		if (strcasecmp(property, "Connected") == 0) {
			gboolean connected = FALSE;
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &connected);

			event = connected ? BLUETOOTH_EVENT_DEVICE_CONNECTED :
					BLUETOOTH_EVENT_DEVICE_DISCONNECTED;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			BT_DBG("connected: %d", connected);
			BT_DBG("address: %s", address);

			/* Send event to application */
			_bt_send_event(BT_DEVICE_EVENT,
					event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

			g_free(address);
		} else if (strcasecmp(property, "Paired") == 0) {
			gboolean paired = FALSE;
			bt_remote_dev_info_t *remote_dev_info;
			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &paired);

			ret_if(paired == FALSE);

			/* BlueZ sends paired signal for each paired device */
			/* during activation, We should ignore this, otherwise*/
			/* application thinks that a new device got paired */
			if (_bt_adapter_get_status() != BT_ACTIVATED) {
				BT_DBG("BT is not activated, so ignore this");
				return;
			}

			if (_bt_is_device_creating() == TRUE) {
				BT_DBG("Try to Pair by me");
				return;
			}

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			remote_dev_info = _bt_get_remote_device_info(address);
			if (remote_dev_info == NULL) {
				g_free(address);
				return;
			}

			_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_BONDING_FINISHED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_UINT32, &remote_dev_info->class,
				DBUS_TYPE_INT16, &remote_dev_info->rssi,
				DBUS_TYPE_STRING, &remote_dev_info->name,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->connected,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&remote_dev_info->uuids, remote_dev_info->uuid_count,
				DBUS_TYPE_INVALID);

			_bt_free_device_info(remote_dev_info);
			g_free(address);
		}
	}
}

void __bt_set_audio_values(gboolean connected, char *address)
{
	char *name = NULL;
	int bt_device_state = VCONFKEY_BT_DEVICE_NONE;

	/*  Set the headset name */
	if (connected == TRUE) {
		name = __bt_get_headset_name(address);
	} else {
		name = g_strdup("");
	}

	if (vconf_set_str(VCONFKEY_BT_HEADSET_NAME,
					name) != 0) {
		BT_ERR("vconf_set_str failed");
	}

	g_free(name);

	/*  Set the headset state */
	if (vconf_get_int(VCONFKEY_BT_DEVICE,
				&bt_device_state) != 0) {
		BT_ERR("vconf_get_str failed");
	}

	if (connected == TRUE) {
		bt_device_state |= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	} else if (bt_device_state & VCONFKEY_BT_DEVICE_HEADSET_CONNECTED) {
		bt_device_state ^= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	}

	if (vconf_set_int(VCONFKEY_BT_DEVICE,
				bt_device_state) != 0) {
		BT_ERR("vconf_set_int failed");
	}
}

void _bt_handle_headset_event(DBusMessage *msg)
{
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	gboolean property_flag = FALSE;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	/* We allow only 1 headset connection (HSP or HFP)*/
	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		char *address;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		if (property_flag == TRUE) {
			event = BLUETOOTH_EVENT_AG_CONNECTED;
		} else {
			event = BLUETOOTH_EVENT_AG_DISCONNECTED;
		}

		__bt_set_audio_values(property_flag, address);

		_bt_send_event(BT_HEADSET_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	} else if (strcasecmp(property, "State") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		int sco_connected = FALSE;
		char *state = NULL;
		char *address;

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &state);

		/* This code assumes we support only 1 headset connection */
		/* Need to use the headset list, if we support multi-headsets */
		if (strcasecmp(property, "Playing") == 0) {
			event = BLUETOOTH_EVENT_AG_AUDIO_CONNECTED;
			sco_connected = TRUE;
		} else if (strcasecmp(property, "connected") == 0 ||
			    strcasecmp(property, "disconnected") == 0) {
			event = BLUETOOTH_EVENT_AG_AUDIO_DISCONNECTED;
			sco_connected = FALSE;
		} else {
			BT_ERR("Not handled state");
			g_free(address);
			return;
		}

		if (vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, sco_connected) < 0)
			BT_ERR("vconf_set_bool - Failed\n");

		_bt_send_event(BT_HEADSET_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	} else if (strcasecmp(property, "SpeakerGain") == 0) {
		guint16 spkr_gain;
		char *address;

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &spkr_gain);

		_bt_send_event(BT_HEADSET_EVENT, BLUETOOTH_EVENT_AG_SPEAKER_GAIN,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_UINT16, &spkr_gain,
			DBUS_TYPE_INVALID);

		g_free(address);
	} else if (strcasecmp(property, "MicrophoneGain") == 0) {
		guint16 mic_gain;
		char *address;

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &mic_gain);

		_bt_send_event(BT_HEADSET_EVENT, BLUETOOTH_EVENT_AG_MIC_GAIN,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_UINT16, &mic_gain,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
}

void _bt_handle_sink_event(DBusMessage *msg)
{
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	gboolean property_flag = FALSE;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		char *address;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		event = (property_flag == TRUE) ?
				BLUETOOTH_EVENT_AV_CONNECTED :
				BLUETOOTH_EVENT_AV_DISCONNECTED;

		_bt_send_event(BT_HEADSET_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		_bt_send_event(BT_AVRCP_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
}

void _bt_handle_agent_event(DBusMessage *msg)
{
	const char *member = dbus_message_get_member(msg);
	int result = BLUETOOTH_ERROR_NONE;
	char *address = NULL;
	char *name = NULL;
	char *uuid = NULL;

	ret_if(member == NULL);

	if (strcasecmp(member, "ObexAuthorize") == 0) {
		__bt_get_agent_signal_info(msg, &address, &name, &uuid);

		_bt_send_event(BT_OPP_SERVER_EVENT,
			BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INVALID);
	} else if (strcasecmp(member, "RfcommAuthorize") == 0) {
		bt_rfcomm_server_info_t *server_info;

		__bt_get_agent_signal_info(msg, &address, &name, &uuid);

		server_info = _bt_rfcomm_get_server_info_using_uuid(uuid);
		ret_if(server_info == NULL);
		ret_if(server_info->server_type != BT_CUSTOM_SERVER);

		_bt_send_event(BT_RFCOMM_SERVER_EVENT,
			BLUETOOTH_EVENT_RFCOMM_AUTHORIZE,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_STRING, &uuid,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_INT16, &server_info->control_fd,
			DBUS_TYPE_INVALID);
	}
}

static DBusHandlerResult __bt_manager_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, "AdapterAdded") == 0) {
		BT_DBG("AdapterAdded");
		_bt_handle_adapter_added();
	} else if (strcasecmp(member, "AdapterRemoved") == 0) {
		BT_DBG("AdapterRemoved");
	} else if (strcasecmp(member, "NameOwnerChanged") == 0) {
		gboolean value;
		char *name = NULL;
		char *previous = NULL;
		char *current = NULL;

		if (__bt_get_owner_info(msg, &name, &previous, &current)) {
			BT_ERR("Fail to get the owner info");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (*current != '\0')
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (strcasecmp(name, "org.bluez") == 0) {
			BT_DBG("Bluetoothd is terminated");
			_bt_handle_adapter_removed();
		}

		_bt_obex_server_check_allocation(&value);

		if (value == TRUE) {
			/* Check if the obex server was terminated abnormally */
			_bt_obex_server_check_termination(name);
		}

		_bt_rfcomm_server_check_existence(&value);

		if (value == TRUE) {
			/* The obex server was terminated abnormally */
			_bt_rfcomm_server_check_termination(name);
		}
	} else  if (dbus_message_has_interface(msg, BT_ADAPTER_INTERFACE)) {
		_bt_handle_adapter_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_INPUT_INTERFACE)) {
		_bt_handle_input_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_NETWORK_SERVER_INTERFACE)) {
		_bt_handle_network_server_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_NETWORK_CLIENT_INTERFACE)) {
		_bt_handle_network_client_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_HEADSET_INTERFACE)) {
		_bt_handle_headset_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_SINK_INTERFACE)) {
		_bt_handle_sink_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_AGENT_INTERFACE)) {
		_bt_handle_agent_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_DEVICE_INTERFACE)) {
		_bt_handle_device_event(msg);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult __bt_obexd_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	const char *path = dbus_message_get_path(msg);
	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, "TransferStarted") == 0) {
		char *transfer_path = NULL;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_OBJECT_PATH, &transfer_path,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_obex_transfer_started(transfer_path);
	} else if (strcasecmp(member, "Progress") == 0) {
		gint total = 0;
		gint transfer = 0;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_INT32, &total,
			DBUS_TYPE_INT32, &transfer,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_obex_transfer_progress(path, total, transfer);
	} else if (strcasecmp(member, "TransferCompleted") == 0) {
		char *transfer_path = NULL;
		gboolean success;

		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_OBJECT_PATH, &transfer_path,
			DBUS_TYPE_BOOLEAN, &success,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_obex_transfer_completed(transfer_path, success);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int _bt_register_service_event(DBusGConnection *g_conn, int event_type)
{
	DBusError dbus_error;
	char *match1 = NULL;
	char *match2 = NULL;
	DBusConnection *conn;
	DBusHandleMessageFunction event_func = NULL;

	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = dbus_g_connection_get_connection(g_conn);
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	switch (event_type) {
	case BT_MANAGER_EVENT:
		event_func = __bt_manager_event_filter;
		match1 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_MANAGER_PATH);

		match2 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_FREEDESKTOP_INTERFACE,
					BT_FREEDESKTOP_PATH);
		break;
	case BT_DEVICE_EVENT:
		match1 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_DEVICE_INTERFACE);
		break;
	case BT_HID_EVENT:
		match1 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_INPUT_INTERFACE);
		break;
	case BT_NETWORK_EVENT:
		match1 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_NETWORK_SERVER_INTERFACE);

		match2 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_NETWORK_CLIENT_INTERFACE);
		break;
	case BT_HEADSET_EVENT:
		match1 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_HEADSET_INTERFACE);

		match2 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_SINK_INTERFACE);
		break;
	case BT_OPP_SERVER_EVENT:
		event_func = __bt_obexd_event_filter;
		match1 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_OBEXD_MANAGER_INTERFACE,
					BT_MANAGER_PATH);

		match2 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_OBEXD_TRANSFER_INTERFACE);
		break;
	default:
		BT_ERR("Unknown event");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (event_func) {
		if (!dbus_connection_add_filter(conn, event_func,
						NULL, NULL)) {
			BT_ERR("Fail to add filter");
			goto fail;
		}
	}

	dbus_error_init(&dbus_error);

	if (match1)
		dbus_bus_add_match(conn, match1, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	if (match2)
		dbus_bus_add_match(conn, match2, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	g_free(match1);
	g_free(match2);

	return BLUETOOTH_ERROR_NONE;
fail:
	g_free(match1);
	g_free(match2);
	return BLUETOOTH_ERROR_INTERNAL;
}

void _bt_unregister_service_event(DBusGConnection *g_conn, int event_type)
{
	DBusConnection *conn;
	DBusHandleMessageFunction event_func;

	ret_if(g_conn == NULL);
	conn = dbus_g_connection_get_connection(g_conn);

	switch (event_type) {
	case BT_MANAGER_EVENT:
		event_func = __bt_manager_event_filter;
		break;
	case BT_OPP_SERVER_EVENT:
		event_func = __bt_obexd_event_filter;
		break;
	default:
		BT_ERR("Unknown event");
		return;
	}

	ret_if(conn == NULL);

	dbus_connection_remove_filter(conn, event_func, NULL);
}

static int __bt_init_manager_receiver(void)
{
	GError *error = NULL;

	if (manager_conn == NULL) {
		manager_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
		if (error != NULL) {
			BT_ERR("ERROR: Can't get on system bus [%s]", error->message);
			g_error_free(error);
		}
		retv_if(manager_conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	}

	if (_bt_register_service_event(manager_conn,
				BT_MANAGER_EVENT) != BLUETOOTH_ERROR_NONE)
		goto fail;

	if (_bt_register_service_event(manager_conn,
				BT_DEVICE_EVENT) != BLUETOOTH_ERROR_NONE)
		goto fail;

	if (_bt_register_service_event(manager_conn,
				BT_HID_EVENT) != BLUETOOTH_ERROR_NONE)
		goto fail;

	if (_bt_register_service_event(manager_conn,
				BT_HEADSET_EVENT) != BLUETOOTH_ERROR_NONE)
		goto fail;

	if (_bt_register_service_event(manager_conn,
				BT_NETWORK_EVENT) != BLUETOOTH_ERROR_NONE)
		goto fail;

	return BLUETOOTH_ERROR_NONE;
fail:
	if (manager_conn) {
		dbus_g_connection_unref(manager_conn);
		manager_conn = NULL;
	}

	return BLUETOOTH_ERROR_INTERNAL;
}

static int __bt_init_obexd_receiver(void)
{
	GError *error = NULL;

	if (obexd_conn == NULL) {
		obexd_conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
		if (error != NULL) {
			BT_ERR("ERROR: Can't get on session bus [%s]", error->message);
			g_error_free(error);
		}
		retv_if(obexd_conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	}

	if (_bt_register_service_event(obexd_conn,
				BT_OPP_SERVER_EVENT) != BLUETOOTH_ERROR_NONE) {
		dbus_g_connection_unref(obexd_conn);
		obexd_conn = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

/* To receive the event from bluez */
int _bt_init_service_event_receiver(void)
{
	int result;

	result = __bt_init_manager_receiver();
	retv_if(result != BLUETOOTH_ERROR_NONE, result);

	result = __bt_init_obexd_receiver();
	if (result != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to init obexd receiver");

	return BLUETOOTH_ERROR_NONE;
}

void _bt_deinit_service_event_reciever(void)
{
	_bt_unregister_service_event(manager_conn, BT_MANAGER_EVENT);

	_bt_unregister_service_event(obexd_conn, BT_OPP_SERVER_EVENT);

	if (manager_conn) {
		dbus_g_connection_unref(manager_conn);
		manager_conn = NULL;
	}

	if (obexd_conn) {
		dbus_g_connection_unref(obexd_conn);
		obexd_conn = NULL;
	}

	if (event_id > 0)
		g_source_remove(event_id);
}
