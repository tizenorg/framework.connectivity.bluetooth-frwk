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
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <string.h>
#include <dlog.h>
#include <vconf.h>
#include <vconf-internal-bt-keys.h>
#include <journal/device.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-adapter.h"
#include "bt-service-adapter-le.h"
#include "bt-service-device.h"
#include "bt-service-avrcp.h"
#include "bt-service-obex-server.h"
#include "bt-service-rfcomm-server.h"
#include "bt-service-audio.h"
#include "bt-service-agent.h"
#include "bt-service-pbap.h"
#include "bt-service-headset-connection.h"
#include "bt-service-avrcp-controller.h"

#include "bt-service-opp-client.h"

static DBusGConnection *manager_conn;
static DBusGConnection *obexd_conn;
static GList *p_cache_list = NULL;
static DBusGConnection *opc_obexd_conn = NULL;


static guint event_id;
guint nap_connected_device_count = 0;
static guint hid_connected_device_count = 0;
static GList *p_adv_ind_list;

typedef struct {
	bt_remote_dev_info_t *dev_info;
} bt_cache_info_t;

/**
 * obexd connection type
 */
typedef enum {
	OBEX_OPP = (1 << 1),
	OBEX_FTP = (1 << 2),
	OBEX_BIP = (1 << 3),
	OBEX_PBAP = (1 << 4),
	OBEX_IRMC = (1 << 5),
	OBEX_PCSUITE = (1 << 6),
	OBEX_SYNCEVOLUTION = 	(1 << 7),
	OBEX_MAS = (1 << 8),
} bluetooth_obex_connection_type_t;

void _bt_handle_property_changed_event(DBusMessage *msg);
void _bt_opc_property_changed_event(DBusMessage *msg);
int _bt_register_service_event(DBusGConnection *g_conn, int event_type);
void _bt_unregister_service_event(DBusGConnection *g_conn, int event_type);
void _bt_opp_client_event_deinit(void);
void _bt_handle_network_client_event(DBusMessageIter *msg_iter,
				const char *path);
void __bt_gatt_char_property_changed_event(DBusMessageIter *msg_iter,
				const char *path);

static void __bt_free_bt_le_adv_info_t(bt_le_adv_info_t *adv_info)
{
	g_free(adv_info->addr);
	g_free(adv_info->data);
	g_free(adv_info);
}

static bt_le_adv_info_t *__bt_get_adv_ind_info(char *addr)
{
	retv_if(!addr, NULL);
	bt_le_adv_info_t *adv_info = NULL;
	GList *current = g_list_first((GList *)p_adv_ind_list);
	while(current && current->data) {
		adv_info = (bt_le_adv_info_t *)current->data;
		retv_if(adv_info && !g_strcmp0(adv_info->addr, addr), adv_info);
		current = g_list_next(current);
	}
	return NULL;
}

static void __bt_add_adv_ind_info(bt_le_adv_info_t *adv_info)
{
	ret_if(!adv_info);
	if (__bt_get_adv_ind_info(adv_info->addr) != NULL) {
		BT_ERR("adv_info is already added");
		__bt_free_bt_le_adv_info_t(adv_info);
		return;
	}
	p_adv_ind_list = g_list_append(p_adv_ind_list, adv_info);
}

static void __bt_del_adv_ind_info(char *addr)
{
	ret_if(!addr);
	ret_if(!p_adv_ind_list);
	bt_le_adv_info_t *adv_info = NULL;
	GList *current = g_list_first((GList *)p_adv_ind_list);
	while(current && current->data) {
		adv_info = (bt_le_adv_info_t *)current->data;
		if (adv_info && !g_strcmp0(adv_info->addr, addr)) {
			p_adv_ind_list = g_list_remove(p_adv_ind_list, adv_info);
			__bt_free_bt_le_adv_info_t(adv_info);
			return;
		}
		current = g_list_next(current);
	}
}

static void __bt_free_cache_info(bt_cache_info_t *cache_info)
{
	ret_if(cache_info == NULL);

	_bt_free_device_info(cache_info->dev_info);
	g_free(cache_info);
}

static gboolean __bt_parse_device_properties(DBusMessageIter *item_iter,
						bt_remote_dev_info_t *dev_info)
{
	BT_DBG("+");
	DBusMessageIter value_iter;
	char *value;

	dbus_message_iter_recurse(item_iter, &value_iter);

	if (dbus_message_iter_get_arg_type(&value_iter) != DBUS_TYPE_DICT_ENTRY) {
		BT_DBG("No entry");
		return FALSE;
	}

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

		if (dev_info) {
			if (strcasecmp(key, "Address") == 0) {
				const char *address = NULL;
				dbus_message_iter_get_basic(&iter_dict_val, &address);
				dev_info->address = g_strdup(address);
			} else if (strcasecmp(key, "Class") == 0) {
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
			} else if (strcasecmp(key, "LastAddrType") == 0) {
				dbus_message_iter_get_basic(&iter_dict_val,
						&dev_info->addr_type);
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
					dbus_message_iter_next(&value_iter);
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
			} else if (strcasecmp(key, "ManufacturerDataLen") == 0) {
				dbus_message_iter_get_basic(&iter_dict_val,
									&dev_info->manufacturer_data_len);

				if (dev_info->manufacturer_data_len > BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX) {
					BT_ERR("manufacturer_data_len is too long(len = %d)", dev_info->manufacturer_data_len);
					dev_info->manufacturer_data_len = BLUETOOTH_MANUFACTURER_DATA_LENGTH_MAX;
				}

				if (dev_info->manufacturer_data_len == 0)
					dev_info->manufacturer_data = g_strdup("");
			} else if (strcasecmp(key, "ManufacturerData") == 0) {
				DBusMessageIter manufacturer_iter;
				int i = 0;
				int len = 0;
				char *manufacturer_data = NULL;
				char byte = 0;
				DBusMessageIter count_iter;
				dbus_message_iter_recurse(&iter_dict_val, &manufacturer_iter);
				dbus_message_iter_recurse(&iter_dict_val, &count_iter);

				while (dbus_message_iter_get_arg_type(&count_iter) == DBUS_TYPE_BYTE) {
					len++;
					dbus_message_iter_next(&count_iter);
				}

				dev_info->manufacturer_data = g_malloc0(len);
				manufacturer_data = dev_info->manufacturer_data;

				while (dbus_message_iter_get_arg_type(&manufacturer_iter) == DBUS_TYPE_BYTE) {
					dbus_message_iter_get_basic(&manufacturer_iter, &byte);
					manufacturer_data[i] = byte;
					i++;
					dbus_message_iter_next(&manufacturer_iter);
				}
			}
		}

		dbus_message_iter_next(&value_iter);
	}

	BT_DBG("-");
	return TRUE;
}

static gboolean __bt_parse_interface(DBusMessage *msg,
					bt_remote_dev_info_t *dev_info)
{
	BT_DBG("+");

	DBusMessageIter msg_iter;
	DBusMessageIter value_iter;
	char *object_path = NULL;

	retv_if(dbus_message_iter_init(msg, &msg_iter) == FALSE, FALSE);

	dbus_message_iter_get_basic(&msg_iter, &object_path);
	retv_if(object_path == NULL, FALSE);

	/* object array (oa) */
	retv_if(dbus_message_iter_next(&msg_iter) == FALSE, FALSE);
	retv_if(dbus_message_iter_get_arg_type(&msg_iter) !=
				DBUS_TYPE_ARRAY, FALSE);

	dbus_message_iter_recurse(&msg_iter, &value_iter);

	/* string array (sa) */
	while (dbus_message_iter_get_arg_type(&value_iter) ==
					DBUS_TYPE_DICT_ENTRY) {
		char *interface_name = NULL;
		DBusMessageIter interface_iter;

		dbus_message_iter_recurse(&value_iter, &interface_iter);

		retv_if(dbus_message_iter_get_arg_type(&interface_iter) !=
			DBUS_TYPE_STRING, FALSE);

		dbus_message_iter_get_basic(&interface_iter, &interface_name);

		retv_if(dbus_message_iter_next(&interface_iter) == FALSE, FALSE);

		retv_if(dbus_message_iter_get_arg_type(&interface_iter) !=
			DBUS_TYPE_ARRAY, FALSE);

		BT_DBG("interface: %s", interface_name);

		if (g_strcmp0(interface_name, "org.bluez.Device1") == 0) {
			BT_DBG("Found a device: %s", object_path);

			if (__bt_parse_device_properties(&interface_iter,
					dev_info) == FALSE) {
				BT_ERR("Fail to parse the properies");
				return FALSE;
			} else {
				return TRUE;
			}
		}

		dbus_message_iter_next(&value_iter);
	}

	BT_DBG("-");

	return FALSE;
}

static int __bt_get_owner_info(DBusMessage *msg, char **name,
				char **previous, char **current)
{
	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, name);

	retv_if(*name == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, previous);

	retv_if(*previous == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, current);

	retv_if(*current == NULL, BLUETOOTH_ERROR_INTERNAL);

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_get_agent_signal_info(DBusMessage *msg, char **address,
				char **name, char **uuid)
{
	BT_DBG("+");

	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, address);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, name);

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, uuid);

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

void __bt_set_device_values(gboolean connected, int state)
{
	int bt_device_state = VCONFKEY_BT_DEVICE_NONE;

	if (vconf_get_int(VCONFKEY_BT_DEVICE, &bt_device_state) != 0) {
		BT_ERR("vconf_get_int failed");
	}

	if (connected == TRUE)
		bt_device_state |= state;
	else if (bt_device_state & state)
		bt_device_state ^= state;

	if (vconf_set_int(VCONFKEY_BT_DEVICE, bt_device_state) != 0) {
		BT_ERR("vconf_set_int failed");
	}
}

gboolean _bt_discovery_finished_cb(gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	event_id = 0;

	if (_bt_get_discovering_property(DISCOVERY_ROLE_BREDR) == FALSE) {
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

static gboolean __bt_le_discovery_finished_cb(gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	event_id = 0;

	if (_bt_get_discovering_property(DISCOVERY_ROLE_LE) == FALSE) {
		if (_bt_get_cancel_by_user() == TRUE) {
			result = BLUETOOTH_ERROR_CANCEL_BY_USER;
		}

		_bt_set_cancel_by_user(FALSE);
		_bt_disable_all_scanner_status();
		_bt_set_le_scan_status(FALSE);
		_bt_send_event(BT_LE_ADAPTER_EVENT,
			BLUETOOTH_EVENT_LE_DISCOVERY_FINISHED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_INVALID);
	}

	return FALSE;
}

void __bt_update_remote_cache_devinfo(const char *address, gboolean paired_status)
{
	BT_DBG("+");

	ret_if(address == NULL);

	GList * node;
	bt_cache_info_t *cache_info;
	bt_remote_dev_info_t *dev_info;

	node = g_list_first(p_cache_list);

	while (node != NULL){
		cache_info = (bt_cache_info_t *)node->data;

		if (cache_info == NULL) {
			node = g_list_next(node);
			continue;
		}

		dev_info = cache_info->dev_info;
		if (strcasecmp(dev_info->address,
					address) == 0) {
			BT_DBG("Device Found");
			if (paired_status == TRUE)
				cache_info->dev_info->paired = TRUE;
			else
				cache_info->dev_info->paired = FALSE;
			break;
		}
		node = g_list_next(node);
	}
	BT_DBG("-");
}

static void __bt_device_remote_connected_properties(
				bt_remote_dev_info_t *remote_dev_info,
				char *address, gboolean connected)
{
	int result = BLUETOOTH_ERROR_NONE;
	int i;

	BT_DBG("+");

	if (remote_dev_info->uuid_count > 0 ) {
		for (i = 0; i<remote_dev_info->uuid_count; i++) {
			char *uuid = remote_dev_info->uuids[i];
			if (strcasecmp(uuid, HID_UUID) == 0){
				int event = BLUETOOTH_EVENT_NONE;

				event = (connected == TRUE) ?
					BLUETOOTH_HID_CONNECTED :
					BLUETOOTH_HID_DISCONNECTED;

				_bt_send_event(BT_HID_EVENT, event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);
				break;
			}
		}
	}

	BT_DBG("-");
}

void _bt_handle_adapter_event(DBusMessage *msg)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	const char *member = dbus_message_get_member(msg);

	ret_if(member == NULL);

	if (strcasecmp(member, "DeviceCreated") == 0) {
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
	} else if (strcasecmp(member, "InterfacesRemoved") == 0) {
		const char *object_path = NULL;
		char *address;
		bt_cache_info_t *cache_info;
		bt_remote_dev_info_t *dev_info;
		GList * node;

		/* Bonding from remote device */
		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		/* Fix : NULL_RETURNS */
		if (address == NULL)
			return;

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &object_path);
		dbus_message_iter_next(&item_iter);

		_bt_convert_device_path_to_address(object_path, address);

		node = g_list_first(p_cache_list);

		while (node != NULL){
			cache_info = (bt_cache_info_t *)node->data;

			if (cache_info == NULL) {
				node = g_list_next(node);
				continue;
			}

			dev_info = cache_info->dev_info;
			if (strcasecmp(dev_info->address,
						address) == 0) {
				p_cache_list = g_list_remove(p_cache_list,
						cache_info);
				__bt_free_cache_info(cache_info);
				break;
			}
			node = g_list_next(node);
		}

		g_free(address);
	} else if (strcasecmp(member, "AdvertisingEnabled") == 0) {
		BT_DBG("Advertising Enabled");
		int slot_id;
		const char *sender;
		int event;
		int adv_handle;
		gboolean status = FALSE;

		dbus_message_iter_init(msg, &item_iter);

		dbus_message_iter_get_basic(&item_iter, &slot_id);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &status);

		BT_DBG("Advertising Enabled : slot_id [%d]  status [%d]", slot_id, status);

		/* Send event to application */
		_bt_set_advertising_status(slot_id, status);

		sender = _bt_get_adv_slot_owner(slot_id);
		adv_handle = _bt_get_adv_slot_adv_handle(slot_id);

		if (status)
			event = BLUETOOTH_EVENT_ADVERTISING_STARTED;
		else
			event = BLUETOOTH_EVENT_ADVERTISING_STOPPED;

		_bt_send_event_to_dest(sender, BT_ADAPTER_EVENT,
				event,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_INT32, &adv_handle,
				DBUS_TYPE_INVALID);

		if (event == BLUETOOTH_EVENT_ADVERTISING_STOPPED)
			__bt_unregister_adv_slot_owner(slot_id);
	} else if (strcasecmp(member, "RssiEnabled") == 0) {
		BT_DBG("RSSI Enabled");
		gboolean status = FALSE;
		char *address = NULL;
		int link_type;

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &address);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &link_type);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &status);

		BT_DBG("RSSI Enabled [%s %d]", address, status);
		_bt_send_event(BT_DEVICE_EVENT,
				BLUETOOTH_EVENT_RSSI_ENABLED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INT32, &link_type,
				DBUS_TYPE_BOOLEAN, &status,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(member, "RssiAlert") == 0) {
		BT_DBG("RSSI Alert");
		int alert_type;
		int rssi_dbm;
		int link_type;
		char *address = NULL;

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &address);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &link_type);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &alert_type);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &rssi_dbm);

		BT_DBG("RSSI Alert: [Address %s LinkType %d] [Type %d DBM %d]",
				address, alert_type, rssi_dbm);

		_bt_send_event(BT_DEVICE_EVENT,
				BLUETOOTH_EVENT_RSSI_ALERT,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INT32, &link_type,
				DBUS_TYPE_INT32, &alert_type,
				DBUS_TYPE_INT32, &rssi_dbm,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(member, "RawRssi") == 0) {
		BT_DBG("RSSI Raw");
		int rssi_dbm;
		int link_type;
		char *address = NULL;

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &address);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &link_type);
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_get_basic(&item_iter, &rssi_dbm);

		BT_DBG("Raw RSSI: [Address %s] [Link Type %d][RSSI DBM %d]",
				address, link_type, rssi_dbm);

		_bt_send_event(BT_DEVICE_EVENT,
				BLUETOOTH_EVENT_RAW_RSSI,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INT32, &link_type,
				DBUS_TYPE_INT32, &rssi_dbm,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(member, BT_HARDWARE_ERROR) == 0) {
		BT_ERR_C("Hardware error received from BLUEZ");
		_bt_recover_adapter();
	} else if (strcasecmp(member, BT_TX_TIMEOUT_ERROR) == 0) {
		BT_ERR_C("Tx timeout error received from BLUEZ");
		_bt_recover_adapter();
	}
	BT_DBG("-");
}

static void __bt_adapter_property_changed_event(DBusMessageIter *msg_iter, const char *path)
{
	DBusGProxy *adapter_proxy;
	int mode = 0;
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter value_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter item_iter;
	GValue timeout = { 0 };
	const char *property = NULL;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_recurse(&item_iter, &dict_iter);

	dbus_message_iter_get_basic(&dict_iter, &property);
	ret_if(property == NULL);

	ret_if(!dbus_message_iter_next(&dict_iter));

	if (strcasecmp(property, "Discovering") == 0) {
		gboolean discovering = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
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
	} else if (strcasecmp(property, "LEDiscovering") == 0) {
		gboolean le_discovering = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &le_discovering);

		/* Send event to application */
		if (le_discovering == TRUE) {
			_bt_set_le_scan_status(TRUE);
			_bt_send_event(BT_LE_ADAPTER_EVENT,
				BLUETOOTH_EVENT_LE_DISCOVERY_STARTED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_INVALID);
		} else {
			ret_if(event_id > 0);

			adapter_proxy = _bt_get_adapter_proxy();
			ret_if(adapter_proxy == NULL);

			/* Need to stop searching */
			dbus_g_proxy_call(adapter_proxy,
						"StopLEDiscovery",
						NULL,
						G_TYPE_INVALID,
						G_TYPE_INVALID);

			event_id = g_timeout_add(BT_DISCOVERY_FINISHED_DELAY,
				      (GSourceFunc)__bt_le_discovery_finished_cb, NULL);
		}
	} else if (strcasecmp(property, "Name") == 0) {
		char *name = NULL;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &name);

		/* Send event to application */
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(property, "Alias") == 0) {
		char *alias = NULL;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &alias);

		/* Send event to application */
		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_LOCAL_NAME_CHANGED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &alias,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(property, "Discoverable") == 0) {
		gboolean discoverable = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &discoverable);

		if (discoverable == FALSE) {
			if (_bt_get_discoverable_timeout_property() > 0) {
				g_value_init(&timeout, G_TYPE_UINT);
				g_value_set_uint(&timeout, 0);

				adapter_proxy = _bt_get_adapter_properties_proxy();
				ret_if(adapter_proxy == NULL);

				dbus_g_proxy_call_no_reply(adapter_proxy, "Set",
						G_TYPE_STRING, BT_ADAPTER_INTERFACE,
						G_TYPE_STRING, "DiscoverableTimeout",
						G_TYPE_VALUE, &timeout,
						G_TYPE_INVALID);

				g_value_unset(&timeout);
			}

			mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;

			/* Send event to application */
			BT_INFO("[Connectable]");
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_INT16, &mode,
					DBUS_TYPE_INVALID);
		} else {
			_bt_get_discoverable_mode(&mode);

			/* Event will be sent by "DiscoverableTimeout" signal */
			if (mode != BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE)
				return;

			/* Send event to application */
			BT_INFO("[General Discoverable]");
			_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_INT16, &mode,
					DBUS_TYPE_INVALID);
		}
	} else if (strcasecmp(property, "DiscoverableTimeout") == 0) {
		_bt_get_discoverable_mode(&mode);

		/* Event was already sent by "Discoverable" signal */
		if (mode == BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE ||
		    mode == BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE)
			return;

		/* Send event to application */
		BT_INFO("[Limited Discoverable (Timeout %u secs)]",
				_bt_get_discoverable_timeout_property());

		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_INT16, &mode,
				DBUS_TYPE_INVALID);
	} else if (strcasecmp(property, "Powered") == 0) {
/* TODO: Need to check this operation!! */
		gboolean powered = FALSE;
		int bt_state;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &powered);
		BT_DBG("Powered = %d", powered);
		if (powered == FALSE) {
			if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_state) == 0 &&
				bt_state != VCONFKEY_BT_STATUS_OFF) {
				_bt_disable_adapter();
			}
			if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_state) == 0 &&
				bt_state != VCONFKEY_BT_LE_STATUS_OFF) {
				_bt_set_le_disabled(BLUETOOTH_ERROR_NONE);
			}
		}
	} else if (strcasecmp(property, "Connectable") == 0) {
		gboolean connectable = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &connectable);

		BT_DBG("Connectable property is changed : %d", connectable);

		_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_CONNECTABLE_CHANGED,
				DBUS_TYPE_BOOLEAN, &connectable,
				DBUS_TYPE_INVALID);

		if (_bt_adapter_get_status() == BT_DEACTIVATING &&
			_bt_adapter_get_le_status() == BT_LE_ACTIVATED &&
			connectable == 0)
				_bt_set_disabled(BLUETOOTH_ERROR_NONE);
	} else if (strcasecmp(property, "SupportedLEFeatures") == 0) {
		char *name = NULL;
		char *val = NULL;
		DBusMessageIter array_iter;

		dbus_message_iter_recurse(&dict_iter, &value_iter);

		if(dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_ARRAY) {
			dbus_message_iter_recurse(&value_iter, &array_iter);

			while (dbus_message_iter_get_arg_type(&array_iter) !=
						DBUS_TYPE_INVALID) {
				dbus_message_iter_get_basic(&array_iter, &name);
				if (!dbus_message_iter_next(&array_iter))
					break;

				dbus_message_iter_get_basic(&array_iter, &val);

				BT_DBG("name[%s] value[%s]", name, val);

				if (FALSE == _bt_update_le_feature_support(name, val))
					BT_INFO("Fail to update LE feature info");

				if (!dbus_message_iter_next(&array_iter))
					break;
			}
		}
	} else {
		BT_DBG("property : [%s]", property);
	}
}

static void __bt_obex_property_changed_event(DBusMessageIter *msg_iter, const char *path)
{
	BT_DBG("+");

	DBusMessageIter value_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter item_iter;
	const char *property = NULL;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	do {
		dbus_message_iter_recurse(&item_iter, &dict_iter);

		dbus_message_iter_get_basic(&dict_iter, &property);
		ret_if(property == NULL);

		ret_if(!dbus_message_iter_next(&dict_iter));

		BT_DBG("property :%s", property);

		if (strcasecmp(property, "Status") == 0) {
			const char  *status;
			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &status);

			if (strcasecmp(status, "active") == 0){
				_bt_obex_transfer_started(path);
			} else if (strcasecmp(status, "complete") == 0) {
				_bt_obex_transfer_completed(path, TRUE);
				_bt_pbap_obex_transfer_completed(path, TRUE);
			} else if (strcasecmp(status, "error") == 0){
				_bt_obex_transfer_completed(path, FALSE);
				_bt_pbap_obex_transfer_completed(path, FALSE);
			}
		} else if (strcasecmp(property, "Transferred") == 0) {
			static int transferred  = 0;
			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &transferred);

			_bt_obex_transfer_progress(path,transferred);
		}

		dbus_message_iter_next(&item_iter);
	} while (dbus_message_iter_get_arg_type(&item_iter) ==
			DBUS_TYPE_DICT_ENTRY);

	BT_DBG("-");
}

static void __bt_device_property_changed_event(DBusMessageIter *msg_iter, const char *path)
{
	BT_DBG("+");

	int event;
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter value_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter item_iter;
	const char *property = NULL;
	char *address;
	bt_remote_dev_info_t *remote_dev_info;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	do {
		dbus_message_iter_recurse(&item_iter, &dict_iter);

		dbus_message_iter_get_basic(&dict_iter, &property);
		ret_if(property == NULL);

		ret_if(!dbus_message_iter_next(&dict_iter));

		if (strcasecmp(property, "Connected") == 0) {
			guint connected = 0;

			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &connected);

			event = (connected != BLUETOOTH_CONNECTED_LINK_NONE) ?
				BLUETOOTH_EVENT_DEVICE_CONNECTED :
				BLUETOOTH_EVENT_DEVICE_DISCONNECTED;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			BT_DBG("connected: %d", connected);
			BT_DBG("address: %s", address);

			remote_dev_info = _bt_get_remote_device_info(address);

			if (remote_dev_info != NULL) {
				__bt_device_remote_connected_properties(
						remote_dev_info, address,
						connected != BLUETOOTH_CONNECTED_LINK_NONE ?
						TRUE : FALSE);
				_bt_free_device_info(remote_dev_info);
			}

			/* Send event to application */
			_bt_send_event(BT_DEVICE_EVENT,
					event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

			g_free(address);
		} else if (strcasecmp(property, "RSSI") == 0) {
			bt_remote_dev_info_t *remote_dev_info;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);
			BT_DBG("address: %s", address);

			remote_dev_info = _bt_get_remote_device_info(address);
			if (remote_dev_info == NULL) {
				g_free(address);
				return;
			}
			BT_DBG("Address type  %d", remote_dev_info->addr_type);

			if (remote_dev_info && remote_dev_info->addr_type == 0) {
				BT_DBG("Name %s", remote_dev_info->name);
				_bt_send_event(BT_ADAPTER_EVENT,
				BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &remote_dev_info->address,
				DBUS_TYPE_UINT32, &remote_dev_info->class,
				DBUS_TYPE_INT16, &remote_dev_info->rssi,
				DBUS_TYPE_STRING, &remote_dev_info->name,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
				DBUS_TYPE_UINT32, &remote_dev_info->connected,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&remote_dev_info->uuids, remote_dev_info->uuid_count,
				DBUS_TYPE_INT16, &remote_dev_info->manufacturer_data_len,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
				&remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
				DBUS_TYPE_INVALID);
			}
			_bt_free_device_info(remote_dev_info);
			g_free(address);
		} else if (strcasecmp(property, "GattConnected") == 0) {
			gboolean gatt_connected = FALSE;

			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &gatt_connected);

			event = gatt_connected ? BLUETOOTH_EVENT_GATT_CONNECTED :
					BLUETOOTH_EVENT_GATT_DISCONNECTED;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			BT_DBG("gatt_connected: %d", gatt_connected);
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

			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &paired);

			_bt_agent_set_canceled(FALSE);
			/* BlueZ sends paired signal for each paired device */
			/* during activation, We should ignore this, otherwise*/
			/* application thinks that a new device got paired */
			if (_bt_adapter_get_status() != BT_ACTIVATED) {
				BT_DBG("BT is not activated, so ignore this");
				return;
			}

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			remote_dev_info = _bt_get_remote_device_info(address);
			if (remote_dev_info == NULL) {
				g_free(address);
				return;
			}

			if(paired == FALSE) {
				BT_INFO("Unpaired: %s", address);
				__bt_update_remote_cache_devinfo(address, FALSE);
				_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);
			} else {
				BT_INFO("Paired: %s", address);
				__bt_update_remote_cache_devinfo(address, TRUE);

				if (_bt_is_device_creating() == TRUE) {
					BT_DBG("Try to Pair by me");
					_bt_free_device_info(remote_dev_info);
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
					DBUS_TYPE_UINT32, &remote_dev_info->connected,
					DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
					DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
					&remote_dev_info->uuids, remote_dev_info->uuid_count,
					DBUS_TYPE_INT16, &remote_dev_info->manufacturer_data_len,
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
					&remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
					DBUS_TYPE_INVALID);
			}
			_bt_free_device_info(remote_dev_info);
			g_free(address);
		} else if (strcasecmp(property, "LegacyPaired") == 0) {
			gboolean paired = FALSE;
			bt_remote_dev_info_t *remote_dev_info;

			if (_bt_adapter_get_status() != BT_ACTIVATED) {
				BT_DBG("BT is not activated, so ignore this");
				return;
			}

			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &paired);
			address = g_malloc0(BT_ADDRESS_STRING_SIZE);
			BT_DBG("LegacyPaired: %d", paired);
			_bt_convert_device_path_to_address(path, address);

			remote_dev_info = _bt_get_remote_device_info(address);
			if (remote_dev_info == NULL) {
				g_free(address);
				return;
			}

			BT_DBG("LegacyPairing Failed with %s. Show Error Popup",
					remote_dev_info->name);
			_bt_launch_system_popup(BT_AGENT_EVENT_LEGACY_PAIR_FAILED_FROM_REMOTE,
						remote_dev_info->name, NULL, NULL, NULL);

			_bt_free_device_info(remote_dev_info);
			g_free(address);
		} else if (strcasecmp(property, "Trusted") == 0) {
			gboolean trusted = FALSE;

			dbus_message_iter_recurse(&dict_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &trusted);

			event = trusted ? BLUETOOTH_EVENT_DEVICE_AUTHORIZED :
					BLUETOOTH_EVENT_DEVICE_UNAUTHORIZED;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			BT_DBG("trusted: %d", trusted);
			BT_DBG("address: %s", address);

			/* Send event to application */
			_bt_send_event(BT_DEVICE_EVENT,
					event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

			g_free(address);
		}
		dbus_message_iter_next(&item_iter);
	} while (dbus_message_iter_get_arg_type(&item_iter) ==
			DBUS_TYPE_DICT_ENTRY);
	BT_DBG("-");
}

static void __bt_media_control_changed_event(DBusMessageIter *msg_iter, const char *path)
{
	int event;
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter value_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter item_iter;
	const char *property = NULL;
	char *address;
	bt_remote_dev_info_t *remote_dev_info;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_recurse(&item_iter, &dict_iter);

	dbus_message_iter_get_basic(&dict_iter, &property);
	ret_if(property == NULL);

	ret_if(!dbus_message_iter_next(&dict_iter));

	if (strcasecmp(property, "Connected") == 0) {
		gboolean connected = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &connected);

		event = connected ? BLUETOOTH_EVENT_AVRCP_CONNECTED :
				BLUETOOTH_EVENT_AVRCP_DISCONNECTED;

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		BT_DBG("connected: %d", connected);
		BT_DBG("address: %s", address);

		remote_dev_info = _bt_get_remote_device_info(address);

		if (remote_dev_info != NULL) {
			__bt_device_remote_connected_properties(
			remote_dev_info, address, connected);
			_bt_free_device_info(remote_dev_info);
		}

		/* Send event to application */
		_bt_send_event(BT_AVRCP_EVENT,
			event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);
		g_free(address);
	}

	BT_DBG("-");
}

void _bt_handle_property_changed_event(DBusMessage *msg)
{
	DBusMessageIter item_iter;
	const char *member = dbus_message_get_member(msg);
	const char *interface_name = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &interface_name);

	ret_if(interface_name == NULL);

	ret_if(dbus_message_iter_next(&item_iter) == FALSE);

	ret_if(dbus_message_iter_get_arg_type(&item_iter) != DBUS_TYPE_ARRAY);

	if (strcasecmp(interface_name, BT_ADAPTER_INTERFACE) == 0) {
		__bt_adapter_property_changed_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_DEVICE_INTERFACE) == 0) {
		__bt_device_property_changed_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_OBEX_TRANSFER_INTERFACE) == 0) {
		BT_DBG("BT_OBEX_TRANSFER_INTERFACE");
		__bt_obex_property_changed_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_MEDIA_CONTROL_INTERFACE) == 0) {
		__bt_media_control_changed_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_PLAYER_CONTROL_INTERFACE) == 0) {
		_bt_handle_avrcp_control_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_NETWORK_CLIENT_INTERFACE) == 0) {
		BT_DBG("BT_NETWORK_CLIENT_INTERFACE");
		_bt_handle_network_client_event(&item_iter,
					dbus_message_get_path(msg));
	} else if (strcasecmp(interface_name, BT_GATT_CHAR_INTERFACE) == 0) {
		__bt_gatt_char_property_changed_event(&item_iter,
					dbus_message_get_path(msg));
	}
}

void __bt_opc_property_changed_event(DBusMessageIter *msg_iter,
						const char *path)
{
	DBusMessageIter value_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter item_iter;
	const char *property = NULL;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
				!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_recurse(&item_iter, &dict_iter);

	dbus_message_iter_get_basic(&dict_iter, &property);
	ret_if(property == NULL);

	ret_if(!dbus_message_iter_next(&dict_iter));

	if (strcasecmp(property, "Status") == 0) {
		const char *status = NULL;
		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &status);

		BT_INFO("Status is %s", status);

		if(strcasecmp(status, "active") == 0){
			_bt_obex_client_started(path);
		}else if (strcasecmp(status, "complete") == 0) {
			_bt_obex_client_completed(path, TRUE);
		}else if (strcasecmp(status, "error") == 0){
			_bt_obex_client_completed(path, FALSE);
		}
	} else if (strcasecmp(property, "Transferred") == 0) {
		static int transferred  = 0;
		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &transferred);

		_bt_obex_client_progress(path, transferred);
	} else {
		BT_DBG("property : [%s]", property);
	}
}

void _bt_opc_property_changed_event(DBusMessage *msg)
{
	DBusMessageIter item_iter;
	const char *member = dbus_message_get_member(msg);
	const char *interface_name = NULL;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
				!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &interface_name);

	ret_if(interface_name == NULL);

	BT_DBG("interface: %s", interface_name);

	ret_if(dbus_message_iter_next(&item_iter) == FALSE);

	ret_if(dbus_message_iter_get_arg_type(&item_iter) != DBUS_TYPE_ARRAY);

	if (strcasecmp(interface_name, BT_OBEX_TRANSFER_INTERFACE) == 0) {
		__bt_opc_property_changed_event(&item_iter,
					dbus_message_get_path(msg));
	} else {
		BT_DBG("interface_name : [%s]", interface_name);
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
	bt_remote_dev_info_t *remote_dev_info;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
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

		/* Check HID connection type (Keyboard or Mouse) and update the status */
		remote_dev_info = _bt_get_remote_device_info(address);

		if (property_flag == TRUE) {
			hid_connected_device_count++;
			__bt_set_device_values(TRUE,
					VCONFKEY_BT_DEVICE_HID_CONNECTED);
		} else {
			hid_connected_device_count--;
			if (hid_connected_device_count == 0)
				__bt_set_device_values(FALSE,
						VCONFKEY_BT_DEVICE_HID_CONNECTED);
		}

		if (remote_dev_info != NULL) {
			BT_DBG("HID device class [%x]", remote_dev_info->class);
			if (remote_dev_info->class &
					BLUETOOTH_DEVICE_MINOR_CLASS_KEY_BOARD) {
				__bt_set_device_values(property_flag,
						VCONFKEY_BT_DEVICE_HID_KEYBOARD_CONNECTED);

			}

			if (remote_dev_info->class &
					BLUETOOTH_DEVICE_MINOR_CLASS_POINTING_DEVICE)
			{
				__bt_set_device_values(property_flag,
						VCONFKEY_BT_DEVICE_HID_MOUSE_CONNECTED);
			}
			_bt_free_device_info(remote_dev_info);
		}

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

		__bt_set_device_values(TRUE,
				VCONFKEY_BT_DEVICE_PAN_CONNECTED);

		_bt_send_event(BT_NETWORK_EVENT, BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);
		 nap_connected_device_count++;
	} else if (strcasecmp(member, "PeerDisconnected") == 0) {
		if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID)) {
			BT_ERR("Unexpected parameters in signal");
			return;
		}
		nap_connected_device_count--;
		if (nap_connected_device_count == 0)
			__bt_set_device_values(FALSE,
				VCONFKEY_BT_DEVICE_PAN_CONNECTED);

		_bt_send_event(BT_NETWORK_EVENT, BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &device,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);
	}
}

void _bt_handle_network_client_event(DBusMessageIter *msg_iter,
				const char *path)
{
	BT_DBG("+");

	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter value_iter;
	gboolean property_flag = FALSE;
	const char *property = NULL;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_recurse(&item_iter, &dict_iter);
	dbus_message_iter_get_basic(&dict_iter, &property);

	ret_if(property == NULL);
	ret_if(!dbus_message_iter_next(&dict_iter));

	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		char *address;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		BT_DBG("property_flag %d", property_flag);
		if (property_flag == TRUE) {
			event = BLUETOOTH_EVENT_NETWORK_CONNECTED;
			nap_connected_device_count++;
			__bt_set_device_values(TRUE,
				VCONFKEY_BT_DEVICE_PAN_CONNECTED);
		} else {
			event = BLUETOOTH_EVENT_NETWORK_DISCONNECTED;
			nap_connected_device_count--;
			if (nap_connected_device_count == 0)
				__bt_set_device_values(FALSE,
					VCONFKEY_BT_DEVICE_PAN_CONNECTED);
		}

		_bt_send_event(BT_NETWORK_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		g_free(address);
	}
	BT_DBG("-");
}

void __bt_gatt_char_property_changed_event(DBusMessageIter *msg_iter,
				const char *path)
{
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter dict_iter;
	DBusMessageIter value_iter;
	const char *property = NULL;
	char * char_handle = NULL;

	dbus_message_iter_recurse(msg_iter, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_DICT_ENTRY) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_recurse(&item_iter, &dict_iter);
	dbus_message_iter_get_basic(&dict_iter, &property);

	ret_if(property == NULL);
	ret_if(!dbus_message_iter_next(&dict_iter));

	char_handle = g_strdup(path);

	if (strcasecmp(property, "Notifying") == 0) {
		gboolean property_flag = FALSE;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		if (property_flag == TRUE)
			BT_DBG("notifying is enabled");
		else
			BT_DBG("notifying is disabled");
	} else if (strcasecmp(property, "ChangedValue") == 0) {
		DBusMessageIter array_value_iter;
		char byte = 0;
		GByteArray *gp_byte_array = NULL;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_recurse(&value_iter, &array_value_iter);
		gp_byte_array = g_byte_array_new();

		while (dbus_message_iter_get_arg_type(&array_value_iter) == DBUS_TYPE_BYTE) {
				dbus_message_iter_get_basic(&array_value_iter, &byte);
				g_byte_array_append(gp_byte_array, (const guint8 *)&byte, 1);
				dbus_message_iter_next(&array_value_iter);
		}
		if (gp_byte_array->len != 0) {
			_bt_send_event(BT_DEVICE_EVENT,
				BLUETOOTH_EVENT_GATT_CHAR_VAL_CHANGED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &char_handle,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
				&gp_byte_array->data, gp_byte_array->len,
				DBUS_TYPE_INVALID);
		}
		g_byte_array_free(gp_byte_array, TRUE);
	}
	g_free(char_handle);
}
void _bt_handle_device_event(DBusMessage *msg)
{
	int event;
	int result = BLUETOOTH_ERROR_NONE;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	char *address;
	char *dev_name;
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	ret_if(path == NULL);
	ret_if(member == NULL);

	if (strcasecmp(member, "PropertyChanged") == 0) {
		dbus_message_iter_init(msg, &item_iter);

		if (dbus_message_iter_get_arg_type(&item_iter)
						!= DBUS_TYPE_STRING) {
			BT_ERR("This is bad format dbus");
			return;
		}

		dbus_message_iter_get_basic(&item_iter, &property);

		ret_if(property == NULL);

		if (strcasecmp(property, "GattConnected") == 0) {
			gboolean connected = FALSE;
			char *address;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);
			_bt_convert_device_path_to_address(path, address);

			dbus_message_iter_next(&item_iter);
			dbus_message_iter_recurse(&item_iter, &value_iter);
			dbus_message_iter_get_basic(&value_iter, &connected);

			event = connected ? BLUETOOTH_EVENT_GATT_CONNECTED :
					BLUETOOTH_EVENT_GATT_DISCONNECTED;

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
				DBUS_TYPE_UINT32, &remote_dev_info->connected,
				DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&remote_dev_info->uuids, remote_dev_info->uuid_count,
				DBUS_TYPE_INT16, &remote_dev_info->manufacturer_data_len,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
				&remote_dev_info->manufacturer_data, remote_dev_info->manufacturer_data_len,
				DBUS_TYPE_INVALID);

			_bt_free_device_info(remote_dev_info);
			g_free(address);

		} else if (strcasecmp(property, "UUIDs") == 0) {
			/* Once we get the updated uuid information after
			 * reverse service search, update it to application */

			bt_remote_dev_info_t *remote_dev_info;

			ret_if(_bt_is_device_creating() == TRUE);

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			remote_dev_info = _bt_get_remote_device_info(address);
			if (remote_dev_info == NULL) {
				g_free(address);
				return;
			}

			BT_DBG("UUID's count = %d", remote_dev_info->uuid_count);
			if (remote_dev_info->paired && remote_dev_info->uuid_count)
				_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_SERVICE_SEARCHED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_UINT32, &remote_dev_info->class,
					DBUS_TYPE_INT16, &remote_dev_info->rssi,
					DBUS_TYPE_STRING, &remote_dev_info->name,
					DBUS_TYPE_BOOLEAN, &remote_dev_info->paired,
					DBUS_TYPE_UINT32, &remote_dev_info->connected,
					DBUS_TYPE_BOOLEAN, &remote_dev_info->trust,
					DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
					&remote_dev_info->uuids, remote_dev_info->uuid_count,
					DBUS_TYPE_INVALID);

			_bt_free_device_info(remote_dev_info);
			g_free(address);
		}
	} else if (strcasecmp(member, "DeviceConnected") == 0) {
		unsigned char addr_type = 0;

		dbus_message_iter_init(msg, &item_iter);
		dbus_message_iter_get_basic(&item_iter, &addr_type);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);
		dev_name = _bt_get_bonded_device_name(address);

		BT_INFO("Address : %s Type : %d", address, addr_type);
		BT_ERR_C("Connected [%s] [%s]", !addr_type ? "BREDR" : "LE",
				!addr_type ? dev_name : address);
		g_free(dev_name);

		_bt_logging_connection(TRUE, addr_type);
		journal_bt_connected();

		/*Send event to application*/
		_bt_send_event(BT_DEVICE_EVENT,
					BLUETOOTH_EVENT_DEVICE_CONNECTED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_BYTE, &addr_type,
					DBUS_TYPE_INVALID);

		g_free(address);
	} else if (strcasecmp(member, "Disconnected") == 0) {
		unsigned char disc_reason = 0;
		unsigned char addr_type = 0;
		gboolean sending = FALSE;

		if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_BYTE, &addr_type,
				DBUS_TYPE_BYTE, &disc_reason,
				DBUS_TYPE_INVALID))
			return;

		result = disc_reason;

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);
		dev_name = _bt_get_bonded_device_name(address);
		journal_bt_disconnected();

		/* 0x00 BDADDR_BRDER
		      0x01 BDADDR_LE_PUBLIC
		      0x02 BDADDR_LE_RANDOM */
		BT_INFO("Address : %s Type : %d", address, addr_type);
		BT_ERR_C("Disconnected [%s] [%d : %s] [%s]", !addr_type ? "BREDR" : "LE",
				disc_reason, _bt_convert_disc_reason_to_string(disc_reason),
				!addr_type ? dev_name : address);
		g_free(dev_name);

		_bt_headset_set_local_connection(FALSE);
		_bt_logging_connection(FALSE, addr_type);

		/*Check for any OPP transfer on the device and cancel
		 * the transfer
		 */
		_bt_obex_check_pending_transfer(address);
		_bt_opp_client_is_sending(&sending);
		if(sending == TRUE)
			_bt_opp_client_check_pending_transfer(address);

		_bt_send_event(BT_DEVICE_EVENT,
					BLUETOOTH_EVENT_DEVICE_DISCONNECTED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_BYTE, &addr_type,
					DBUS_TYPE_INVALID);

		g_free(address);
	} else if (strcasecmp(member, "ProfileStateChanged") == 0) {
		int state = 0;
		char *profile_uuid = NULL;
		bt_headset_wait_t *wait_list;

		if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_STRING, &profile_uuid,
				DBUS_TYPE_INT32, &state,
				DBUS_TYPE_INVALID)) {
			return;
		}
		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		_bt_convert_device_path_to_address(path, address);

		BT_DBG("Address: %s", address);
		BT_DBG("Profile UUID: %s", profile_uuid);
		BT_DBG("State: %d", state);

		if ((strcmp(profile_uuid, A2DP_SINK_UUID) == 0)  &&
			(state == BT_PROFILE_STATE_CONNECTED)) {

			int event = BLUETOOTH_EVENT_AV_CONNECTED;
			char connected_address[BT_ADDRESS_STRING_SIZE + 1];
			bluetooth_device_address_t device_address;
			gboolean connected;
			bt_headset_wait_t *wait_list;

			__bt_set_device_values(TRUE,
				VCONFKEY_BT_DEVICE_A2DP_HEADSET_CONNECTED);

			__bt_connection_manager_set_state(address, event);

			if (_bt_headset_get_local_connection() == FALSE)
				_bt_start_timer_for_connection(address, BT_AUDIO_HSP);
			else {
				/* Connection Started from local device therefore no need to
				 * intiate connection for pending profile */
				_bt_headset_set_local_connection(FALSE);
			}

			_bt_send_event(BT_HEADSET_EVENT, event,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID);

			connected = _bt_is_headset_type_connected(BT_AUDIO_A2DP,
						connected_address);
			if (connected) {
				if (g_strcmp0(connected_address, address) != 0) {
					_bt_convert_addr_string_to_type(
						device_address.addr,
						connected_address);
					_bt_audio_disconnect(0, BT_AUDIO_A2DP,
						&device_address, NULL);
				}
			}

			_bt_add_headset_to_list(BT_AUDIO_A2DP,
						BT_STATE_CONNECTED, address);

			wait_list = _bt_get_audio_wait_data();
			if (wait_list != NULL &&
				(g_strcmp0(wait_list->address, address) == 0))
				_bt_rel_wait_data();

		} else if ((strcmp(profile_uuid, A2DP_SINK_UUID) == 0)  &&
			(state == BT_PROFILE_STATE_DISCONNECTED)) {

			int event = BLUETOOTH_EVENT_AV_DISCONNECTED;

			if (!_bt_is_service_connected(address, BT_AUDIO_A2DP)) {
				g_free(address);
				return;
			}

			__bt_set_device_values(FALSE,
				VCONFKEY_BT_DEVICE_A2DP_HEADSET_CONNECTED);

			__bt_connection_manager_set_state(address, event);

			_bt_send_event(BT_HEADSET_EVENT, event,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID);

			/* Remove data from the connected list */
			_bt_remove_headset_from_list(BT_AUDIO_A2DP, address);
			wait_list = _bt_get_audio_wait_data();

			if (wait_list == NULL) {
				g_free(address);
				return;
			}

			if (((wait_list->type == BT_AUDIO_ALL) &&
				(wait_list->ag_flag == TRUE)) ||
				(wait_list->type == BT_AUDIO_A2DP) ||
				(wait_list->disconnection_type == BT_AUDIO_A2DP)) {
				bluetooth_device_address_t device_address;
				_bt_convert_addr_string_to_type(
							device_address.addr,
							wait_list->address);

				_bt_audio_connect(wait_list->req_id,
							wait_list->type,
							&device_address,
							wait_list->out_param1);
			}
		} else if (strcmp(profile_uuid, AVRCP_TARGET_UUID) == 0) {

			if (state == BT_PROFILE_STATE_CONNECTED) {
				int event = BLUETOOTH_EVENT_AVRCP_CONTROL_CONNECTED;
				char connected_address[BT_ADDRESS_STRING_SIZE + 1];
				bluetooth_device_address_t device_address;
				gboolean connected;

				_bt_send_event(BT_AVRCP_CONTROL_EVENT, event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);
				connected = _bt_is_headset_type_connected(
							BT_AVRCP,
							connected_address);
				if (connected) {
					if (g_strcmp0(connected_address,
								address) != 0) {
						_bt_convert_addr_string_to_type(
							device_address.addr,
							connected_address);
						_bt_audio_disconnect(0,
							BT_AVRCP,
							&device_address, NULL);
					}
				}
				BT_DBG("device Path: %s", path);
				_bt_add_headset_to_list(BT_AVRCP,
						BT_STATE_CONNECTED, address);
			} else if (state == BT_PROFILE_STATE_DISCONNECTED) {
				int event = BLUETOOTH_EVENT_AVRCP_CONTROL_DISCONNECTED;

				_bt_send_event(BT_AVRCP_CONTROL_EVENT, event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

				/* Remove data from the connected list */
				_bt_remove_headset_from_list(BT_AVRCP, address);
				}
		} else if (strcasecmp(profile_uuid, A2DP_SOURCE_UUID) == 0) {
			if (state == BT_PROFILE_STATE_CONNECTED) {
				int event = BLUETOOTH_EVENT_AV_SOURCE_CONNECTED;
				BT_INFO("A2DP Source is connected");
				_bt_send_event(BT_A2DP_SOURCE_EVENT, event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

			} else if (state == BT_PROFILE_STATE_DISCONNECTED) {
				int event = BLUETOOTH_EVENT_AV_SOURCE_DISCONNECTED;
				BT_INFO("A2DP Source Disconnected");

				_bt_send_event(BT_A2DP_SOURCE_EVENT, event,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);
			}
		} else if ((strcmp(profile_uuid, HID_UUID) == 0) &&
			((state == BT_PROFILE_STATE_CONNECTED) ||
				(state == BT_PROFILE_STATE_DISCONNECTED))) {
			int event;
			if (state == BT_PROFILE_STATE_CONNECTED)
				event = BLUETOOTH_HID_CONNECTED;
			else
				event = BLUETOOTH_HID_DISCONNECTED;

			_bt_send_event(BT_HID_EVENT, event,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID);
		}
		g_free(address);
	} else if (strcasecmp(member, "AdvReport") == 0) {

		bt_remote_le_dev_info_t *le_dev_info = NULL;
		char *buffer = NULL;
		int buffer_len = 0;
		bt_le_adv_info_t *adv_info = NULL;

		ret_if(_bt_is_le_scanning() == FALSE);

		le_dev_info = g_malloc0(sizeof(bt_remote_le_dev_info_t));

		if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_STRING, &le_dev_info->address,
				DBUS_TYPE_BYTE, &le_dev_info->addr_type,
				DBUS_TYPE_BYTE, &le_dev_info->adv_type,
				DBUS_TYPE_INT32, &le_dev_info->rssi,
				DBUS_TYPE_INT32, &le_dev_info->adv_data_len,
				DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &buffer, &buffer_len,
				DBUS_TYPE_INVALID)) {
			g_free(le_dev_info);
			return;
		}

		le_dev_info->adv_data = g_memdup(buffer, buffer_len);

		if (_bt_get_le_scan_type() == BT_LE_PASSIVE_SCAN) {
			_bt_send_scan_result_event(le_dev_info, NULL);
			_bt_free_le_device_info(le_dev_info);
			return;
		}

		if (le_dev_info->adv_type != BT_LE_ADV_SCAN_RSP) {       /* ADV_IND */
			adv_info = g_malloc0(sizeof(bt_le_adv_info_t));
			adv_info->addr = g_strdup(le_dev_info->address);
			adv_info->data_len = le_dev_info->adv_data_len;
			adv_info->data = g_malloc0(le_dev_info->adv_data_len);
			memcpy(adv_info->data, le_dev_info->adv_data,
					le_dev_info->adv_data_len);

			__bt_add_adv_ind_info(adv_info);

		} else {     /* SCAN_RSP */
			adv_info = __bt_get_adv_ind_info(le_dev_info->address);
			if (adv_info) {
				_bt_send_scan_result_event(le_dev_info, adv_info);
				__bt_del_adv_ind_info(le_dev_info->address);
			}
		}
		_bt_free_le_device_info(le_dev_info);
	}
}

void __bt_set_audio_values(gboolean connected, char *address)
{
	char *name = NULL;
	int bt_device_state = VCONFKEY_BT_DEVICE_NONE;

	/*  Set the headset name */
	if (connected == TRUE) {
		name = _bt_get_bonded_device_name(address);
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

#ifdef TIZEN_SUPPORT_DUAL_HF
	if ((connected == TRUE) &&
		(FALSE == __bt_is_companion_device(address))) {
		bt_device_state |= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	} else if ((bt_device_state & VCONFKEY_BT_DEVICE_HEADSET_CONNECTED) &&
			(FALSE == __bt_is_companion_device(address))) {
		bt_device_state ^= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	}
#else
	if (connected == TRUE) {
		bt_device_state |= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	} else if (bt_device_state & VCONFKEY_BT_DEVICE_HEADSET_CONNECTED) {
		bt_device_state ^= VCONFKEY_BT_DEVICE_HEADSET_CONNECTED;
	}
#endif

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
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	BT_DBG("Property = %s \n", property);

	/* We allow only 1 headset connection (HSP or HFP)*/
	if (strcasecmp(property, "Connected") == 0) {
		int event = BLUETOOTH_EVENT_NONE;
		bt_headset_wait_t *wait_list;
		char *address;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &property_flag);

		address = g_malloc0(BT_ADDRESS_STRING_SIZE);

		/* Fix : NULL_RETURNS */
		if (address == NULL)
			return;

		_bt_convert_device_path_to_address(path, address);

		if (property_flag == TRUE) {
			event = BLUETOOTH_EVENT_AG_CONNECTED;
			if (_bt_headset_get_local_connection() == FALSE)
				_bt_start_timer_for_connection(address, BT_AUDIO_A2DP);
			else
				_bt_headset_set_local_connection(FALSE);
		} else {
			int previous_state;

			event = BLUETOOTH_EVENT_AG_DISCONNECTED;

			previous_state = _bt_get_device_state_from_list(BT_AUDIO_HSP, address);
			if (previous_state == BT_STATE_DISCONNECTING)
				_bt_send_hf_local_term_event(address);
		}
		/* Set the State machine here */
		__bt_connection_manager_set_state(address, event);
		__bt_set_audio_values(property_flag, address);

		_bt_send_event(BT_HEADSET_EVENT, event,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID);

		if (event == BLUETOOTH_EVENT_AG_DISCONNECTED) {
			/* Remove data from the connected list */
			_bt_remove_headset_from_list(BT_AUDIO_HSP, address);

			wait_list = _bt_get_audio_wait_data();
			if (wait_list == NULL) {
				g_free(address);
				return;
			}

			bluetooth_device_address_t device_address;

			_bt_set_audio_wait_data_flag(TRUE);

			_bt_convert_addr_string_to_type(device_address.addr,
							wait_list->address);
			_bt_audio_connect(wait_list->req_id, wait_list->type,
					&device_address, wait_list->out_param1);
			_bt_rel_wait_data();
		} else if (event == BLUETOOTH_EVENT_AG_CONNECTED) {
			/* Add data to the connected list */
			_bt_add_headset_to_list(BT_AUDIO_HSP,
						BT_STATE_CONNECTED, address);

			wait_list = _bt_get_audio_wait_data();
			if (wait_list != NULL &&
				(g_strcmp0(wait_list->address, address) == 0))
			_bt_rel_wait_data();

			BT_INFO("Check A2DP pending connect");
			_bt_audio_check_pending_connect();
		}
		g_free(address);
	} else if (strcasecmp(property, "State") == 0) {
		char *state = NULL;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &state);

		/* This code assumes we support only 1 headset connection */
		/* Need to use the headset list, if we support multi-headsets */
		if (strcasecmp(state, "Playing") == 0) {
			BT_DBG("Playing: Sco Connected");
		} else if (strcasecmp(state, "connected") == 0 ||
				strcasecmp(state, "disconnected") == 0) {
			BT_DBG("connected/disconnected: Sco Disconnected");
		} else {
			BT_ERR("Not handled state - %s", state);
			return;
		}
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
	const char *member = dbus_message_get_member(msg);
	const char *path = dbus_message_get_path(msg);
	const char *property = NULL;

	bt_headset_wait_t *wait_list;

	ret_if(member == NULL);

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus");
		return;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	ret_if(property == NULL);

	BT_DBG("Property: %s", property);

	if (strcasecmp(property, "State") == 0) {

		const char *value;

		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &value);

		BT_DBG("value: %s", value);

		if (g_strcmp0(value, "disconnected") == 0) {
			char *address;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			__bt_set_device_values(FALSE,
				VCONFKEY_BT_DEVICE_A2DP_HEADSET_CONNECTED);

			_bt_send_event(BT_HEADSET_EVENT,
				BLUETOOTH_EVENT_AV_DISCONNECTED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID);

			/* Remove data from the connected list */
			_bt_remove_headset_from_list(BT_AUDIO_A2DP, address);
			wait_list = _bt_get_audio_wait_data();
			if (wait_list == NULL) {
				g_free(address);
				return;
			}

			if (((wait_list->type == BT_AUDIO_ALL) &&
				(wait_list->ag_flag == TRUE)) ||
				(wait_list->type == BT_AUDIO_A2DP) ||
				(wait_list->disconnection_type == BT_AUDIO_A2DP)) {
				bluetooth_device_address_t device_address;
				_bt_convert_addr_string_to_type(
							device_address.addr,
							wait_list->address);

				_bt_audio_connect(wait_list->req_id,
							wait_list->type,
							&device_address,
							wait_list->out_param1);
			}
			g_free(address);
		}else if (strcasecmp(value, "Connected") == 0) {
			char *address;
			char connected_address[BT_ADDRESS_STRING_SIZE + 1];
			bluetooth_device_address_t device_address;
			gboolean connected;

			address = g_malloc0(BT_ADDRESS_STRING_SIZE);

			_bt_convert_device_path_to_address(path, address);

			__bt_set_device_values(TRUE,
					VCONFKEY_BT_DEVICE_A2DP_HEADSET_CONNECTED);

			_bt_send_event(BT_HEADSET_EVENT,
				BLUETOOTH_EVENT_AV_CONNECTED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID);

			/* Check for existing Media device to disconnect */
			connected = _bt_is_headset_type_connected(BT_AUDIO_A2DP,
								connected_address);
			if (connected) {
				/* Match connected device address */
				if (g_strcmp0(connected_address, address) != 0) {
					/* Convert BD adress from string type */
					_bt_convert_addr_string_to_type(
							device_address.addr,
							connected_address);
					_bt_audio_disconnect(0, BT_AUDIO_A2DP,
							&device_address, NULL);
				}
			}

			/* Add data to the connected list */
			_bt_add_headset_to_list(BT_AUDIO_A2DP,
					BT_STATE_CONNECTED, address);

			g_free(address);
		}
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

static int __bt_get_object_path(DBusMessage *msg, char **path)
{
	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_OBJECT_PATH) {
		BT_ERR("This is bad format dbus");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, path);

	if (*path == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_parse_event(DBusMessage *msg)
{
	DBusMessageIter msg_iter;
	DBusMessageIter value_iter;

	retv_if(dbus_message_iter_init(msg, &msg_iter) == FALSE, 0);

	/* object array (oa) */
	retv_if(dbus_message_iter_next(&msg_iter) == FALSE, 0);
	retv_if(dbus_message_iter_get_arg_type(&msg_iter) !=
					DBUS_TYPE_ARRAY, 0);

	dbus_message_iter_recurse(&msg_iter, &value_iter);

	/* string array (sa) */
	while (dbus_message_iter_get_arg_type(&value_iter) ==
					DBUS_TYPE_DICT_ENTRY) {
		char *interface_name = NULL;
		DBusMessageIter interface_iter;

		dbus_message_iter_recurse(&value_iter, &interface_iter);

		retv_if(dbus_message_iter_get_arg_type(&interface_iter) !=
						DBUS_TYPE_STRING, 0);

		dbus_message_iter_get_basic(&interface_iter,
						&interface_name);

                retv_if(dbus_message_iter_next(&interface_iter) == FALSE,
								0);

		retv_if(dbus_message_iter_get_arg_type(&interface_iter) !=
					DBUS_TYPE_ARRAY, 0);

		BT_DBG("interface: %s", interface_name);

		if (g_strcmp0(interface_name,
				BT_DEVICE_INTERFACE) == 0) {
			return BT_DEVICE_EVENT;
		} else if (g_strcmp0(interface_name,
				BT_MEDIATRANSPORT_INTERFACE) == 0) {
			return BT_MEDIA_TRANSFER_EVENT;
		} else if (g_strcmp0(interface_name,
				BT_PLAYER_CONTROL_INTERFACE) == 0) {
			return BT_AVRCP_CONTROL_EVENT;
		}
		dbus_message_iter_next(&value_iter);
	}

	return 0;
}

static int __bt_parse_remove_event(DBusMessage *msg)
{
	DBusMessageIter msg_iter;
	DBusMessageIter value_iter;

	retv_if(dbus_message_iter_init(msg, &msg_iter) ==
					FALSE, 0);

	retv_if(dbus_message_iter_next(&msg_iter) == FALSE,
						0);
	retv_if(dbus_message_iter_get_arg_type(&msg_iter) !=
					DBUS_TYPE_ARRAY, 0);

	dbus_message_iter_recurse(&msg_iter, &value_iter);

	while (dbus_message_iter_get_arg_type(&value_iter)
					!= DBUS_TYPE_INVALID) {
		char *key;

		dbus_message_iter_get_basic(&value_iter, &key);

		if (key == NULL) {
			dbus_message_iter_next(&value_iter);
			continue;
		}

		BT_DBG("key = %s", key);

		if (g_strcmp0(key, BT_MEDIATRANSPORT_INTERFACE) == 0) {
			return BT_MEDIA_TRANSFER_EVENT;
		} else if (g_strcmp0(key, BT_DEVICE_INTERFACE) == 0) {
			return BT_DEVICE_EVENT;
		} else if (g_strcmp0(key, BT_PLAYER_CONTROL_INTERFACE) == 0) {
			return BT_AVRCP_CONTROL_EVENT;
		}
		dbus_message_iter_next(&value_iter);
	}

	return 0;
}

static void __bt_devices_list_free(void)
{
	bt_cache_info_t *cache_info;
	GList *node;

	node = g_list_first(p_cache_list);

	while (node != NULL){
		cache_info = (bt_cache_info_t *)node->data;

		p_cache_list = g_list_remove(p_cache_list, cache_info);
		__bt_free_cache_info(cache_info);

		node = g_list_next(node);
	}
}

static DBusHandlerResult __bt_manager_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	const char *member = dbus_message_get_member(msg);
	bt_event_type_t bt_event;
	int result = BLUETOOTH_ERROR_NONE;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, "InterfacesAdded") == 0) {
		char *object_path = NULL;

		BT_DBG("InterfacesAdded");

		if (__bt_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (strcasecmp(object_path, BT_BLUEZ_HCI_PATH) == 0) {
			_bt_handle_adapter_added();
		} else {
			bt_event = __bt_parse_event(msg);
			if (bt_event == BT_DEVICE_EVENT) {
				bt_cache_info_t *cache_info;
				bt_remote_dev_info_t *dev_info;

				retv_if(_bt_is_discovering() == FALSE &&
						_bt_is_le_scanning() == FALSE,
							DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

				cache_info = g_malloc0(sizeof(bt_cache_info_t));
				dev_info = g_malloc0(sizeof(bt_remote_dev_info_t));
				/* Fix : NULL_RETURNS */
				if (cache_info == NULL || dev_info == NULL) {
					BT_ERR("Fail to allocate memory");
					return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
				}
				cache_info->dev_info = dev_info;

				if (__bt_parse_interface(msg, dev_info) == FALSE) {
					BT_ERR("Fail to parse the properies");
					__bt_free_cache_info(cache_info);
					return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
				}

				if (dev_info->addr_type != BDADDR_BREDR) {
					/* Whenever emit the property changed from bluez,
						some property doesn't reach to bt-service.
						So LE device is handled as AdvReport signal */
					__bt_free_cache_info(cache_info);
					return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
				}

				if (dev_info->name == NULL)
					/* If Remote device name is NULL or still RNR is not done
					 * then display address as name.
					 */
					dev_info->name = g_strdup(dev_info->address);

				_bt_send_event(BT_ADAPTER_EVENT,
					BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &dev_info->address,
					DBUS_TYPE_UINT32, &dev_info->class,
					DBUS_TYPE_INT16, &dev_info->rssi,
					DBUS_TYPE_STRING, &dev_info->name,
					DBUS_TYPE_BOOLEAN, &dev_info->paired,
					DBUS_TYPE_UINT32, &dev_info->connected,
					DBUS_TYPE_BOOLEAN, &dev_info->trust,
					DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
					&dev_info->uuids, dev_info->uuid_count,
					DBUS_TYPE_INT16, &dev_info->manufacturer_data_len,
					DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
					&dev_info->manufacturer_data, dev_info->manufacturer_data_len,
					DBUS_TYPE_INVALID);

				p_cache_list = g_list_append(p_cache_list, cache_info);
			} else if (bt_event == BT_AVRCP_CONTROL_EVENT) {
				BT_DBG("Device path : %s ", object_path);
				_bt_set_control_device_path(object_path);
			}
		}
	} else if (strcasecmp(member, "InterfacesRemoved") == 0) {
		bt_event = __bt_parse_remove_event(msg);

		if ((bt_event != 0) && (bt_event != BT_MEDIA_TRANSFER_EVENT)) {
			_bt_handle_adapter_event(msg);
			if (bt_event == BT_AVRCP_CONTROL_EVENT) {
				char *object_path = NULL;
				result = __bt_get_object_path(msg, &object_path);
				if (result == BLUETOOTH_ERROR_NONE)
					_bt_remove_control_device_path(object_path);
			}
		}

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

		if (strcasecmp(name, BT_BLUEZ_NAME) == 0) {
			BT_DBG("Bluetoothd is terminated");
			if (_bt_adapter_get_status() == BT_ACTIVATED)
				 __bt_disable_cb();

			_bt_handle_adapter_removed();
			__bt_devices_list_free();
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

		/* Stop advertising started by terminated process */
		_bt_stop_advertising_by_terminated_process(name);
		/* Stop LE Scan */
		_bt_stop_le_scan(name);
	} else	if (dbus_message_has_interface(msg, BT_PROPERTIES_INTERFACE)) {
		const char *path = dbus_message_get_path(msg);

		if (strncmp(path, BT_MEDIA_OBJECT_PATH,
				strlen(BT_MEDIA_OBJECT_PATH)) == 0)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		_bt_handle_property_changed_event(msg);
	} else  if (dbus_message_has_interface(msg, BT_ADAPTER_INTERFACE)) {
		_bt_handle_adapter_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_INPUT_INTERFACE)) {
		_bt_handle_input_event(msg);
	} else	if (dbus_message_has_interface(msg, BT_NETWORK_SERVER_INTERFACE)) {
		_bt_handle_network_server_event(msg);
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

static gboolean __bt_is_obexd_event(DBusMessage *msg)
{
	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return FALSE;

	retv_if(member == NULL, FALSE);

	if (dbus_message_has_interface(msg, BT_PROPERTIES_INTERFACE)) {

		DBusMessageIter item_iter;
		const char *interface_name = NULL;

		dbus_message_iter_init(msg, &item_iter);

		if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
			BT_ERR("This is bad format dbus");
			return FALSE;
                }

		dbus_message_iter_get_basic(&item_iter, &interface_name);

		retv_if(interface_name == NULL, FALSE);

		BT_DBG("interface: %s", interface_name);

		retv_if(dbus_message_iter_next(&item_iter) == FALSE, FALSE);

		retv_if(dbus_message_iter_get_arg_type(&item_iter) != DBUS_TYPE_ARRAY,
									FALSE);

		if (strcasecmp(interface_name, BT_OBEX_TRANSFER_INTERFACE) == 0) {
			BT_DBG("BT_OBEX_TRANSFER_INTERFACE");
			return TRUE;
		}
	}

	return FALSE;
}

static DBusHandlerResult __bt_obexd_event_filter(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	const char *member = dbus_message_get_member(msg);
	char *object_path = NULL;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, "InterfacesAdded") == 0) {
		BT_DBG("InterfacesAdded");
		if (__bt_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		BT_INFO("object_path = [%s]", object_path);

		/*Handle OPP_SERVER_CONNECTED_EVENT here */
		if (strncmp(object_path, BT_SESSION_BASEPATH_SERVER,
				strlen(BT_SESSION_BASEPATH_SERVER)) != 0)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (g_strrstr(object_path, "session") && g_strrstr(object_path, "transfer")) {
			BT_DBG("Obex_Server_Session_Transfer connected");
			_bt_obex_transfer_connected();
		}
	} else if (strcasecmp(member, "InterfacesRemoved") == 0) {
		/*Handle OPP_SERVER_DISCONNECTED_EVENT here */
		BT_DBG("InterfacesRemoved");
		if (__bt_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		BT_INFO("object_path = [%s]", object_path);

		if (strncmp(object_path, BT_SESSION_BASEPATH_CLIENT,
				strlen(BT_SESSION_BASEPATH_CLIENT)) == 0) {
			BT_DBG("Call PBAP Disconnected");
			_bt_obex_pbap_client_disconnect(object_path);
		}

		if (strncmp(object_path, BT_SESSION_BASEPATH_SERVER,
				strlen(BT_SESSION_BASEPATH_SERVER)) != 0)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (g_strrstr(object_path, "session") && g_strrstr(object_path, "transfer")) {
			BT_DBG("Obex_Server_Session_Transfer disconnected");
			_bt_obex_transfer_disconnected();
		}
	} else if (__bt_is_obexd_event(msg) == TRUE) {
		const char *path = dbus_message_get_path(msg);

		if (strncmp(path, BT_SESSION_BASEPATH_SERVER,
				strlen(BT_SESSION_BASEPATH_SERVER)) != 0 &&
			strncmp(path, BT_SESSION_BASEPATH_CLIENT,
				strlen(BT_SESSION_BASEPATH_CLIENT)) != 0) {
			BT_DBG("DBUS_HANDLER_RESULT_NOT_YET_HANDLED");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_handle_property_changed_event(msg);
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean __bt_is_obexd_client_event(DBusMessage *msg)
{
	BT_DBG("+");

	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return FALSE;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (dbus_message_has_interface(msg, BT_PROPERTIES_INTERFACE)) {
		DBusMessageIter item_iter;
		const char *interface_name = NULL;

		dbus_message_iter_init(msg, &item_iter);

		if (dbus_message_iter_get_arg_type(&item_iter)
						!= DBUS_TYPE_STRING) {
			BT_ERR("This is bad format dbus");
			return FALSE;
		}

		dbus_message_iter_get_basic(&item_iter, &interface_name);

		retv_if(interface_name == NULL, FALSE);

		BT_DBG("interface: %s", interface_name);

		retv_if(dbus_message_iter_next(&item_iter) == FALSE,
								FALSE);

		retv_if(dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_ARRAY, FALSE);

		if (strcasecmp(interface_name,
					BT_OBEX_TRANSFER_INTERFACE) == 0) {
			BT_DBG("-");
			return TRUE;
		}
	}

	BT_DBG("-");

	return FALSE;
}

static DBusHandlerResult __bt_opc_event_filter(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	retv_if(member == NULL, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

	if (strcasecmp(member, "InterfacesAdded") == 0) {
		BT_DBG("InterfacesAdded");
	}else if (strcasecmp(member, "InterfacesRemoved") == 0) {
		char *object_path = NULL;

		BT_DBG("InterfacesRemoved");

		if (__bt_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		BT_DBG("object_path =%s",object_path);

		if (strncmp(object_path, BT_SESSION_BASEPATH_CLIENT,
				strlen(BT_SESSION_BASEPATH_CLIENT)) != 0
				|| strstr(object_path, "transfer") == NULL)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		else if (strncmp(object_path, BT_SESSION_BASEPATH_CLIENT,
				strlen(BT_SESSION_BASEPATH_CLIENT)) == 0) {
			BT_DBG("Going to call opc disconnected");
			_bt_opc_disconnected(object_path);
		}

		_bt_sending_files();

	}else if (__bt_is_obexd_client_event(msg) == TRUE){
		const char *path = dbus_message_get_path(msg);

		if (strncmp(path, BT_SESSION_BASEPATH_CLIENT,
			strlen(BT_SESSION_BASEPATH_CLIENT)) != 0) {
			BT_DBG("NOT BT_SESSION_BASEPATH_CLIENT");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		_bt_opc_property_changed_event(msg);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int _bt_opp_client_event_init(void)
{
	GError *error = NULL;

	if (opc_obexd_conn == NULL) {
		opc_obexd_conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
		if (error != NULL) {
			BT_ERR("ERROR: Can't get on session bus [%s]",
							 error->message);
			g_error_free(error);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		retv_if(opc_obexd_conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	}

	if (_bt_register_service_event(opc_obexd_conn,
			BT_OPP_CLIENT_EVENT) != BLUETOOTH_ERROR_NONE) {
		dbus_g_connection_unref(opc_obexd_conn);
		opc_obexd_conn = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

void _bt_opp_client_event_deinit(void)
{
	if (opc_obexd_conn) {
		_bt_unregister_service_event(opc_obexd_conn,
						BT_OPP_CLIENT_EVENT);
		dbus_g_connection_unref(opc_obexd_conn);
		opc_obexd_conn = NULL;
	}
}

int _bt_register_service_event(DBusGConnection *g_conn, int event_type)
{
	BT_DBG("+");

	DBusError dbus_error;
	char *match1 = NULL;
	char *match2 = NULL;
	char *match3 = NULL;
	char *match4 = NULL;
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
					BT_INTERFACES_ADDED);

		match2 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_INTERFACES_REMOVED);

		match3 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_FREEDESKTOP_INTERFACE,
					BT_NAME_OWNER_CHANGED);

		match4 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_PROPERTIES_INTERFACE,
					BT_PROPERTIES_CHANGED);
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
		BT_ERR("BT_OPP_SERVER_EVENT: register service event");
		event_func = __bt_obexd_event_filter;
		match1 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_PROPERTIES_INTERFACE,
					BT_PROPERTIES_CHANGED);

		match2 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_INTERFACES_ADDED);

		match3 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_INTERFACES_REMOVED);
		break;
	case BT_OPP_CLIENT_EVENT:
		BT_ERR("BT_OPP_CLIENT_EVENT: register service event");
		event_func = __bt_opc_event_filter;
		match1 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_PROPERTIES_INTERFACE,
					BT_PROPERTIES_CHANGED);

		match2 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_INTERFACES_ADDED);

		match3 = g_strdup_printf(MANAGER_EVENT_MATCH_RULE,
					BT_MANAGER_INTERFACE,
					BT_INTERFACES_REMOVED);
		break;
	case BT_A2DP_SOURCE_EVENT:
		BT_INFO("A2dp Source event");
		match1 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_A2DP_SOURCE_INTERFACE);

		match2 = g_strdup_printf(EVENT_MATCH_RULE,
					BT_SINK_INTERFACE);
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

	if (match3)
		dbus_bus_add_match(conn, match3, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	if (match4)
		dbus_bus_add_match(conn, match4, &dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	g_free(match3);
	g_free(match4);

	g_free(match1);
	g_free(match2);

	return BLUETOOTH_ERROR_NONE;
fail:
	g_free(match1);
	g_free(match2);

	g_free(match3);
	g_free(match4);

	BT_DBG("-");

	return BLUETOOTH_ERROR_INTERNAL;
}

void _bt_unregister_service_event(DBusGConnection *g_conn, int event_type)
{
	BT_DBG("+");

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
	case BT_OPP_CLIENT_EVENT:
		event_func = __bt_opc_event_filter;
		break;
	default:
		BT_ERR("Unknown event");
		return;
	}

	ret_if(conn == NULL);

	dbus_connection_remove_filter(conn, event_func, NULL);
	BT_DBG("-");
}

static int __bt_init_manager_receiver(void)
{
	BT_DBG("+");

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

	BT_DBG("-");

	return BLUETOOTH_ERROR_INTERNAL;
}

static int __bt_init_obexd_receiver(void)
{
	BT_DBG("+");
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

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

/* To receive the event from bluez */
int _bt_init_service_event_receiver(void)
{
	BT_DBG("+");

	int result;

	result = __bt_init_manager_receiver();
	retv_if(result != BLUETOOTH_ERROR_NONE, result);

	result = __bt_init_obexd_receiver();
	if (result != BLUETOOTH_ERROR_NONE)
		BT_ERR("Fail to init obexd receiver");

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}

void _bt_deinit_service_event_receiver(void)
{
	BT_DBG("+");

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

	BT_DBG("-");
}
