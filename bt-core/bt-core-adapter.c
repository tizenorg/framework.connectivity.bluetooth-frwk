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
#include <vconf-internal-keys.h>
#include <bundle.h>
#include <eventsystem.h>

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
#ifdef TIZEN_BT_FLIGHTMODE_ENABLED
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
#ifdef USB_BLUETOOTH
	char *argv_up[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "up", NULL};
	ret = __execute_command("/usr/bin/hciconfig", argv_up);
#else
	ret = __execute_command("/usr/etc/bluetooth/bt-stack-up.sh", NULL);
#endif
	if (ret < 0) {
		BT_ERR("running script failed");
#ifdef USB_BLUETOOTH
		char *argv_down[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "down", NULL};
		ret = __execute_command("/usr/bin/hciconfig", argv_down);
#else
		ret = __execute_command("/usr/etc/bluetooth/bt-dev-end.sh", NULL);
#endif
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
#if 0 /* only the concept of private */
	if (le_status == BT_LE_ACTIVATED) {
		/* Turn off PSCAN, (ISCAN if needed) */
		/* Return with 0 for the Disabled response. */
		__bt_core_set_status(BT_DEACTIVATED);
		BT_INFO("BR/EDR is disabled. now LE only mode");
		g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
		return 0;
	}
#endif

	status = _bt_core_get_status();
	if (status == BT_ACTIVATING) {
		/* Forcely terminate */
#ifdef USB_BLUETOOTH
		char *argv_down[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "down", NULL};
		if (__execute_command("/usr/bin/hciconfig", argv_down) < 0) {
#else
		if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
#endif
			BT_ERR("running script failed");
		}
		_bt_core_terminate();
		return 0;
	} else if (status != BT_ACTIVATED) {
		BT_ERR("Invalid state %d", status);
	}

	__bt_core_set_status(BT_DEACTIVATING);
#ifdef USB_BLUETOOTH
	char *argv_down[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "down", NULL};
	if (__execute_command("/usr/bin/hciconfig", argv_down) < 0) {
#else
	if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
#endif
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
#ifdef USB_BLUETOOTH
		char *argv_up[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "up", NULL};
		ret = __execute_command("/usr/bin/hciconfig", argv_up);
#else
		ret = __execute_command("/usr/etc/bluetooth/bt-stack-up.sh", NULL);
#endif
		if (ret < 0) {
			BT_ERR("running script failed");
#ifdef USB_BLUETOOTH
			char *argv_down[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "down", NULL};
			ret = __execute_command("/usr/bin/hciconfig", argv_down);
#else
			ret = __execute_command("/usr/etc/bluetooth/bt-dev-end.sh &", NULL);
#endif
			__bt_core_set_status(BT_DEACTIVATED);
			__bt_core_set_le_status(BT_LE_DEACTIVATED);
			return -1;
		}
	} else {
		__bt_core_set_le_status(BT_LE_ACTIVATED);
		g_timeout_add(BT_CORE_IDLE_TERM_TIME, __bt_core_idle_terminate, NULL);
	}
#ifdef HPS_FEATURE
	ret = _bt_core_start_httpproxy();
	if (ret < 0) {
		BT_ERR("_bt_core_start_httpproxy() failed");
	}
#endif

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

#ifdef HPS_FEATURE
	_bt_core_stop_httpproxy();
#endif

	status = _bt_core_get_status();
	BT_DBG("status : %d", status);

	if (status == BT_DEACTIVATED) {
		__bt_core_set_le_status(BT_LE_DEACTIVATING);
#ifdef USB_BLUETOOTH
		char *argv_down[] = {"/usr/bin/hciconfig", "/usr/bin/hciconfig", "hci0", "down", NULL};
		if (__execute_command("/usr/bin/hciconfig", argv_down) < 0) {
#else
		if (__execute_command("/usr/etc/bluetooth/bt-stack-down.sh", NULL) < 0) {
#endif
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
		if (err != NULL) {
			BT_ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		} else {
			BT_ERR("Fail to create proxy");
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

		if(bt_status & VCONFKEY_BT_STATUS_ON)
			__bt_core_set_status(BT_ACTIVATED);
		if(bt_le_status & VCONFKEY_BT_LE_STATUS_ON)
			__bt_core_set_le_status(BT_ACTIVATED);
	}
}

gboolean _bt_core_enable_adapter(void)
{
	int ret;

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, 0) != 0)
		BT_ERR("Set vconf failed");

	ret = _bt_enable_adapter();
	if (ret < 0)
		return FALSE;
	else
		return TRUE;
}

gboolean _bt_core_disable_adapter(void)
{
	int ret;

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, 0) != 0)
		BT_ERR("Set vconf failed");

	ret = _bt_disable_adapter();
	if (ret < 0)
		return FALSE;
	else
		return TRUE;
}

gboolean _bt_core_recover_adapter(void)
{
	int ret;
	int ret_le;

	BT_INFO_C("Recover bt adapter");

	_bt_set_flightmode_request(FALSE);
	if (vconf_set_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, 0) != 0)
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

gboolean _bt_core_enable_adapter_le(void)
{
	int ret;

	ret = _bt_enable_adapter_le();
	if (ret < 0)
		return FALSE;
	else
		return TRUE;
}

gboolean _bt_core_disable_adapter_le(void)
{
	BT_DBG("+");

	int ret;

	ret = _bt_disable_adapter_le();
	if (ret < 0)
		return FALSE;
	else
		return TRUE;
}

gboolean __bt_core_reset_adapter(void)
{
	/* Forcely terminate */
	if (__execute_command("/usr/etc/bluetooth/bt-reset-env.sh", NULL) < 0) {
		BT_ERR("running script failed");
	}
	_bt_core_terminate();

	return TRUE;
}

static gboolean __bt_core_enable_core_timeout_cb(gpointer data)
{
	BT_DBG("+");

	_bt_core_init_vconf_value();

	return FALSE;
}

gboolean _bt_core_enable_core(void)
{
	BT_DBG("+");

	_bt_core_update_status();

	g_timeout_add(200, (GSourceFunc)__bt_core_enable_core_timeout_cb, NULL);

	BT_DBG("-");
	return TRUE;
}

gboolean _bt_core_factory_test_mode(const char *type, const char *arg)
{

	char *cmd = NULL;
	char *arg_list[3] = { NULL, NULL, NULL };

	BT_DBG("Test item : %s", type);

	if (g_strcmp0(type, "Enable_RF_Test") == 0) {
		cmd = "/usr/etc/bluetooth/bt-edutm-on.sh";
		arg_list[0] = "bt-edutm-on.sh";
	} else if (g_strcmp0(type, "Disable_RF_Test") == 0) {
		cmd = "/usr/etc/bluetooth/bt-edutm-off.sh";
		arg_list[0] = "bt-edutm-off.sh";
	} else if (g_strcmp0(type, "Slave_Mode") == 0) {
		cmd = "/usr/etc/bluetooth/bt-mode-slave.sh";
		arg_list[0] = "bt-mode-slave.sh";
	} else if (g_strcmp0(type, "Master_Mode") == 0) {
		cmd = "/usr/etc/bluetooth/bt-mode-master.sh";
		arg_list[0] = "bt-mode-master.sh";
	} else if (g_strcmp0(type, "SSP_Debug_Mode") == 0) {
		cmd = "/usr/etc/bluetooth/bt-set-ssp-debug-mode.sh";
		arg_list[0] = "bt-set-ssp-debug-mode.sh";
		arg_list[1] = (char *)arg;
	} else if (g_strcmp0(type, "RF_Channel") == 0) {
		cmd = "/usr/etc/bluetooth/bt-enable-rf-channel.sh";
		arg_list[0] = "bt-enable-rf-channel.sh";
		arg_list[1] = (char *)arg;
	} else {
		_bt_core_terminate();
		return FALSE;
	}

	BT_DBG("Run %s", cmd);
	if (__execute_command(cmd, arg_list) < 0) {
		BT_ERR("running script failed");
	}

	_bt_core_terminate();
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

	if (vconf_set_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, 0) != 0)
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

	if (vconf_set_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, 1) != 0)
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

static int __bt_eventsystem_set_value(const char *event, const char *key, const char *value)
{
	int ret;
	bundle *b = NULL;

	b = bundle_create();

	bundle_add_str(b, key, value);

	ret = eventsystem_send_system_event(event, b);

	BT_DBG("eventsystem_send_system_event result: %d", ret);

	bundle_free(b);

	return ret;
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

	if (__bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_STATE,
						EVT_VAL_BT_OFF) != ES_R_OK)
		BT_ERR("Fail to set value");

	if (__bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_LE_STATE,
						EVT_VAL_BT_LE_OFF) != ES_R_OK)
		BT_ERR("Fail to set value");

	if (is_recovery_mode == TRUE)
	{
		if (timer_id < 0)
			timer_id = g_timeout_add(2000, (GSourceFunc)__bt_core_recovery_cb, NULL);
		return;
	}

	if (vconf_get_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");

	if (vconf_get_int(VCONFKEY_BT_POWERSAVING_MODE_DEACTIVATED, &power_saving_mode) != 0)
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
