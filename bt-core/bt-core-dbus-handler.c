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
#include <vconf.h>
#include <vconf-keys.h>

#include "bt-core-adapter.h"
#include "bt-core-common.h"
#include "bt-core-dbus-handler.h"
#include "bt-internal-types.h"
#include "bt-core-noti-handler.h"
#include "bt-core-main.h"

#define BT_SERVICE_NAME		"org.projectx.bt"
#define BT_SERVICE_PATH		"/org/projectx/bt_service"

#ifdef HPS_FEATURE
#define BT_HPS_SERVICE_NAME "org.projectx.httpproxy"
#define BT_HPS_OBJECT_PATH "/org/projectx/httpproxy"
#define BT_HPS_INTERFACE_NAME "org.projectx.httpproxy_service"
#endif

static GDBusConnection *service_gconn;
static GDBusProxy *service_gproxy;
#ifdef HPS_FEATURE
static GDBusProxy *hps_gproxy;
#endif

void _bt_core_fill_garray_from_variant(GVariant *var, GArray *param)
{
	char *data;
	int size;

	size = g_variant_get_size(var);
	if (size > 0) {
		data = (char *)g_variant_get_data(var);
		if (data)
			param = g_array_append_vals(param, data, size);

	}
}

GDBusConnection * _bt_core_get_gdbus_connection(void)
{
	GError *err = NULL;

	if (service_gconn == NULL)
		service_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

	if (!service_gconn) {
		if (err) {
			BT_ERR("Unable to connect to dbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	return service_gconn;
}

static GDBusProxy *__bt_core_gdbus_init_service_proxy(void)
{
	GDBusProxy *proxy;
	GError *err = NULL;
	GDBusConnection *conn;

	g_type_init();

	conn = _bt_core_get_gdbus_connection();
	if (!conn)
		return NULL;

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_SERVICE_NAME,
			BT_SERVICE_PATH,
			BT_SERVICE_NAME,
			NULL, &err);
	if (!proxy) {
		if (err) {
			 BT_ERR("Unable to create proxy: %s", err->message);
			 g_clear_error(&err);
		}

		return NULL;
	}

	service_gproxy = proxy;

	return proxy;
}

GDBusProxy *_bt_core_gdbus_get_service_proxy(void)
{
	return (service_gproxy) ? service_gproxy : __bt_core_gdbus_init_service_proxy();
}

#ifdef HPS_FEATURE
int _bt_core_start_httpproxy(void)
{
	GVariant *variant = NULL;
	unsigned char enabled;

	BT_DBG(" ");

	hps_gproxy = _bt_core_gdbus_get_hps_proxy();
	if (!hps_gproxy) {
		BT_DBG("Couldn't get service proxy");
		return -1;
	}

	variant = g_dbus_proxy_call_sync(hps_gproxy, "enable",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	if (variant) {
		g_variant_get(variant, "(y)", &enabled);
		BT_ERR("HPS enabled status 0x%x", enabled);
	}
	return 0;
}

int _bt_core_stop_httpproxy(void)
{
	GVariant *variant = NULL;
	unsigned char enabled;

	BT_DBG(" ");

	hps_gproxy = _bt_core_gdbus_get_hps_proxy();
	if (!hps_gproxy) {
		BT_DBG("Couldn't get service proxy");
		return -1;
	}

	variant = g_dbus_proxy_call_sync(hps_gproxy, "disable",
				NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	if (variant) {
		g_variant_get(variant, "(y)", &enabled);
		BT_ERR("HPS disabled status 0x%x", enabled);
	}
	return 0;
}

static GDBusProxy *_bt_core_gdbus_init_hps_proxy(void)
{
	GDBusProxy *proxy;
	GError *err = NULL;
	GDBusConnection *conn;

	g_type_init();

	BT_DBG(" ");

	conn = _bt_core_get_gdbus_connection();
	if (!conn)
		return NULL;

	proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_HPS_SERVICE_NAME,
			BT_HPS_OBJECT_PATH,
			BT_HPS_INTERFACE_NAME,
			NULL, &err);
	if (proxy == NULL) {
		if (err) {
			 BT_ERR("Unable to create proxy: %s", err->message);
			 g_clear_error(&err);
		}
		return NULL;
	}

	hps_gproxy = proxy;

	return proxy;
}

GDBusProxy *_bt_core_gdbus_get_hps_proxy(void)
{
	return (hps_gproxy) ? hps_gproxy : _bt_core_gdbus_init_hps_proxy();
}
#endif

void _bt_core_gdbus_deinit_proxys(void)
{
	BT_DBG("");

	if (service_gproxy) {
		g_object_unref(service_gproxy);
		service_gproxy = NULL;
	}

#ifdef HPS_FEATURE
	if (hps_gproxy) {
		g_object_unref(hps_gproxy);
		hps_gproxy = NULL;
	}
#endif

	if (service_gconn) {
		g_object_unref(service_gconn);
		service_gconn = NULL;
	}
}

int _bt_core_service_request(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			GArray **out_param1)
{
	GDBusProxy  *proxy;
	GVariant *ret;
	GVariant *param1;
	GVariant *param2;
	GVariant *param3;
	GVariant *param4;
	GVariant *param5;

	int result = BLUETOOTH_ERROR_NONE;
	GError *error = NULL;
	GArray *in_param5 = NULL;

	int retry = 5;

	proxy = _bt_core_gdbus_get_service_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;
	in_param5 = g_array_new(TRUE, TRUE, sizeof(gchar));

	while (--retry >= 0) {
		param1 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param1->data, in_param1->len,
					TRUE, NULL, NULL);
		param2 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param2->data, in_param2->len,
					TRUE, NULL, NULL);
		param3 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param3->data, in_param3->len,
					TRUE, NULL, NULL);
		param4 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param4->data, in_param4->len,
					TRUE, NULL, NULL);
		param5 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param5->data, in_param5->len,
					TRUE, NULL, NULL);

		ret = g_dbus_proxy_call_sync(proxy, "service_request",
					g_variant_new("(iii@ay@ay@ay@ay@ay)",
						service_type, service_function,
						BT_SYNC_REQ, param1,
						param2, param3,
						param4, param5),
					G_DBUS_CALL_FLAGS_NONE, 2000,
					NULL, &error);
		if (ret == NULL && error != NULL) {
			if (error->code == G_IO_ERROR_TIMED_OUT) {
				BT_ERR("D-Bus Timed out.");
				g_clear_error(&error);
				continue;
			}
		}

		break;
	}

	g_array_free(in_param5, TRUE);

	if (ret == NULL) {
		/* dBUS-RPC is failed */
		BT_ERR("dBUS-RPC is failed");

		if (error != NULL) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
			       error->code, error->message);

			g_clear_error(&error);
		} else {
			/* dBUS does not give error cause dBUS-RPC is failed */
			BT_ERR("error returned was NULL");
		}

		return BLUETOOTH_ERROR_INTERNAL;
	}

	param1 = NULL;

	g_variant_get(ret, "(iv)", &result, &param1);

	if (param1) {
		*out_param1 = g_array_new(TRUE, TRUE, sizeof(gchar));
		_bt_core_fill_garray_from_variant(param1, *out_param1);
		g_variant_unref(param1);
	}

	g_variant_unref(ret);

	return result;
}

static const gchar bt_core_introspection_xml[] =
"<node name='/'>"
"  <interface name='org.projectx.btcore'>"
"     <method name='EnableAdapter'>"
"     </method>"
"     <method name='DisableAdapter'>"
"     </method>"
"     <method name='RecoverAdapter'>"
"     </method>"
"     <method name='ResetAdapter'>"
"     </method>"
"     <method name='EnableAdapterLe'>"
"     </method>"
"     <method name='DisableAdapterLe'>"
"     </method>"
"     <method name='EnableCore'>"
"     </method>"
"     <method name='FactoryTestMode'>"
"          <arg type='s' name='type' direction='in'/>"
"          <arg type='s' name='arg' direction='in'/>"
"	   <arg type='i' name='ret' direction='out'/>"
"     </method>"
" </interface>"
"</node>";

static guint obj_id, sig_id1, sig_id2, sig_id3;

static void __bt_core_dbus_method(GDBusConnection *connection,
			const gchar *sender,
			const gchar *object_path,
			const gchar *interface_name,
			const gchar *method_name,
			GVariant *parameters,
			GDBusMethodInvocation *invocation,
			gpointer user_data)
{
	gboolean ret;

	BT_DBG("method %s", method_name);

	if (g_strcmp0(method_name, "EnableAdapter") == 0) {
		ret = _bt_core_enable_adapter();
	} else if (g_strcmp0(method_name, "DisableAdapter") == 0) {
		ret = _bt_core_disable_adapter();
	} else if (g_strcmp0(method_name, "RecoverAdapter") == 0) {
		ret = _bt_core_recover_adapter();
	} else if (g_strcmp0(method_name, "ResetAdapter") == 0) {
		ret = __bt_core_reset_adapter();
	} else if (g_strcmp0(method_name, "EnableAdapterLe") == 0) {
		ret = _bt_core_enable_adapter_le();
	} else if (g_strcmp0(method_name, "DisableAdapterLe") == 0) {
		ret = _bt_core_disable_adapter_le();
	} else if (g_strcmp0(method_name, "EnableCore") == 0) {
		ret = _bt_core_enable_core();
	} else if (g_strcmp0(method_name, "FactoryTestMode") == 0) {
		const char *type = NULL;
		const char *arg = NULL;

		g_variant_get(parameters, "(&s&s)", &type, &arg);
		ret = _bt_core_factory_test_mode(type, arg);
		g_dbus_method_invocation_return_value(invocation,
				g_variant_new("(i)", ret));
		 return;
	} else {
		ret = FALSE;
	}

	if (!ret) {
		GQuark quark = g_quark_from_string("bt-core");
		GError *err = g_error_new(quark, 0, "Failed");
		g_dbus_method_invocation_return_gerror(invocation, err);
		g_error_free(err);
	} else {
		g_dbus_method_invocation_return_value(invocation, NULL);
	}

	BT_DBG("-");
}

static const GDBusInterfaceVTable method_table = {
	__bt_core_dbus_method,
	NULL,
	NULL,
};

static GDBusNodeInfo *__bt_core_create_node_info(
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

gboolean __is_interface_and_signal_valid(const gchar *interface_name,
						const gchar *signal_name)
{
	if (g_strcmp0(interface_name, "org.freedesktop.DBus") &&
		g_strcmp0(interface_name, "org.freedesktop.DBus.ObjectManager"))
		return FALSE;

	if (g_strcmp0(signal_name, "NameOwnerChanged") &&
		g_strcmp0(signal_name, "InterfacesAdded") &&
		g_strcmp0(signal_name, "InterfacesRemoved"))
		return FALSE;

	return TRUE;
}

static void __handle_name_owner_changed(const char *name)
{
	gboolean flight_mode_status;

	BT_DBG("");

	flight_mode_status = _bt_core_is_flight_mode_enabled();

	if (flight_mode_status == FALSE && _bt_is_flightmode_request() == TRUE) {
		BT_DBG("flightmode requested");
		return;
	}

	if ((g_strcmp0(name, "org.bluez") == 0) ||
		(g_strcmp0(name, "org.projectx.bt") == 0)) {
		BT_DBG("%s is terminated", name);
		if (_bt_check_terminating_condition() == TRUE) {
			_bt_disable_adapter();
			_bt_disable_adapter_le();
			_bt_core_terminate();
		}
	}
}

static void __bt_core_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data)
{
	if (!__is_interface_and_signal_valid(interface_name, signal_name))
		return;

	if (!g_strcmp0(signal_name, "InterfacesAdded")) {
		char *obj_path = NULL;
		GVariant *optional_param;

		g_variant_get(parameters, "(&o@a{sa{sv}})",
						&obj_path, &optional_param);

		if (g_strcmp0(obj_path, "/org/bluez/hci0") == 0) {
			_bt_core_adapter_added_cb();
		}
	} else if (!g_strcmp0(signal_name, "InterfacesRemoved")) {
		char *obj_path = NULL;
		GVariant *optional_param;

		g_variant_get(parameters, "(&o@as)", &obj_path,
							&optional_param);

		if (g_strcmp0(obj_path, "/org/bluez/hci0") == 0) {
			_bt_core_adapter_removed_cb();
		}
	} else { /* NameOwnerChanged */
		const char *name = NULL;
		const char *old_owner = NULL;
		const char *new_owner = NULL;

		g_variant_get(parameters, "(&s&s&s)", &name, &old_owner,
								&new_owner);

		if (new_owner != NULL && *new_owner == '\0')
			__handle_name_owner_changed(name);
	}
}

gboolean _bt_core_register_dbus(void)
{
	GError *error = NULL;
	guint owner_id;
	GDBusNodeInfo *node_info;
	gchar *path;
	GDBusConnection *conn;

	conn = _bt_core_get_gdbus_connection();
	if (!conn)
		return FALSE;

	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				BT_CORE_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				NULL, NULL, NULL,
				NULL, NULL);

	BT_DBG("owner_id is [%d]", owner_id);

	node_info = __bt_core_create_node_info(bt_core_introspection_xml);
	if (node_info == NULL)
		return FALSE;

	path = g_strdup(BT_CORE_PATH);
	BT_DBG("path is [%s]", path);

	obj_id = g_dbus_connection_register_object(conn, path,
					node_info->interfaces[0],
					&method_table,
					NULL, NULL, &error);
	if (obj_id == 0) {
		BT_ERR("Failed to register: %s", error->message);
		g_error_free(error);
		g_free(path);
		return FALSE;
	}

	g_free(path);

	sig_id1 = g_dbus_connection_signal_subscribe(conn,
				NULL, "org.freedesktop.DBus",
				"NameOwnerChanged", NULL, NULL, 0,
				__bt_core_event_filter, NULL, NULL);
	sig_id2 = g_dbus_connection_signal_subscribe(conn,
				NULL, "org.freedesktop.DBus.ObjectManager",
				"InterfacesAdded", NULL, NULL,
				0, __bt_core_event_filter, NULL, NULL);
	sig_id2 = g_dbus_connection_signal_subscribe(conn,
				NULL, "org.freedesktop.DBus.ObjectManager",
				"InterfacesRemoved", NULL,
				NULL, 0, __bt_core_event_filter, NULL, NULL);

	return TRUE;
}

void  _bt_core_unregister_dbus(void)
{
	GDBusConnection *conn;

	BT_DBG("");

	conn = _bt_core_get_gdbus_connection();
	if (!conn)
		return;

	if (obj_id > 0) {
		g_dbus_connection_unregister_object(conn, obj_id);
		obj_id = 0;
	}

	if (sig_id1 > 0) {
		g_dbus_connection_signal_unsubscribe(conn, sig_id1);
		sig_id1 = 0;
	}
	if (sig_id2 > 0) {
		g_dbus_connection_signal_unsubscribe(conn, sig_id2);
		sig_id2 = 0;
	}
	if (sig_id3 > 0) {
		g_dbus_connection_signal_unsubscribe(conn, sig_id3);
		sig_id3 = 0;
	}
}
