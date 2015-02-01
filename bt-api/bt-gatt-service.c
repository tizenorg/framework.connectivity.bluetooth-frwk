/*
 * Bluetooth-frwk low energy
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hocheol Seo <hocheol.seo@samsung.com>
 *		    Chanyeol Park <chanyeol.park@samsung.com>
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

#include<gio/gio.h>
#include<glib.h>
#include<glib/gprintf.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdint.h>

#include "bt-common.h"

#define NUMBER_OF_FLAGS	10

GDBusConnection *g_conn;
guint owner_id;
guint manager_id;
static gboolean new_service = FALSE;
static gboolean new_char = FALSE;
static int serv_id = 1;

/* Introspection data for the service we are exporting */
static const gchar service_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.GattService1'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"    <property type='s' name='Includes' access='read'>"
"    </property>"
"  </interface>"
"</node>";

/* Introspection data for the characteristics we are exporting */
static const gchar characteristics_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.GattCharacteristic1'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"    <property type='o' name='Service' access='read'>"
"    </property>"
"    <property type='ay' name='Value' access='readwrite'>"
"    </property>"
"    <property type='as' name='Flags' access='read'>"
"    </property>"
"  </interface>"
"</node>";

/* Introspection data for the descriptor we are exporting */
static const gchar descriptor_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.GattDescriptor1'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"    <property type='o' name='Characteristic' access='read'>"
"    </property>"
"    <property type='ay' name='Value' access='readwrite'>"
"    </property>"
"    <property type='s' name='Permissions' access='read'>"
"    </property>"
"  </interface>"
"</node>";

static const gchar manager_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.freedesktop.DBus.ObjectManager'>"
"    <method name='GetManagedObjects'>"
"     <arg type='a{oa{sa{sv}}}' name='object_paths_interfaces_and_properties' direction='out'/>"
"	 </method>"
"  </interface>"
"</node>";

static const gchar properties_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.freedesktop.DBus.Properties'>"
"	 <method name='Set'>"
"		<arg type='s' name='interface' direction='in'/>"
"		<arg type='s' name='name' direction='in'/>"
"		<arg type='v' name='value' direction='in'/>"
"	 </method>"
"  </interface>"
"</node>";

struct gatt_service_info {
	gchar *serv_path;
	guint serv_id;
	gchar *service_uuid;
	guint manager_id;
	guint prop_id;
	GSList *char_data;
	gboolean is_svc_registered;
};

struct gatt_char_info {
	gchar *char_path;
	guint char_id;
	gchar *char_uuid;
	gchar *char_value;
	gchar *char_flags[NUMBER_OF_FLAGS];
	int value_length;
	int flags_length;
	GSList *desc_data;
};

struct gatt_desc_info {
	gchar *desc_path;
	guint desc_id;
	gchar *desc_uuid;
	gchar *permissions;
	gchar *desc_value;
	int value_length;
};

static GSList *gatt_services = NULL;

#define BT_GATT_SERVICE_NAME	"org.frwk.gatt_service"
#define BT_GATT_SERVICE_PATH "/org/frwk/gatt_service"

#define GATT_SERV_OBJECT_PATH	"/service"

#define GATT_MNGR_INTERFACE		"org.bluez.GattManager1"
#define GATT_SERV_INTERFACE		"org.bluez.GattService1"
#define GATT_CHAR_INTERFACE		"org.bluez.GattCharacteristic1"
#define GATT_DESC_INTERFACE		"org.bluez.GattDescriptor1"

static GDBusProxy *manager_gproxy = NULL;

static void __bt_gatt_manager_method_call(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *method_name,
					GVariant *parameters,
					GDBusMethodInvocation *invocation,
					gpointer user_data)
{
	if (g_strcmp0(method_name, "GetManagedObjects") == 0) {

		BT_DBG("Getting values for service, chars and descriptors");
		GVariantBuilder *builder;
		/*Main Builder */
		builder = g_variant_builder_new(
				G_VARIANT_TYPE("a{oa{sa{sv}}}"));

		/* Prepare inner builder for GattService1 interface */

		GVariantBuilder *svc_builder = NULL;
		GVariantBuilder *inner_builder = NULL;

		GSList *l1 = g_slist_last(gatt_services);
		struct gatt_service_info *serv_info = l1->data;
		if (serv_info == NULL) {
			BT_ERR("service info value is NULL");
			g_dbus_method_invocation_return_value(invocation, NULL);
			return;
		}

		/* Prepare inner builder for GattService1 interface */
		BT_DBG("Creating builder for service");
		svc_builder = g_variant_builder_new(
					G_VARIANT_TYPE("a{sa{sv}}"));
		inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

		g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new_string(serv_info->service_uuid));

		g_variant_builder_add(svc_builder, "{sa{sv}}",
							GATT_SERV_INTERFACE,
							inner_builder);

		g_variant_builder_add(builder, "{oa{sa{sv}}}",
							serv_info->serv_path,
							svc_builder);

		/* Prepare inner builder for GattCharacteristic1 interface */

		GSList *l2 = serv_info->char_data;
		BT_DBG("Creating builder for characteristics \n");

		if (l2 == NULL)
			BT_DBG("characteristic data is NULL");

		for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {

			GVariantBuilder *char_builder = NULL;
			GVariantBuilder *inner_builder = NULL;
			GVariantBuilder *builder1 = NULL;
			GVariantBuilder *builder2 = NULL;
			GVariant *char_val = NULL;
			GVariant *flags_val = NULL;
			int i = 0;

			char_builder = g_variant_builder_new(
							G_VARIANT_TYPE(
								"a{sa{sv}}"));
			inner_builder = g_variant_builder_new(
							G_VARIANT_TYPE(
								"a{sv}"));

			struct gatt_char_info *char_info = l2->data;
			if (char_info == NULL) {
				BT_ERR("char_info is NULL");
				continue;
			}

			/*Uuid*/
			g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new_string(char_info->char_uuid));
			/*Service*/
			g_variant_builder_add(inner_builder, "{sv}", "Service",
				g_variant_new("o", serv_info->serv_path));
			/*Value*/
			builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

			for (i = 0; i < char_info->value_length; i++) {
				g_variant_builder_add(builder1, "y",
					char_info->char_value[i]);
			}
			char_val = g_variant_new("ay", builder1);
			g_variant_builder_add(inner_builder, "{sv}",
					"Value", char_val);
			/*Flags*/
			builder2 = g_variant_builder_new(G_VARIANT_TYPE("as"));

			for (i = 0; i < char_info->flags_length; i++) {
				g_variant_builder_add(builder2, "s",
					char_info->char_flags[i]);
			}

			flags_val = g_variant_new("as", builder2);
			g_variant_builder_add(inner_builder, "{sv}", "Flags",
						flags_val);

			g_variant_builder_add(char_builder, "{sa{sv}}",
					GATT_CHAR_INTERFACE , inner_builder);
			g_variant_builder_add(builder, "{oa{sa{sv}}}",
					char_info->char_path, char_builder);

			/*Prepare inner builder for GattDescriptor1 interface*/

			GSList *l3 = char_info->desc_data;

			if (l3 == NULL)
				BT_DBG("descriptor data is NULL");

			for (l3 = char_info->desc_data; l3 != NULL; l3 = l3->next) {

				BT_DBG("Creating builder for descriptor \n");

				GVariantBuilder *desc_builder = NULL;
				GVariantBuilder *inner_builder = NULL;
				GVariantBuilder *builder1 = NULL;
				GVariant *desc_val = NULL;

				desc_builder = g_variant_builder_new(
							G_VARIANT_TYPE(
							"a{sa{sv}}"));
				inner_builder = g_variant_builder_new(
							G_VARIANT_TYPE(
							"a{sv}"));

				struct gatt_desc_info *desc_info = l3->data;
				if (desc_info == NULL) {
					BT_ERR("desc_info is NULL");
					continue;
				}
				/*Uuid*/
				g_variant_builder_add(inner_builder,
					"{sv}", "UUID",
					g_variant_new_string(
						desc_info->desc_uuid));
				/*Characteristic*/
				g_variant_builder_add(inner_builder, "{sv}",
					"Characteristic",
					g_variant_new("o",
						char_info->char_path));
				/*Value*/
				builder1 = g_variant_builder_new(
							G_VARIANT_TYPE_ARRAY);

				for (i = 0; i < desc_info->value_length; i++) {
					g_variant_builder_add(builder1, "y",
						desc_info->desc_value[i]);
				}
				desc_val = g_variant_new("ay", builder1);
				g_variant_builder_add(inner_builder, "{sv}",
						"Value", desc_val);
				/*Permissions*/
				g_variant_builder_add(inner_builder, "{sv}",
					"Permissions", g_variant_new("s",
						 desc_info->permissions));

				g_variant_builder_add(desc_builder, "{sa{sv}}",
						GATT_DESC_INTERFACE,
						inner_builder);

				g_variant_builder_add(builder, "{oa{sa{sv}}}",
						desc_info->desc_path,
						desc_builder);

				/*unref descriptor builder pointers*/
				g_variant_builder_unref(builder1);
				g_variant_builder_unref(inner_builder);
				g_variant_builder_unref(desc_builder);
			}

			/*unref char builder pointers*/
			g_variant_builder_unref(builder1);
			g_variant_builder_unref(builder2);
			g_variant_builder_unref(inner_builder);
			g_variant_builder_unref(char_builder);
		}

		/*unref service builder pointers*/
		g_variant_builder_unref(inner_builder);
		g_variant_builder_unref(svc_builder);

		/* Return builder as method reply */
		BT_DBG("Sending gatt service builder values to Bluez");
		g_dbus_method_invocation_return_value(invocation,
						g_variant_new(
						"(a{oa{sa{sv}}})",
						builder));
	}
}

static void __bt_gatt_prop_method_call(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *method_name,
					GVariant *parameters,
					GDBusMethodInvocation *invocation,
					gpointer user_data)
{
	GError *error = NULL;
	gboolean ret;

	if (g_strcmp0(method_name, "Set") == 0) {
		GVariant *var = NULL;
		const char *interface = NULL;
		const char *name = NULL;

		g_variant_get(parameters, "(ssv)", &interface, &name, &var);
		if (g_strcmp0(name, "Value") == 0) {
			GVariantBuilder *builder;
			bt_gatt_char_value_t char_val = {0, };
			bt_user_info_t *user_info;
			int len;
				len = g_variant_get_size(var);

			builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
			g_variant_builder_add(builder, "{sv}", "Value", var);

			char_val.char_handle = object_path;
			if (len > 0) {
				char_val.char_value = (char *)malloc(len);
				memcpy(char_val.char_value, (guint8 *)var, len);
			}
			char_val.val_len = len;
			user_info = _bt_get_user_data(BT_COMMON);
			_bt_common_event_cb(
				BLUETOOTH_EVENT_GATT_SERVER_ON_WRITE_CHAR,
				BLUETOOTH_ERROR_NONE, &char_val,
				user_info->cb, user_info->user_data);

			ret = g_dbus_connection_emit_signal(connection, NULL,
					object_path,
					"org.freedesktop.DBus.Properties",
					"PropertiesChanged",
					g_variant_new("(sa{sv}as)",
					GATT_CHAR_INTERFACE, builder, NULL),
					&error);
			if (!ret) {
				if (error != NULL) {
					/* dBUS gives error cause */
					BT_ERR("D-Bus API failure: errCode[%x], \
						message[%s]",
						error->code, error->message);
					g_clear_error(&error);
				}
				g_dbus_method_invocation_return_gerror(
						invocation,
						error);
			}
			g_variant_builder_unref(builder);
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
	}
}

gboolean __bt_gatt_emit_interface_removed(gchar *object_path, gchar *interface)
{
	gboolean ret;
	GError *error = NULL;
	GVariantBuilder *array_builder;

	array_builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
	g_variant_builder_init(array_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_add(array_builder, "s", interface);

	ret = g_dbus_connection_emit_signal(g_conn, NULL, "/",
					"org.freedesktop.Dbus.Objectmanager",
					"InterfacesRemoved",
					g_variant_new ("(oas)",
					object_path, array_builder),
					&error);

	if (!ret) {
		if (error != NULL) {
			/* dbus gives error cause */
			BT_ERR("d-bus api failure: errcode[%x], message[%s]",
				error->code, error->message);
			g_clear_error(&error);
		}
	}
	g_variant_builder_unref(array_builder);

	return ret;
}

static gboolean __bt_gatt_desc_set_property(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *property_name,
				GVariant *value,
				GError **err, gpointer user_data)
{
	BT_DBG("+");

	return TRUE;
}

static gboolean __bt_gatt_char_set_property(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *property_name,
				GVariant *value,
				GError **err, gpointer user_data)
{
	BT_DBG("+");

	return TRUE;
}

static GVariant *__bt_gatt_desc_get_property(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *property_name,
				GError **error, gpointer user_data)
{
	BT_DBG("+");

	return NULL;
}

static GVariant *__bt_gatt_char_get_property(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *property_name,
				GError **error, gpointer user_data)
{
	BT_DBG("+");

	return NULL;
}

static GVariant *__bt_gatt_serv_get_property(GDBusConnection *connection,
				const gchar *sender, const gchar *object_path,
				const gchar *interface_name,
				const gchar *property_name,
				GError **error, gpointer user_data)
{
	BT_DBG("+");

	return NULL;
}

static const GDBusInterfaceVTable desc_interface_vtable = {
	NULL,
	__bt_gatt_desc_get_property,
	__bt_gatt_desc_set_property
};

static const GDBusInterfaceVTable char_interface_vtable = {
	NULL,
	__bt_gatt_char_get_property,
	__bt_gatt_char_set_property,
};

static const GDBusInterfaceVTable serv_interface_vtable = {
	NULL,
	__bt_gatt_serv_get_property,
	NULL,
};

static const GDBusInterfaceVTable manager_interface_vtable = {
	__bt_gatt_manager_method_call,
	NULL,
	NULL
};

static const GDBusInterfaceVTable properties_interface_vtable = {
	__bt_gatt_prop_method_call,
	NULL,
	NULL
};

static GDBusNodeInfo *__bt_gatt_create_method_node_info(
				const gchar *introspection_data)
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

static void __on_bus_acquired(GDBusConnection *connection,
				const gchar *name, gpointer user_data)
{
	g_conn = connection;
	BT_DBG("on_bus_acquired: Obtained GDbus connection");

	GDBusNodeInfo *obj_info;
	GError *error = NULL;

	/* Register ObjectManager interface */
	obj_info = __bt_gatt_create_method_node_info(
					manager_introspection_xml);

	if (obj_info == NULL) {
		BT_ERR("failed to get node info");
		return;
	}

	manager_id = g_dbus_connection_register_object(g_conn, "/",
						obj_info->interfaces[0],
						&manager_interface_vtable,
						NULL, NULL, &error);

	if (manager_id == 0) {
		BT_ERR("failed to register: %s", error->message);
		g_error_free(error);
	}
}

static void __on_name_acquired(GDBusConnection *connection,
				const gchar *name, gpointer user_data)
{
	BT_DBG("on_name_acquired: %s", name);
}

static void __on_name_lost(GDBusConnection *connection,
				const gchar *name, gpointer user_data)
{
	BT_ERR("on_name_lost: %s", name);
}

static struct gatt_service_info *__bt_gatt_find_gatt_service_info(
			const char *service_path)
{
	GSList *l;

	for (l = gatt_services; l != NULL; l = l->next) {
		struct gatt_service_info *info = l->data;

		if (g_strcmp0(info->serv_path, service_path) == 0)
			return info;
	}
	BT_ERR("Gatt service not found");
	return NULL;
}

static struct gatt_char_info *__bt_gatt_find_gatt_char_info(
			const char *service_path, const char *char_path)
{
	GSList *l1, *l2;

	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		if (g_strcmp0(serv_info->serv_path, service_path) == 0) {

			for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
				struct gatt_char_info *char_info = l2->data;

				if (g_strcmp0(char_info->char_path, char_path)
							== 0)
					return char_info;
			}
			BT_ERR("Gatt characteristic not found");
			return NULL;
		}
	}
	BT_ERR("Gatt service not found");
	return NULL;
}

static GDBusProxy *__bt_gatt_gdbus_init_manager_proxy(const gchar *service,
				const gchar *path, const gchar *interface)
{
	GDBusProxy *proxy;
	GError *err = NULL;

	if (g_conn == NULL)
		g_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM,
							NULL, &err);

	if (!g_conn) {
		if (err) {
			BT_ERR("Unable to connect to gdbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	proxy =  g_dbus_proxy_new_sync(g_conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			service, path,
			interface, NULL, &err);

	if (!proxy) {
		if (err) {
			BT_ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}
	manager_gproxy = proxy;

	return proxy;
}

static GDBusProxy *__bt_gatt_gdbus_get_manager_proxy(const gchar *service,
				const gchar *path, const gchar *interface)
{
	return (manager_gproxy) ? manager_gproxy :
			__bt_gatt_gdbus_init_manager_proxy(service,
				path, interface);
}

static gboolean __bt_gatt_export_properties_method(const char *svc_path)
{
	guint prop_id;
	GDBusNodeInfo *prop_info;
	GError *error = NULL;
	struct gatt_service_info *svc_info;

	BT_DBG("svc_path %s", svc_path);
	svc_info = __bt_gatt_find_gatt_service_info(svc_path);

	if (!svc_info) {
	    BT_ERR("Unable to find service info");
		return FALSE;
	}

	/* Register ObjectManager interface */
	prop_info = __bt_gatt_create_method_node_info(
						properties_introspection_xml);

	if (prop_info == NULL) {
		BT_ERR("failed to get node info");
		return FALSE;
	}

	prop_id = g_dbus_connection_register_object(g_conn, svc_path,
						prop_info->interfaces[0],
						&properties_interface_vtable,
						NULL, NULL, &error);

	if (prop_id == 0) {
		BT_ERR("failed to register: %s", error->message);
		g_error_free(error);
		return FALSE;
	}
	svc_info->prop_id = prop_id;

	return TRUE;
}


static void __bt_gatt_set_service_state(const char *service_path,
			gboolean state)
{
	struct gatt_service_info *svc_info = NULL;
	svc_info = __bt_gatt_find_gatt_service_info(service_path);

	if (svc_info != NULL) {
		BT_DBG("Updating the gatt service register state %d", state);
		svc_info->is_svc_registered = state;
		return;
	}

	BT_DBG("gatt service not found");
}

static gboolean __bt_gatt_get_service_state(const char *service_path)
{
	struct gatt_service_info *svc_info = NULL;

	svc_info = __bt_gatt_find_gatt_service_info(service_path);

	if (svc_info != NULL) {
		BT_DBG("Return the state of the gatt service %d",
			svc_info->is_svc_registered);
		return svc_info->is_svc_registered;
	}

	BT_DBG("gatt service info is NULL");
	return FALSE;
}

void register_service_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
	BT_DBG("register_service_cb\n");

	GError *error = NULL;
	GVariant *result;

	result = g_dbus_proxy_call_finish(manager_gproxy, res, &error);

	if (result == NULL) {
		/* dBUS-RPC is failed */
		BT_ERR("Dbus-RPC is failed\n");

		if (error != NULL) {
		/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]\n",
						error->code, error->message);
			g_clear_error(&error);
		}
	}
}

void unregister_service_cb(GObject *object, GAsyncResult *res,
		gpointer user_data)
{
	BT_DBG("unregister_service_cb\n");

	GError *error = NULL;
	GVariant *result;

	result = g_dbus_proxy_call_finish(manager_gproxy, res, &error);

	if (result == NULL) {
		/* dBUS-RPC is failed */
		BT_ERR("Dbus-RPC is failed\n");

		if (error != NULL) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	}
}

static int __bt_gatt_unregister_service(const char *service_path)
{
	if (!__bt_gatt_get_service_state(service_path)) {
		BT_DBG("service not registered \n");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	GDBusProxy *proxy = NULL;

	proxy = __bt_gatt_gdbus_get_manager_proxy("org.bluez",
					"/org/bluez", GATT_MNGR_INTERFACE);

	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	/* Async Call to Unregister Service */
	g_dbus_proxy_call(proxy,
				"UnregisterService",
				g_variant_new("(o)",
				service_path),
				G_DBUS_CALL_FLAGS_NONE, -1,
				NULL,
				(GAsyncReadyCallback) unregister_service_cb,
				NULL);

	__bt_gatt_set_service_state(service_path, FALSE);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_init(void)
{
	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				BT_GATT_SERVICE_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				__on_bus_acquired,
				__on_name_acquired,
				__on_name_lost,
				NULL, NULL);

	BT_DBG("owner_id is [%d]", owner_id);

	serv_id = 1;

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_deinit()
{
	/* Unown gdbus bus */
	if (owner_id) {

		/* remove/unregister all services */
		BT_DBG("removing all registered gatt service\n");
		bluetooth_gatt_delete_services();

		g_bus_unown_name(owner_id);

		/* unregister the exported interface for object manager */
		g_dbus_connection_unregister_object(g_conn,
					manager_id);

		BT_DBG("Gatt service deinitialized \n");

		g_slist_free(gatt_services);
		gatt_services = NULL;

		return BLUETOOTH_ERROR_NONE;
	}

	return BLUETOOTH_ERROR_NOT_FOUND;
}

BT_EXPORT_API int bluetooth_gatt_add_service(const char *svc_uuid,
			char **svc_path)
{
	GError *error = NULL;
	guint object_id;
	GDBusNodeInfo *node_info;
	gchar *path = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	struct gatt_service_info *serv_info = NULL;

	node_info = __bt_gatt_create_method_node_info(
					service_introspection_xml);

	if (node_info == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	path = g_strdup_printf(GATT_SERV_OBJECT_PATH"%d", serv_id++);
	BT_DBG("gatt service path is [%s]", path);

	object_id = g_dbus_connection_register_object(g_conn, path,
					node_info->interfaces[0],
					&serv_interface_vtable,
					NULL, NULL, &error);

	if (object_id == 0) {
		BT_ERR("failed to register: %s", error->message);
		g_error_free(error);
		g_free(path);

		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* Add object_id/gatt service information; it's required at the time of
	 *  service unregister and Getmanagedobjects
	 */
	serv_info = g_new0(struct gatt_service_info, 1);

	serv_info->serv_path = g_strdup(path);
	serv_info->serv_id = object_id;
	serv_info->service_uuid = g_strdup(svc_uuid);
	serv_info->is_svc_registered = FALSE;

	gatt_services = g_slist_append(gatt_services, serv_info);

	/* emit interfacesadded signal here for service path */
	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}",
		"UUID", g_variant_new_string(svc_uuid));

	g_variant_builder_add(builder, "{sa{sv}}",
		GATT_SERV_INTERFACE, inner_builder);

	g_dbus_connection_emit_signal(g_conn, NULL, "/",
				"org.freedesktop.Dbus.ObjectManager",
				"InterfacesAdded",
				g_variant_new("(oa{sa{sv}})",
				path, builder),
				&error);

	new_service = TRUE;

	*svc_path = g_strdup(path);

	g_free(path);
	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_add_characteristics(
			const char *char_uuid, const char *char_value,
			int value_length, const char *char_flags[],
			int flags_length, const char *svc_path,
			char **char_path)
{
	static int char_id;
	GError *error = NULL;
	guint object_id;
	GDBusNodeInfo *node_info;
	gchar *path = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	struct gatt_service_info *serv_info = NULL;
	struct gatt_char_info *char_info = NULL;
	GVariantBuilder *builder1 = NULL;
	GVariantBuilder *builder2 = NULL;
	GVariant *char_val = NULL;
	GVariant *flags_val = NULL;
	int i = 0;

	if (new_service) {
		char_id = 1;
		new_service = FALSE;
	}

	node_info = __bt_gatt_create_method_node_info(
					characteristics_introspection_xml);

	if (node_info == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	path = g_strdup_printf("%s/characteristic%d", svc_path, char_id++);
	BT_DBG("gatt characteristic path is [%s]", path);

	object_id = g_dbus_connection_register_object(g_conn, path,
					node_info->interfaces[0],
					&char_interface_vtable,
					NULL, NULL, &error);

	if (object_id == 0) {
		BT_ERR("failed to register: %s", error->message);
		g_error_free(error);
		g_free(path);

		return BLUETOOTH_ERROR_INTERNAL;
	}

	BT_DBG("gatt svc_path path is [%s]", svc_path);
	serv_info = __bt_gatt_find_gatt_service_info(svc_path);

	if (serv_info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	char_info = g_new0(struct gatt_char_info, 1);

	char_info->char_path = g_strdup(path);
	char_info->char_id = object_id;
	char_info->char_uuid = g_strdup(char_uuid);
	char_info->value_length = value_length;

	char_info->char_value = (char *)malloc(value_length);
	for (i = 0; i < value_length; i++)
		char_info->char_value[i] = char_value[i];

	for (i = 0; i < flags_length; i++)
		char_info->char_flags[i] = char_flags[i];

	char_info->flags_length = flags_length;

	serv_info->char_data = g_slist_append(serv_info->char_data, char_info);

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new("s", char_uuid));
	g_variant_builder_add(inner_builder, "{sv}", "Service",
				g_variant_new("o", svc_path));

	builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < value_length; i++) {
		g_variant_builder_add(builder1, "y", char_value[i]);
	}
	char_val = g_variant_new("ay", builder1);
	g_variant_builder_add(inner_builder, "{sv}", "Value", char_val);

	builder2 = g_variant_builder_new(G_VARIANT_TYPE("as"));

	for (i = 0; i < flags_length; i++) {
		g_variant_builder_add(builder2, "s", char_flags[i]);
	}
	flags_val = g_variant_new("as", builder2);
	g_variant_builder_add(inner_builder, "{sv}", "Flags",
				flags_val);

	g_variant_builder_add(builder, "{sa{sv}}",
				GATT_CHAR_INTERFACE,
				inner_builder);

	g_dbus_connection_emit_signal(g_conn, NULL, "/",
				"org.freedesktop.Dbus.ObjectManager",
				"InterfacesAdded",
				g_variant_new("(oa{sa{sv}})",
				path, builder),
				&error);

	*char_path = g_strdup(path);

	new_char = TRUE;

	g_free(path);

	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_add_descriptor(const char *desc_uuid,
			const char *desc_value, int value_length,
			const char *permissions, const char *char_path,
			char **desc_path)
{
	static int desc_id = 1;
	GError *error = NULL;
	guint object_id;
	GDBusNodeInfo *node_info;
	gchar *path = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	GVariantBuilder *builder1 = NULL;
	struct gatt_char_info *char_info = NULL;
	struct gatt_desc_info *desc_info = NULL;
	gchar **line_argv = NULL;
	char *serv_path;
	GVariant *desc_val = NULL;
	int i ;

	if (new_char) {
		desc_id = 1;
		new_char = FALSE;
	}

	node_info = __bt_gatt_create_method_node_info(
					descriptor_introspection_xml);

	if (node_info == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	path = g_strdup_printf("%s/descriptor%d", char_path, desc_id++);
	BT_DBG("gatt descriptor path is [%s]", path);

	object_id = g_dbus_connection_register_object(g_conn, path,
				node_info->interfaces[0],
				&desc_interface_vtable,
				NULL, NULL, &error);

	if (object_id == 0) {
		BT_ERR("failed to register: %s", error->message);
		g_error_free(error);
		g_free(path);

		return BLUETOOTH_ERROR_INTERNAL;
	}

	line_argv = g_strsplit_set(char_path, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);

	char_info = __bt_gatt_find_gatt_char_info(serv_path, char_path);

	g_free(serv_path);

	if (char_info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

	desc_info = g_new0(struct gatt_desc_info, 1);

	desc_info->desc_path = g_strdup(path);
	desc_info->desc_id = object_id;
	desc_info->desc_uuid = g_strdup(desc_uuid);
	desc_info->desc_value = (char *)malloc(value_length);

	for (i = 0; i < value_length; i++)
		desc_info->desc_value[i] = desc_value[i];

	desc_info->value_length = value_length;
	desc_info->permissions = g_strdup(permissions);

	char_info->desc_data = g_slist_append(char_info->desc_data, desc_info);

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new("s", desc_uuid));
	g_variant_builder_add(inner_builder, "{sv}", "Characteristic",
				g_variant_new("o", char_path));

	builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < value_length; i++) {
		g_variant_builder_add(builder1, "y", desc_value[i]);
	}
	desc_val = g_variant_new("ay", builder1);
	g_variant_builder_add(inner_builder, "{sv}", "Value", desc_val);

	g_variant_builder_add(inner_builder, "{sv}", "Permissions",
				g_variant_new("s", permissions));

	g_variant_builder_add(builder, "{sa{sv}}",
				GATT_DESC_INTERFACE,
				inner_builder);

	g_dbus_connection_emit_signal(g_conn, NULL, "/",
				"org.freedesktop.Dbus.ObjectManager",
				"InterfacesAdded",
				g_variant_new("(oa{sa{sv}})",
				path, builder),
				&error);

	*desc_path = g_strdup(path);

	g_free(path);
	g_strfreev(line_argv);
	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);
	g_variant_builder_unref(builder1);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_register_service(
			const char *svc_path)
{
	GDBusProxy *proxy = NULL;
	gchar *path = NULL;

	if (__bt_gatt_get_service_state(svc_path)) {
		BT_DBG("service already registered \n");
		return BLUETOOTH_ERROR_NONE;
	}

	if (!__bt_gatt_export_properties_method(svc_path)) {
		BT_ERR("Failed to export Object manager method");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	proxy = __bt_gatt_gdbus_get_manager_proxy("org.bluez",
					"/org/bluez", GATT_MNGR_INTERFACE);
	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	path = g_strdup(svc_path);

	g_dbus_proxy_call(proxy,
			"RegisterService",
			g_variant_new("(oa{sv})",
			path, NULL),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL,
			(GAsyncReadyCallback) register_service_cb,
			NULL);

	__bt_gatt_set_service_state(svc_path, TRUE);

	g_free(path);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_delete_services(void)
{
	GSList *l;
	int error = BLUETOOTH_ERROR_NONE;
	l = gatt_services;

	if (l != NULL) {
		for (l = gatt_services; l != NULL; l = l->next) {
			struct gatt_service_info *info = l->data;
			BT_DBG("svc_path is %s", info->serv_path);
			if (bluetooth_gatt_remove_service(info->serv_path)
					!= BLUETOOTH_ERROR_NONE) {
				error = BLUETOOTH_ERROR_INTERNAL;
				BT_DBG(" Error in removing service %s \n",
						 info->serv_path);
			}
		}
		BT_DBG(" All services removed successfully.\n ");
	}
	else {
		BT_DBG(" There are no registered services.\n ");
	}

	g_slist_free(gatt_services);
	gatt_services = NULL;
	serv_id = 1;

	if (error != BLUETOOTH_ERROR_NONE)
		return error;

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_update_characteristic(
			const char *char_path, const char* char_value,
			int value_length)
{
	GVariantBuilder *outer_builder;
	GVariantBuilder *inner_builder;
	GVariantBuilder *invalidated_builder;
	GVariant *update_value = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	int err = BLUETOOTH_ERROR_NONE;
	int i = 0;
	gchar **line_argv = NULL;
	gchar *serv_path = NULL;

	line_argv = g_strsplit_set(char_path, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);

	if (!__bt_gatt_get_service_state(serv_path)) {
		BT_DBG("service not registered for this characteristic \n");
		g_free(serv_path);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_free(serv_path);

	outer_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
	invalidated_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

	inner_builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
	for (i = 0; i < value_length; i++) {
		g_variant_builder_add(inner_builder, "y", char_value[i]);
	}

	update_value = g_variant_new("ay", inner_builder);

	outer_builder = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add(outer_builder, "{sv}", "Value",
					update_value);

	BT_DBG("Updating characteristic value \n");
	ret = g_dbus_connection_emit_signal(g_conn, NULL,
					char_path,
					"org.freedesktop.DBus.Properties",
					"PropertiesChanged",
					g_variant_new("(sa{sv}as)",
					"org.bluez.GattCharacteristic1",
					outer_builder, invalidated_builder),
					&error);

	if (!ret) {
		if (error != NULL) {
			BT_ERR("D-Bus API failure: errCode[%x], \
					message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		}
		err = BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(outer_builder);

	return err;
}

BT_EXPORT_API int bluetooth_gatt_remove_service(const char *svc_path)
{
	GSList *l, *l1;
	struct gatt_service_info *svc_info;
	gboolean ret;
	int err = BLUETOOTH_ERROR_NONE;
	GSList *tmp;

	BT_DBG("svc_path %s", svc_path);
	svc_info = __bt_gatt_find_gatt_service_info(svc_path);

	if (!svc_info) {
		BT_ERR("Unable to find service info");
		return BLUETOOTH_ERROR_NOT_FOUND;
	}

	err = __bt_gatt_unregister_service(svc_path);
	if (err != BLUETOOTH_ERROR_NONE) {
		BT_DBG("Could not unregister application");
		return err;
	}

	for (l = svc_info->char_data; l != NULL; l = l->next) {
		struct gatt_char_info *char_info = l->data;

		for (l1 = char_info->desc_data; l1 != NULL; l1 = l1->next) {
			struct gatt_desc_info *desc_info = l1->data;

			ret = g_dbus_connection_unregister_object(g_conn,
						desc_info->desc_id);
			if (ret) {
				__bt_gatt_emit_interface_removed(
						desc_info->desc_path,
						GATT_DESC_INTERFACE);
			} else {
				err = BLUETOOTH_ERROR_INTERNAL;
			}
		}
		ret = g_dbus_connection_unregister_object(g_conn,
					char_info->char_id);
		if (ret) {
			__bt_gatt_emit_interface_removed(char_info->char_path,
						GATT_CHAR_INTERFACE);
		} else {
			err = BLUETOOTH_ERROR_INTERNAL;
		}
	}
	ret = g_dbus_connection_unregister_object(g_conn, svc_info->serv_id);
	if (ret) {
		__bt_gatt_emit_interface_removed(svc_info->serv_path,
						GATT_SERV_INTERFACE);
	} else {
		err = BLUETOOTH_ERROR_INTERNAL;
	}

	ret = g_dbus_connection_unregister_object(g_conn, svc_info->prop_id);
	if (ret) {
		BT_DBG("Unregistered the service on properties interface");
	}

	for (tmp = gatt_services; tmp != NULL; tmp = tmp->next) {
		struct gatt_service_info *info = tmp->data;

		if (g_strcmp0(info->serv_path, svc_path) == 0) {
			g_slist_delete_link(gatt_services, tmp->data);
		}
	}

	new_service = FALSE;

	if (gatt_services->next == NULL)
		serv_id--;

	return err;
}
