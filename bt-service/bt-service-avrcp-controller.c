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
	GVariant *reply = NULL;
	GError *err = NULL;
	GDBusConnection *conn = NULL;
	GDBusProxy *proxy = NULL;
	char *control_path = NULL;

	retv_if(name == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);
	BT_DBG("control_path %s", control_path);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, control_path,
			BT_PLAYER_CONTROL_INTERFACE, NULL, &err);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy \n");
		if (err) {
			BT_ERR("%s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	reply = g_dbus_proxy_call_sync(proxy, name, NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

	g_object_unref(proxy);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			BT_ERR("%s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(reply);

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

GDBusProxy *__bt_get_control_properties_proxy(void)
{
	GDBusProxy *proxy = NULL;
	GError *error = NULL;
	char *control_path = NULL;
	GDBusConnection *conn = NULL;

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, NULL);
	BT_DBG("control_path = %s", control_path);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, control_path,
			BT_PROPERTIES_INTERFACE, NULL, &error);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy");
		if (error) {
			BT_ERR("%s", error->message);
			g_clear_error(&error);
		}
		return NULL;
	}

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
	GDBusProxy *proxy = NULL;
	char *name = NULL;
	int ret = BLUETOOTH_ERROR_NONE;
	GError *err = NULL;
	GVariant *reply = NULL;

	BT_CHECK_PARAMETER(value, return);

	proxy = __bt_get_control_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);

	reply = g_dbus_proxy_call_sync(proxy,
					"Get", g_variant_new("ss", BT_PLAYER_CONTROL_INTERFACE, __bt_media_type_to_str(type)),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

	g_object_unref(proxy);

	if (!reply) {
		BT_ERR("Can't get managed objects");
		if (err) {
			BT_ERR("%s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	} else {
			switch (type) {
			case EQUALIZER:
			case REPEAT:
			case SHUFFLE:
			case SCAN:
			case STATUS:
				name =(char *)g_variant_get_string(reply, NULL);
				*value = __bt_media_attrval_to_val(type, name);
				BT_DBG("Type[%s] and Value[%s]", __bt_media_type_to_str(type), name);
				break;
			case POSITION:
				*value = g_variant_get_uint32(reply);
				break;
			default:
				BT_DBG("Invalid Type\n");
				ret =  BLUETOOTH_ERROR_INTERNAL;
			}
		}
	g_variant_unref(reply);
	return ret;
}

int _bt_avrcp_control_set_property(int type, unsigned int value)
{
	GValue *attr_value = NULL;
	GDBusProxy *proxy = NULL;
	GError *error = NULL;
	GVariant *reply, *param;

	g_value_init(attr_value, G_TYPE_STRING);

	switch (type) {
	case EQUALIZER:
		param = g_variant_new("s", equalizer_status[value].property);
		BT_DBG("equalizer_status %s", equalizer_status[value].property);
		break;
	case REPEAT:
		param = g_variant_new("s", repeat_status[value].property);
		BT_DBG("repeat_status %s", repeat_status[value].property);
		break;
	case SHUFFLE:
		param = g_variant_new("s", shuffle_settings[value].property);
		BT_DBG("shuffle_settings %s", shuffle_settings[value].property);
		break;
	case SCAN:
		param = g_variant_new("s", scan_status[value].property);
		BT_DBG("scan_status %s", scan_status[value].property);
		break;
	default:
		BT_ERR("Invalid property type: %d", type);
		g_value_unset(attr_value);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	proxy = __bt_get_control_properties_proxy();
	retv_if(proxy == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);

	reply = g_dbus_proxy_call_sync(proxy,
					"Set", g_variant_new("ssv", BT_PLAYER_CONTROL_INTERFACE, __bt_media_type_to_str(type), param),
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	g_object_unref(proxy);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Can't get managed objects");
		if (error) {
			BT_ERR("SetProperty Fail: %s", error->message);
			g_clear_error(&error);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	}

	g_variant_unref(reply);
	g_value_unset(attr_value);

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_avrcp_control_parse_properties(
				media_metadata_attributes_t *metadata,
				GVariant *item)
{
	GVariant *value = NULL;
	GVariantIter iter;
	char *value_string = NULL;
	unsigned int value_uint;
	const char *key = NULL;

	g_variant_iter_init(&iter, item);
	while (g_variant_iter_loop(&iter, "{sv}", &key, &value)){
		if (strcasecmp(key, "Title") == 0){
			value_string = (char *)g_variant_get_string(value, NULL);
			BT_DBG("Value : %s ", value_string);
			metadata->title = value_string;
		} else if (strcasecmp(key, "Artist") == 0) {
			value_string =(char *)g_variant_get_string(value, NULL);
			BT_DBG("Value : %s ", value_string);
			metadata->artist = value_string;
		} else if (strcasecmp(key, "Album") == 0) {
			value_string =(char *)g_variant_get_string(value, NULL);
			BT_DBG("Value : %s ", value_string);
			metadata->album = value_string;
		} else if (strcasecmp(key, "Genre") == 0) {
			value_string =(char *)g_variant_get_string(value, NULL);
			BT_DBG("Value : %s ", value_string);
			metadata->genre = value_string;
		} else if (strcasecmp(key, "Duration") == 0) {
			value_uint = g_variant_get_uint32(value);
			metadata->duration = value_uint;
		} else if (strcasecmp(key, "NumberOfTracks") == 0) {
			value_uint = g_variant_get_uint32(value);
			metadata->total_tracks = value_uint;
		} else if (strcasecmp(key, "TrackNumber") == 0) {
			value_uint = g_variant_get_uint32(value);
			metadata->number = value_uint;
		} else
			BT_DBG("%s not supported, ignoring", key);
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
	GDBusProxy *proxy = NULL;
	GVariant *reply = NULL;
	GVariant *item = NULL;
	GError *err = NULL;
	GDBusConnection *conn = NULL;
	char *control_path = NULL;
	char *interface_name = NULL;
	char *property_name = NULL;
	GVariant *parameters = NULL;
	int ret = BLUETOOTH_ERROR_NONE;

	retv_if(metadata == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	control_path = __bt_get_control_device_path();
	retv_if(control_path == NULL, BLUETOOTH_ERROR_NOT_CONNECTED);
	BT_DBG("control_path %s", control_path);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, control_path,
			BT_PROPERTIES_INTERFACE, NULL, &err);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy \n");
		if (err) {
			BT_ERR("%s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	interface_name = g_strdup(BT_PLAYER_CONTROL_INTERFACE);
	property_name = g_strdup("Track");

	parameters = g_variant_new("(ss)", interface_name, property_name);

	g_free(interface_name);
	g_free(property_name);

	reply = g_dbus_proxy_call_sync(proxy, "Get", parameters,
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);

	g_object_unref(proxy);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			BT_ERR("%s", err->message);
			g_clear_error(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_get(reply, "(v)", &item);

	ret = __bt_avrcp_control_parse_properties(metadata, item);

	g_variant_unref(reply);
	BT_DBG("-");
	return ret;
}

void _bt_handle_avrcp_control_event(GVariant *reply, const char *path)
{
	GVariant *param = NULL;
	const char *property = NULL;

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		return;
	}

	GVariantIter iter;
	GVariant *value = NULL;
	g_variant_iter_init(&iter, reply);
	while (g_variant_iter_loop(&iter, "{sv}", &property,
				&value)) {
		if ((strcasecmp(property, "Equalizer") == 0) ||
			(strcasecmp(property, "Repeat") == 0) ||
			(strcasecmp(property, "Shuffle") == 0) ||
			(strcasecmp(property, "Scan") == 0) ||
			(strcasecmp(property, "Status") == 0)){
				const char *valstr;
				unsigned int type, val;

				valstr = g_variant_get_string(value, NULL);
				BT_DBG("Value : %s ", valstr);
				type = __bt_media_attr_to_type(property);
				val = __bt_media_attrval_to_val(type, valstr);

				/* Send event to application */
				param = g_variant_new("(u)", val);
				_bt_send_event(BT_AVRCP_CONTROL_EVENT,
					__bt_media_attr_to_event(property), param);
		} else if (strcasecmp(property, "Position") == 0) {
			unsigned int val;

			val = g_variant_get_uint32(value);
			BT_DBG("Value : %d ", val);

			/* Send event to application */
			param = g_variant_new("(u)", val);
			_bt_send_event(BT_AVRCP_CONTROL_EVENT,
				__bt_media_attr_to_event(property), param);
		} else if (strcasecmp(property, "Track") == 0) {
			int ret = BLUETOOTH_ERROR_NONE;
			media_metadata_attributes_t metadata;

			memset(&metadata, 0x00, sizeof(media_metadata_attributes_t));

			ret = __bt_avrcp_control_parse_properties(
								&metadata, reply);
			if (BLUETOOTH_ERROR_NONE != ret){
				/* Free key and value because of break unless free not required */
				free((char *)property);
				g_variant_unref(value);
				break;
			}

				/* Send event to application */
			param = g_variant_new("(ssssuuu)",
							metadata.title,
							metadata.artist,
							metadata.album,
							metadata.genre,
							metadata.total_tracks,
							metadata.number,
							metadata.duration);
			_bt_send_event(BT_AVRCP_CONTROL_EVENT,
				BLUETOOTH_EVENT_AVRCP_TRACK_CHANGED, param);

			g_free((char *)metadata.title);
			g_free((char *)metadata.artist);
			g_free((char *)metadata.album);
			g_free((char *)metadata.genre);
		} else {
			BT_DBG("Property not handled");
		}
	}

	BT_DBG("-");
}
