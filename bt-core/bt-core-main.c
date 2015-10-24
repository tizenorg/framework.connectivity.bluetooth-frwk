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

#include "bt-core-adapter.h"
#include "bt-core-dbus-handler.h"
#include "bt-core-main.h"
#include "bt-core-noti-handler.h"
#include "bt-core-common.h"

static GMainLoop *main_loop = NULL;

gboolean _bt_check_terminating_condition(void)
{
	int bt_off_flight_mode = 0;	/* if BT was off due to FlightMode*/
	int bt_off_ps_mode = 0;

	if (_bt_core_is_recovery_mode() == TRUE) {
		BT_DBG("Bt core not terminated");
		return FALSE;
	}

	if (vconf_get_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, &bt_off_flight_mode) != 0)
		BT_ERR("Fail to get the BT off due to FlightMode value");
	if (vconf_get_int(VCONFKEY_BT_POWERSAVING_MODE_DEACTIVATED, &bt_off_ps_mode) != 0)
		BT_ERR("Fail to get the ps_mode_deactivated value");

	if (bt_off_flight_mode == 1 || bt_off_ps_mode == 1) {
		BT_DBG("Bt core not terminated");
		return FALSE;
	}

	return TRUE;
}

void _bt_core_terminate(void)
{
	if (_bt_check_terminating_condition() == FALSE)
		return;

	_bt_core_gdbus_deinit_proxys();
	_bt_core_unregister_vconf_handler();
	if (main_loop) {
		g_main_loop_quit(main_loop);
	} else {
		BT_DBG("Terminating bt-core daemon");
		exit(0);
	}
}

static void __bt_core_sigterm_handler(int signo)
{
	BT_DBG("Got the signal: %d", signo);

	_bt_core_terminate();
}

static gboolean __bt_check_bt_core(void *data)
{
	int bt_status = VCONFKEY_BT_STATUS_OFF;
	int bt_le_status = VCONFKEY_BT_LE_STATUS_OFF;
	bt_status_t status = BT_DEACTIVATED;
	bt_le_status_t le_status = BT_LE_DEACTIVATED;
	int flight_mode_deactivation = 0;
	int bt_off_due_to_timeout = 0;
	int ps_mode_deactivation = 0;
	int ret;

	status = _bt_core_get_status();
	le_status = _bt_core_get_le_status();
	BT_DBG("State: %d, LE State: %d", status, le_status);

	if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0) {
		BT_DBG("no bluetooth device info, so BT was disabled at previous session");
	}

	if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_le_status) < 0) {
		BT_ERR("no bluetooth le info, so BT LE was disabled at previous session");
	}

	if (vconf_get_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, &flight_mode_deactivation) != 0)
		BT_ERR("Fail to get the flight_mode_deactivation value");

	if (vconf_get_int(VCONFKEY_BT_POWERSAVING_MODE_DEACTIVATED, &ps_mode_deactivation) != 0)
		BT_ERR("Fail to get the ps_mode_deactivation value");

	if (vconf_get_int(VCONFKEY_BT_OFF_DUE_TO_TIMEOUT, &bt_off_due_to_timeout) != 0)
		BT_ERR("Fail to get VCONFKEY_BT_OFF_DUE_TO_TIMEOUT");

	if ((bt_status != VCONFKEY_BT_STATUS_OFF || bt_off_due_to_timeout) &&
		(status == BT_DEACTIVATED)) {
		BT_DBG("Previous session was enabled.");

		/* Enable the BT */
		_bt_core_service_request_adapter(BT_ENABLE_ADAPTER);
		_bt_enable_adapter();
	} else if (bt_status == VCONFKEY_BT_STATUS_OFF &&
			(flight_mode_deactivation == 1 || ps_mode_deactivation > 0)) {
		_bt_core_handle_flight_mode_noti();
		_bt_core_handle_power_saving_mode_noti();

		_bt_core_set_bt_status(BT_FLIGHT_MODE, flight_mode_deactivation);
		_bt_core_set_bt_status(BT_POWER_SAVING_MODE, ps_mode_deactivation);
	}

	if ((bt_le_status == VCONFKEY_BT_LE_STATUS_ON) && (le_status == BT_LE_DEACTIVATED)) {
		BT_DBG("Previous session was le enabled. Turn BT LE on automatically.");

		/* Enable the BT LE */
		_bt_core_service_request_adapter(BT_ENABLE_ADAPTER_LE);
		_bt_enable_adapter_le();
	} else {
		status = _bt_core_get_status();
		le_status = _bt_core_get_le_status();
		BT_DBG("State: %d, LE State: %d", status, le_status);

		if ((status != BT_ACTIVATING && status != BT_ACTIVATED) &&
				(le_status != BT_LE_ACTIVATING && le_status != BT_LE_ACTIVATED))
			_bt_core_terminate();
	}

	return FALSE;
}

int main(void)
{
	gboolean ret;
	struct sigaction sa;

	g_type_init();
	BT_INFO_C("Starting bt-core daemeon");

	_bt_core_update_status();

	ret = _bt_core_register_dbus();
	if (!ret) {
		BT_ERR("_bt_core_register_dbus failed");
		goto fail;
	}


	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = __bt_core_sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_timeout_add(500, (GSourceFunc)__bt_check_bt_core, NULL);

	main_loop = g_main_loop_new(NULL, FALSE);
	if (!main_loop) {
		BT_ERR("creating main loop failed");
		goto fail;
	}

	g_main_loop_run(main_loop);

fail:
	_bt_core_unregister_dbus();

	if (main_loop)
		g_main_loop_unref(main_loop);

	BT_INFO_C("Terminating bt-core daemon");

	return 0;
}
