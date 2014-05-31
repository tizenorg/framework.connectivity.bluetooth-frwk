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

struct player_settinngs_t {
	int key;
	const char *property;
};

static struct player_settinngs_t equalizer_settings[] = {
	{ EQUALIZER_INVALID, "" },
	{ EQUALIZER_OFF, "off" },
	{ EQUALIZER_ON, "on" },
	{ EQUALIZER_INVALID, "" }
};

static struct player_settinngs_t repeat_settings[] = {
	{ REPEAT_INVALID, "" },
	{ REPEAT_MODE_OFF, "off" },
	{ REPEAT_SINGLE_TRACK, "singletrack" },
	{ REPEAT_ALL_TRACK, "alltracks" },
	{ REPEAT_GROUP, "group" },
	{ REPEAT_INVALID, "" }
};

static struct player_settinngs_t shuffle_settings[] = {
	{ SHUFFLE_INVALID, "" },
	{ SHUFFLE_MODE_OFF, "off" },
	{ SHUFFLE_ALL_TRACK, "alltracks" },
	{ SHUFFLE_GROUP, "group" },
	{ SHUFFLE_INVALID, "" }
};

static struct player_settinngs_t scan_settings[] = {
	{ SCAN_INVALID, "" },
	{ SCAN_MODE_OFF, "off" },
	{ SCAN_ALL_TRACK, "alltracks" },
	{ SCAN_GROUP, "group" },
	{ SCAN_INVALID, "" }
};

static struct player_settinngs_t player_status[] = {
	{ STATUS_STOPPED, "stopped" },
	{ STATUS_PLAYING, "playing" },
	{ STATUS_PAUSED, "paused" },
	{ STATUS_FORWARD_SEEK, "forward-seek" },
	{ STATUS_REVERSE_SEEK, "reverse-seek" },
	{ STATUS_ERROR, "error" },
	{ STATUS_INVALID, "" }
};

typedef struct {
	GObject parent;
} BtMediaAgent;

typedef struct {
	GObjectClass parent;
} BtMediaAgentClass;

GType bt_media_agent_get_type(void);

#define BT_MEDIA_TYPE_AGENT (bt_media_agent_get_type())
#define BT_MEDIA_GET_AGENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BT_MEDIA_TYPE_AGENT, BtMediaAgent))
#define BT_MEDIA_IS_AGENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BT_MEDIA_TYPE_AGENT))
#define BT_MEDIA_AGENT_CLASS(class) (G_TYPE_CHECK_CLASS_CAST((class), BT_MEDIA_TYPE_AGENT, \
										BtMediaAgentClass))
#define BT_MEDIA_GET_AGENT_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), BT_MEDIA_TYPE_AGENT, \
										BtMediaAgentClass))
#define BT_MEDIA_IS_AGENT_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE((class), BT_MEDIA_TYPE_AGENT))

G_DEFINE_TYPE(BtMediaAgent, bt_media_agent, G_TYPE_OBJECT)

static gboolean bt_media_agent_set_property(BtMediaAgent *agent,
						const char *property, GValue *value,
						DBusGMethodInvocation *context);

static BtMediaAgent *bt_media_obj = NULL;

#include "bt-media-agent-method.h"

typedef enum {
	BT_MEDIA_AGENT_ERROR_INVALID_PARAM,
	BT_MEDIA_AGENT_ERROR_NOT_AVAILABLE,
	BT_MEDIA_AGENT_ERROR_BUSY,
} BtMediaAgentError;

#define BT_MEDIA_AGENT_ERROR (bt_media_agent_error_quark())

static GQuark bt_media_agent_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("agent");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GError *bt_media_agent_error(BtMediaAgentError error, const char *err_msg)
{
	return g_error_new(BT_MEDIA_AGENT_ERROR, error, err_msg, NULL);
}

static void bt_media_agent_init(BtMediaAgent *agent)
{
	BT_DBG("agent %p\n", agent);
}

static void bt_media_agent_finalize(GObject *agent)
{
	BT_DBG("Free agent %p\n", agent);

	G_OBJECT_CLASS(bt_media_agent_parent_class)->finalize(agent);
}

static void bt_media_agent_class_init(BtMediaAgentClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	BT_DBG("class %p\n", klass);

	object_class->finalize = bt_media_agent_finalize;

	dbus_g_object_type_install_info(BT_MEDIA_TYPE_AGENT,
					&dbus_glib_bt_media_agent_object_info);
}

static BtMediaAgent *__bt_media_agent_new(void)
{
	BtMediaAgent *agent;

	agent = BT_MEDIA_GET_AGENT(g_object_new(BT_MEDIA_TYPE_AGENT, NULL));

	BT_DBG("agent %p\n", agent);

	return agent;
}

static gboolean bt_media_agent_set_property(BtMediaAgent *agent,
						const char *property, GValue *val,
						DBusGMethodInvocation *context)
{
	GError *error;
	const gchar *value;
	unsigned int status;

	BT_DBG("property %s\n", property);

	if (!(G_VALUE_TYPE (val) == G_TYPE_STRING)) {
		error = bt_media_agent_error(
					BT_MEDIA_AGENT_ERROR_INVALID_PARAM,
					"Invalid Arguments");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		return FALSE;
	}
	value = g_value_get_string (val);
	BT_DBG("value %s\n", value);

	if (g_strcmp0(property, "Shuffle") == 0) {
		if (g_strcmp0(value, "alltracks") == 0)
			status = SHUFFLE_ALL_TRACK;
		else if (g_strcmp0(value, "group") == 0)
			status = SHUFFLE_GROUP;
		else if (g_strcmp0(value, "off") == 0)
			status = SHUFFLE_MODE_OFF;
		else
			status = SHUFFLE_INVALID;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);

	} else if (g_strcmp0(property, "Equalizer") == 0) {
		status = (g_strcmp0(value, "off") == 0) ? EQUALIZER_OFF : EQUALIZER_ON;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_EQUALIZER_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);

	} else if (g_strcmp0(property, "Repeat") == 0) {
		if (g_strcmp0(value, "singletrack") == 0)
			status = REPEAT_SINGLE_TRACK;
		else if (g_strcmp0(value, "alltracks") == 0)
			status = REPEAT_ALL_TRACK;
		else if (g_strcmp0(value, "group") == 0)
			status = REPEAT_GROUP;
		else if (g_strcmp0(value, "off") == 0)
			status = REPEAT_MODE_OFF;
		else
			status = REPEAT_INVALID;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);
	} else if (g_strcmp0(property, "Scan") == 0) {
		if (g_strcmp0(value, "alltracks") == 0)
			status = SCAN_ALL_TRACK;
		else if (g_strcmp0(value, "group") == 0)
			status = SCAN_GROUP;
		else if (g_strcmp0(value, "off") == 0)
			status = SCAN_MODE_OFF;
		else
			status = SCAN_INVALID;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_SCAN_STATUS,
				DBUS_TYPE_UINT32, &status,
				DBUS_TYPE_INVALID);
	}
	dbus_g_method_return(context);
	return TRUE;

}

static void __bt_media_append_variant(DBusMessageIter *iter,
			int type, void *value)
{
	const char *sig;
	DBusMessageIter value_iter;

	switch (type) {
	case DBUS_TYPE_BOOLEAN:
		sig = DBUS_TYPE_BOOLEAN_AS_STRING;
		break;
	case DBUS_TYPE_STRING:
		sig = DBUS_TYPE_STRING_AS_STRING;
		break;
	case DBUS_TYPE_BYTE:
		sig = DBUS_TYPE_BYTE_AS_STRING;
		break;
	case DBUS_TYPE_UINT16:
		sig = DBUS_TYPE_UINT16_AS_STRING;
		break;
	case DBUS_TYPE_UINT32:
		sig = DBUS_TYPE_UINT32_AS_STRING;
		break;
	case DBUS_TYPE_INT16:
		sig = DBUS_TYPE_INT16_AS_STRING;
		break;
	case DBUS_TYPE_INT32:
		sig = DBUS_TYPE_INT32_AS_STRING;
		break;
	case DBUS_TYPE_OBJECT_PATH:
		sig = DBUS_TYPE_OBJECT_PATH_AS_STRING;
		break;
	default:
		sig = DBUS_TYPE_VARIANT_AS_STRING;
		break;
	}

	dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, sig,
							&value_iter);

	dbus_message_iter_append_basic(&value_iter, type, value);

	dbus_message_iter_close_container(iter, &value_iter);
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
	DBusMessageIter entry;
	gboolean ret;

	sig = dbus_message_new_signal(path, interface,
					"PropertyChanged");
	retv_if(sig == NULL, FALSE);

	dbus_message_iter_init_append(sig, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);

	__bt_media_append_variant(&entry, type, property);

	ret = dbus_connection_send(connection, sig, NULL);
	dbus_message_unref(sig);

	return ret;
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

int _bt_register_media_player(void)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter property_dict;
	DBusError err;
	char *object;
	char *adapter_path;
	DBusConnection *conn;
	DBusGConnection *gconn;

	media_player_settings_t player_settings = {0,};

	player_settings.equalizer = EQUALIZER_OFF;
	player_settings.repeat  = REPEAT_MODE_OFF;
	player_settings.shuffle = SHUFFLE_MODE_OFF;
	player_settings.scan = SCAN_MODE_OFF;
	player_settings.status = STATUS_STOPPED;
	player_settings.position = 0;

	gconn = _bt_get_system_gconn();
	retv_if(gconn  == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (!bt_media_obj) {
		bt_media_obj = __bt_media_agent_new();

		retv_if(bt_media_obj == NULL, BLUETOOTH_ERROR_INTERNAL);

		dbus_g_connection_register_g_object(gconn,
							BT_MEDIA_OBJECT_PATH,
							G_OBJECT(bt_media_obj));
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
		"Equalizer",
		DBUS_TYPE_STRING,
		&equalizer_settings[player_settings.equalizer].property);

	__bt_media_append_dict_entry(&property_dict,
		"Repeat",
		DBUS_TYPE_STRING,
		&repeat_settings[player_settings.repeat].property);

	__bt_media_append_dict_entry(&property_dict,
		"Shuffle",
		DBUS_TYPE_STRING,
		&shuffle_settings[player_settings.shuffle].property);

	__bt_media_append_dict_entry(&property_dict,
		"Scan",
		DBUS_TYPE_STRING,
		&scan_settings[player_settings.scan].property);

	__bt_media_append_dict_entry(&property_dict,
		"Status",
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

		if (bt_media_obj) {
			dbus_g_connection_unregister_g_object(gconn,
							G_OBJECT(bt_media_obj));
			g_object_unref(bt_media_obj);
			bt_media_obj = NULL;
		}
	}

	if (reply)
		dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

int _bt_unregister_media_player(void)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	char *object;
	char *adapter_path;
	DBusConnection *conn;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BT_BLUEZ_NAME, adapter_path,
				BT_MEDIA_INTERFACE, "UnregisterPlayer");

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
			BT_DBG("%s", err.message);
			dbus_error_free(&err);
			return BLUETOOTH_ERROR_INTERNAL;
		}
	} else {
		dbus_message_unref(reply);
	}

	if (bt_media_obj) {
		dbus_g_connection_unregister_g_object(_bt_get_system_gconn(),
						G_OBJECT(bt_media_obj));
		g_object_unref(bt_media_obj);
		bt_media_obj = NULL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_track_info(media_metadata_attributes_t *meta_data)
{
	DBusMessage *signal;
	DBusMessageIter iter;
	DBusMessageIter metadata_dict;
	DBusConnection *conn;

	retv_if(meta_data == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	signal = dbus_message_new_signal(BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE, "TrackChanged");

	retv_if(signal == NULL, BLUETOOTH_ERROR_INTERNAL);

	dbus_message_iter_init_append(signal, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &metadata_dict);

	if (meta_data->title) {
		__bt_media_append_dict_entry(&metadata_dict,
			"Title",
			DBUS_TYPE_STRING, &meta_data->title);
	}

	if (meta_data->artist) {
		__bt_media_append_dict_entry(&metadata_dict,
			"Artist",
			DBUS_TYPE_STRING, &meta_data->artist);
	}

	if (meta_data->album) {
		__bt_media_append_dict_entry(&metadata_dict,
			"Album",
			DBUS_TYPE_STRING, &meta_data->album);
	}

	if (meta_data->genre) {
		__bt_media_append_dict_entry(&metadata_dict,
			"Genre",
			DBUS_TYPE_STRING, &meta_data->genre);
	}

	if (0 != meta_data->total_tracks)
		__bt_media_append_dict_entry(&metadata_dict,
			"NumberOfTracks",
			DBUS_TYPE_UINT32, &meta_data->total_tracks);

	if (0 != meta_data->number)
		__bt_media_append_dict_entry(&metadata_dict,
			"Number",
			DBUS_TYPE_UINT32, &meta_data->number);

	if (0 != meta_data->duration)
		__bt_media_append_dict_entry(&metadata_dict,
			"Duration",
			DBUS_TYPE_UINT32, &meta_data->duration);

	dbus_message_iter_close_container(&iter, &metadata_dict);

	if (!dbus_connection_send(conn, signal, NULL))
		BT_ERR("Unable to send TrackChanged signal\n");

	dbus_message_unref(signal);

	return BLUETOOTH_ERROR_NONE;
}


int _bt_avrcp_set_properties(media_player_settings_t *properties)
{
	if (_bt_avrcp_set_property(EQUALIZER,
			properties->equalizer) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_property(REPEAT,
			properties->repeat) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_property(SHUFFLE,
			properties->shuffle) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_property(SCAN,
			properties->scan) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_property(STATUS,
			properties->status) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (_bt_avrcp_set_property(POSITION,
			properties->position) != BLUETOOTH_ERROR_NONE) {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_property(int type, unsigned int value)
{
	DBusConnection *conn;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	switch (type) {
	case EQUALIZER:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Equalizer",
			DBUS_TYPE_STRING,
			&equalizer_settings[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case REPEAT:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Repeat",
			DBUS_TYPE_STRING,
			&repeat_settings[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case SHUFFLE:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Shuffle",
			DBUS_TYPE_STRING,
			&shuffle_settings[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case SCAN:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Scan",
			DBUS_TYPE_STRING,
			&scan_settings[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case STATUS:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Status",
			DBUS_TYPE_STRING,
			&player_status[value].property)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case POSITION:
		if (!__bt_media_emit_property_changed(
			conn,
			BT_MEDIA_OBJECT_PATH,
			BT_MEDIA_PLAYER_INTERFACE,
			"Position",
			DBUS_TYPE_UINT32,
			&value)) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	default:
		BT_ERR("Invalid Type\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}

