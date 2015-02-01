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

#include "bt-core-adapter.h"
#include "bt-core-common.h"
#include "bt-core-noti-handler.h"

static gboolean flightmode_request = FALSE;

void _bt_set_flightmode_request(gboolean value)
{
	flightmode_request = value;
}

gboolean _bt_is_flightmode_request(void)
{
	return flightmode_request;
}

static gboolean __bt_off_cause_conflict_check(void)
{
	int flight_mode_value = 0;
	int ps_mode_value = 0;
	gboolean ret = FALSE;

	retv_if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE,
					&flight_mode_value) != 0, FALSE);
	retv_if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE,
					&ps_mode_value) != 0, FALSE);

	if (flight_mode_value == 1 || ps_mode_value > 0) {
		BT_DBG("Bt should not turn on");
		ret = TRUE;
	}

	return ret;
}

static void __bt_core_handle_adapter_with_flight_mode(gboolean flight_mode)
{
	bt_status_t adapter_status;
	bt_le_status_t adapter_status_le;

	adapter_status = _bt_core_get_status();
	adapter_status_le = _bt_core_get_le_status();

	BT_INFO("bt status %d, le status %d", adapter_status, adapter_status_le);
	if (flight_mode == TRUE) {
		BT_INFO_C("Flight mode on. Turn off BT");

		if (adapter_status == BT_ACTIVATING || adapter_status_le == BT_LE_ACTIVATING) {
			BT_INFO("BT adapter is activating. Turn off BT after activation");
			_bt_set_flightmode_request(TRUE);
			return;
		}
		if (adapter_status != BT_ACTIVATED && adapter_status_le != BT_LE_ACTIVATED) {
			BT_INFO("No need to control bt status");
			return;
		}

		if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 1) != 0)
			BT_ERR("Set vconf failed");

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
	} else {
		int flight_mode_value = 0;

		BT_INFO_C("Flight mode off. Turn on BT");

		if (adapter_status == BT_DEACTIVATING || adapter_status_le == BT_LE_DEACTIVATING) {
			BT_INFO("BT adapter is activating. Turn off BT after activation");
			_bt_set_flightmode_request(TRUE);
			return;
		}
		if (adapter_status != BT_DEACTIVATED && adapter_status_le != BT_LE_DEACTIVATED) {
			BT_INFO("No need to control bt status");
			return;
		}

		if (vconf_get_int(BT_OFF_DUE_TO_FLIGHT_MODE, &flight_mode_value))
			BT_ERR("Fail get flight mode value");

		if (flight_mode_value == 0)
			return;

		if (vconf_set_int(BT_OFF_DUE_TO_FLIGHT_MODE, 0) != 0)
			BT_ERR("Set vconf failed");

		ret_if(__bt_off_cause_conflict_check());

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
	}
}

static void __bt_core_handle_adapter_with_power_saving_mode(int power_saving_mode)
{
	bt_status_t adapter_status;
	bt_le_status_t adapter_status_le;

	adapter_status = _bt_core_get_status();
	adapter_status_le = _bt_core_get_le_status();

	if (power_saving_mode == 2) {
		BT_DBG("Deactivate Bluetooth Service");
		if (vconf_set_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, 1) != 0)
			BT_ERR("Set vconf failed");

		if (adapter_status == BT_ACTIVATED) {
			int bt_status_before_mode = 0;
			if (vconf_get_int(VCONFKEY_BT_STATUS, &bt_status_before_mode) == 0)
				_bt_core_set_bt_status(BT_POWER_SAVING_MODE, bt_status_before_mode);

			_bt_core_service_request_adapter(BT_DISABLE_ADAPTER);
			_bt_disable_adapter();
		}
		if (adapter_status_le == BT_LE_ACTIVATED) {
			int bt_le_status_before_mode = 0;
			if (vconf_get_int(VCONFKEY_BT_LE_STATUS, &bt_le_status_before_mode) == 0)
				_bt_core_set_bt_le_status(BT_POWER_SAVING_MODE, bt_le_status_before_mode);

			/* Disable the BT LE */
			_bt_core_service_request_adapter(BT_DISABLE_ADAPTER_LE);
			_bt_disable_adapter_le();
		}
	} else {
		int ps_mode_value = 0;

		if (vconf_get_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, &ps_mode_value))
			BT_ERR("Fail get power saving mode value");

		if (ps_mode_value == 0)
			return;

		BT_DBG("Activate Bluetooth");
		if (vconf_set_int(BT_OFF_DUE_TO_POWER_SAVING_MODE, 0))
			BT_ERR("Set vconf failed");

		ret_if(__bt_off_cause_conflict_check());

		BT_DBG("BT status before Emergency mode() :%d",
			_bt_core_get_bt_status(BT_POWER_SAVING_MODE));

		if (adapter_status == BT_DEACTIVATED && (_bt_core_get_bt_status(BT_POWER_SAVING_MODE) != 0)) {
			_bt_core_set_bt_status(BT_POWER_SAVING_MODE, 0);
			_bt_core_service_request_adapter(BT_ENABLE_ADAPTER);
			_bt_enable_adapter();
		}
		BT_DBG("BT LE status before Emergency mode() :%d", _bt_core_get_bt_le_status(BT_POWER_SAVING_MODE));
		if (adapter_status_le == BT_LE_DEACTIVATED &&  _bt_core_get_bt_le_status(BT_POWER_SAVING_MODE) != 0) {
			_bt_core_set_bt_le_status(BT_POWER_SAVING_MODE, 0);
			/* Enable the BT LE */
			_bt_core_service_request_adapter(BT_ENABLE_ADAPTER_LE);
			_bt_enable_adapter_le();
		}
	}
}

static void __bt_core_flight_mode_cb(keynode_t *node, void *data)
{
	gboolean flight_mode = FALSE;
	int type;

	BT_DBG("key = %s", vconf_keynode_get_name(node));

	type = vconf_keynode_get_type(node);
	if (type != VCONF_TYPE_BOOL) {
		BT_ERR("Invaild vconf key type : %d", type);
		return;
	}

	flight_mode = vconf_keynode_get_bool(node);

	__bt_core_handle_adapter_with_flight_mode(flight_mode);
}

#ifndef TIZEN_WEARABLE
static void __bt_core_power_saving_mode_cb(keynode_t *node, void *data)
{
	int power_saving_mode = 0;

	DBG_SECURE("key=%s", vconf_keynode_get_name(node));

	if (vconf_keynode_get_type(node) != VCONF_TYPE_INT) {
		BT_ERR("Wrong vconf type");
		return;
	}

	power_saving_mode = vconf_keynode_get_int(node);

	BT_DBG("value=%d", power_saving_mode);

	__bt_core_handle_adapter_with_power_saving_mode(power_saving_mode);
}
#endif

void _bt_core_init_vconf_value(void)
{
	gboolean flight_mode = FALSE;
	int power_saving_mode = 0;

	_bt_core_handle_flight_mode_noti();
	_bt_core_handle_power_saving_mode_noti();

	if (vconf_get_bool(VCONFKEY_TELEPHONY_FLIGHT_MODE, &flight_mode) != 0)
		BT_ERR("Fail to get the flight_mode status value");
#ifndef TIZEN_WEARABLE
	if (vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &power_saving_mode) != 0)
		BT_ERR("Fail to get the power_saving_mode status value");
#endif
	BT_DBG("flight_mode = %d, power_saving_mode = %d", flight_mode, power_saving_mode);

	if (flight_mode == TRUE)
		__bt_core_handle_adapter_with_flight_mode(flight_mode);
	else if (power_saving_mode > 0)
		__bt_core_handle_adapter_with_power_saving_mode(power_saving_mode);
	else
		BT_ERR("");
}

void _bt_core_handle_flight_mode_noti(void)
{
	int ret;

	BT_DBG("+");

	ret = vconf_notify_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_core_flight_mode_cb, NULL);
	if (ret < 0)
		BT_ERR("Unable to register key handler");
}

void _bt_core_handle_power_saving_mode_noti(void)
{
#ifndef TIZEN_WEARABLE
	int ret;

	BT_DBG("+");
	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_core_power_saving_mode_cb, NULL);

	if (ret < 0)
		BT_ERR("Unable to register key handler");
#endif
}

void _bt_core_unregister_vconf_handler(void)
{
	vconf_ignore_key_changed(VCONFKEY_TELEPHONY_FLIGHT_MODE,
			(vconf_callback_fn)__bt_core_flight_mode_cb);

#ifndef TIZEN_WEARABLE
	vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE,
			(vconf_callback_fn)__bt_core_power_saving_mode_cb);
#endif

	return;
}

