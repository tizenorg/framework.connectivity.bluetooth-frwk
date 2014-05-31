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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vconf.h>
#include <vconf-keys.h>

#include "bt_core.h"
#include "bt-internal-types.h"

static GMainLoop *main_loop = NULL;

typedef enum {
	BT_DEACTIVATED,
	BT_ACTIVATED,
	BT_ACTIVATING,
	BT_DEACTIVATING,
} bt_status_t;

static bt_status_t adapter_status = BT_DEACTIVATED;

static void __bt_core_terminate(void)
{
	if (main_loop) {
		g_main_loop_quit(main_loop);
	} else {
		BT_DBG("Terminating bt-core daemon");
		exit(0);
	}
}

static void __bt_core_set_status(bt_status_t status)
{
	adapter_status = status;
}

static bt_status_t __bt_core_get_status(void)
{
	return adapter_status;
}

static gboolean bt_core_enable_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_disable_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_reset_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static int __execute_command(const char *cmd, char *const arg_list[]);


#include "bt_core_glue.h"

GType bt_core_get_type (void);


G_DEFINE_TYPE(BtCore, bt_core, G_TYPE_OBJECT);

/*This is part of platform provided code skeleton for client server model*/
static void bt_core_class_init (BtCoreClass *bt_core_class)
{
	dbus_g_object_type_install_info(G_TYPE_FROM_CLASS(bt_core_class),
					&dbus_glib_bt_core_object_info);
}

/*This is part of platform provided code skeleton for client server model*/
static void bt_core_init (BtCore *core)
{
}

typedef enum {
	BT_CORE_ERROR_REJECT,
	BT_CORE_ERROR_CANCEL,
	BT_CORE_ERROR_TIMEOUT,
} BtCoreError;

#define BT_CORE_ERROR (bt_core_error_quark())

static GQuark bt_core_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("BtCore");

	return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GError *bt_core_error(BtCoreError error, const char *err_msg)
{
	return g_error_new(BT_CORE_ERROR, error, err_msg, NULL);
}

static int __execute_command(const char *cmd, char *const arg_list[])
{
	int pid;
	int pid2;
	int status = 0;
	BT_DBG("+");

	pid = fork();
	switch (pid) {
	case -1:
		BT_ERR("fork failed");
		return -1;

	case 0:
		pid2 = fork();
		if (pid2 == -1) {
			BT_ERR("fork failed");
		} else if (pid2 == 0) {
			execv(cmd, arg_list);
			exit(256);
		}
		exit(0);
		break;

	default:
		BT_DBG("parent : forked[%d]", pid);
		waitpid(pid, &status, 0);
		BT_DBG("child is terminated : %d", status);
		break;
	}
	BT_DBG("-");
	return 0;
}


static int __bt_enable_adapter(void)
{
	int ret;
	bt_status_t status;

	BT_INFO("");

	status = __bt_core_get_status();
	if (status != BT_DEACTIVATED) {
		BT_DBG("Invalid state %d", status);
		return -1;
	}

	__bt_core_set_status(BT_ACTIVATING);

	ret = __execute_command("/usr/etc/bluetooth/bt-stack-up.sh", NULL);
	if (ret < 0) {
		BT_DBG("running script failed");
		ret = __execute_command("/usr/etc/bluetooth/bt-dev-end.sh", NULL);
		__bt_core_set_status(BT_DEACTIVATED);
		return -1;
	}

	return 0;
}

static int __bt_disable_adapter(void)
{
	bt_status_t status;

	BT_INFO("");

	status = __bt_core_get_status();
	if (status == BT_ACTIVATING) {
		/* Forcely terminate */
		if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
			BT_DBG("running script failed");
		}
		__bt_core_terminate();
		return 0;
	} else if (status != BT_ACTIVATED) {
		BT_DBG("Invalid state %d", status);
		return -1;
	}

	__bt_core_set_status(BT_DEACTIVATING);

	if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
			BT_DBG("running script failed");
			__bt_core_set_status( BT_ACTIVATED);
			return -1;
	}

	return 0;
}

static gboolean bt_core_enable_adapter(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);
	int ret;

	if (sender == NULL)
		return FALSE;

	ret = __bt_enable_adapter();
	if (ret < 0) {
		GError *error = bt_core_error(BT_CORE_ERROR_REJECT,
							"Activation failed");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	} else {
		dbus_g_method_return(context);
	}

	g_free(sender);
	return TRUE;
}

static gboolean bt_core_disable_adapter(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);
	int ret;

	if (sender == NULL)
		return FALSE;

	ret = __bt_disable_adapter();
	if (ret < 0) {
		GError *error = bt_core_error(BT_CORE_ERROR_REJECT,
							"Deactivation failed");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	} else {
		dbus_g_method_return(context);
	}

	g_free(sender);
	return TRUE;
}

static int __bt_reset_adapter(void)
{
	/* Forcely terminate */
	if (__execute_command("/usr/etc/bluetooth/bt-reset-env.sh", NULL) < 0) {
		BT_DBG("running script failed");
	}
	__bt_core_terminate();
	return 0;
}

static gboolean bt_core_reset_adapter(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);
	int ret;

	if (sender == NULL)
		return FALSE;

	ret = __bt_reset_adapter();
	if (ret < 0) {
		GError *error = bt_core_error(BT_CORE_ERROR_REJECT,
							"Deactivation failed");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	} else {
		dbus_g_method_return(context);
	}

	g_free(sender);
	return TRUE;
}

static void __name_owner_changed(DBusGProxy *object, const char *name,
					const char *prev, const char *new,
							gpointer user_data)
{
	if (g_strcmp0(name, "org.bluez") == 0 && *new == '\0') {
		BT_DBG("BlueZ is terminated");
		__bt_disable_adapter();
		__bt_core_terminate();
	} else if (g_strcmp0(name, "org.projectx.bt") == 0 && *new == '\0') {
		BT_DBG("bt-service is terminated abnormally");
		__bt_disable_adapter();
	}
}

static DBusGProxy * __bt_core_register(DBusGConnection *conn, BtCore *bt_core)
{
	DBusGProxy *proxy;
	GError *err = NULL;
	guint result = 0;

	proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE_DBUS,
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

	dbus_g_proxy_add_signal(proxy, "NameOwnerChanged", G_TYPE_STRING,
						G_TYPE_STRING, G_TYPE_STRING,
						G_TYPE_INVALID);

	dbus_g_proxy_connect_signal(proxy, "NameOwnerChanged",
					G_CALLBACK(__name_owner_changed),
					NULL, NULL);

	dbus_g_connection_register_g_object(conn, BT_CORE_PATH,
					G_OBJECT(bt_core));

	return proxy;
}

static void __bt_core_unregister(DBusGConnection *conn, BtCore *bt_core,
					DBusGProxy *dbus_proxy)
{
	if (!bt_core || !dbus_proxy)
		return;

	dbus_g_proxy_disconnect_signal(dbus_proxy, "NameOwnerChanged",
						G_CALLBACK(__name_owner_changed),
						NULL);

	dbus_g_connection_unregister_g_object(conn, G_OBJECT(bt_core));

	g_object_unref(bt_core);
	g_object_unref(dbus_proxy);

}

static void __adapter_added_cb(DBusGProxy *manager_proxy,
						const char *adapter_path,
						gpointer user_data)
{
	BT_DBG("");

	__bt_core_set_status(BT_ACTIVATED);
}

static  void __adapter_removed_cb(DBusGProxy *manager_proxy,
						const char *adapter_path,
						gpointer user_data)
{
	BT_DBG("");

	__bt_core_set_status(BT_DEACTIVATED);

	__bt_core_terminate();
}

static DBusGProxy *__bt_core_manager_init(DBusGConnection *conn)
{
	DBusGProxy *manager_proxy;

	manager_proxy = dbus_g_proxy_new_for_name(conn, "org.bluez", "/",
							"org.bluez.Manager");
	if (manager_proxy == NULL) {
		BT_ERR("ERROR: Can't make manager proxy");
		return NULL;
	}

	dbus_g_proxy_add_signal(manager_proxy, "AdapterAdded",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(manager_proxy, "AdapterAdded",
					G_CALLBACK(__adapter_added_cb),
					NULL, NULL);

	dbus_g_proxy_add_signal(manager_proxy, "AdapterRemoved",
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(manager_proxy, "AdapterRemoved",
					G_CALLBACK(__adapter_removed_cb),
					NULL, NULL);

	return manager_proxy;


}

static void __bt_core_manager_exit(DBusGProxy *manager_proxy)
{
	if (!manager_proxy)
		return;

	dbus_g_proxy_disconnect_signal(manager_proxy, "AdapterAdded",
						G_CALLBACK(__adapter_added_cb),
						NULL);

	dbus_g_proxy_disconnect_signal(manager_proxy, "AdapterRemoved",
					G_CALLBACK(__adapter_removed_cb),
					NULL);

	g_object_unref(manager_proxy);
}

static void __bt_core_sigterm_handler(int signo)
{
	BT_DBG("Got the signal: %d", signo);

	__bt_core_terminate();
}

int main(void)
{
	DBusGConnection *conn = NULL;
	GError *error = NULL;
	BtCore *bt_core;
	DBusGProxy *manager_proxy = NULL;
	DBusGProxy *dbus_proxy = NULL;
	struct sigaction sa;

	BT_INFO("Starting bt-core daemeon");

	g_type_init();

	conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		BT_ERR("ERROR: Can't get on system bus [%s]", error->message);
		g_error_free(error);
		return FALSE;
	}

	bt_core = g_object_new(BT_CORE_TYPE, NULL);

	dbus_proxy = __bt_core_register(conn, bt_core);
	if (!dbus_proxy) {
		BT_ERR("__bt_core_register failed");
		g_object_unref(bt_core);
		bt_core = NULL;
		goto fail;
	}

	manager_proxy = __bt_core_manager_init(conn);
	if (!manager_proxy) {
		BT_ERR("__bt_core_manager_init failed");
		goto fail;
	}

	main_loop = g_main_loop_new(NULL, FALSE);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = __bt_core_sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_main_loop_run(main_loop);

fail:
	__bt_core_unregister(conn, bt_core, dbus_proxy);

	 __bt_core_manager_exit(manager_proxy);

	if (main_loop)
		g_main_loop_unref(main_loop);

	dbus_g_connection_unref(conn);

	BT_INFO("Terminating bt-core daemon");
	return FALSE;
}
