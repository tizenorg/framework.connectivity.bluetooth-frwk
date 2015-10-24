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
static int register_pending_cnt = 0;

/* Introspection data for the service we are exporting */
static const gchar service_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.freedesktop.DBus.ObjectManager'>"
"	 <method name='GetManagedObjects'>"
"	  <arg type='a{oa{sa{sv}}}' name='object_paths_interfaces_and_properties' direction='out'/>"
"	 </method>"
"  </interface>"
"  <interface name='org.bluez.GattService1'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"	 <property type='b' name='primary' access='read'>"
"	 </property>"
"	 <property type='o' name='Device' access='read'>"
"	 </property>"
"	 <property type='ao' name='Characteristics' access='read'>"
"	 </property>"
"	 <property type='s' name='Includes' access='read'>"
"	 </property>"
"  </interface>"
"</node>";

/* Introspection data for the characteristics we are exporting */
static const gchar characteristics_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.GattCharacteristic1'>"
"	 <method name='ReadValue'>"
"		<arg type='s' name='address' direction='in'/>"
"		<arg type='y' name='id' direction='in'/>"
"		<arg type='q' name='offset' direction='in'/>"
"		<arg type='ay' name='Value' direction='out'/>"
"	 </method>"
"	 <method name='WriteValue'>"
"		<arg type='s' name='address' direction='in'/>"
"		<arg type='y' name='id' direction='in'/>"
"		<arg type='q' name='offset' direction='in'/>"
"		<arg type='ay' name='value' direction='in'/>"
"	 </method>"
"	 <method name='StartNotify'>"
"	 </method>"
"	 <method name='StopNotify'>"
"	 </method>"
"  </interface>"
"  <interface name='org.freedesktop.DBus.Properties'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"    <property type='o' name='Service' access='read'>"
"    </property>"
"    <property type='ay' name='Value' access='readwrite'>"
"    </property>"
"    <property type='b' name='Notifying' access='read'>"
"    </property>"
"    <property type='as' name='Flags' access='read'>"
"    </property>"
"    <property type='s' name='Unicast' access='read'>"
"    </property>"
"    <property type='ao' name='Descriptors' access='read'>"
"    </property>"
"  </interface>"
"</node>";

/* Introspection data for the descriptor we are exporting */
static const gchar descriptor_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.bluez.GattDescriptor1'>"
"	 <method name='ReadValue'>"
"		<arg type='s' name='address' direction='in'/>"
"		<arg type='y' name='id' direction='in'/>"
"		<arg type='q' name='offset' direction='in'/>"
"		<arg type='ay' name='Value' direction='out'/>"
"	 </method>"
"	 <method name='WriteValue'>"
"		<arg type='s' name='address' direction='in'/>"
"		<arg type='y' name='id' direction='in'/>"
"		<arg type='q' name='offset' direction='in'/>"
"		<arg type='ay' name='value' direction='in'/>"
"	 </method>"
"  </interface>"
"  <interface name='org.freedesktop.DBus.Properties'>"
"    <property type='s' name='UUID' access='read'>"
"    </property>"
"    <property type='o' name='Characteristic' access='read'>"
"    </property>"
"    <property type='ay' name='Value' access='read'>"
"    </property>"
"    <property type='s' name='Permissions' access='read'>"
"    </property>"
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
	gboolean is_svc_primary;
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
	gchar *desc_value;
	int value_length;
};

struct gatt_req_info {
	gchar *attr_path;
	gchar *svc_path;
	guint  request_id;
	guint  offset;
	GDBusMethodInvocation *context;
};

static GSList *gatt_services = NULL;
static GSList *gatt_requests = NULL;

#define BT_GATT_SERVICE_NAME	"org.frwk.gatt_service"
#define BT_GATT_SERVICE_PATH "/org/frwk/gatt_service"

#define GATT_SERV_OBJECT_PATH	"/service"

#define GATT_MNGR_INTERFACE		"org.bluez.GattManager1"
#define GATT_SERV_INTERFACE		"org.bluez.GattService1"
#define GATT_CHAR_INTERFACE		"org.bluez.GattCharacteristic1"
#define GATT_DESC_INTERFACE		"org.bluez.GattDescriptor1"

#ifdef HPS_FEATURE
#define BT_HPS_OBJECT_PATH "/org/projectx/httpproxy"
#define BT_HPS_INTERFACE_NAME "org.projectx.httpproxy_service"
#define PROPERTIES_CHANGED "PropertiesChanged"
#define BT_HPS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#endif

static GDBusProxy *manager_gproxy = NULL;

static struct gatt_char_info *__bt_gatt_find_gatt_char_info(
			const char *service_path, const char *char_path);
static struct gatt_desc_info *__bt_gatt_find_gatt_desc_info(
			const char *serv_path, const char *char_path,
			const char *desc_path);

static struct gatt_req_info *__bt_gatt_find_request_info(guint request_id);

#ifdef HPS_FEATURE
static int __bt_send_event_to_hps(int event, GVariant *var)
{
	GError *error = NULL;
	GVariant *parameters;
	GDBusMessage *msg = NULL;

	BT_DBG(" ");

	retv_if(g_conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	if (event == BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED) {
		GVariantBuilder *inner_builder;
		GVariantBuilder *invalidated_builder;

		BT_DBG("BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED");
		inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

		g_variant_builder_add(inner_builder, "{sv}", "WriteValue", var);

		invalidated_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

		parameters = g_variant_new("(a{sv}as)", inner_builder, invalidated_builder);
		g_variant_builder_unref(invalidated_builder);
		g_variant_builder_unref(inner_builder);
	} else if (BLUETOOTH_EVENT_GATT_SERVER_READ_REQUESTED) {
		GVariantBuilder *inner_builder;
		GVariantBuilder *invalidated_builder;

		BT_DBG("BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED");
		inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

		g_variant_builder_add(inner_builder, "{sv}", "ReadValue", var);

		invalidated_builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

		parameters = g_variant_new("(a{sv}as)", inner_builder, invalidated_builder);
		g_variant_builder_unref(invalidated_builder);
		g_variant_builder_unref(inner_builder);
	}

	msg = g_dbus_message_new_signal(BT_HPS_OBJECT_PATH, BT_HPS_INTERFACE_NAME, PROPERTIES_CHANGED);
	g_dbus_message_set_body(msg, parameters);
	if (!g_dbus_connection_send_message(g_conn, msg,G_DBUS_SEND_MESSAGE_FLAGS_NONE, 0, NULL)) {
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

static void __bt_gatt_serv_method_call(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *method_name,
					GVariant *parameters,
					GDBusMethodInvocation *invocation,
					gpointer user_data)
{
	GSList *l1;

	if (g_strcmp0(method_name, "GetManagedObjects") == 0) {
		BT_DBG("Getting values for service, chars and descriptors");

		GVariantBuilder *builder;
		GVariantBuilder *inner_builder1 = NULL;
		GVariant *svc_char = NULL;
		GSList *l4;
		/*Main Builder */
		builder = g_variant_builder_new(
				G_VARIANT_TYPE("a{oa{sa{sv}}}"));

		/* Prepare inner builder for GattService1 interface */

		GVariantBuilder *svc_builder = NULL;
		GVariantBuilder *inner_builder = NULL;

		if (register_pending_cnt > 1) {
			int len = g_slist_length(gatt_services);
			l1 = g_slist_nth(gatt_services, len - register_pending_cnt);
		} else {
			l1 = g_slist_last(gatt_services);
		}
		register_pending_cnt--;

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

		g_variant_builder_add(inner_builder, "{sv}", "Primary",
				g_variant_new_boolean(serv_info->is_svc_primary));

		/*Characteristics*/
		inner_builder1 = g_variant_builder_new(G_VARIANT_TYPE("ao"));
		BT_DBG("Adding Charatarisitcs list");
		for (l4 = serv_info->char_data; l4 != NULL; l4 = l4->next) {
			struct gatt_char_info *char_info = l4->data;
				g_variant_builder_add(inner_builder1, "o",
					char_info->char_path);
				BT_DBG("%s", char_info->char_path);
		}

		svc_char = g_variant_new("ao", inner_builder1);
		g_variant_builder_add(inner_builder, "{sv}", "Characteristics",
					svc_char);

		g_variant_builder_add(svc_builder, "{sa{sv}}",
							GATT_SERV_INTERFACE,
							inner_builder);

		g_variant_builder_add(builder, "{oa{sa{sv}}}",
							serv_info->serv_path,
							svc_builder);

		g_variant_builder_unref(inner_builder1);

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
			GVariantBuilder *builder3 = NULL;
			GVariant *char_val = NULL;
			GVariant *flags_val = NULL;
			GVariant *char_desc = NULL;
			char *unicast = NULL;
			gboolean notify = FALSE;
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

			if(char_info->char_value != NULL) {
				for (i = 0; i < char_info->value_length; i++) {
					g_variant_builder_add(builder1, "y",
						char_info->char_value[i]);
				}
				char_val = g_variant_new("ay", builder1);
				g_variant_builder_add(inner_builder, "{sv}",
						"Value", char_val);
			}
			/*Flags*/
			builder2 = g_variant_builder_new(G_VARIANT_TYPE("as"));

			for (i = 0; i < char_info->flags_length; i++) {
				g_variant_builder_add(builder2, "s",
					char_info->char_flags[i]);
			}

			flags_val = g_variant_new("as", builder2);
			g_variant_builder_add(inner_builder, "{sv}", "Flags",
						flags_val);

			/* Notifying */
			g_variant_builder_add(inner_builder, "{sv}", "Notifying",
						g_variant_new("b", notify));

			/* Unicast */
			unicast = g_strdup("00:00:00:00:00:00");
			g_variant_builder_add(inner_builder, "{sv}", "Unicast",
						g_variant_new("s", unicast));

			/*Descriptors*/
			builder3 = g_variant_builder_new(G_VARIANT_TYPE("ao"));
			BT_DBG("Adding Descriptors list");

			for (l4 = char_info->desc_data; l4 != NULL; l4 = l4->next) {
				struct gatt_desc_info *desc_info = l4->data;
					g_variant_builder_add(builder3, "o",
						desc_info->desc_path);
					BT_DBG("%s", desc_info->desc_path);
			}

			char_desc = g_variant_new("ao", builder3);
			g_variant_builder_add(inner_builder, "{sv}", "Descriptors",
						char_desc);

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
				builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

				if(desc_info->desc_value != NULL) {
					for (i = 0; i < desc_info->value_length; i++) {
						g_variant_builder_add(builder1, "y",
							desc_info->desc_value[i]);
					}
					desc_val = g_variant_new("ay", builder1);
					g_variant_builder_add(inner_builder, "{sv}",
							"Value", desc_val);
				}

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

			if (unicast)
				g_free(unicast);
			/*unref char builder pointers*/
			g_variant_builder_unref(builder1);
			g_variant_builder_unref(builder2);
			g_variant_builder_unref(builder3);
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

static struct gatt_service_info *__bt_gatt_find_gatt_service_from_char(const char *char_path)
{
	GSList *l1, *l2;

	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
			struct gatt_char_info *char_info = l2->data;

			if (g_strcmp0(char_info->char_path, char_path)
						== 0)
				return serv_info;
		}
	}
	BT_ERR("Gatt service not found");
	return NULL;
}

static struct gatt_service_info *__bt_gatt_find_gatt_service_from_desc(const char *desc_path)
{
	GSList *l1, *l2, *l3;

	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
			struct gatt_char_info *char_info = l2->data;

			for (l3 = char_info->desc_data; l3 != NULL; l3 = l3->next) {
				struct gatt_desc_info *desc_info = l3->data;

				if (g_strcmp0(desc_info->desc_path, desc_path)
							== 0)
					return serv_info;
			}
		}
	}
	BT_ERR("Gatt service not found");
	return NULL;
}

static struct gatt_char_info *__bt_gatt_find_gatt_char_from_desc(const char *desc_path)
{
	GSList *l1, *l2, *l3;

	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
			struct gatt_char_info *char_info = l2->data;

			for (l3 = char_info->desc_data; l3 != NULL; l3 = l3->next) {
				struct gatt_desc_info *desc_info = l3->data;

				if (g_strcmp0(desc_info->desc_path, desc_path)
							== 0)
					return char_info;
			}
		}
	}
	BT_ERR("Gatt Characterisitc not found");
	return NULL;
}

static void __bt_gatt_char_method_call(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *method_name,
					GVariant *parameters,
					GDBusMethodInvocation *invocation,
					gpointer user_data)
{

	if (g_strcmp0(method_name, "ReadValue") == 0) {
		gchar *addr = NULL;
		guint8 req_id = 1;
		guint16 offset = 0;
		bt_gatt_read_req_t read_req = {0, };
		bt_user_info_t *user_info = NULL;
		struct gatt_req_info *req_info = NULL;
		struct gatt_service_info *svc_info = NULL;

		int i;
		BT_DBG("ReadValue");

		g_variant_get(parameters, "(&syq)", &addr, &req_id, &offset);

		BT_DBG("Application path = %s", object_path);

		BT_DBG("Remote Device address number = %s", addr);
		BT_DBG("Request id = %d, Offset = %d", req_id, offset);

		BT_DBG("Sender = %s", sender);

		read_req.att_handle = g_strdup(object_path);
		read_req.address = g_strdup(addr);
		read_req.req_id = req_id;
		read_req.offset = offset;
		svc_info = __bt_gatt_find_gatt_service_from_char(object_path);
		if (svc_info != NULL) {
			read_req.service_handle = g_strdup(svc_info->serv_path);
			user_info = _bt_get_user_data(BT_COMMON);
#ifdef HPS_FEATURE
			GVariant *param = NULL;
#endif

			/* Store requets information */
			req_info = g_new0(struct gatt_req_info, 1);
			req_info->attr_path = g_strdup(object_path);
			req_info->svc_path = g_strdup(read_req.service_handle);
			req_info->request_id= req_id;
			req_info->offset = offset;
			req_info->context = invocation;
			gatt_requests = g_slist_append(gatt_requests, req_info);

			if (user_info != NULL) {
				struct gatt_char_info *char_info = NULL;
				_bt_common_event_cb(
					BLUETOOTH_EVENT_GATT_SERVER_READ_REQUESTED,
					BLUETOOTH_ERROR_NONE, &read_req,
					user_info->cb, user_info->user_data);
			}
#ifdef HPS_FEATURE
			param = g_variant_new("(sssyq)",
					read_req.att_handle,
					read_req.service_handle,
					read_req.address,
					read_req.req_id,
					read_req.offset);
			__bt_send_event_to_hps(BLUETOOTH_EVENT_GATT_SERVER_READ_REQUESTED, param);
#endif
		}

		if (read_req.att_handle)
			g_free(read_req.att_handle);
		if (read_req.address)
			g_free(read_req.address);
		if (read_req.service_handle)
			g_free(read_req.service_handle);
		return;
	} else if (g_strcmp0(method_name, "WriteValue") == 0) {
		GVariant *var = NULL;
		gchar *addr = NULL;
		guint8 req_id = 0;
		guint16 offset = 0;
		bt_gatt_value_change_t value_change = {0, };
		bt_user_info_t *user_info = NULL;
		int len = 0;
		struct gatt_service_info *svc_info = NULL;
		struct gatt_req_info *req_info = NULL;
#ifdef HPS_FEATURE
		GVariant *param = NULL;
#endif

		BT_DBG("WriteValue");
		BT_DBG("Application path = %s", object_path);
		BT_DBG("Sender = %s", sender);

		g_variant_get(parameters, "(&syq@ay)", &addr, &req_id, &offset, &var);

		value_change.att_handle = g_strdup(object_path);
		value_change.address = g_strdup(addr);
		svc_info = __bt_gatt_find_gatt_service_from_char(object_path);
		if (svc_info == NULL) {
			g_variant_unref(var);
			g_dbus_method_invocation_return_value(invocation, NULL);
			return;
		}

		value_change.service_handle = g_strdup(svc_info->serv_path);
		value_change.offset = offset;
		value_change.req_id = req_id;

		len = g_variant_get_size(var);
		if (len > 0) {
			char *data;

			value_change.att_value = (guint8 *)malloc(len);
			if (!value_change.att_value) {
				BT_ERR("att_value is NULL");
				g_variant_unref(var);
				g_dbus_method_invocation_return_value(invocation, NULL);
				return;
			}

			data = (char *)g_variant_get_data(var);
			memcpy(value_change.att_value, data, len);
		}

		value_change.val_len = len;

		/* Store requets information */
		req_info = g_new0(struct gatt_req_info, 1);
		req_info->attr_path = g_strdup(object_path);
		req_info->svc_path = g_strdup(value_change.service_handle);
		req_info->request_id= req_id;
		req_info->offset = offset;
		req_info->context = invocation;
		gatt_requests = g_slist_append(gatt_requests, req_info);

		user_info = _bt_get_user_data(BT_COMMON);
		if (user_info != NULL) {
			_bt_common_event_cb(
				BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED,
				BLUETOOTH_ERROR_NONE, &value_change,
				user_info->cb, user_info->user_data);
		}
#ifdef HPS_FEATURE
		if (len > 0) {
			gchar *svc_path;
			svc_path = g_strdup(svc_info->serv_path);
			param = g_variant_new("(sssyq@ay)",
					object_path,
					svc_path,
					addr,
					req_id,
					offset,
					var);
			__bt_send_event_to_hps(BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED, param);
			if (svc_path)
				g_free(svc_path);
		}
#endif
		g_variant_unref(var);
		return;
	} else if (g_strcmp0(method_name, "StartNotify") == 0) {
		bt_user_info_t *user_info = NULL;
		bt_gatt_char_notify_change_t notify_change = {0, };
		BT_DBG("StartNotify");
		user_info = _bt_get_user_data(BT_COMMON);
		if (user_info != NULL) {
			struct gatt_service_info *svc_info = NULL;
			svc_info = __bt_gatt_find_gatt_service_from_char(object_path);
			if (svc_info) {
				notify_change.service_handle = g_strdup(svc_info->serv_path);
				notify_change.att_handle = g_strdup(object_path);
				notify_change.att_notify = TRUE;
				_bt_common_event_cb(
					BLUETOOTH_EVENT_GATT_SERVER_NOTIFICATION_STATE_CHANGED,
					BLUETOOTH_ERROR_NONE, &notify_change,
					user_info->cb, user_info->user_data);
			}
		}
	} else if (g_strcmp0(method_name, "StopNotify") == 0) {
		bt_user_info_t *user_info = NULL;
		bt_gatt_char_notify_change_t notify_change = {0, };
		BT_DBG("StopNotify");
		user_info = _bt_get_user_data(BT_COMMON);
		if (user_info != NULL) {
			struct gatt_service_info *svc_info = NULL;
			svc_info = __bt_gatt_find_gatt_service_from_char(object_path);
			if (svc_info) {
				notify_change.service_handle = g_strdup(svc_info->serv_path);
				notify_change.att_handle = g_strdup(object_path);
				notify_change.att_notify = FALSE;
				_bt_common_event_cb(
					BLUETOOTH_EVENT_GATT_SERVER_NOTIFICATION_STATE_CHANGED,
					BLUETOOTH_ERROR_NONE, &notify_change,
					user_info->cb, user_info->user_data);
			}
		}
	}
	g_dbus_method_invocation_return_value(invocation, NULL);
}

static void __bt_gatt_desc_method_call(GDBusConnection *connection,
					const gchar *sender,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *method_name,
					GVariant *parameters,
					GDBusMethodInvocation *invocation,
					gpointer user_data)
{
	GVariantBuilder *inner_builder = NULL;
	int i;

	if (g_strcmp0(method_name, "ReadValue") == 0) {
		gchar *addr = NULL;
		guint8 req_id = 1;
		guint16 offset = 0;
		bt_gatt_read_req_t read_req = {0, };
		bt_user_info_t *user_info = NULL;
		struct gatt_req_info *req_info = NULL;
		struct gatt_service_info *svc_info = NULL;
		BT_DBG("ReadValue");

		g_variant_get(parameters, "(&syq)", &addr, &req_id, &offset);

		BT_DBG("Application path = %s", object_path);

		BT_DBG("Remote Device address number = %s", addr);
		BT_DBG("Request id = %d, Offset = %d", req_id, offset);

		BT_DBG("Sender = %s", sender);

		read_req.att_handle = g_strdup(object_path);
		read_req.address = g_strdup(addr);
		read_req.req_id = req_id;
		read_req.offset = offset;
		svc_info = __bt_gatt_find_gatt_service_from_desc(object_path);
		if (svc_info != NULL) {
			read_req.service_handle = g_strdup(svc_info->serv_path);
			user_info = _bt_get_user_data(BT_COMMON);

			/* Store requets information */
			req_info = g_new0(struct gatt_req_info, 1);
			req_info->attr_path = g_strdup(object_path);
			req_info->svc_path = g_strdup(read_req.service_handle);
			req_info->request_id= req_id;
			req_info->offset = offset;
			req_info->context = invocation;
			gatt_requests = g_slist_append(gatt_requests, req_info);

			if (user_info != NULL) {
				struct gatt_char_info *char_info = NULL;

				_bt_common_event_cb(
					BLUETOOTH_EVENT_GATT_SERVER_READ_REQUESTED,
					BLUETOOTH_ERROR_NONE, &read_req,
					user_info->cb, user_info->user_data);
			}
		}

		if (read_req.att_handle)
			g_free(read_req.att_handle);
		if (read_req.address)
			g_free(read_req.address);
		if (read_req.service_handle)
			g_free(read_req.service_handle);

		return;
	} else if (g_strcmp0(method_name, "WriteValue") == 0) {
		GVariant *var = NULL;
		gchar *addr = NULL;
		guint8 req_id = 0;
		guint16 offset = 0;
		bt_gatt_value_change_t value_change = {0, };
		bt_user_info_t *user_info = NULL;
		int len = 0;
		struct gatt_service_info *svc_info = NULL;
		struct gatt_req_info *req_info = NULL;

		BT_DBG("WriteValue");
		BT_DBG("Application path = %s", object_path);
		BT_DBG("Sender = %s", sender);

		g_variant_get(parameters, "(&syq@ay)", &addr, &req_id, &offset, &var);

		value_change.att_handle = g_strdup(object_path);
		value_change.address = g_strdup(addr);
		svc_info = __bt_gatt_find_gatt_service_from_desc(object_path);
		if (svc_info == NULL) {
			g_variant_unref(var);
			g_dbus_method_invocation_return_value(invocation, NULL);
			return;
		}

		value_change.service_handle = g_strdup(svc_info->serv_path);
		value_change.offset = offset;
		value_change.req_id = req_id;

		len = g_variant_get_size(var);
		if (len > 0) {
			char *data;

			value_change.att_value = (guint8 *)malloc(len);
			if (!value_change.att_value) {
				BT_ERR("att_value is NULL");
				g_variant_unref(var);
				g_dbus_method_invocation_return_value(invocation, NULL);
				return;
			}
			data = (char *)g_variant_get_data(var);
			memcpy(value_change.att_value, data, len);
		}
		g_variant_unref(var);

		value_change.val_len = len;

		/* Store requets information */
		req_info = g_new0(struct gatt_req_info, 1);
		req_info->attr_path = g_strdup(object_path);
		req_info->svc_path = g_strdup(value_change.service_handle);
		req_info->request_id= req_id;
		req_info->offset = offset;
		req_info->context = invocation;
		gatt_requests = g_slist_append(gatt_requests, req_info);

		user_info = _bt_get_user_data(BT_COMMON);
		if (user_info != NULL) {
			_bt_common_event_cb(
				BLUETOOTH_EVENT_GATT_SERVER_VALUE_CHANGED,
				BLUETOOTH_ERROR_NONE, &value_change,
				user_info->cb, user_info->user_data);
		}
		return;
	}
	g_dbus_method_invocation_return_value(invocation, NULL);
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

static const GDBusInterfaceVTable desc_interface_vtable = {
	__bt_gatt_desc_method_call,
	NULL,
	NULL,
};

static const GDBusInterfaceVTable char_interface_vtable = {
	__bt_gatt_char_method_call,
	NULL,
	NULL,
};

static const GDBusInterfaceVTable serv_interface_vtable = {
	__bt_gatt_serv_method_call,
	NULL,
	NULL,
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

static struct gatt_desc_info *__bt_gatt_find_gatt_desc_info(
			const char *serv_path, const char *char_path,
			const char *desc_path)
{
	GSList *l1, *l2, *l3;

	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		if (g_strcmp0(serv_info->serv_path, serv_path) == 0) {
			for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
				struct gatt_char_info *char_info = l2->data;

				if (g_strcmp0(char_info->char_path, char_path)
							== 0) {
					for (l3 = char_info->desc_data; l3 != NULL; l3 = l3->next) {
						struct gatt_desc_info *desc_info = l3->data;
						if (g_strcmp0(desc_info->desc_path,
							desc_path) == 0) {
							return desc_info;
						}
					}
				}
			}
		}
	}
	BT_ERR("Gatt descriptor not found");
	return NULL;
}

static struct gatt_req_info *__bt_gatt_find_request_info(guint request_id)
{
	GSList *l;

	for (l = gatt_requests; l != NULL; l = l->next) {
		struct gatt_req_info *req_info = l->data;

		if (req_info && req_info->request_id == request_id) {
			return req_info;
		}
	}
	BT_ERR("Gatt Request not found");
	return NULL;
}

static int char_info_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct gatt_char_info *attrib1 = a1;
	const struct gatt_char_info *attrib2 = a2;

	return g_strcmp0(attrib1->char_path, attrib2->char_path);
}

static int desc_info_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct gatt_desc_info *attrib1 = a1;
	const struct gatt_desc_info *attrib2 = a2;

	return g_strcmp0(attrib1->desc_path, attrib2->desc_path);
}

static gboolean __bt_gatt_update_attribute_info(struct gatt_req_info *req_info,
			char *value, int value_length)
{
	GSList *l1, *l2, *l3;
	int found = 0;
	for (l1 = gatt_services; l1 != NULL; l1 = l1->next) {
		struct gatt_service_info *serv_info = l1->data;

		if (serv_info && g_strcmp0(serv_info->serv_path, req_info->svc_path) == 0) {

			for (l2 = serv_info->char_data; l2 != NULL; l2 = l2->next) {
				struct gatt_char_info *char_info = l2->data;

				if (char_info) {
					if (g_strcmp0(char_info->char_path, req_info->attr_path) == 0) {
						memcpy(&char_info->char_value[req_info->offset], value, value_length);
						serv_info->char_data = g_slist_insert_sorted (serv_info->char_data, char_info, char_info_cmp);
						found = 1;
						break;
					} else {
						for (l3 = char_info->desc_data; l3 != NULL; l3 = l3->next) {
							struct gatt_desc_info *desc_info = l3->data;

							if (desc_info && g_strcmp0(desc_info->desc_path, req_info->attr_path)
										== 0) {
								memcpy(&desc_info->desc_value[req_info->offset], value, value_length);
								char_info->desc_data = g_slist_insert_sorted (char_info->desc_data, desc_info, desc_info_cmp);
								found = 1;
								break;
							}
						}
					}
				}
			}
		}
		if (found) {
			return TRUE;;
		}
	}
	return FALSE;
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

int bluetooth_gatt_convert_prop2string(
			bt_gatt_characteristic_property_t properties,
			char *char_properties[])
{
	int flag_count = 0;

	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_BROADCAST) {
		char_properties[flag_count] = g_strdup("broadcast");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ) {
		char_properties[flag_count] = g_strdup("read");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE_NO_RESPONSE) {
		char_properties[flag_count] = g_strdup("write-without-response");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE) {
		char_properties[flag_count] = g_strdup("write");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_NOTIFY) {
		char_properties[flag_count] = g_strdup("notify");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_INDICATE) {
		char_properties[flag_count] = g_strdup("indicate");
		flag_count++;
	}
	if (properties & BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_SIGNED_WRITE) {
		char_properties[flag_count] = g_strdup("authenticated-signed-writes");
		flag_count++;
	}

	if (flag_count == 0) {
		char_properties[flag_count] = g_strdup("read");
		flag_count++;
	}

	return flag_count;
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

void get_service_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *result;
	GVariantIter *iter = NULL;
	const gchar *key = NULL;
	GVariant *value = NULL;
	const gchar *service = NULL;
	const gchar *characteristic = NULL;
	const gchar *descriptor = NULL;
	int n_char = 1;

	BT_DBG(" ");
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
	} else {
		char *char_cmp = NULL;
		g_variant_get (result, "(a{sv})", &iter);
		char_cmp = g_strdup_printf("Characteristic%d", n_char);

		while (g_variant_iter_loop(iter, "{sv}", &key, &value)) {
			if (g_strcmp0(key, "Service") == 0) {
				service = g_variant_get_string(value, NULL);
				BT_DBG("Service %s", service);
			} else if (g_strcmp0(key, char_cmp) == 0) {
				characteristic = g_variant_get_string(value, NULL);
				g_free(char_cmp);
				char_cmp = g_strdup_printf("Characteristic%d", ++n_char);
				BT_DBG("%s", characteristic);
			} else if (g_strcmp0(key, "Descriptor") == 0) {
				descriptor = g_variant_get_string(value, NULL);
				BT_DBG("Descriptor %s", descriptor);
			}
		}
		/* TODO: Store the service informationa and
		 * Send respponse to CAPI layer. */

		g_variant_unref(result);
		if (char_cmp)
			g_free(char_cmp);
	}
}
void register_service_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
	BT_DBG("register_service_cb\n");

	GError *error = NULL;
	GVariant *result;

	register_pending_cnt = 0;

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
					"/org/bluez/hci0", GATT_MNGR_INTERFACE);

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

static GDBusConnection *__bt_gatt_get_gdbus_connection(void)
{
	GDBusConnection *local_system_gconn = NULL;
	GError *err = NULL;

	if (g_conn == NULL) {
		g_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
		if (!g_conn) {
			if (err) {
				BT_ERR("Unable to connect to dbus: %s", err->message);
				g_clear_error(&err);
			}
			g_conn = NULL;
		}
	} else if (g_dbus_connection_is_closed(g_conn)) {
		local_system_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

		if (!local_system_gconn) {
			BT_ERR("Unable to connect to dbus: %s", err->message);
			g_clear_error(&err);
		}

		g_conn = local_system_gconn;
	}

	return g_conn;
}

BT_EXPORT_API int bluetooth_gatt_init(void)
{
	GDBusConnection *conn;

	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				BT_GATT_SERVICE_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				NULL, NULL, NULL, NULL, NULL);

	BT_DBG("owner_id is [%d]", owner_id);

	serv_id = 1;

	conn = __bt_gatt_get_gdbus_connection();
	if (!conn) {
		BT_ERR("Unable to get connection");
		return BLUETOOTH_ERROR_INTERNAL;
	}

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
	GVariantBuilder *builder1 = NULL;
	GVariantBuilder *inner_builder = NULL;
	gboolean svc_primary = TRUE;
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
	serv_info->is_svc_primary = svc_primary;

	gatt_services = g_slist_append(gatt_services, serv_info);

	/* emit interfacesadded signal here for service path */
	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}",
		"UUID", g_variant_new_string(svc_uuid));

	g_variant_builder_add(inner_builder, "{sv}",
		"Primary", g_variant_new_boolean(svc_primary));

	builder1 = g_variant_builder_new(G_VARIANT_TYPE("ao"));

	g_variant_builder_add(inner_builder, "{sv}", "Characteristics",
				g_variant_new("ao", builder1));

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
	g_variant_builder_unref(builder1);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_add_new_characteristic(
			const char *svc_path, const char *char_uuid,
			bt_gatt_characteristic_property_t properties,
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
	GVariantBuilder *builder2 = NULL;
	GVariantBuilder *builder3 = NULL;
	GVariant *flags_val = NULL;
	int i = 0;
	char *char_flags[NUMBER_OF_FLAGS];
	int flag_count = 0;

	if (new_service) {
		char_id = 1;
		new_service = FALSE;
	}

	BT_DBG("gatt svc_path path is [%s]", svc_path);
	serv_info = __bt_gatt_find_gatt_service_info(svc_path);
	if (serv_info == NULL)
		return BLUETOOTH_ERROR_INVALID_PARAM;

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

	flag_count = bluetooth_gatt_convert_prop2string(properties, char_flags);

	char_info = g_new0(struct gatt_char_info, 1);

	char_info->char_path = g_strdup(path);
	char_info->char_id = object_id;
	char_info->char_uuid = g_strdup(char_uuid);

	for (i = 0; i < flag_count; i++) {
		char_info->char_flags[i] = char_flags[i];
		}

	char_info->flags_length = flag_count;

	serv_info->char_data = g_slist_append(serv_info->char_data, char_info);

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new("s", char_uuid));
	g_variant_builder_add(inner_builder, "{sv}", "Service",
				g_variant_new("o", svc_path));

	builder2 = g_variant_builder_new(G_VARIANT_TYPE("as"));

	for (i = 0; i < flag_count; i++) {
		g_variant_builder_add(builder2, "s", char_flags[i]);
	}

	flags_val = g_variant_new("as", builder2);
	g_variant_builder_add(inner_builder, "{sv}", "Flags",
				flags_val);

	builder3 = g_variant_builder_new(G_VARIANT_TYPE("ao"));

	g_variant_builder_add(inner_builder, "{sv}", "Descriptors",
				g_variant_new("ao", builder3));

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
	g_variant_builder_unref(builder2);
	g_variant_builder_unref(builder3);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_set_characteristic_value(
			const char *characteristic, const char *char_value,
			int	value_length)
{
	gchar **line_argv = NULL;
	char *serv_path = NULL;
	struct gatt_char_info *char_info = NULL;
	GVariantBuilder *builder1 = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	GVariant *char_val = NULL;
	GError *error = NULL;
	int i = 0;
	int res = BLUETOOTH_ERROR_NONE;

	line_argv = g_strsplit_set(characteristic, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);

	char_info = __bt_gatt_find_gatt_char_info(serv_path, characteristic);

	if (char_info == NULL) {
		/* Fix : RESOURCE_LEAK */
		res = BLUETOOTH_ERROR_INVALID_PARAM;
		goto done;
	}

	char_info->value_length = value_length;

	char_info->char_value = (char *)malloc(value_length);
	/* Fix : NULL_RETURNS */
	if (char_info->char_value == NULL) {
		res = BLUETOOTH_ERROR_MEMORY_ALLOCATION;
		goto done;
	}

	for (i = 0; i < value_length; i++)
		char_info->char_value[i] = char_value[i];

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < value_length; i++) {
		g_variant_builder_add(builder1, "y", char_value[i]);
	}

	char_val = g_variant_new("ay", builder1);
		g_variant_builder_add(inner_builder, "{sv}", "Value", char_val);

	g_variant_builder_add(builder, "{sa{sv}}",
			GATT_CHAR_INTERFACE,
			inner_builder);

	g_dbus_connection_emit_signal(g_conn, NULL, "/",
			"org.freedesktop.Dbus.ObjectManager",
			"InterfacesAdded",
			g_variant_new("(oa{sa{sv}})",
			char_info->char_path, builder),
			&error);

	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);
	g_variant_builder_unref(builder1);
done:
	g_strfreev(line_argv);
	g_free(serv_path);

	return res;
}

BT_EXPORT_API int bluetooth_gatt_add_descriptor(
			const char *char_path, const char *desc_uuid,
			char **desc_path)
{
	static int desc_id = 1;
	GError *error = NULL;
	guint object_id;
	GDBusNodeInfo *node_info;
	gchar *path = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	struct gatt_char_info *char_info = NULL;
	struct gatt_desc_info *desc_info = NULL;
	gchar **line_argv = NULL;
	char *serv_path;

	if (new_char) {
		desc_id = 1;
		new_char = FALSE;
	}

	line_argv = g_strsplit_set(char_path, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);

	char_info = __bt_gatt_find_gatt_char_info(serv_path, char_path);
	if (char_info == NULL) {
		g_strfreev(line_argv);
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	node_info = __bt_gatt_create_method_node_info(
					descriptor_introspection_xml);

	if (node_info == NULL) {
		g_strfreev(line_argv);
		return BLUETOOTH_ERROR_INTERNAL;
	}

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
		g_strfreev(line_argv);

		return BLUETOOTH_ERROR_INTERNAL;
	}

	desc_info = g_new0(struct gatt_desc_info, 1);

	desc_info->desc_path = g_strdup(path);
	desc_info->desc_id = object_id;
	desc_info->desc_uuid = g_strdup(desc_uuid);

	char_info->desc_data = g_slist_append(char_info->desc_data, desc_info);

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	g_variant_builder_add(inner_builder, "{sv}", "UUID",
				g_variant_new("s", desc_uuid));
	g_variant_builder_add(inner_builder, "{sv}", "Characteristic",
				g_variant_new("o", char_path));

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

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_set_descriptor_value(
			const char *desc_path, const char *desc_value,
			int value_length)
{
	GError *error = NULL;
	GVariantBuilder *builder = NULL;
	GVariantBuilder *inner_builder = NULL;
	GVariantBuilder *builder1 = NULL;
	struct gatt_desc_info *desc_info = NULL;
	gchar **line_argv = NULL;
	char *char_path;
	GVariant *desc_val = NULL;
	char *serv_path = NULL;
	int i ;

	line_argv = g_strsplit_set(desc_path, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);
	char_path = g_strdup_printf("%s/%s", serv_path, line_argv[2]);

	desc_info = __bt_gatt_find_gatt_desc_info(serv_path, char_path, desc_path);

	/* Free the allocated memory */
	g_strfreev(line_argv);
	g_free(serv_path);
	g_free(char_path);

	/* Fix : NULL_RETURNS */
	retv_if(desc_info == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	desc_info->desc_value = (char *)malloc(value_length);

	/* Fix : NULL_RETURNS */
	retv_if(desc_info->desc_value == NULL, BLUETOOTH_ERROR_MEMORY_ALLOCATION);

	for (i = 0; i < value_length; i++)
		desc_info->desc_value[i] = desc_value[i];

	desc_info->value_length = value_length;

	builder = g_variant_builder_new(G_VARIANT_TYPE("a{sa{sv}}"));
	inner_builder = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));

	builder1 = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < value_length; i++) {
		g_variant_builder_add(builder1, "y", desc_value[i]);
	}
	desc_val = g_variant_new("ay", builder1);
	g_variant_builder_add(inner_builder, "{sv}", "Value", desc_val);

	g_variant_builder_add(builder, "{sa{sv}}",
				GATT_DESC_INTERFACE,
				inner_builder);

	g_dbus_connection_emit_signal(g_conn, NULL, "/",
				"org.freedesktop.Dbus.ObjectManager",
				"InterfacesAdded",
				g_variant_new("(oa{sa{sv}})",
				desc_info->desc_path, builder),
				&error);

	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(builder);
	g_variant_builder_unref(builder1);

	return BLUETOOTH_ERROR_NONE;
}

int bluetooth_gatt_get_service(const char *svc_uuid)
{
	GDBusProxy *proxy = NULL;
	gchar *uuid = NULL;

	proxy = __bt_gatt_gdbus_get_manager_proxy("org.bluez",
					"/org/bluez/hci0", GATT_MNGR_INTERFACE);
	if (proxy == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	uuid = g_strdup(svc_uuid);

	g_dbus_proxy_call(proxy,
			"GetService",
			g_variant_new("(s)",
			uuid),
			G_DBUS_CALL_FLAGS_NONE, -1,
			NULL,
			(GAsyncReadyCallback) get_service_cb,
			NULL);

	g_free(uuid);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_gatt_register_service(
			const char *svc_path)
{
	GDBusProxy *proxy = NULL;
	gchar *path = NULL;

	register_pending_cnt++;

	if (__bt_gatt_get_service_state(svc_path)) {
		BT_DBG("service already registered \n");
		return BLUETOOTH_ERROR_NONE;
	}

	proxy = __bt_gatt_gdbus_get_manager_proxy("org.bluez",
					"/org/bluez/hci0", GATT_MNGR_INTERFACE);
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
			if (bluetooth_gatt_unregister_service(info->serv_path)
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

BT_EXPORT_API int bluetooth_gatt_notify_characteristics_value_change(
			const char *char_path, const char* char_value,
			int value_length, bluetooth_device_address_t *unicast_address)
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
	char addr[20] = { 0 };

	line_argv = g_strsplit_set(char_path, "/", 0);
	serv_path = g_strdup_printf("/%s", line_argv[1]);

	if (!__bt_gatt_get_service_state(serv_path)) {
		BT_DBG("service not registered for this characteristic \n");
		g_strfreev(line_argv);
		return BLUETOOTH_ERROR_INTERNAL;
	}

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

	if (unicast_address) {
		_bt_convert_addr_type_to_string(addr,
					(unsigned char *)unicast_address->addr);
	}
	g_variant_builder_add(outer_builder, "{sv}", "Unicast",
				g_variant_new("s", addr));


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
	} else {
		struct gatt_char_info *char_info = NULL;

		char_info = __bt_gatt_find_gatt_char_info(serv_path, char_path);
		if (char_info == NULL) {
			return BLUETOOTH_ERROR_INVALID_DATA;
		}

		char_info->value_length = value_length;

		char_info->char_value = (char *)realloc(char_info->char_value, value_length);
		if (char_info->char_value) {
			for (i = 0; i < value_length; i++) {
				char_info->char_value[i] = char_value[i];
			}
		}
	}

	g_strfreev(line_argv);
	g_variant_builder_unref(inner_builder);
	g_variant_builder_unref(outer_builder);
	g_variant_builder_unref(invalidated_builder);

	return err;
}

BT_EXPORT_API int bluetooth_gatt_unregister_service(const char *svc_path)
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
			gatt_services = g_slist_delete_link(gatt_services, tmp->data);
		}
	}

	new_service = FALSE;

	if (gatt_services->next == NULL)
		serv_id--;

	return err;
}

BT_EXPORT_API int bluetooth_gatt_send_response(int request_id, guint req_type,
					int resp_state, int offset, char *value, int value_length)
{
	struct gatt_req_info *req_info = NULL;

	req_info = __bt_gatt_find_request_info(request_id);

	if (req_info) {
		if (resp_state != BLUETOOTH_ERROR_NONE) {

			GQuark quark = g_quark_from_string("gatt-server");
			GError *err = g_error_new(quark, 0, "Application Error");
			g_dbus_method_invocation_return_gerror(req_info->context, err);
			g_error_free(err);

			gatt_requests = g_slist_remove(gatt_requests, req_info);

			req_info->context = NULL;
			if (req_info->attr_path)
				g_free(req_info->attr_path);
			if (req_info->svc_path)
				g_free(req_info->svc_path);
			g_free(req_info);

			return BLUETOOTH_ERROR_NONE;
		}
		if (req_type == BLUETOOTH_GATT_ATT_REQUEST_TYPE_READ) {
			int i;
			GVariantBuilder *inner_builder = NULL;
			inner_builder = g_variant_builder_new(G_VARIANT_TYPE ("ay"));
			if (value_length > 0 && value != NULL) {
				for (i = 0; i < value_length; i++)
					g_variant_builder_add(inner_builder, "y", value[i]);
			}
			g_dbus_method_invocation_return_value(req_info->context,
						g_variant_new("(ay)", inner_builder));
			g_variant_builder_unref(inner_builder);
		} else {
			g_dbus_method_invocation_return_value(req_info->context, NULL);
		}
		gatt_requests = g_slist_remove(gatt_requests, req_info);

		req_info->context = NULL;
		if (req_info->attr_path)
			g_free(req_info->attr_path);
		if (req_info->svc_path)
			g_free(req_info->svc_path);
		g_free(req_info);
	} else {
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
}
