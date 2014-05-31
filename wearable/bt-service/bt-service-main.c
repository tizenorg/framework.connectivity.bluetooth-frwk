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

#include <dbus/dbus-glib.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <privilege-control.h>
#include <vconf.h>

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-util.h"
#include "bt-request-handler.h"
#include "bt-service-adapter.h"

static GMainLoop *main_loop;
static gboolean terminated;

extern gboolean is_recovery_mode;

static void __bt_release_service(void)
{
	_bt_deinit_service_event_sender();
	_bt_deinit_service_event_receiver();

	_bt_service_unregister();

	_bt_deinit_proxys();

	_bt_clear_request_list();

	BT_INFO("Terminating the bt-service daemon");
}

static void __bt_sigterm_handler(int signo)
{
	BT_INFO("Get the signal: %d", signo);

	_bt_terminate_service(NULL);
}

gboolean _bt_terminate_service(gpointer user_data)
{
	int flight_mode_value = 0;
	int derr_value = 0;
/*
	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");
*/
	if (vconf_get_int(BT_OFF_DUE_TO_DEVICE_ERROR,
						&derr_value) != 0)
		BT_ERR("Fail to get the derr_deactivated value");

	if (flight_mode_value == 1 || derr_value > 0) {
		BT_DBG("Bt service not terminated");

		_bt_deinit_bluez_proxy();

		return FALSE;
	}

	if (main_loop != NULL) {
		g_main_loop_quit(main_loop);
	} else {
		BT_ERR("main_loop == NULL");
		__bt_release_service();
		terminated = TRUE;
		exit(0);
	}

	return FALSE;
}

static gboolean __bt_recovery_cb(gpointer data)
{
	BT_INFO("+");

	_bt_enable_adapter();

	return FALSE;
}

gboolean _bt_reliable_terminate_service(gpointer user_data)
{
	int flight_mode_value = 0;
	int derr_value = 0;

	if (is_recovery_mode == TRUE) {
		BT_DBG("Recovery mode");
		is_recovery_mode = FALSE;

		_bt_deinit_proxys();
		_bt_clear_request_list();
		_bt_set_disabled(BLUETOOTH_ERROR_NONE);

		g_timeout_add(2000, (GSourceFunc)__bt_recovery_cb, NULL);
		return FALSE;
	}
/*
	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");
*/
	if (vconf_get_int(BT_OFF_DUE_TO_DEVICE_ERROR,
						&derr_value) != 0)
		BT_ERR("Fail to get the derr_deactivated value");

	if (flight_mode_value == 1 || derr_value > 0) {
		BT_DBG("Bt service not terminated");

		_bt_set_disabled(BLUETOOTH_ERROR_NONE);
		_bt_deinit_bluez_proxy();

		return FALSE;
	}

	_bt_deinit_service_event_receiver();

	_bt_deinit_proxys();

	_bt_clear_request_list();

	_bt_set_disabled(BLUETOOTH_ERROR_NONE);

	_bt_deinit_service_event_sender();

	_bt_service_unregister();

	terminated = TRUE;

	BT_INFO("Terminating the bt-service daemon");

	if (main_loop != NULL) {
		g_main_loop_quit(main_loop);
	} else {
		exit(0);
	}

	return FALSE;
}

static gboolean __bt_check_bt_service(void *data)
{
	int bt_status = VCONFKEY_BT_STATUS_OFF;
	int flight_mode_deactivation = 0;
	int derr_deactivation = 0;
	int bt_off_due_to_timeout = 0;

	if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0) {
		BT_DBG("no bluetooth device info, so BT was disabled at previous session");
	}
/*
	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
						&flight_mode_deactivation) != 0)
			BT_ERR("Fail to get the flight_mode_deactivation value");
*/
	if (vconf_get_int(BT_OFF_DUE_TO_DEVICE_ERROR,
						&derr_deactivation) != 0)
			BT_ERR("Fail to get the derr_deactivation value");

	if (vconf_get_int(BT_OFF_DUE_TO_TIMEOUT, &bt_off_due_to_timeout) != 0)
			BT_ERR("Fail to get BT_OFF_DUE_TO_TIMEOUT");

	if (bt_status != VCONFKEY_BT_STATUS_OFF ||
		bt_off_due_to_timeout) {
		BT_DBG("Previous session was enabled.");

		/* Enable the BT */
		_bt_enable_adapter();
	} else if (bt_status == VCONFKEY_BT_STATUS_OFF &&
			(flight_mode_deactivation == 1 || derr_deactivation > 0)) {
/*
		if (flight_mode_deactivation == 1)
			_bt_handle_flight_mode_noti();

		if (derr_deactivation > 0)
			_bt_handle_power_saving_mode_noti();
*/
	} else {
		bt_status_t status = _bt_adapter_get_status();
		int adapter_enabled = 0;

		_bt_check_adapter(&adapter_enabled);

		BT_DBG("State: %d", status);
		BT_INFO("Adapter enabled: %d", adapter_enabled);

		/* Notify application on BT Activation via script. When the
		 * state is BT_ACTIVATING, we should not handle adapter added
		 * because it will be handled from the AdapterAdded signal.
		 * When activating from script case, the status will be
		 * BT_DEACTIVATED and we should handle it.
		 */
		if (adapter_enabled == 1 && status == BT_DEACTIVATED) {
			_bt_handle_adapter_added();
			return FALSE;
		}

		if (status != BT_ACTIVATING && status != BT_ACTIVATED) {
			_bt_terminate_service(NULL);
		}
	}

	return FALSE;
}

int main(void)
{
	struct sigaction sa;
	BT_INFO("Starting the bt-service daemon");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = __bt_sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_type_init();

	if (perm_app_set_privilege("bluetooth-frwk-service", NULL, NULL) !=
								PC_OPERATION_SUCCESS)
		BT_ERR("Failed to set app privilege.\n");

	/* Event reciever Init */
	if (_bt_init_service_event_receiver() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init event reciever");
		return 0;
	}

	/* Event sender Init */
	if (_bt_init_service_event_sender() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init event sender");
		return 0;
	}

	if (_bt_service_register() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to register service");
		return 0;
	}

	_bt_init_request_id();

	_bt_init_request_list();

	g_timeout_add(500, (GSourceFunc)__bt_check_bt_service, NULL);

	if (terminated == TRUE) {
		__bt_release_service();
		return 0;
	}

	main_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(main_loop);

	if (main_loop != NULL) {
		g_main_loop_unref(main_loop);
	}

	if (terminated == FALSE)
		__bt_release_service();

	return 0;
}

