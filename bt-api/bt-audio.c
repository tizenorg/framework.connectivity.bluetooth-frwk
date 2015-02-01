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
	if (ret != BLUETOOTH_ERROR_NONE &&
	    ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
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
	int service_function = BT_AUDIO_CONNECT;
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_AUDIO);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, service_function,
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

#define BT_HF_SERVICE_NAME "org.bluez.hf_agent"
#define BT_HF_OBJECT_PATH "/org/bluez/handsfree_agent"
#define BT_HF_INTERFACE "org.tizen.HfApp"


static DBusMessage* __bt_hf_agent_dbus_send(const char *path,
			const char *interface, const char *method, DBusError *err,  int type, ...)
{
	DBusMessage *msg;
	DBusMessage *reply;
	va_list args;

	msg = dbus_message_new_method_call(BT_HF_SERVICE_NAME,
			path, interface, method);
	if (!msg) {
		BT_ERR("Unable to allocate new D-Bus %s message \n", method);
		return NULL;
	}

	va_start(args, type);

	if (!dbus_message_append_args_valist(msg, type, args)) {
		dbus_message_unref(msg);
		va_end(args);
		return NULL;
	}

	va_end(args);

	dbus_error_init(err);

	BT_DBG("DBus HF API call, method = %s", method);

	reply = dbus_connection_send_with_reply_and_block(_bt_get_system_conn(),
					msg, 4000, err);
	dbus_message_unref(msg);

	return reply;
}

static int __bt_hf_agent_read_call_list(DBusMessage *reply,
				bt_hf_call_list_s **call_list) {

	DBusMessageIter iter;
	DBusMessageIter iter_struct;
	int32_t call_count;

	BT_DBG("+");

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_get_basic(&iter, &call_count);
	if(call_count <= 0) {
		*call_list = NULL;
		return BLUETOOTH_ERROR_NOT_FOUND;
	}
	BT_DBG("Call count = %d", call_count);

	*call_list = g_malloc0(sizeof(bt_hf_call_list_s));
	(*call_list)->count = call_count;
	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &iter_struct);
	while(dbus_message_iter_get_arg_type(&iter_struct) ==
			DBUS_TYPE_STRUCT) {

		gchar *number = NULL;
		bt_hf_call_status_info_t *call_info;
		DBusMessageIter entry_iter;

		call_info = g_malloc0(sizeof(bt_hf_call_status_info_t));

		dbus_message_iter_recurse(&iter_struct,&entry_iter);

		dbus_message_iter_get_basic(&entry_iter, &number);
		call_info->number = g_strdup(number);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &call_info->direction);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &call_info->status);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &call_info->mpart);
		dbus_message_iter_next(&entry_iter);
		dbus_message_iter_get_basic(&entry_iter, &call_info->idx);

		(*call_list)->list = g_list_append((*call_list)->list,
							(gpointer)call_info);
		dbus_message_iter_next(&iter_struct);
	}
	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_init(bt_hf_func_ptr cb, void *user_data)
{
	int ret;

	if (cb == NULL) {
		BT_ERR("callback is NULL");
		return BLUETOOTH_ERROR_INVALID_PARAM;
	}

	ret = dbus_threads_init_default();

	if (ret)
		BT_ERR("dbus_thread_init_default Success");
	else
		BT_ERR("dbus_thread_init_default Fail");

	ret = _bt_init_event_handler();

	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		BT_ERR("Fail to init the event handler");
		return ret;
	}

	_bt_set_user_data(BT_HF, (void *)cb, user_data);

	/* Register All events */
	ret = _bt_register_event(BT_HF_AGENT_EVENT, (void *)cb, user_data);
	if (ret != BLUETOOTH_ERROR_NONE &&
	     ret != BLUETOOTH_ERROR_ALREADY_INITIALIZED) {
		_bt_deinit_event_handler();
		return ret;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_deinit(void)
{
	int ret;

	ret = _bt_unregister_event(BT_HF_AGENT_EVENT);
	if (ret != BLUETOOTH_ERROR_NONE )
		BT_ERR("_bt_unregister_event failed");

	_bt_set_user_data(BT_HF, NULL, NULL);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_connect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_ENABLED(return);
	BT_CHECK_PARAMETER(remote_address, return);

	user_info = _bt_get_user_data(BT_HF);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_HF_CONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_hf_disconnect(bluetooth_device_address_t *remote_address)
{
	int result;
	bt_user_info_t *user_info;

	BT_CHECK_PARAMETER(remote_address, return);
	BT_CHECK_ENABLED(return);

	user_info = _bt_get_user_data(BT_HF);
	retv_if(user_info->cb == NULL, BLUETOOTH_ERROR_INTERNAL);

	BT_INIT_PARAMS();
	BT_ALLOC_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	g_array_append_vals(in_param1, remote_address, sizeof(bluetooth_device_address_t));

	result = _bt_send_request_async(BT_BLUEZ_SERVICE, BT_HF_DISCONNECT,
		in_param1, in_param2, in_param3, in_param4,
		user_info->cb, user_info->user_data);

	BT_FREE_PARAMS(in_param1, in_param2, in_param3, in_param4, out_param);

	return result;
}

BT_EXPORT_API int bluetooth_hf_answer_call()
{
	DBusMessage *reply;
	DBusError err;
	int ret = BLUETOOTH_ERROR_INTERNAL;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"AnswerCall", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			if (strcmp(err.message, "Operation not supported") == 0)
				ret = BLUETOOTH_ERROR_NOT_IN_OPERATION;
			else if (strcmp(err.message, "Operation not allowed") == 0)
				ret = BLUETOOTH_ERROR_PERMISSION_DEINED;
			else
				ret = BLUETOOTH_ERROR_INTERNAL;
			dbus_error_free(&err);
		}
		return ret;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;

}

BT_EXPORT_API int bluetooth_hf_terminate_call()
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"TerminateCall", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_initiate_call(char *number)
{
	DBusMessage *reply;
	DBusError err;
	int ret = BLUETOOTH_ERROR_INTERNAL;

	BT_CHECK_ENABLED(return);

	if (!number)
		number = "";

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"InitiateCall", &err, DBUS_TYPE_STRING, &number, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			if (strcmp(err.message, "NotConnected") == 0)
				ret = BLUETOOTH_ERROR_NOT_CONNECTED;
			else if (strcmp(err.message, "Operation not allowed") == 0)
				ret = BLUETOOTH_ERROR_IN_PROGRESS;
			else
				ret = BLUETOOTH_ERROR_INTERNAL;
			dbus_error_free(&err);
		}
		return ret;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_voice_recognition(unsigned int status)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"VoiceRecognition", &err, DBUS_TYPE_INT32, &status, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_audio_disconnect(void)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"ScoDisconnect", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_set_speaker_gain(unsigned int speaker_gain)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"SpeakerGain", &err, DBUS_TYPE_UINT32, &speaker_gain, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_send_dtmf(char *dtmf)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"SendDtmf", &err, DBUS_TYPE_STRING, &dtmf, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_send_xsat_cmd(int app_id, char *xsat_cmd)
{
	DBusMessage *reply;
	DBusError err;
	char buffer[200] = {0,};
	char *ptr = buffer;

	BT_CHECK_ENABLED(return);

	strcpy(buffer, "AT+XSAT=");
	snprintf(buffer + strlen(buffer), sizeof(buffer), "%d,", app_id);
	strncat(buffer, xsat_cmd, (sizeof(buffer) - 1) - strlen(buffer));
	BT_DBG("Xsat cmd received = %s", buffer);
	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
					"SendAtCmd", &err, DBUS_TYPE_STRING,
						&ptr, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_release_and_accept(void)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"ReleaseAndAccept", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_swap_call(void)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"CallSwap", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_release_all_call(void)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"ReleaseAllCall", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_join_call(void)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"JoinCall", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_get_call_list(void *call_list,
								bt_hf_call_status_info_t **call_status)
{
	int i;
	GList *list = call_list;
	int call_count;
	bt_hf_call_status_info_t * call_info;

	BT_CHECK_ENABLED(return);
	retv_if(list == NULL, BLUETOOTH_ERROR_INVALID_PARAM);
	retv_if(call_status == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	call_count = g_list_length(list);

	BT_DBG(" call_count = [%d]", call_count);

	for (i = 0; i < call_count; i++) {
		call_info = g_list_nth_data(list, i);
		BT_DBG(" direction = [%d]", call_info->direction);
		BT_DBG(" status = [%d]", call_info->status);
		BT_DBG(" mpart = [%d]", call_info->mpart);
		BT_DBG(" number = [%s]", call_info->number);
		BT_DBG(" idx = [%d]", call_info->idx);
		call_status[i] = call_info;
	}

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_free_call_list(bt_hf_call_list_s *call_list)
{
	bt_hf_call_list_s *handle;
	bt_hf_call_status_info_t *call_status;

	retv_if(call_list == NULL, BLUETOOTH_ERROR_INVALID_PARAM);

	handle = (bt_hf_call_list_s *)call_list;
	do  {
		call_status = (bt_hf_call_status_info_t *)g_list_nth_data(
							handle->list, 0);
		if (call_status == NULL)
			break;
		handle->list = g_list_remove(handle->list, call_status);
		g_free(call_status->number);
		g_free(call_status);
	} while (1);
	g_free(handle);
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_request_call_list(
					bt_hf_call_list_s **call_list)
{
	DBusMessage *reply;
	DBusError err;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"RequestCallList", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("dbus Error or call list is null\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		*call_list = NULL;
		return BLUETOOTH_ERROR_INTERNAL;
	}
	__bt_hf_agent_read_call_list(reply, call_list);

	dbus_message_unref(reply);
	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_get_codec(unsigned int *codec_id)
{
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter iter;
	int32_t current_codec;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"GetCurrentCodec", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_get_basic(&iter, &current_codec);
	*codec_id = current_codec;
	BT_DBG(" Codec ID is : %d", *codec_id);

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_get_audio_connected(unsigned int *audio_connected)
{
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter iter;
	int32_t sco_audio_connected_from_bt_agent;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"GetAudioConnected", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_get_basic(&iter, &sco_audio_connected_from_bt_agent);
	*audio_connected = sco_audio_connected_from_bt_agent;

	if (*audio_connected == BLUETOOTH_HF_AUDIO_CONNECTED) {
		BT_DBG("SCO Audio is Connected");
	} else {
		BT_DBG("SCO Audio is Disconnected");
	}

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_hf_is_hf_connected(gboolean *hf_connected)
{
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter iter;
	gboolean hf_connected_from_bt_agent;

	BT_CHECK_ENABLED(return);

	reply = __bt_hf_agent_dbus_send(BT_HF_OBJECT_PATH, BT_HF_INTERFACE,
			"IsHfConnected", &err, DBUS_TYPE_INVALID);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}
	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_get_basic(&iter, &hf_connected_from_bt_agent);
	*hf_connected = hf_connected_from_bt_agent;

	BT_DBG("%s", *hf_connected ? "Connected":"Disconnected");

	dbus_message_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

