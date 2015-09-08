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

#include <vconf.h>
#include <vconf-keys.h>

#include "bt-core-main.h"
#include "bt-core-adapter.h"
#include "bt-core-common.h"
#include "bt-core-dbus-handler.h"
#include "bt-core-noti-handler.h"

#define BT_CORE_IDLE_TERM_TIME 200 /* 200ms */
#define BT_CORE_CHECK_ADAPTER_OBJECT_PATH_MAX 50


static bt_status_t adapter_status = BT_DEACTIVATED;
static bt_le_status_t adapter_le_status = BT_LE_DEACTIVATED;
static gboolean is_recovery_mode = FALSE;

static int bt_status_before[BT_MODE_MAX] = { VCONFKEY_BT_STATUS_OFF, };
static int bt_le_status_before[BT_MODE_MAX] = { VCONFKEY_BT_LE_STATUS_OFF, };

static void __bt_core_set_status(bt_status_t status)
{
	adapter_status = status;
}

bt_status_t _bt_core_get_status(void)
{
	return adapter_status;
}

static void __bt_core_set_le_status(bt_le_status_t status)
{
	adapter_le_status = status;
}

bt_le_status_t _bt_core_get_le_status(void)
{
	return adapter_le_status;
}

int _bt_core_get_bt_status(bt_mode_e mode)
{
	return bt_status_before[mode];
}

int _bt_core_get_bt_le_status(bt_mode_e mode)
{
	return bt_le_status_before[mode];
}

void _bt_core_set_bt_status(bt_mode_e mode, int status)
{
	bt_status_before[mode] = status;
}

void _bt_core_set_bt_le_status(bt_mode_e mode, int status)
{
	bt_le_status_before[mode] = status;
}

gboolean _bt_core_is_recovery_mode(void)
{
	return is_recovery_mode;
}

static gboolean __bt_core_idle_terminate(gpointer data)
{
	BT_DBG("+");
	_bt_core_terminate();
	return FALSE;
}

gboolean _bt_core_is_flight_mode_enabled(void)
{
#ifdef TIZEN_TELEPHONY_ENABLED
	int isFlightMode = 0;
	int ret = -1;

	ret = vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &isFlightMode);
	if (ret != 0) {
		BT_ERR("vconf_get_bool failed");
	}
	return isFlightMode;
#else
	return FALSE;
#endif
}

static gboolean bt_core_enable_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_disable_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_recover_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_enable_adapter_le(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_disable_adapter_le(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_reset_adapter(BtCore *agent,
						DBusGMethodInvocation *context);

static gboolean bt_core_enable_core(BtCore *agent,
						DBusGMethodInvocation *context);

static int __execute_command(const char *cmd, char *const arg_list[]);

#include "bt-core-adapter-method.h"


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

int _bt_enable_adapter(void)
{
	int ret;
	bt_status_t status;
	bt_le_status_t le_status;

	BT_INFO("");

	status = _bt_core_get_status();
	if (status != BT_DEACTIVATED) {
		BT_ERR("Invalid state %d", status);
		return -1;
	}

	le_status = _bt_core_get_le_status();
	if (le_status == BT_LE_ACTIVATED) {
		/* Turn on PSCAN, (ISCAN if needed) */
		/* Return with 0 for the Enabled response. */
		__bt_core_set_status(BT_ACTIVATED);
		BT_INFO("BR/EDR is enabled.");
		g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
		return 0;
	}

	__bt_core_set_status(BT_ACTIVATING);

	ret = __execute_command("/usr/etc/bluetooth/bt-stack-up.sh", NULL);
	if (ret < 0) {
		BT_ERR("running script failed");
		ret = __execute_command("/usr/etc/bluetooth/bt-dev-end.sh", NULL);
		__bt_core_set_status(BT_DEACTIVATED);
		return -1;
	}

	return 0;
}

int _bt_disable_adapter(void)
{
	bt_status_t status;
	bt_le_status_t le_status;

	BT_INFO_C("Disable adapter");

	le_status = _bt_core_get_le_status();
	BT_DBG("le_status : %d", le_status);
	if (le_status == BT_LE_ACTIVATED) {
		/* Turn off PSCAN, (ISCAN if needed) */
		/* Return with 0 for the Disabled response. */
		__bt_core_set_status(BT_DEACTIVATED);
		BT_INFO("BR/EDR is disabled. now LE only mode");
		g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
		return 0;
	}

	status = _bt_core_get_status();
	if (status == BT_ACTIVATING) {
		/* Forcely terminate */
		if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
			BT_ERR("running script failed");
		}
		_bt_core_terminate();
		return 0;
	} else if (status != BT_ACTIVATED) {
		BT_ERR("Invalid state %d", status);
	}

	__bt_core_set_status(BT_DEACTIVATING);

	if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
		BT_ERR("running script failed");
		__bt_core_set_status( BT_ACTIVATED);
		return -1;
	}

	return 0;
}

int _bt_enable_adapter_le(void)
{
	BT_DBG("");

	int ret;
	bt_status_t status;
	bt_le_status_t le_status;
	le_status = _bt_core_get_le_status();
	retv_if(le_status != BT_LE_DEACTIVATED, -1);

	status = _bt_core_get_status();
	if (status == BT_DEACTIVATED) {
		__bt_core_set_le_status(BT_LE_ACTIVATING);
		BT_DBG("Activate BT");
		ret = __execute_command("/usr/etc/bluetooth/bt-stack-up.sh", NULL);
		if (ret < 0) {
			BT_ERR("running script failed");
			ret = __execute_command("/usr/etc/bluetooth/bt-dev-end.sh &", NULL);
			__bt_core_set_status(BT_DEACTIVATED);
			__bt_core_set_le_status(BT_LE_DEACTIVATED);
			return -1;
		}
	} else {
		__bt_core_set_le_status(BT_LE_ACTIVATED);
		g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
	}
	return 0;
}

int _bt_disable_adapter_le(void)
{
	BT_DBG("+");

	bt_status_t status;
	bt_le_status_t le_status;

	le_status = _bt_core_get_le_status();
	retv_if(le_status == BT_LE_DEACTIVATED, 0);
	retv_if(le_status == BT_LE_DEACTIVATING, -1);

	status = _bt_core_get_status();
	BT_DBG("status : %d", status);

	if (status == BT_DEACTIVATED) {
		__bt_core_set_le_status(BT_LE_DEACTIVATING);

		if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
			BT_ERR("running script failed");
			__bt_core_set_le_status(BT_LE_ACTIVATED);
			return -1;
		}
	} else {
			g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
	}

	__bt_core_set_le_status(BT_LE_DEACTIVATED);

	BT_DBG("-");
	return 0;
}

int _bt_core_service_request_adapter(int service_function)
{
	int ret = -1;

	GArray *in_param1 = NULL;
	GArray *in_param2 = NULL;
	GArray *in_param3 = NULL;
	GArray *in_param4 = NULL;
	GArray *out_param = NULL;

	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	ret = _bt_core_service_request(BT_CORE_SERVICE, service_function,
			in_param1, in_param2, in_param3, in_param4, &out_param);
	if (ret < 0)
		BT_ERR("_bt_core_service_request_adapter() failed");

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return ret;
}

static gboolean __bt_core_check_the_adapter_path(GDBusConnection *conn)
{
	GError *err = NULL;
	GDBusProxy *manager_proxy = NULL;
	GVariant *result = NULL;
	char *adapter_path = NULL;

	if(conn == NULL)
		return FALSE;

	manager_proxy =  g_dbus_proxy_new_sync(conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			"org.bluez",
			"/",
			"org.freedesktop.DBus.ObjectManager",
			NULL, &err);

	if (!manager_proxy) {
		if(err != NULL) {
			BT_ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
		goto fail;
	}

	result = g_dbus_proxy_call_sync(manager_proxy, "DefaultAdapter", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &err);
	if (!result) {
		if (err != NULL) {
			BT_ERR("Fail to get DefaultAdapter (Error: %s)", err->message);
			g_clear_error(&err);
		} else{
			BT_ERR("Fail to get DefaultAdapter");
		}
		goto fail;
	}

	if (g_strcmp0(g_variant_get_type_string(result), "(o)")) {
		BT_ERR("Incorrect result\n");
		goto fail;
	}

	g_variant_get(result, "(&o)", &adapter_path);

	if (adapter_path == NULL ||
		strlen(adapter_path) >= BT_CORE_CHECK_ADAPTER_OBJECT_PATH_MAX) {
		BT_ERR("Adapter path is inproper\n");
		goto fail;
	}

	g_variant_unref(result);
	g_object_unref(manager_proxy);

	return TRUE;

fail:
	if (result)
		g_variant_unref(result);

	if (manager_proxy)
		g_object_unref(manager_proxy);

	return FALSE;
}

void _bt_core_update_status(void)
{
	int bt_status = VCONFKEY_BT_STATUS_OFF;
	int bt_le_status = VCONFKEY_BT_LE_STATUS_OFF;
	gboolean ret = FALSE;

	ret = __bt_core_check_the_adapter_path(_bt_core_get_gdbus_connection());
	BT_INFO("check the real status of bt_adapter");

	if (ret != TRUE) {
		__bt_core_set_status(BT_DEACTIVATED);
		__bt_core_set_le_status(BT_DEACTIVATED);
	} else {
		if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0)
			BT_ERR("no bluetooth device info, so BT was disabled at previous session");
		if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_le_status) < 0)
			BT_ERR("no bluetooth le info, so BT LE was disabled at previous session");

		BT_INFO("bt_status = %d, bt_le_status = %d", bt_status, bt_le_status);

		if(bt_status == VCONFKEY_BT_STATUS_ON)
			__bt_core_set_status(BT_ACTIVATED);
		if(bt_le_status == VCONFKEY_BT_LE_STATUS_ON)
			__bt_core_set_le_status(BT_ACTIVATED);
	}
}

static gboolean bt_core_enable_adapter(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);
	int ret;

	if (sender == NULL)
		return FALSE;

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
		BT_ERR("Set vconf failed");

	ret = _bt_enable_adapter();
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

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
		BT_ERR("Set vconf failed");

	ret = _bt_disable_adapter();
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

static gboolean bt_core_recover_adapter(BtCore *agent,
						DBusGMethodInvocation *context)
{
	int ret;
	int ret_le;

	BT_INFO_C("Recover bt adapter");

	dbus_g_method_return(context);

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
		BT_ERR("Set vconf failed");

	is_recovery_mode = TRUE;

	_bt_core_update_status();

	if (_bt_core_get_status() == BT_ACTIVATED) {
		_bt_core_set_bt_status(BT_RECOVERY_MODE, 1);
		_bt_core_service_request_adapter(BT_DISABLE_ADAPTER);
	}
	if (_bt_core_get_le_status() == BT_LE_ACTIVATED) {
		_bt_core_set_bt_le_status(BT_RECOVERY_MODE, 1);
		_bt_core_service_request_adapter(BT_DISABLE_ADAPTER_LE);
	}

	ret = _bt_disable_adapter();
	if (ret < 0)
		BT_ERR("_bt_disable_adapter() failed");
	ret_le = _bt_disable_adapter_le();
	if (ret_le < 0)
		BT_ERR("_bt_disable_adapter_le() failed");

	return TRUE;
}

static gboolean bt_core_enable_adapter_le(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);
	int ret;

	if (sender == NULL)
		return FALSE;

	ret = _bt_enable_adapter_le();
	if (ret < 0) {
		GError *error = bt_core_error(BT_CORE_ERROR_REJECT,
							"LE Activation failed");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		BT_DBG("-");
		return FALSE;
	} else {
		dbus_g_method_return(context);
	}

	g_free(sender);
	BT_DBG("-");
	return TRUE;
}

static gboolean bt_core_disable_adapter_le(BtCore *agent,
						DBusGMethodInvocation *context)
{
	BT_DBG("+");

	char *sender = dbus_g_method_get_sender(context);
	BT_DBG("sender : %s", sender);
	int ret;

	if (sender == NULL)
		return FALSE;

	ret = _bt_disable_adapter_le();
	if (ret < 0) {
		GError *error = bt_core_error(BT_CORE_ERROR_REJECT,
							"LE Deactivation failed");
		dbus_g_method_return_error(context, error);
		g_error_free(error);
		g_free(sender);
		return FALSE;
	} else {
		dbus_g_method_return(context);
	}

	g_free(sender);
	BT_DBG("-");
	return TRUE;
}

static int __bt_reset_adapter(void)
{
	/* Forcely terminate */
	if (__execute_command("/usr/etc/bluetooth/bt-reset-env.sh", NULL) < 0) {
		BT_ERR("running script failed");
	}
	_bt_core_terminate();
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

static gboolean __bt_core_enable_core_timeout_cb(gpointer data)
{
	BT_DBG("+");

	_bt_core_init_vconf_value();

	return FALSE;
}

static gboolean bt_core_enable_core(BtCore *agent,
						DBusGMethodInvocation *context)
{
	char *sender = dbus_g_method_get_sender(context);

	if (sender == NULL)
		return FALSE;

	BT_DBG("+");

	_bt_core_update_status();

	g_timeout_add(200, (GSourceFunc)__bt_core_enable_core_timeout_cb, NULL);

	dbus_g_method_return(context);
	g_free(sender);

	BT_DBG("-");
	return TRUE;
}

static gboolean __bt_core_recovery_cb(gpointer data)
{
	int ret = 0;
	gboolean is_request_failed = FALSE;
	static gboolean is_first_failure = TRUE;

	BT_DBG("+");

	if (_bt_core_get_bt_status(BT_RECOVERY_MODE) == 1) {
		ret = _bt_core_service_request_adapter(BT_ENABLE_ADAPTER);
		if (ret < 0)
			is_request_failed = TRUE;
	}

	if (_bt_core_get_bt_le_status(BT_RECOVERY_MODE) == 1) {
		ret = _bt_core_service_request_adapter(BT_ENABLE_ADAPTER_LE);
		if (ret < 0)
			is_request_failed = TRUE;
	}

	if (is_request_failed == TRUE) {
		BT_ERR("Recovery is failed.");
		if (is_first_failure == TRUE) {
			g_timeout_add(2000, (GSourceFunc)__bt_core_recovery_cb, NULL);
			is_first_failure = FALSE;
			return FALSE;
		} else {
			is_first_failure = TRUE;
			return FALSE;
		}
	} else
		is_first_failure = TRUE;

	if (_bt_core_get_bt_status(BT_RECOVERY_MODE) == 1) {
		_bt_core_set_bt_status(BT_RECOVERY_MODE, 0);
		ret = _bt_enable_adapter();
		if (ret < 0)
			BT_ERR("_bt_enable_adapter() failed");
	}
	if (_bt_core_get_bt_le_status(BT_RECOVERY_MODE) == 1) {
		_bt_core_set_bt_le_status(BT_RECOVERY_MODE, 0);
		ret = _bt_enable_adapter_le();
		if (ret < 0)
			BT_ERR("_bt_enable_adapter_le() failed");
	}

	is_recovery_mode = FALSE;

	BT_DBG("-");

	return FALSE;
}

static gboolean __bt_core_enable_timeout_cb(gpointer data)
{
	bt_status_t adapter_status;
	bt_le_status_t adapter_status_le;

	BT_DBG("");

	if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
		BT_ERR("Set vconf failed");

	adapter_status = _bt_core_get_status();
	adapter_status_le = _bt_core_get_le_status();

	if (adapter_status == BT_DEACTIVATED &&
			_bt_core_get_bt_status(BT_FLIGHT_MODE) != 0) {
		_bt_core_set_bt_status(BT_FLIGHT_MODE, 0);
		_bt_core_service_request_adapter(BT_ENABLE_ADAPTER);
		_bt_enable_adapter();
	}

	if (adapter_status_le == BT_LE_DEACTIVATED &&
			_bt_core_get_bt_le_status(BT_FLIGHT_MODE) != 0) {
		_bt_core_set_bt_le_status(BT_FLIGHT_MODE, 0);
		_bt_core_service_request_adapter(BT_ENABLE_ADAPTER_LE);
		_bt_enable_adapter_le();
	}

	return FALSE;
}

static gboolean __bt_core_disable_timeout_cb(gpointer data)
{
	bt_status_t adapter_status;
	bt_le_status_t adapter_status_le;

	BT_DBG("");

	if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 1) != 0)
		BT_ERR("Set vconf failed");

	adapter_status = _bt_core_get_status();
	adapter_status_le = _bt_core_get_le_status();

	if (adapter_status == BT_ACTIVATED) {
		int bt_status_before_mode = 0;

		if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status_before_mode) == 0)
			_bt_core_set_bt_status(BT_FLIGHT_MODE, bt_status_before_mode);

		_bt_core_service_request_adapter(BT_DISABLE_ADAPTER);
		_bt_disable_adapter();
	}

	if (adapter_status_le == BT_LE_ACTIVATED) {
		int bt_le_status_before_mode = 0;

		if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_le_status_before_mode) == 0)
			_bt_core_set_bt_le_status(BT_FLIGHT_MODE, bt_le_status_before_mode);

		_bt_core_service_request_adapter(BT_DISABLE_ADAPTER_LE);
		_bt_disable_adapter_le();
	}

	return FALSE;
}

void _bt_core_adapter_added_cb(void)
{
	bt_status_t status;
	bt_le_status_t le_status;
	gboolean flight_mode_status;

	BT_DBG("");

	status = _bt_core_get_status();
	BT_DBG("status : %d", status);
	le_status = _bt_core_get_le_status();
	BT_DBG("le_status : %d", le_status);

	if (status == BT_ACTIVATING)
		__bt_core_set_status(BT_ACTIVATED);
	if (le_status == BT_LE_ACTIVATING)
		__bt_core_set_le_status(BT_LE_ACTIVATED);

	flight_mode_status = _bt_core_is_flight_mode_enabled();

	if (flight_mode_status == TRUE && _bt_is_flightmode_request() == TRUE) {
		_bt_set_flightmode_request(FALSE);
		g_timeout_add(2000, (GSourceFunc)__bt_core_disable_timeout_cb, NULL);
		return;
	}
	_bt_set_flightmode_request(FALSE);
	_bt_core_terminate();
}

void _bt_core_adapter_removed_cb(void)
{
	int flight_mode_value = 0;
	int power_saving_mode = 0;
	gboolean flight_mode_status;
	static int timer_id = -1;

	BT_DBG("");

	__bt_core_set_status(BT_DEACTIVATED);
	__bt_core_set_le_status(BT_LE_DEACTIVATED);
	if (vconf_set_int(VCONFKEY_BT_STATUS, VCONFKEY_BT_STATUS_OFF) != 0)
		BT_ERR("Set vconf failed");
	if (vconf_set_int(VCONFKEY_BT_LE_STATUS, VCONFKEY_BT_LE_STATUS_OFF) != 0)
		BT_ERR("Set vconf failed");

	if (is_recovery_mode == TRUE)
	{
		if (timer_id < 0)
			timer_id = g_timeout_add(2000, (GSourceFunc)__bt_core_recovery_cb, NULL);
		return;
	}

	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");

	if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, &power_saving_mode) != 0)
		BT_ERR("Fail to get the ps_mode_deactivated value");

	flight_mode_status = _bt_core_is_flight_mode_enabled();

	if (flight_mode_status == FALSE && _bt_is_flightmode_request() == TRUE) {
		_bt_set_flightmode_request(FALSE);
		if (timer_id < 0)
			timer_id = g_timeout_add(2000, (GSourceFunc)__bt_core_enable_timeout_cb, NULL);
		return;
	}
	_bt_set_flightmode_request(FALSE);

	if (flight_mode_value == 1 || power_saving_mode == 1){
		BT_DBG("Bt Core not terminated");
		return;
	}

	_bt_core_terminate();
}

