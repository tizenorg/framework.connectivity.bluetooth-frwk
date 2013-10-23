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

#include "bluetooth-api.h"
#include "bluetooth-audio-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

BT_EXPORT_API int bluetooth_audio_init(bt_audio_func_ptr cb, void *user_data)
{
	int ret;

	if (cb == NULL) {
		BT_ERR("callback is NULL");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}
	ret = _bt_init_event_handler();

	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to init the event handler");
		return ret;
	}

	_bt_set_user_data(BT_AUDIO, (void *)cb, user_data);

	/* Register All events */
	ret = _bt_register_event(BT_HEADSET_EVENT, (void *)cb, user_data);
	if (ret != BLUETOOTH_ERROR_NONE) {
		_bt_deinit_event_handler();
		return ret;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_audio_deinit(void)
{
	_bt_unregister_event(BT_HEADSET_EVENT);

	_bt_set_user_data(BT_AUDIO, NULL, NULL);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_audio_connect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AUDIO_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_audio_disconnect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AUDIO_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_ag_connect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AG_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_ag_disconnect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AG_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_av_connect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AV_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_av_disconnect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_AV_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_ag_get_headset_volume(unsigned int *speaker_gain)
{
	int result;

	BT_CHECK_PARAMETER(speaker_gain, return);
	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_GET_SPEAKER_GAIN,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	if (result == BLUETOOTH_ERROR_NONE) {
		*speaker_gain = g_array_index(out_param,
				unsigned int, 0);
	}

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_ag_set_speaker_gain(unsigned int speaker_gain)
{
	int result;

	BT_CHECK_ENABLED(return);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, &speaker_gain, sizeof(unsigned int));

	result = _bt_send_request(BT_BLUEZ_SERVICE, BT_SET_SPEAKER_GAIN,
		in_param1, in_param2, in_param3, in_param4, &out_param);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

