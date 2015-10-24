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

#include <gio/gio.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <syspopup_caller.h>
#include <dbus/dbus.h>

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

GDBusConnection *bt_gdbus_conn = NULL;
static guint avrcp_reg_id = 0;
static GDBusProxy *service_gproxy = NULL;

/* Introspection data exposed from bt-service */
static const gchar bt_avrcp_bluez_introspection_xml[] =
"<node name='/'>"
" <interface name='org.freedesktop.DBus.Properties'>"
"     <method name='Set'>"
"          <arg type='s' name='interface' direction='in'/>"
"          <arg type='s' name='property' direction='in'/>"
"          <arg type='v' name='value' direction='in'/>"
"     </method>"
" </interface>"
"</node>";

static gboolean __bt_media_emit_property_changed(GDBusConnection *connection,
		const char *path, const char *interface, const char *name,
		const GVariant *variant)
{
	GVariantBuilder *builder = NULL;
	GError *error = NULL;

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(builder, "{sv}", name, variant);

	g_dbus_connection_emit_signal(connection, NULL, path,
				DBUS_INTERFACE_PROPERTIES,
				"PropertiesChanged",
				g_variant_new("(sa{sv})",
				interface, builder),
				&error);

	g_variant_builder_unref(builder);
	if (error) {
		BT_ERR("Could not Emit PropertiesChanged Signal: errCode[%x], message[%s]",
			error->code, error->message);
		g_clear_error(&error);
		return FALSE;
	}

	return TRUE;
}

static GQuark __bt_avrcp_error_quark(void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string("bt-avrcp");

	return quark;
}

static GError *__bt_avrcp_set_error(bt_avrcp_error_t error)
{
	BT_ERR("error[%d]\n", error);

	switch (error) {
	case BT_AVRCP_ERROR_INVALID_PARAM:
		return g_error_new(BT_AVRCP_ERROR, error,
				BT_ERROR_INVALID_PARAM);
	case BT_AVRCP_ERROR_INVALID_INTERFACE:
		return g_error_new(BT_AVRCP_ERROR, error,
				BT_ERROR_INVALID_INTERFACE);
	case BT_AVRCP_ERROR_INTERNAL:
	default:
		return g_error_new(BT_AVRCP_ERROR, error,
				BT_ERROR_INTERNAL);
	}
}

static void __bt_avrcp_agent_method(GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	BT_DBG("+");
	BT_INFO("method %s", method_name);
	BT_INFO("object_path %s", object_path);
	int ret = BT_AVRCP_ERROR_NONE;
	GError *err = NULL;
	gboolean shuffle_status;
	guint32 status;
	gchar *interface = NULL;
	gchar *property = NULL;
	gchar *loop_status = NULL;
	GVariant *value = NULL;

	if (g_strcmp0(method_name, "Set") == 0) {
		g_variant_get(parameters, "(&s&sv)", &interface, &property,
				&value);

		if (g_strcmp0(interface, BT_MEDIA_PLAYER_INTERFACE) != 0) {
			ret = BT_AVRCP_ERROR_INVALID_INTERFACE;
			goto fail;
		}
	}

	if (value == NULL) {
		BT_ERR("value is NULL");
		goto fail;
	}

	BT_DBG("Property: %s\n", property);
	if (g_strcmp0(property, "Shuffle") == 0) {

		if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
			BT_ERR("Error");
			ret = BT_AVRCP_ERROR_INVALID_PARAM;
			goto fail;
		}

		shuffle_status = g_variant_get_boolean(value);
		BT_DBG("Value: %s\n", shuffle_status ? "TRUE" : "FALSE");
		if (shuffle_status == TRUE)
			status = SHUFFLE_ALL_TRACK;
		else
			status = SHUFFLE_MODE_OFF;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_SHUFFLE_STATUS,
				g_variant_new("(u)", status));
	} else if (g_strcmp0(property, "LoopStatus") == 0) {

		if (!g_variant_is_of_type(value, G_VARIANT_TYPE_STRING)) {
			BT_ERR("Error");
			ret = BT_AVRCP_ERROR_INVALID_PARAM;
			goto fail;
		}

		loop_status = (gchar *)g_variant_get_string(value, NULL);
		BT_DBG("Value: %s\n", loop_status);

		if (g_strcmp0(loop_status, "Track") == 0)
			status = REPEAT_SINGLE_TRACK;
		else if (g_strcmp0(loop_status, "Playlist") == 0)
			status = REPEAT_ALL_TRACK;
		else if (g_strcmp0(loop_status, "None") == 0)
			status = REPEAT_MODE_OFF;
		else
			status = REPEAT_INVALID;

		_bt_send_event(BT_AVRCP_EVENT,
				BLUETOOTH_EVENT_AVRCP_SETTING_REPEAT_STATUS,
				g_variant_new("(u)", status));
	}

	BT_DBG("-");
	return;

fail:
	if (value)
		g_variant_unref(value);
	err = __bt_avrcp_set_error(ret);
	g_dbus_method_invocation_return_gerror(invocation, err);
	g_clear_error(&err);
	BT_INFO("-");
}

static const GDBusInterfaceVTable method_table = {
	__bt_avrcp_agent_method,
	NULL,
	NULL,
};

static GDBusNodeInfo *__bt_avrcp_create_method_node_info
				(const gchar *introspection_data)
{
	GError *err = NULL;
	GDBusNodeInfo *node_info = NULL;

	if (introspection_data == NULL)
		return NULL;

	node_info = g_dbus_node_info_new_for_xml(introspection_data, &err);

	if (err) {
		BT_ERR("Unable to create node: %s", err->message);
		g_clear_error(&err);
	}

	return node_info;
}

static GDBusProxy *__bt_avrcp_gdbus_init_service_proxy(void)
{
	BT_DBG("+");
	GDBusProxy *proxy;
	GError *err = NULL;
	char *adapter_path;

	if (bt_gdbus_conn == NULL)
		bt_gdbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

	if (!bt_gdbus_conn) {
		if (err) {
			BT_ERR("Unable to connect to gdbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, NULL);

	proxy =  g_dbus_proxy_new_sync(bt_gdbus_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, adapter_path,
			BT_MEDIA_INTERFACE, NULL, &err);
	g_free(adapter_path);

	if (!proxy) {
		BT_ERR("Unable to create proxy");
		if (err) {
			BT_ERR("Error: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	BT_DBG("-");;
	return proxy;
}

static GDBusProxy *__bt_avrcp_gdbus_get_service_proxy(void)
{
	return (service_gproxy) ? service_gproxy :
			__bt_avrcp_gdbus_init_service_proxy();
}

int _bt_register_media_player(void)
{
	BT_DBG("+");
	gchar *adapter_path;
	gboolean shuffle_status;
	gchar *path;
	GDBusConnection *conn;
	GDBusNodeInfo *node_info;
	GDBusProxy *proxy;
	GVariantBuilder *builder;
	GVariant *ret;
	GError *error = NULL;

	media_player_settings_t player_settings = {0,};

	player_settings.repeat  = REPEAT_MODE_OFF;
	player_settings.status = STATUS_STOPPED;
	player_settings.position = 0;
	shuffle_status = FALSE;

	conn = _bt_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);
	bt_gdbus_conn = conn;

	node_info = __bt_avrcp_create_method_node_info(
				bt_avrcp_bluez_introspection_xml);
	if (node_info == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	avrcp_reg_id = g_dbus_connection_register_object(bt_gdbus_conn,
					BT_MEDIA_OBJECT_PATH,
					node_info->interfaces[0],
					&method_table,
					NULL, NULL, &error);
	g_dbus_node_info_unref(node_info);

	if (avrcp_reg_id == 0) {
		BT_ERR("Failed to register: %s", error->message);
		g_clear_error(&error);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	adapter_path = _bt_get_adapter_path();
	retv_if(adapter_path == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_BLUEZ_NAME, adapter_path,
			BT_MEDIA_INTERFACE, NULL, &error);
	g_free(adapter_path);

	if (proxy == NULL) {
		BT_ERR("Unable to create proxy");
		if (error) {
			BT_ERR("Error: %s", error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(builder, "{sv}", "LoopStatus",
			g_variant_new("s",
			loopstatus_settings[player_settings.repeat].property));
	BT_ERR("LoopStatus: %s", loopstatus_settings[player_settings.repeat].property);

	g_variant_builder_add(builder, "{sv}", "Shuffle",
			g_variant_new("b", shuffle_status));

	g_variant_builder_add(builder, "{sv}", "PlaybackStatus",
			g_variant_new("s",
			player_status[player_settings.status].property));
	BT_ERR("PlaybackStatus: %s", player_status[player_settings.status].property);

	g_variant_builder_add(builder, "{sv}", "Position",
			g_variant_new("u", player_settings.position));

	path = g_strdup(BT_MEDIA_OBJECT_PATH);
	ret = g_dbus_proxy_call_sync(proxy, "RegisterPlayer",
			g_variant_new("(oa{sv})", path, builder),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &error);

	g_object_unref(proxy);
	g_free(path);
	g_variant_builder_unref(builder);

	if (ret == NULL) {
		BT_ERR("Call RegisterPlayer Failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(ret);
	return BLUETOOTH_ERROR_NONE;
}

static void __bt_avrcp_unregister_object_path(void)
{
	if (avrcp_reg_id > 0) {
		g_dbus_connection_unregister_object(bt_gdbus_conn,
							avrcp_reg_id);
		avrcp_reg_id = 0;
	}
}

int _bt_unregister_media_player(void)
{
	BT_DBG("+");
	GDBusProxy *proxy;
	GVariant *ret;
	GError *error = NULL;
	GDBusConnection *conn;
	gchar *path;

	conn = bt_gdbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	proxy = __bt_avrcp_gdbus_get_service_proxy();
	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	path = g_strdup(BT_MEDIA_OBJECT_PATH);
	BT_DBG("path is [%s]", path);

	ret = g_dbus_proxy_call_sync(proxy, "UnregisterPlayer",
			g_variant_new("(o)", path),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL, &error);
	g_free(path);

	if (ret == NULL) {
		BT_ERR("UnregisterPlayer failed");
		if (error) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	__bt_avrcp_unregister_object_path();

	g_variant_unref(ret);
	g_object_unref(bt_gdbus_conn);
	bt_gdbus_conn = NULL;

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_track_info(media_metadata_attributes_t *meta_data)
{
	BT_DBG("+");
	char *interface = BT_MEDIA_PLAYER_INTERFACE;
	GDBusConnection *conn;
	GError *error = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	GVariant *children[1];
	gboolean ret;

	retv_if(meta_data == NULL, BLUETOOTH_ERROR_INTERNAL);

	conn = bt_gdbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:title", g_variant_new_string(meta_data->title));

	children[0] = g_variant_new_string(meta_data->artist);
	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:artist", g_variant_new_array(G_VARIANT_TYPE_STRING,
		children, 1));

	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:album", g_variant_new_string(meta_data->album));

	children[0] = g_variant_new_string(meta_data->genre);
	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:genre", g_variant_new_array(G_VARIANT_TYPE_STRING,
		children, 1));

	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:totalTracks", g_variant_new_int32(meta_data->total_tracks));

	g_variant_builder_add(inner_builder, "{sv}",
		"xesam:trackNumber", g_variant_new_int32(meta_data->number));

	g_variant_builder_add(inner_builder, "{sv}",
		"mpris:lenght", g_variant_new_int64(meta_data->duration));

	g_variant_builder_add(builder, "{sv}",
		"Metadata", g_variant_new("a{sv}", inner_builder));

	ret = g_dbus_connection_emit_signal(conn, NULL, BT_MEDIA_OBJECT_PATH,
				DBUS_INTERFACE_PROPERTIES,
				"PropertiesChanged",
				g_variant_new("(sa{sv})",
				interface, builder),
				&error);

	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);

	if (!ret) {
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
				error->code, error->message);
			g_clear_error(&error);
		}
	}

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_avrcp_set_interal_property(int type, media_player_settings_t *properties)
{
	BT_DBG("+");
	GDBusConnection *conn;
	int value;
	media_metadata_attributes_t meta_data;
	gboolean shuffle;
	GVariantBuilder *builder = NULL;
	GVariant *children[1];

	conn = bt_gdbus_conn;
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	switch (type) {
	case REPEAT:
		value = properties->repeat;
		if (!__bt_media_emit_property_changed(conn, BT_MEDIA_OBJECT_PATH,
				BT_MEDIA_PLAYER_INTERFACE, "LoopStatus",
				g_variant_new_string(loopstatus_settings[value].property))) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case SHUFFLE:
		value = properties->shuffle;
		if (g_strcmp0(shuffle_settings[value].property, "off") == 0)
			shuffle = FALSE;
		else
			shuffle = TRUE;

		if (!__bt_media_emit_property_changed(conn, BT_MEDIA_OBJECT_PATH,
				BT_MEDIA_PLAYER_INTERFACE, "Shuffle",
				g_variant_new_boolean(shuffle))) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case STATUS:
		value = properties->status;
		if (!__bt_media_emit_property_changed(conn, BT_MEDIA_OBJECT_PATH,
				BT_MEDIA_PLAYER_INTERFACE, "PlaybackStatus",
				g_variant_new_string(player_status[value].property))) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case POSITION:
		value = properties->position;
		if (!__bt_media_emit_property_changed(conn, BT_MEDIA_OBJECT_PATH,
				BT_MEDIA_PLAYER_INTERFACE, "Position",
				g_variant_new_uint32(value))) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			return BLUETOOTH_ERROR_INTERNAL;
		}
		break;
	case METADATA:
		meta_data = properties->metadata;

		builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
		g_variant_builder_add(builder, "{sv}",
				"xesam:title", g_variant_new_string(meta_data.title));

		children[0] = g_variant_new_string(meta_data.artist);
		g_variant_builder_add(builder, "{sv}",
				"xesam:artist", g_variant_new_array(G_VARIANT_TYPE_STRING,
						children, 1));

		g_variant_builder_add(builder, "{sv}",
				"xesam:album", g_variant_new_string(meta_data.album));

		children[0] = g_variant_new_string(meta_data.genre);
		g_variant_builder_add(builder, "{sv}",
				"xesam:genre", g_variant_new_array(G_VARIANT_TYPE_STRING,
						children, 1));

		g_variant_builder_add(builder, "{sv}",
				"xesam:totalTracks", g_variant_new_int32(meta_data.total_tracks));

		g_variant_builder_add(builder, "{sv}",
				"xesam:trackNumber", g_variant_new_int32(meta_data.number));

		g_variant_builder_add(builder, "{sv}",
				"mpris:lenght", g_variant_new_int64(meta_data.duration));

		if (!__bt_media_emit_property_changed(conn, BT_MEDIA_OBJECT_PATH,
				BT_MEDIA_PLAYER_INTERFACE, "Metadata",
				g_variant_new("a{sv}", builder))) {
			BT_ERR("Error sending the PropertyChanged signal \n");
			g_variant_builder_unref(builder);
			return BLUETOOTH_ERROR_INTERNAL;
		}
		g_variant_builder_unref(builder);
		break;
	default:
		BT_ERR("Invalid Type\n");
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
