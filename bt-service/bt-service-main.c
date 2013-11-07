/*
 * bluetooth-frwk
 *
 * Copyright (c) 2012-2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
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
#include <systemd/sd-daemon.h>

#include "bt-internal-types.h"
#include "bt-service-common.h"
#include "bt-service-event.h"
#include "bt-service-main.h"
#include "bt-service-util.h"
#include "bt-request-handler.h"
#include "bt-service-adapter.h"

#include <sys/file.h>
#include <errno.h>
#define LOCK_BT_SERVICE "/tmp/lock_bt_service"

static GMainLoop *main_loop;
static gboolean terminated;

static void __bt_release_service(void)
{
	_bt_deinit_service_event_sender();
	_bt_deinit_service_event_reciever();

	_bt_service_unregister();

	_bt_deinit_proxys();

	_bt_clear_request_list();

	BT_DBG("Terminating the bt-service daemon");
}

static void __bt_sigterm_handler(int signo)
{
	BT_DBG("Get the signal: %d", signo);

	_bt_terminate_service(NULL);
}

gboolean _bt_terminate_service(gpointer user_data)
{
	int flight_mode_value = 0;
	int ps_mode_value = 0;

	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");

	if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
						&ps_mode_value) != 0)
		BT_ERR("Fail to get the ps_mode_deactivated value");

	if (flight_mode_value == 1 || ps_mode_value > 0) {
		BT_DBG("Bt service not terminated");

		_bt_deinit_bluez_proxy();

		return FALSE;
	}

	if (main_loop != NULL) {
		g_main_loop_quit(main_loop);
	} else {
		BT_DBG("main_loop == NULL");
		__bt_release_service();
		terminated = TRUE;
		exit(0);
	}

	return FALSE;
}

gboolean _bt_reliable_terminate_service(gpointer user_data)
{
	int flight_mode_value = 0;
	int ps_mode_value = 0;

	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value) != 0)
		BT_ERR("Fail to get the flight_mode_deactivated value");

	if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
						&ps_mode_value) != 0)
		BT_ERR("Fail to get the ps_mode_deactivated value");

	if (flight_mode_value == 1 || ps_mode_value > 0) {
		BT_DBG("Bt service not terminated");

		_bt_set_disabled(BLUETOOTH_ERROR_NONE);
		_bt_deinit_bluez_proxy();

		return FALSE;
	}

	_bt_deinit_service_event_reciever();

	_bt_deinit_proxys();

	_bt_clear_request_list();

	_bt_set_disabled(BLUETOOTH_ERROR_NONE);

	_bt_deinit_service_event_sender();

	_bt_service_unregister();

	terminated = TRUE;

	BT_DBG("Terminating the bt-service daemon");

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
	int ps_mode_deactivation = 0;
	int bt_off_due_to_timeout = 0;

	if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status) < 0) {
		BT_DBG("no bluetooth device info, so BT was disabled at previous session");
	}

	if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
						&flight_mode_deactivation) != 0)
			BT_ERR("Fail to get the flight_mode_deactivation value");

	if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
						&ps_mode_deactivation) != 0)
			BT_ERR("Fail to get the ps_mode_deactivation value");

	if (vconf_get_int(BT_OFF_DUE_TO_TIMEOUT, &bt_off_due_to_timeout) != 0)
			BT_ERR("Fail to get %s",BT_OFF_DUE_TO_TIMEOUT);

	if (bt_status != VCONFKEY_BT_STATUS_OFF ||
		bt_off_due_to_timeout) {
		BT_DBG("Previous session was enabled.");

		/* Enable the BT */
		_bt_enable_adapter();
	} else if (bt_status == VCONFKEY_BT_STATUS_OFF &&
			(flight_mode_deactivation == 1 || ps_mode_deactivation > 0)) {
		if (flight_mode_deactivation == 1)
			_bt_handle_flight_mode_noti();
		if (ps_mode_deactivation > 0)
			_bt_handle_power_saving_mode_noti();
	} else {
		bt_status_t status = _bt_adapter_get_status();
		int adapter_enabled = 0;

		_bt_check_adapter(&adapter_enabled);

		BT_DBG("State: %d", status);
		BT_DBG("Adapter enabled: %d", adapter_enabled);

		if (adapter_enabled == 1) {
			_bt_handle_adapter_added();
			return FALSE;
		}

		if (status != BT_ACTIVATING && status != BT_ACTIVATED) {
			_bt_terminate_service(NULL);
		}
	}

	return FALSE;
}

static int __lock_bt_service(void)
{
	int fd;
	int ret;

	fd = open(LOCK_BT_SERVICE, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
	if (fd == -1) {
		BT_ERR("FAIL: open(%s)", LOCK_BT_SERVICE);
		return -1;
	}

	ret = flock(fd, LOCK_EX | LOCK_NB);
	if (ret == -1) {
		BT_DBG("FAIL: flock(fd, LOCK_EX | LOCK_NB), errno: %d", errno);
		close(fd);
		return -1;
	}

	return fd;
}

static int __unlock_bt_service(int fd)
{
	int ret;

	if (fd == -1) return -1;

	ret = flock(fd, LOCK_UN);

	if (ret == -1) {
		BT_ERR("FAIL: flock(fd, LOCK_UN)");
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

int main(void)
{
	struct sigaction sa;
	int ret, fd;
	BT_DBG("Starting the bt-service daemon");
	BT_DBG("TCT_BT: Starting the bt-service daemon");
	BT_DBG("TCT_BT: Apply Lock in bt-service");

	fd = __lock_bt_service();
	if (fd == -1) {
		BT_ERR("TCT_BT: FAIL: __lock_bt_service()");
		return -1;
	}

	BT_DBG("TCT_BT: After setting service_running");

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
		goto unlock;
	}

	/* Event sender Init */
	if (_bt_init_service_event_sender() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init event sender");
		goto unlock;
	}

	if (_bt_service_register() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to register service");
		goto unlock;
	}

	_bt_init_request_id();

	_bt_init_request_list();

	g_timeout_add_seconds(1, (GSourceFunc)__bt_check_bt_service, NULL);

	if (terminated == TRUE) {
		__bt_release_service();
		return 0;
	}

	main_loop = g_main_loop_new(NULL, FALSE);

	sd_notify(0, "READY=1");

	g_main_loop_run(main_loop);

	if (main_loop != NULL) {
		g_main_loop_unref(main_loop);
	}

	if (terminated == FALSE)
		__bt_release_service();

	BT_DBG("TCT_BT: Terminated the bt-service daemon");

unlock:
	BT_DBG("TCT_BT: unlock");

	ret = __unlock_bt_service(fd);
	if (ret < 0)
		BT_ERR("TCT_BT: FAIL: __unlock_bt_service(fd)");

	return 0;
}

