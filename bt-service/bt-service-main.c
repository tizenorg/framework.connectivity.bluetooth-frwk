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

//#include <dbus/dbus-glib.h>
#include <glib.h>
#include <dlog.h>
#include <string.h>
#include <vconf.h>
#include <vconf-internal-keys.h>

#ifndef TIZEN_WEARABLE
#include <privilege-control.h>
#endif
#include <bundle.h>
#include <eventsystem.h>

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-util.h"
#include "bt-request-handler.h"
#include "bt-service-adapter.h"
#include "bt-service-adapter-le.h"

static GMainLoop *main_loop;
static gboolean terminated = FALSE;

static void __bt_release_service(void)
{
	_bt_service_unregister_vconf_handler();

	_bt_service_adapter_le_deinit();
	_bt_deinit_service_event_sender();
	_bt_deinit_hf_local_term_event_sender();
	_bt_deinit_service_event_receiver();

	_bt_service_unregister();

	_bt_deinit_proxys();

	_bt_clear_request_list();

#ifndef GATT_NO_RELAY
	_bt_clear_gatt_client_senders();
#endif

	BT_DBG("Terminating the bt-service daemon");
}

static void __bt_sigterm_handler(int signo, siginfo_t *info, void *data)
{
	int ret;

	BT_INFO("signal [%d] is sent by [%d]", signo, info->si_pid);

	ret = _bt_recover_adapter();
	if (ret != BLUETOOTH_ERROR_NONE)
		BT_ERR("_bt_recover_adapter is failed : %d", ret);

	return;
}

gboolean _bt_terminate_service(gpointer user_data)
{
	int bt_status = VCONFKEY_BT_STATUS_OFF;

	if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0) {
		BT_ERR("no bluetooth device info, so BT was disabled at previous session");
	} else {
		if (bt_status != VCONFKEY_BT_STATUS_OFF) {
			if(vconf_set_int(VCONFKEY_BT_STATUS,
					VCONFKEY_BT_STATUS_OFF) != 0)
				BT_ERR("Set vconf failed\n");

			if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_STATE,
							EVT_VAL_BT_OFF) != ES_R_OK)
				BT_ERR("Fail to set value");
		}
	}

	if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_status) < 0) {
		BT_ERR("no bluetooth device info, so BT was disabled at previous session");
	} else {
		if (bt_status != VCONFKEY_BT_LE_STATUS_OFF) {
			if(vconf_set_int(VCONFKEY_BT_LE_STATUS,
					VCONFKEY_BT_LE_STATUS_OFF) != 0)
				BT_ERR("Set vconf failed\n");
			if (_bt_eventsystem_set_value(SYS_EVENT_BT_STATE, EVT_KEY_BT_LE_STATE,
							EVT_VAL_BT_LE_OFF) != ES_R_OK)
				BT_ERR("Fail to set value");
		}
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

gboolean _bt_reliable_terminate_service(gpointer user_data)
{
	_bt_deinit_service_event_receiver();

	_bt_deinit_proxys();

	_bt_clear_request_list();

	_bt_set_disabled(BLUETOOTH_ERROR_NONE);

	_bt_service_adapter_le_deinit();
	_bt_deinit_service_event_sender();
	_bt_deinit_hf_local_term_event_sender();

	_bt_service_unregister();

	terminated = TRUE;

	BT_INFO_C("Terminating the bt-service daemon");

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
	int bt_le_status = VCONFKEY_BT_LE_STATUS_OFF;
	bt_status_t status = BT_DEACTIVATED;
	bt_le_status_t le_status = BT_LE_DEACTIVATED;
	int flight_mode_deactivation = 0;
	int bt_off_due_to_timeout = 0;
#if 0
	int ps_mode_deactivation = 0;
#endif

	status = _bt_adapter_get_status();
	le_status = _bt_adapter_get_le_status();
	BT_DBG("State: %d, LE State: %d", status, le_status);

	if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0) {
		BT_DBG("no bluetooth device info, so BT was disabled at previous session");
	}

	if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_le_status) < 0) {
		BT_ERR("no bluetooth le info, so BT LE was disabled at previous session");
	}

	if (vconf_get_int(VCONFKEY_BT_FLIGHT_MODE_DEACTIVATED, &flight_mode_deactivation) != 0)
		BT_ERR("Fail to get the flight_mode_deactivation value");

#if 0
	if (vconf_get_int(VCONFKEY_BT_POWERSAVING_MODE_DEACTIVATED, &ps_mode_deactivation) != 0)
		BT_ERR("Fail to get the ps_mode_deactivation value");
#endif

	if (vconf_get_int(VCONFKEY_BT_OFF_DUE_TO_TIMEOUT, &bt_off_due_to_timeout) != 0)
		BT_ERR("Fail to get VCONFKEY_BT_OFF_DUE_TO_TIMEOUT");

	if ((bt_status != VCONFKEY_BT_STATUS_OFF || bt_off_due_to_timeout) &&
		(status == BT_DEACTIVATED)) {
		BT_DBG("Previous session was enabled.");

		/* Enable the BT */
		_bt_enable_adapter();
	} else if (bt_status == VCONFKEY_BT_STATUS_OFF && flight_mode_deactivation == 1) {
		_bt_enable_core();
	}

	if ((bt_le_status == VCONFKEY_BT_LE_STATUS_ON) && (le_status == BT_LE_DEACTIVATED)) {
		BT_DBG("Previous session was le enabled. Turn BT LE on automatically.");

		/* Enable the BT LE */
		_bt_enable_adapter_le();
	} else {
		status = _bt_adapter_get_status();
		le_status = _bt_adapter_get_le_status();
		BT_DBG("State: %d, LE State: %d", status, le_status);

		if ((status != BT_ACTIVATING && status != BT_ACTIVATED) &&
				(le_status != BT_LE_ACTIVATING && le_status != BT_LE_ACTIVATED)){
			_bt_terminate_service(NULL);
		}
	}

	return FALSE;
}

int main(void)
{
	struct sigaction sa;
	BT_INFO_C("Starting the bt-service daemon");

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = __bt_sigterm_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	g_type_init();
#ifndef TIZEN_WEARABLE
	if (perm_app_set_privilege("bluetooth-frwk-service", NULL, NULL) !=
		PC_OPERATION_SUCCESS)
		BT_ERR("Failed to set app privilege");
#endif
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

	if (_bt_init_hf_local_term_event_sender() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init core event sender");
		return 0;
	}

	if (_bt_service_register() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to register service");
		return 0;
	}

	if (_bt_service_adapter_le_init() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init le");
		return 0;
	}

	_bt_init_request_id();

	_bt_init_request_list();

#ifndef GATT_NO_RELAY
	_bt_init_gatt_client_senders();
#endif

	g_timeout_add(500, (GSourceFunc)__bt_check_bt_service, NULL);

	if (terminated == TRUE) {
		__bt_release_service();
		return 0;
	}

	main_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(main_loop);
	BT_DBG("g_main_loop_quit called!");

	if (main_loop != NULL) {
		g_main_loop_unref(main_loop);
	}

	if (terminated == FALSE)
		__bt_release_service();

	return 0;
}

