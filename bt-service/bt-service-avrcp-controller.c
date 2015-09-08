/*
 * Bluetooth-frwk
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Hocheol Seo <hocheol.seo@samsung.com>
 *		 Chanyeol Park <chanyeol.park@samsung.com>
 *		 Rakesh M K <rakesh.mk@samsung.com>
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

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-avrcp-controller.h"
#include "bt-service-audio.h"
#include "bt-service-event.h"

static bt_player_settinngs_t repeat_status[] = {
	{ REPEAT_INVALID, "" },
	{ REPEAT_MODE_OFF, "off" },
	{ REPEAT_SINGLE_TRACK, "singletrack" },
	{ REPEAT_ALL_TRACK, "alltracks" },
	{ REPEAT_GROUP, "group" },
	{ REPEAT_INVALID, "" }
};

static bt_player_settinngs_t equalizer_status[] = {
	{ EQUALIZER_INVALID, "" },
	{ EQUALIZER_OFF, "off" },
	{ EQUALIZER_ON, "on" },
	{ EQUALIZER_INVALID, "" },
};

static bt_player_settinngs_t scan_status[] = {
	{ SCAN_INVALID, "" },
	{ SCAN_MODE_OFF, "off" },
	{ SCAN_ALL_TRACK, "alltracks" },
	{ SCAN_GROUP, "group" },
	{ SCAN_INVALID, "" },
};

static bt_player_settinngs_t shuffle_settings[] = {
	{ SHUFFLE_INVALID, "" },
	{ SHUFFLE_MODE_OFF, "off" },
	{ SHUFFLE_ALL_TRACK, "alltracks" },
	{ SHUFFLE_GROUP, "group" },
	{ SHUFFLE_INVALID, "" }
};

static char *avrcp_control_path = NULL;

void _bt_set_control_device_path(const char *path)
{

	ret_if(path == NULL);

	g_free(avrcp_control_path);
	BT_DBG("control_path = %s", path);
	avrcp_control_path = g_strdup(path);
}

void _bt_remove_control_device_path(const char *path)
{
	ret_if(path == NULL);

	if (avrcp_control_path &&
			!g_strcmp0(avrcp_control_path, path)) {
		BT_DBG("control_path = %s", path);
		g_free(avrcp_control_path);
		avrcp_control_path = NULL;
	}
}

static char *__bt_get_control_device_path(void)
{
	char *adapter_path;
	char *control_path;
	char connected_address[BT_ADDRESS_STRING_SIZE + 1];

	BT_DBG("+");

	retv_if(avrcp_control_path != NULL, avrcp_control_path);

	retv_if(!_bt_is_headset_type_connected(BT_AVRCP,
			connected_address), NULL);

	BT_DBG("device address = %s", connected_address);

	adapter_path = _bt_get_device_object_path(connected_address);
	retv_if(adapter_path == NULL, NULL);

	control_path = g_strdup_printf(BT_MEDIA_CONTROL_PATH, adapter_path);
	g_free(adapter_path);

	avrcp_control_path = control_path;
	BT_DBG("control_path = %s", control_path);
	return control_path;
}

static int __bt_media_send_control_msg(const char *name)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	DBusConnection *conn;
	char *control_path;

	retv_if(name == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);
	BT_DBG("control_path %s", control_path);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, control_path,
				BT_PLAYER_CONTROL_INTERFACE, name);

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
				msg, -1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Error in Sending Control Command");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_control_cmd(int type)
{
	int ret = BLUETOOTH_ERROR_INTERNAL;
	BT_DBG("+");

	switch (type) {
	case PLAY:
		ret = __bt_media_send_control_msg("Play");
		break;
	case PAUSE:
		ret = __bt_media_send_control_msg("Pause");
		break;
	case STOP:
		ret = __bt_media_send_control_msg("Stop");
		break;
	case NEXT:
		ret = __bt_media_send_control_msg("Next");
		break;
	case PREVIOUS:
		ret = __bt_media_send_control_msg("Previous");
		break;
	case FAST_FORWARD:
		ret = __bt_media_send_control_msg("FastForward");
		break;
	case REWIND:
		ret = __bt_media_send_control_msg("Rewind");
		break;
	default:
		BT_DBG("Invalid Type\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return ret;
}

DBusGProxy *__bt_get_control_properties_proxy(void)
{
	DBusGProxy *proxy;
	char *control_path;
	DBusGConnection *conn;

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, NULL);
	BT_DBG("control_path = %s", control_path);

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, NULL);

	proxy = dbus_g_proxy_new_for_name(conn, BT_BLUEZ_NAME,
				control_path, BT_PROPERTIES_INTERFACE);
	return proxy;
}

static int __bt_media_attr_to_event(const char *str)
{
	if (!strcasecmp(str, "Equalizer"))
		return BLUETOOTH_EVENT_AVRCP_CONTROL_EQUALIZER_STATUS;
	else if (!strcasecmp(str, "Repeat"))
		return BLUETOOTH_EVENT_AVRCP_CONTROL_REPEAT_STATUS;
	else if (!strcasecmp(str, "Shuffle"))
		return BLUETOOTH_EVENT_AVRCP_CONTROL_SHUFFLE_STATUS;
	else if (!strcasecmp(str, "Scan"))
		return BLUETOOTH_EVENT_AVRCP_CONTROL_SCAN_STATUS;
	else if (!strcasecmp(str, "Position"))
		return BLUETOOTH_EVENT_AVRCP_SONG_POSITION_STATUS;
	else if (!strcasecmp(str, "Track"))
		return BLUETOOTH_EVENT_AVRCP_TRACK_CHANGED;
	else if (!strcasecmp(str, "Status"))
		return BLUETOOTH_EVENT_AVRCP_PLAY_STATUS_CHANGED;

	return 0;
}

static int __bt_media_attr_to_type(const char *str)
{
	if (!strcasecmp(str, "Equalizer"))
		return EQUALIZER;
	else if (!strcasecmp(str, "Repeat"))
		return REPEAT;
	else if (!strcasecmp(str, "Shuffle"))
		return SHUFFLE;
	else if (!strcasecmp(str, "Scan"))
		return SCAN;
	else if (!strcasecmp(str, "Position"))
		return POSITION;
	else if (!strcasecmp(str, "Track"))
		return METADATA;
	else if (!strcasecmp(str, "Status"))
		return STATUS;

	return 0;
}

static const char *__bt_media_type_to_str(int type)
{
	switch (type) {
	case EQUALIZER:
		return "Equalizer";
	case REPEAT:
		return "Repeat";
	case SHUFFLE:
		return "Shuffle";
	case SCAN:
		return "Scan";
	case POSITION:
		return "Position";
	case METADATA:
		return "Track";
	case STATUS:
		return "Status";
	}
	return NULL;
}

static int __bt_media_attrval_to_val(int type, const char *value)
{
	int ret = 0;

	switch (type) {
	case EQUALIZER:
		if (!strcmp(value, "off"))
			ret = EQUALIZER_OFF;
		else if (!strcmp(value, "on"))
			ret = EQUALIZER_ON;
		else
			ret = EQUALIZER_INVALID;
		break;

	case REPEAT:
		if (!strcmp(value, "off"))
			ret = REPEAT_MODE_OFF;
		else if (!strcmp(value, "singletrack"))
			ret = REPEAT_SINGLE_TRACK;
		else if (!strcmp(value, "alltracks"))
			ret = REPEAT_ALL_TRACK;
		else if (!strcmp(value, "group"))
			ret = REPEAT_GROUP;
		else
			ret = REPEAT_INVALID;
		break;

	case SHUFFLE:
		if (!strcmp(value, "off"))
			ret = SHUFFLE_MODE_OFF;
		else if (!strcmp(value, "alltracks"))
			ret = SHUFFLE_ALL_TRACK;
		else if (!strcmp(value, "group"))
			ret = SHUFFLE_GROUP;
		else
			ret = SHUFFLE_INVALID;
		break;

	case SCAN:
		if (!strcmp(value, "off"))
			ret = SCAN_MODE_OFF;
		else if (!strcmp(value, "alltracks"))
			ret = SCAN_ALL_TRACK;
		else if (!strcmp(value, "group"))
			ret = SCAN_GROUP;
		else
			ret = SCAN_INVALID;
		break;

	case STATUS:
		if (!strcmp(value, "stopped"))
			ret = STATUS_STOPPED;
		else if (!strcmp(value, "playing"))
			ret = STATUS_PLAYING;
		else if (!strcmp(value, "paused"))
			ret = STATUS_PAUSED;
		else if (!strcmp(value, "forward-seek"))
			ret = STATUS_FORWARD_SEEK;
		else if (!strcmp(value, "reverse-seek"))
			ret = STATUS_REVERSE_SEEK;
		else if (!strcmp(value, "error"))
			ret = STATUS_ERROR;
		else
			ret = STATUS_INVALID;
	}
	return ret;
}

int _bt_avrcp_control_get_property(int type, unsigned int *value)
{
	DBusGProxy *proxy;
	char *name = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	GError *err = NULL;
	GValue attr_value = { 0 };

	BT_CHECK_PARAMETER(value, return);

	proxy = __bt_get_control_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);

	if (!dbus_g_proxy_call(proxy, "Get", &err,
			G_TYPE_STRING, BT_PLAYER_CONTROL_INTERFACE,
			G_TYPE_STRING, __bt_media_type_to_str(type),
			G_TYPE_INVALID,
			G_TYPE_VALUE, &attr_value,
			G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("Getting property failed: [%s]\n", err->message);
			g_error_free(err);
		}
		g_object_unref(proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	g_object_unref(proxy);

	switch (type) {
	case EQUALIZER:
	case REPEAT:
	case SHUFFLE:
	case SCAN:
	case STATUS:
		name = (char *)g_value_get_string(&attr_value);
		*value = __bt_media_attrval_to_val(type, name);
		BT_DBG("Type[%s] and Value[%s]", __bt_media_type_to_str(type), name);
		break;
	case POSITION:
		*value = g_value_get_uint(&attr_value);
		break;
	default:
		BT_DBG("Invalid Type\n");
		ret =  BLUETOOTH_ERROR_INTERNAL;
	}

	return ret;
}

int _bt_avrcp_control_set_property(int type, unsigned int value)
{
	GValue attr_value = { 0 };
	DBusGProxy *proxy;
	GError *error = NULL;

	proxy = __bt_get_control_properties_proxy();

	retv_if(proxy == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);
	g_value_init(&attr_value, G_TYPE_STRING);

	switch (type) {
	case EQUALIZER:
		g_value_set_string(&attr_value, equalizer_status[value].property);
		BT_DBG("equalizer_status %s", equalizer_status[value].property);
		break;
	case REPEAT:
		g_value_set_string(&attr_value, repeat_status[value].property);
		BT_DBG("repeat_status %s", repeat_status[value].property);
		break;
	case SHUFFLE:
		g_value_set_string(&attr_value, shuffle_settings[value].property);
		BT_DBG("shuffle_settings %s", shuffle_settings[value].property);
		break;
	case SCAN:
		g_value_set_string(&attr_value, scan_status[value].property);
		BT_DBG("scan_status %s", scan_status[value].property);
		break;
	default:
		BT_ERR("Invalid property type: %d", type);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_g_proxy_call(proxy, "Set", &error,
			G_TYPE_STRING, BT_PLAYER_CONTROL_INTERFACE,
			G_TYPE_STRING, __bt_media_type_to_str(type),
			G_TYPE_VALUE, &attr_value,
			G_TYPE_INVALID, G_TYPE_INVALID);

	g_value_unset(&attr_value);
	g_object_unref(proxy);

	if (error) {
		BT_ERR("SetProperty Fail: %s", error->message);
		g_error_free(error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

static gboolean __bt_avrcp_control_parse_metadata(
					char **value_string,
					unsigned int *value_uint,
					int type,
					DBusMessageIter *iter)
{
	if (dbus_message_iter_get_arg_type(iter) != type)
		return FALSE;

	if (type == DBUS_TYPE_STRING) {
		char *value;
		dbus_message_iter_get_basic(iter, &value);
		*value_string = g_strdup(value);
	} else if (type == DBUS_TYPE_UINT32) {
		int value;
		dbus_message_iter_get_basic(iter, &value);
		*value_uint = value;
	} else
		return FALSE;

	return TRUE;
}


static int __bt_avrcp_control_parse_properties(
				media_metadata_attributes_t *metadata,
				DBusMessageIter *iter)
{
	DBusMessageIter dict;
	DBusMessageIter var;
	int ctype;
	char *value_string;
	unsigned int value_uint;

	ctype = dbus_message_iter_get_arg_type(iter);
	if (ctype != DBUS_TYPE_ARRAY) {
		BT_ERR("ctype error %d", ctype);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_recurse(iter, &dict);

	while ((ctype = dbus_message_iter_get_arg_type(&dict)) !=
							DBUS_TYPE_INVALID) {
		DBusMessageIter entry;
		const char *key;

		if (ctype != DBUS_TYPE_DICT_ENTRY) {
			BT_ERR("ctype error %d", ctype);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		dbus_message_iter_recurse(&dict, &entry);
		if (dbus_message_iter_get_arg_type(&entry) !=
							DBUS_TYPE_STRING) {
			BT_ERR("ctype not DBUS_TYPE_STRING");
			return BLUETOOTH_ERROR_INTERNAL;
		}

		dbus_message_iter_get_basic(&entry, &key);
		dbus_message_iter_next(&entry);

		if (dbus_message_iter_get_arg_type(&entry) !=
							DBUS_TYPE_VARIANT) {
			BT_ERR("ctype not DBUS_TYPE_VARIANT");
			return FALSE;
		}

		dbus_message_iter_recurse(&entry, &var);

		BT_ERR("Key value is %s", key);

		if (strcasecmp(key, "Title") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_STRING, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			BT_DBG("Value : %s ", value_string);
			metadata->title = value_string;
		} else if (strcasecmp(key, "Artist") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_STRING, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			BT_DBG("Value : %s ", value_string);
			metadata->artist = value_string;
		} else if (strcasecmp(key, "Album") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_STRING, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			BT_DBG("Value : %s ", value_string);
			metadata->album = value_string;
		} else if (strcasecmp(key, "Genre") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_STRING, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			BT_DBG("Value : %s ", value_string);
			metadata->genre = value_string;
		} else if (strcasecmp(key, "Duration") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_UINT32, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			metadata->duration = value_uint;
		} else if (strcasecmp(key, "NumberOfTracks") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_UINT32, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			metadata->total_tracks = value_uint;
		} else if (strcasecmp(key, "TrackNumber") == 0) {
			if (!__bt_avrcp_control_parse_metadata(&value_string,
					&value_uint, DBUS_TYPE_UINT32, &var))
				return BLUETOOTH_ERROR_INTERNAL;
			metadata->number = value_uint;
		} else
			BT_DBG("%s not supported, ignoring", key);
		dbus_message_iter_next(&dict);
	}

	if (!metadata->title)
		metadata->title = g_strdup("");
	if (!metadata->artist)
		metadata->artist = g_strdup("");
	if (!metadata->album)
		metadata->album = g_strdup("");
	if (!metadata->genre)
		metadata->genre = g_strdup("");

	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_control_get_track_info(media_metadata_attributes_t *metadata)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	DBusConnection *conn;
	char *control_path;
	char *interface_name;
	char *property_name;
	DBusMessageIter arr, iter;
	int ret = BLUETOOTH_ERROR_NONE;

	retv_if(metadata == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);
	BT_DBG("control_path %s", control_path);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, control_path,
				BT_PROPERTIES_INTERFACE, "Get");

	retv_if(msg == NULL, BLUETOOTH_ERROR_INTERNAL);

	interface_name = g_strdup(BT_PLAYER_CONTROL_INTERFACE);
	property_name = g_strdup("Track");

	dbus_message_append_args(msg,
		DBUS_TYPE_STRING, &interface_name,
		DBUS_TYPE_STRING, &property_name,
		DBUS_TYPE_INVALID);

	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(conn,
				msg, -1, &err);

	g_free(interface_name);
	g_free(property_name);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Error in getting Metadata");
		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &arr);

	ret = __bt_avrcp_control_parse_properties(metadata, &arr);
	dbus_message_unref(reply);

	BT_DBG("-");
	return ret;
}

void _bt_handle_avrcp_control_event(DBusMessageIter *msg_iter, const char *path)
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

	BT_DBG("property : %s ", property);
	ret_if(!dbus_message_iter_next(&dict_iter));

	if ((strcasecmp(property, "Equalizer") == 0) ||
		(strcasecmp(property, "Repeat") == 0) ||
		(strcasecmp(property, "Shuffle") == 0) ||
		(strcasecmp(property, "Scan") == 0) ||
		(strcasecmp(property, "Status") == 0)) {

		const char *valstr;
		int type, value;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &valstr);
		BT_DBG("Value : %s ", valstr);
		type = __bt_media_attr_to_type(property);
		value = __bt_media_attrval_to_val(type, valstr);

				/* Send event to application */
		_bt_send_event(BT_AVRCP_CONTROL_EVENT,
			__bt_media_attr_to_event(property),
			DBUS_TYPE_UINT32, &value,
			DBUS_TYPE_INVALID);
	} else if (strcasecmp(property, "Position") == 0) {
		unsigned int value;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &value);
		BT_DBG("Value : %d ", value);

				/* Send event to application */
		_bt_send_event(BT_AVRCP_CONTROL_EVENT,
			__bt_media_attr_to_event(property),
			DBUS_TYPE_UINT32, &value,
			DBUS_TYPE_INVALID);
	} else if (strcasecmp(property, "Track") == 0) {
		int ret = BLUETOOTH_ERROR_NONE;
		media_metadata_attributes_t metadata;

		dbus_message_iter_recurse(&dict_iter, &value_iter);
		memset(&metadata, 0x00, sizeof(media_metadata_attributes_t));

		ret = __bt_avrcp_control_parse_properties(
							&metadata, &value_iter);
		if (BLUETOOTH_ERROR_NONE != ret)
			return;

				/* Send event to application */
		_bt_send_event(BT_AVRCP_CONTROL_EVENT,
			BLUETOOTH_EVENT_AVRCP_TRACK_CHANGED,
			DBUS_TYPE_STRING, &metadata.title,
			DBUS_TYPE_STRING, &metadata.artist,
			DBUS_TYPE_STRING, &metadata.album,
			DBUS_TYPE_STRING, &metadata.genre,
			DBUS_TYPE_UINT32, &metadata.total_tracks,
			DBUS_TYPE_UINT32, &metadata.number,
			DBUS_TYPE_UINT32, &metadata.duration,
			DBUS_TYPE_INVALID);

		g_free((char *)metadata.title);
		g_free((char *)metadata.artist);
		g_free((char *)metadata.album);
		g_free((char *)metadata.genre);
	} else {
		BT_DBG("Preprty not handled");
	}

	BT_DBG("-");
}
