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

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <syspopup_caller.h>

#include "bluetooth-api.h"
#include "bt-internal-types.h"

#include "bt-service-common.h"
#include "bt-service-avrcp.h"
#include "bt-service-event.h"
#include "bt-service-util.h"
#include "bt-service-audio.h"

static bt_player_settinngs_t loopstatus_settings[] = {
	{ REPEAT_INVALID, "" },
	{ REPEAT_MODE_OFF, "None" },
	{ REPEAT_SINGLE_TRACK, "Track" },
	{ REPEAT_ALL_TRACK, "Playlist" },
	{ REPEAT_INVALID, "" }
};

static bt_player_settinngs_t shuffle_settings[] = {
	{ SHUFFLE_INVALID, "" },
	{ SHUFFLE_MODE_OFF, "off" },
	{ SHUFFLE_ALL_TRACK, "alltracks" },
	{ SHUFFLE_GROUP, "group" },
	{ SHUFFLE_INVALID, "" }
};

static bt_player_settinngs_t player_status[] = {
	{ STATUS_STOPPED, "stopped" },
	{ STATUS_PLAYING, "playing" },
	{ STATUS_PAUSED, "paused" },
	{ STATUS_FORWARD_SEEK, "forward-seek" },
	{ STATUS_REVERSE_SEEK, "reverse-seek" },
	{ STATUS_ERROR, "error" },
	{ STATUS_INVALID, "" }
};

DBusConnection *g_bt_dbus_conn = NULL;

static DBusHandlerResult _bt_avrcp_handle_set_property(DBusConnection *connection,
				DBusMessage *message, void *user_data)
{
	BT_DBG("+");
	const gchar *value;
	unsigned int status;
	gboolean shuffle_status;
	DBusMessageIter args;
	const char *property = NULL;
	const char *interface = NULL;
	DBusMessage *reply = NULL;
	DBusHandlerResult result = DBUS_HANDLER_RESULT_HANDLED;
	DBusMessageIter entry;
	int type;


	dbus_message_iter_init(message, &args);
	dbus_message_iter_get_basic(&args, &interface);
	dbus_message_iter_next(&args);

	if (g_strcmp0(interface, BT_MEDIA_PLAYER_INTERFACE) != 0) {
		result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		goto finish;
	}

	dbus_message_iter_get_basic(&args, &property);
	dbus_message_iter_next(&args);
	dbus_message_iter_recurse(&args, &entry);
	type = dbus_message_iter_get_arg_type(&entry);

	BT_DBG("property %s\n", property);

	if (g_strcmp0(property, "Shuffle") == 0) {
		if (type != DBUS_TYPE_BOOLEAN) {
			BT_DBG("Error");
			reply = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					"Invalid arguments");
			dbus_connection_send(connection, reply, NULL);
			result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			goto finish;
		}
		dbus_message_iter_get_basic(&entry, &shuffle_status);
		BT_DBG("value %d\n", shuffle_status);
		if (shuffle_status == TRUE)
			status = SHUFFLE_ALL_TRACK;
		else
			status = SHUFFLE_MODE_OFF;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);

	} else if (g_strcmp0(property, "LoopStatus") == 0) {
		if (type != DBUS_TYPE_STRING) {
			BT_DBG("Error");
			reply = dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS,
					"Invalid arguments");
			dbus_connection_send(connection, reply, NULL);
			result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			goto finish;
		}
		dbus_message_iter_get_basic(&entry, &value);
		BT_DBG("value %s\n", value);

		if (g_strcmp0(value, "Track") == 0)
			status = REPEAT_SINGLE_TRACK;
		else if (g_strcmp0(value, "Playlist") == 0)
			status = REPEAT_ALL_TRACK;
		else if (g_strcmp0(value, "None") == 0)
			status = REPEAT_MODE_OFF;
		else
			status = REPEAT_INVALID;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);
	}
finish:
	if (reply)
		dbus_message_unref(reply);

	return result;
}

static DBusHandlerResult _bt_avrcp_message_handle(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	BT_DBG("+");

	if (dbus_message_is_method_call(msg, DBUS_INTERFACE_PROPERTIES, "Set"))
		return _bt_avrcp_handle_set_property(conn, msg, user_data);

	BT_DBG("-");
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusObjectPathVTable bt_object_table = {
        .message_function       = _bt_avrcp_message_handle,
};

gboolean bt_dbus_register_object_path(DBusConnection *connection,
						const char *path)
{
	if (!dbus_connection_register_object_path(connection, path,
				&bt_object_table, NULL))
		return FALSE;
	return TRUE;
}

void bt_dbus_unregister_object_path(DBusConnection *connection,
						const char *path)
{
	dbus_connection_unregister_object_path(connection, path);
}

static void __bt_media_append_variant(DBusMessageIter *iter,
			int type, void *value)
{
	char sig[2] = { type, '\0'};
	DBusMessageIter value_iter;

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, sig,
							&value_iter);

	dbus_message_iter_append_basic(&value_iter, type, value);

	dbus_message_iter_close_container(iter, &value_iter);
}

static void __bt_media_append_dict_entry(DBusMessageIter *iter,
			const char *key, int type, void *property)
{
	DBusMessageIter dict_entry;
	const char *str_ptr;

	if (type == DBUS_TYPE_STRING) {
		str_ptr = *((const char **)property);
		ret_if(str_ptr == NULL);
	}

	dbus_message_iter_open_container(iter,
					DBUS_TYPE_DICT_ENTRY,
					NULL, &dict_entry);

	dbus_message_iter_append_basic(&dict_entry, DBUS_TYPE_STRING, &key);

	__bt_media_append_variant(&dict_entry, type, property);

	dbus_message_iter_close_container(iter, &dict_entry);
}

static gboolean __bt_media_emit_property_changed(
                                DBusConnection *connection,
                                const char *path,
                                const char *interface,
                                const char *name,
                                int type,
                                void *property)
{
	DBusMessage *sig;
	DBusMessageIter entry, dict;
	gboolean ret;

	sig = dbus_message_new_signal(path, DBUS_INTERFACE_PROPERTIES,
						"PropertiesChanged");
	retv_if(sig == NULL, FALSE);

	dbus_message_iter_init_append(sig, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &interface);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	__bt_media_append_dict_entry(&dict,
					name, type, property);

	dbus_message_iter_close_container(&entry, &dict);

	ret = dbus_connection_send(connection, sig, NULL);
	dbus_message_unref(sig);

	return ret;
}

int _bt_register_media_player(void)
{
	BT_DBG("+");
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter property_dict;
	DBusError err;
	char *object;
	char *adapter_path;
	DBusConnection *conn;
	DBusGConnection *gconn;
	gboolean shuffle_status;

	media_player_settings_t player_settings = {0,};

	player_settings.repeat  = REPEAT_MODE_OFF;

	player_settings.shuffle = SHUFFLE_MODE_OFF;
	player_settings.status = STATUS_STOPPED;
	player_settings.position = 0;


	gconn = _bt_get_system_gconn();
	retv_if(gconn  == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	g_bt_dbus_conn = conn;


	if (!bt_dbus_register_object_path(conn, BT_MEDIA_OBJECT_PATH)){
		BT_DBG("Could not register interface %s",
				BT_MEDIA_PLAYER_INTERFACE);
	}

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
				BT_MEDIA_INTERFACE, "RegisterPlayer");

	g_free(adapter_path);

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	object = g_strdup(BT_MEDIA_OBJECT_PATH);

	dbus_message_iter_init_append(msg, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &object);
	g_free(object);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &property_dict);

	__bt_media_append_dict_entry(&property_dict,
		"LoopStatus",
		DBUS_TYPE_STRING,
		&loopstatus_settings[player_settings.repeat].property);

	if (player_settings.shuffle == SHUFFLE_MODE_OFF)
		shuffle_status = FALSE;
	else
		shuffle_status = TRUE;

	__bt_media_append_dict_entry(&property_dict,
		"Shuffle",
		DBUS_TYPE_BOOLEAN,
		&shuffle_status);

	__bt_media_append_dict_entry(&property_dict,
		"PlaybackStatus",
		DBUS_TYPE_STRING,
		&player_status[player_settings.status].property);


	__bt_media_append_dict_entry(&property_dict,
		"Position",
		DBUS_TYPE_UINT32, &player_settings.position);

	dbus_message_iter_close_container(&iter, &property_dict);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
				msg, -1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Error in registering the Music Player \n");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}

	if (reply)
		dbus_message_unref(reply);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_media_player(void)
{
	BT_DBG("+");
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	char *object;
	char *adapter_path;
	DBusConnection *conn;

	conn = g_bt_dbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
				BT_MEDIA_INTERFACE, "UnregisterPlayer");


	g_free(adapter_path);

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	object = g_strdup(BT_MEDIA_OBJECT_PATH);

	dbus_message_append_args(msg,
				DBUS_TYPE_OBJECT_PATH, &object,
				DBUS_TYPE_INVALID);

	g_free(object);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
				msg, -1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Error in unregistering the Music Player \n");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	} else {
		dbus_message_unref(reply);
	}

	bt_dbus_unregister_object_path(conn, BT_MEDIA_OBJECT_PATH);
	g_bt_dbus_conn = NULL;

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

static void __bt_media_append_metadata_entry(DBusMessageIter *metadata,
			void *key_type, void *value, int type)
{
	BT_DBG("+");
	DBusMessageIter string_entry;

	dbus_message_iter_open_container(metadata,
				DBUS_TYPE_DICT_ENTRY,
				NULL, &string_entry);

	dbus_message_iter_append_basic(&string_entry, DBUS_TYPE_STRING, key_type);

	__bt_media_append_variant(&string_entry, type, value);

	dbus_message_iter_close_container(metadata, &string_entry);
	BT_DBG("-");
}

static void __bt_media_append_metadata_array(DBusMessageIter *metadata,
			void *key_type, void *value, int type)
{
	BT_DBG("+");
	DBusMessageIter string_entry, variant, array;
	char array_sig[3] = { type, DBUS_TYPE_STRING, '\0' };

	dbus_message_iter_open_container(metadata,
				DBUS_TYPE_DICT_ENTRY,
				NULL, &string_entry);
	dbus_message_iter_append_basic(&string_entry, DBUS_TYPE_STRING, key_type);

	dbus_message_iter_open_container(&string_entry, DBUS_TYPE_VARIANT,
			array_sig, &variant);

	dbus_message_iter_open_container(&variant, type,
				DBUS_TYPE_STRING_AS_STRING, &array);
	dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, value);

	dbus_message_iter_close_container(&variant, &array);
	dbus_message_iter_close_container(&string_entry, &variant);
	dbus_message_iter_close_container(metadata, &string_entry);
	BT_DBG("-");
}

int _bt_avrcp_set_track_info(media_metadata_attributes_t *meta_data)
{
	BT_DBG("+");
	DBusMessage *sig;
	DBusMessageIter iter;
	DBusMessageIter property_dict, metadata_dict, metadata_variant, metadata;
	DBusConnection *conn;
	char *interface = BT_MEDIA_PLAYER_INTERFACE;
	char * metadata_str = "Metadata";
	const char *key_type;

	retv_if(meta_data == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = g_bt_dbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	sig = dbus_message_new_signal(BT_MEDIA_OBJECT_PATH, DBUS_INTERFACE_PROPERTIES,
				"PropertiesChanged");
	retv_if(sig == NULL, FALSE);

	dbus_message_iter_init_append(sig, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &property_dict);

	dbus_message_iter_open_container(&property_dict,
				DBUS_TYPE_DICT_ENTRY,
				NULL, &metadata_dict);

	dbus_message_iter_append_basic(&metadata_dict, DBUS_TYPE_STRING, &metadata_str);

	dbus_message_iter_open_container(&metadata_dict, DBUS_TYPE_VARIANT, "a{sv}",
				&metadata_variant);

	dbus_message_iter_open_container(&metadata_variant, DBUS_TYPE_ARRAY,
				DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
				DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
				DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &metadata);

	if (meta_data->title) {
		key_type = "xesam:title";
		__bt_media_append_metadata_entry(&metadata, &key_type,
				&meta_data->title, DBUS_TYPE_STRING);
	}

	if (meta_data->artist) {
		key_type = "xesam:artist";
		__bt_media_append_metadata_array(&metadata, &key_type,
				&meta_data->artist, DBUS_TYPE_ARRAY);
	}

	if (meta_data->album) {
		key_type = "xesam:album";
		__bt_media_append_metadata_entry(&metadata, &key_type,
				&meta_data->album, DBUS_TYPE_STRING);
	}

	if (meta_data->genre) {
		key_type = "xesam:genre";
		__bt_media_append_metadata_array(&metadata, &key_type,
				&meta_data->genre, DBUS_TYPE_ARRAY);
	}

	if (0 != meta_data->total_tracks) {
		key_type = "xesam:totalTracks";
		__bt_media_append_metadata_entry(&metadata, &key_type,
				&meta_data->total_tracks, DBUS_TYPE_INT32);
	}

	if (0 != meta_data->number) {
		key_type = "xesam:trackNumber";
		__bt_media_append_metadata_entry(&metadata, &key_type,
				&meta_data->number, DBUS_TYPE_INT32);
	}

	if (0 != meta_data->duration) {
		key_type = "mpris:length";
		__bt_media_append_metadata_entry(&metadata, &key_type,
				&meta_data->duration, DBUS_TYPE_INT64);
	}

	dbus_message_iter_close_container(&metadata_variant, &metadata);
	dbus_message_iter_close_container(&metadata_dict, &metadata_variant);
	dbus_message_iter_close_container(&property_dict, &metadata_dict);
	dbus_message_iter_close_container(&iter, &property_dict);

	if (!dbus_connection_send(conn, sig, NULL))
		BT_ERR("Unable to send TrackChanged signal\n");

	dbus_message_unref(sig);
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}


int _bt_avrcp_set_interal_property(int type, media_player_settings_t *properties)
{
	BT_DBG("+");
	DBusConnection *conn;
	int value;
	media_metadata_attributes_t meta_data;
	dbus_bool_t shuffle;

	conn = g_bt_dbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	switch (type) {
	case REPEAT:
		value = properties->repeat;
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"LoopStatus",
			DBUS_TYPE_STRING,
			&loopstatus_settings[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case SHUFFLE:
		value = properties->shuffle;
		if (g_strcmp0(shuffle_settings[value].property, "off") == 0)
			shuffle = 0;
		else
			shuffle = 1;

		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Shuffle",
			DBUS_TYPE_BOOLEAN,
			&shuffle)) {
			BT_DBG("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case STATUS:
		value = properties->status;
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"PlaybackStatus",
			DBUS_TYPE_STRING,
			&player_status[value].property)) {
			BT_DBG("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case POSITION:
		value = properties->position;
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Position",
			DBUS_TYPE_UINT32,
			&value)) {
			BT_DBG("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case METADATA:
		meta_data = properties->metadata;
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Metadata",
			DBUS_TYPE_ARRAY,
			&meta_data)) {
			BT_DBG("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	default:
		BT_DBG("Invalid Type\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_properties(media_player_settings_t *properties)
{
	BT_DBG("+");

	if (_bt_avrcp_set_interal_property(REPEAT,
				properties) != BLUETOOTH_ERROR_NONE) {
			return BLUETOOTH_ERROR_INTERNAL;
	}
	if (_bt_avrcp_set_interal_property(SHUFFLE,
			properties) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_interal_property(STATUS,
			properties) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_interal_property(POSITION,
			properties) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_interal_property(METADATA,
			properties) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_property(int type, unsigned int value)
{
	BT_DBG("+");
	media_player_settings_t properties;

	switch (type) {
	case REPEAT:
		properties.repeat = value;
		break;
	case SHUFFLE:
		properties.shuffle = value;
		break;
	case STATUS:
		properties.status = value;
		break;
	case POSITION:
		properties.position = value;
		break;
	default:
		BT_DBG("Invalid Type\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_interal_property(type,
			&properties) != BLUETOOTH_ERROR_NONE)
		return BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("-");

	return BLUETOOTH_ERROR_NONE;
}
