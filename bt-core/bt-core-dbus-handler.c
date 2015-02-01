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
#include "bt-request-service.h"
#include "bt-core-noti-handler.h"
#include "bt-core-main.h"

#define BT_SERVICE_NAME		"org.projectx.bt"
#define BT_SERVICE_PATH		"/org/projectx/bt_service"

DBusGProxy *service_proxy = NULL;
DBusGConnection *service_conn = NULL;

static GDBusConnection *service_gconn;
static GDBusProxy *service_gproxy;

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

static GDBusProxy *__bt_core_gdbus_init_service_proxy(void)
{
	GDBusProxy *proxy;
	GError *err = NULL;

	g_type_init();

	if (service_gconn == NULL)
		service_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

	if (!service_gconn) {
		if (err) {
			BT_ERR("Unable to connect to dbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	proxy =  g_dbus_proxy_new_sync(service_gconn,
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

		g_object_unref(service_gconn);
		service_gconn = NULL;
		return NULL;
	}

	service_gproxy = proxy;

	return proxy;
}

GDBusProxy *_bt_core_gdbus_get_service_proxy(void)
{
	return (service_gproxy) ? service_gproxy : __bt_core_gdbus_init_service_proxy();
}

void _bt_core_gdbus_deinit_proxys(void)
{
	if (service_gproxy) {
		g_object_unref(service_proxy);
		service_proxy = NULL;
	}

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
	GArray *out_param2 = NULL;

	proxy = _bt_core_gdbus_get_service_proxy();
	if (!proxy)
		return BLUETOOTH_ERROR_INTERNAL;
	in_param5 = g_array_new(TRUE, TRUE, sizeof(gchar));

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
				G_DBUS_CALL_FLAGS_NONE, -1,
				NULL, &error);

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
	param2 = NULL;

	g_variant_get(ret, "(@ay@ay)", &param1, &param2);

	if (param1) {
		*out_param1 = g_array_new(TRUE, TRUE, sizeof(gchar));
		_bt_core_fill_garray_from_variant(param1, *out_param1);
		g_variant_unref(param1);
	}

	if (param2) {
		out_param2 = g_array_new(TRUE, TRUE, sizeof(gchar));
		_bt_core_fill_garray_from_variant(param2, out_param2);
		result = g_array_index(out_param2, int, 0);
		g_variant_unref(param2);
		g_array_free(out_param2, TRUE);
	} else {
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	g_variant_unref(ret);

	return result;
}


static int __bt_core_get_object_path(DBusMessage *msg, char **path)
{
	DBusMessageIter item_iter;

	dbus_message_iter_init(msg, &item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_OBJECT_PATH) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, path);

	if (*path == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

static int __bt_core_get_owner_info(DBusMessage *msg, char **name,
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

	if (*name == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, previous);

	if (*previous == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	dbus_message_iter_next(&item_iter);

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_STRING) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, current);

	if (*current == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	return BLUETOOTH_ERROR_NONE;
}

static DBusHandlerResult __bt_core_event_filter(DBusConnection *conn,
					   DBusMessage *msg, void *data)
{
	char *object_path = NULL;
	const char *member = dbus_message_get_member(msg);

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strcasecmp(member, "InterfacesAdded") == 0) {
		if (__bt_core_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (strcasecmp(object_path, "/org/bluez/hci0") == 0) {
			_bt_core_adapter_added_cb();
		}
	} else if (strcasecmp(member, "InterfacesRemoved") == 0) {
		if (__bt_core_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (strcasecmp(object_path, "/org/bluez/hci0") == 0) {
			_bt_core_adapter_removed_cb();
		}
	} else if (strcasecmp(member, "NameOwnerChanged") == 0) {
		char *name = NULL;
		char *previous = NULL;
		char *current = NULL;
		gboolean flight_mode_status;

		if (__bt_core_get_owner_info(msg, &name, &previous, &current)) {
			BT_ERR("Fail to get the owner info");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (*current != '\0')
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &flight_mode_status) != 0)
			BT_ERR("Fail to get the flight_mode status value");
		if (flight_mode_status == FALSE && _bt_is_flightmode_request() == TRUE) {
			BT_DBG("flightmode requested");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (strcasecmp(name, "org.bluez") == 0) {
			BT_DBG("Bluetoothd is terminated");
			if (_bt_check_terminating_condition() == TRUE) {
				_bt_disable_adapter();
				_bt_disable_adapter_le();
				_bt_core_terminate();
			}
		} else if (strcasecmp(name, "org.projectx.bt") == 0) {
			BT_DBG("bt-service is terminated");
			if (_bt_check_terminating_condition() == TRUE) {
				_bt_disable_adapter();
				_bt_disable_adapter_le();
				_bt_core_terminate();
			}
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusGProxy *_bt_core_register_event_filter(DBusGConnection *g_conn,
						BtCore *bt_core)
{
	DBusError dbus_error;
	DBusConnection *conn;
	DBusGProxy *proxy;
	GError *err = NULL;
	guint result = 0;

	if (g_conn == NULL)
		return NULL;

	conn = dbus_g_connection_get_connection(g_conn);
	if (conn == NULL)
		return NULL;

	proxy = dbus_g_proxy_new_for_name(g_conn, DBUS_SERVICE_DBUS,
				DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (proxy == NULL) {
		BT_ERR("proxy is NULL");
		return NULL;
	}

	if (!dbus_g_proxy_call(proxy, "RequestName", &err, G_TYPE_STRING,
			BT_CORE_NAME, G_TYPE_UINT, 0, G_TYPE_INVALID,
			G_TYPE_UINT, &result, G_TYPE_INVALID)) {
		if (err != NULL) {
			BT_ERR("RequestName RPC failed[%s]\n", err->message);
			g_error_free(err);
		}
		g_object_unref(proxy);
		return NULL;
	}

	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		BT_ERR("Failed to get the primary well-known name.\n");
		g_object_unref(proxy);
		return NULL;
	}

	if (!dbus_connection_add_filter(conn, __bt_core_event_filter,
					NULL, NULL)) {
		BT_ERR("Fail to add filter");
		g_object_unref(proxy);
		return NULL;
	}

	dbus_error_init(&dbus_error);

	dbus_bus_add_match(conn,
			"type='signal',interface='org.freedesktop.DBus'"
			",member='NameOwnerChanged'",
			&dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		g_object_unref(proxy);
		return NULL;
	}

	dbus_bus_add_match(conn,
			"type='signal',interface='org.freedesktop.DBus.ObjectManager'"
			",member='InterfacesAdded'",
			&dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		g_object_unref(proxy);
		return NULL;
	}

	dbus_bus_add_match(conn,
			"type='signal',interface='org.freedesktop.DBus.ObjectManager'"
			",member='InterfacesRemoved'",
			&dbus_error);

	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		g_object_unref(proxy);
		return NULL;
	}

	dbus_g_connection_register_g_object(g_conn, BT_CORE_PATH,
					G_OBJECT(bt_core));

	return proxy;
}

void _bt_unregister_event_filter(DBusGConnection *g_conn,
					BtCore *bt_core,
					DBusGProxy *dbus_proxy)
{
	DBusConnection *conn;

	if (g_conn == NULL ||
	     bt_core == NULL ||
	      dbus_proxy == NULL) {
		BT_ERR("Invalid parameter");
		return;
	}

	conn = dbus_g_connection_get_connection(g_conn);

	dbus_connection_remove_filter(conn, __bt_core_event_filter, NULL);

	dbus_g_connection_unregister_g_object(g_conn, G_OBJECT(bt_core));

	g_object_unref(bt_core);
	g_object_unref(dbus_proxy);
}

