/*
 * Bluetooth-telephony
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

/**
 * @file       bluetooth-telephony-test.c
 * @brief      This is the source file for bluetooth telephony test suite.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <pthread.h>

#include "bluetooth-audio-api.h"
#include "bluetooth-api.h"

#define PRT(format, args...) printf("%s:%d() "format, \
			__FUNCTION__, __LINE__, ##args)
#define TC_PRT(format, args...) PRT(format"\n", ##args)

GMainLoop *main_loop = NULL;
static int timeout_status = 0;
#define DEFAULT_CALL_ID 1
/*Change this number with Testing SIM*/
#define TEST_NUMBER "9980785507"
#define BLUETOOTH_HF_SPEAKER_GAIN 2

typedef struct {
	const char *tc_name;
	int tc_code;
} tc_table_t;

tc_table_t tc_table[] = {
	/*HF Application*/
	{"bluetooth_hf_init", 1},
	{"bluetooth_hf_deinit", 2},
	{"Answer Call", 3},
	{"Terminate Call", 4},
	{"Initiate Call", 5},
	{"Last number Redial ", 6},
	{"(PTS) Connect last bonded device", 7},
	{"Disconnect", 8},
	{"(PTS) Voice Recognition Enable", 9},
	{"Voice RecognitionDisable", 10},
	{"SCO disconnect", 11},
	{"Speaker gain", 12},
	{"Dual Tone mulitple frequency", 13},
	{"Send AT+XSAT=appid command", 14},
	{"Release All Call(CHLD=0)", 15},
	{"Release and Accept(CHLD=1)", 16},
	{"Swap call (CHLD=2)", 17},
	{"Join Call (CHLD=3)", 18},
	{"(PTS) Initiate Codec based SCO", 19},
	{"(PTS) Unbond all devices", 20},
	{"Get Current Codec", 21},
	{"Get Call List", 22},
	{"Get Audio Connected Status", 23},
	{"Is Handsfree Connected?", 24},

	/* -----------------------------------------*/
	{"Finish", 0x00ff},
	{NULL, 0x0000},

};

#define tc_result(success, tc_index) \
	TC_PRT("Test case [%d - %s] %s", tc_table[tc_index].tc_code, \
			tc_table[tc_index].tc_name, \
			((success == TC_PASS) ? "Success" : "Failed"));

void tc_usage_print(void)
{
	int i = 0;

	while (tc_table[i].tc_name) {
		if (tc_table[i].tc_code != 0x00ff) {
			TC_PRT("Key %d : usage %s", tc_table[i].tc_code,
							tc_table[i].tc_name);
		} else {
			TC_PRT("Key %d : usage %s\n\n", 0x00ff,
							tc_table[i].tc_name);
		}

		i++;
	}
}
void bt_event_callback(int event, bluetooth_event_param_t *param, void *user_data)
{
	GMainLoop *main_loop = (GMainLoop*) user_data;

	switch(event)
	{
		// Code for each event
		default:
			break;
	}
}

void hf_event_handler(int event, void *data, void *user_data)
{
	bt_hf_event_param_t *hf_event;

	if (data == NULL)
		return;
	hf_event = data;

	TC_PRT("HF event : [0x%04x]", event);

	switch (event) {
	case BLUETOOTH_EVENT_HF_CONNECTED:
		TC_PRT("BLUETOOTH_EVENT_HF_CONNECTED");
		break;

	case BLUETOOTH_EVENT_HF_DISCONNECTED:
		TC_PRT("BLUETOOTH_EVENT_HF_DISCONNECTED");
		break;

	case BLUETOOTH_EVENT_HF_AUDIO_CONNECTED:
		TC_PRT("BLUETOOTH_EVENT_HF_AUDIO_CONNECTED");
		break;

	case BLUETOOTH_EVENT_HF_AUDIO_DISCONNECTED:
		TC_PRT("BLUETOOTH_EVENT_HF_AUDIO_DISCONNECTED");
		break;

	case BLUETOOTH_EVENT_HF_RING_INDICATOR:
		TC_PRT("BLUETOOTH_EVENT_HF_RING_INDICATOR");
		if (hf_event->param_data)
			TC_PRT("Phone number %s", hf_event->param_data);
		break;

	case BLUETOOTH_EVENT_HF_CALL_WAITING:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_WAITING");
		if (hf_event->param_data)
			TC_PRT("Waiting Phone number %s", hf_event->param_data);
		break;

	case BLUETOOTH_EVENT_HF_CALL_TERMINATED:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_TERMINATED");
		break;

	case BLUETOOTH_EVENT_HF_CALL_STARTED:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_STARTED");
		break;

	case BLUETOOTH_EVENT_HF_CALL_ENDED:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_ENDED");
		break;

	case BLUETOOTH_EVENT_HF_CALL_UNHOLD:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_UNHOLD");
		break;

	case BLUETOOTH_EVENT_HF_CALL_SWAPPED:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_SWAPPED");
		break;

	case BLUETOOTH_EVENT_HF_CALL_ON_HOLD:
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_ON_HOLD");
		break;

	case BLUETOOTH_EVENT_HF_CALL_STATUS:
	{
		TC_PRT("BLUETOOTH_EVENT_HF_CALL_STATUS");
		int i;
		bt_hf_call_list_s * call_list = hf_event->param_data;
		bt_hf_call_status_info_t **call_info;
		TC_PRT("call_list length : %d ", call_list->count);
		call_info = g_malloc0(sizeof(bt_hf_call_status_info_t *) *
						call_list->count);
		bluetooth_hf_get_call_list(call_list->list, call_info);

		for (i= 0; i < call_list->count; i++) {
			TC_PRT("Phone Number : %s ", call_info[i]->number);
			TC_PRT("Direction (in -1, out 0 : %d ", call_info[i]->direction);
			TC_PRT("Call status : %d ", call_info[i]->status);
			TC_PRT("MultyParty : %d ", call_info[i]->mpart);
			TC_PRT("Call ID : %d ", call_info[i]->idx);
		}
		g_free(call_info);
		break;
	}
	case BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_ENABLED:
		TC_PRT("BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_ENABLED");
		break;

	case BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_DISABLED:
		TC_PRT("BLUETOOTH_EVENT_HF_VOICE_RECOGNITION_DISABLED");
		break;

	case BLUETOOTH_EVENT_HF_VOLUME_SPEAKER:
	{
		unsigned int *value;
		value = hf_event->param_data;
		TC_PRT("BLUETOOTH_EVENT_HF_VOLUME_SPEAKER - value = %d", *value);
		break;
	}
	case BLUETOOTH_EVENT_HF_VENDOR_DEP_CMD:
	{
		bluetooth_vendor_dep_at_cmd_t *cmd = hf_event->param_data;
		TC_PRT("BLUETOOTH_EVENT_HF_VENDOR_DEP_CMD - appid = %d, msg = %s",
			cmd->app_id, cmd->message);
		break;
	}

	default:
		break;
	}
}

static int  __bt_unbond_all_bonded_devices(void)
{
	int ret;
	int i;
	bluetooth_device_info_t *ptr;
	char *string;

	GPtrArray *dev_list = NULL;
	dev_list = g_ptr_array_new();
	TC_PRT("g pointer arrary count : [%d]", dev_list->len);

	ret = bluetooth_get_bonded_device_list(&dev_list);
	if (ret < 0) {
		TC_PRT("failed bluetooth_get_bonded_device_list");
		g_ptr_array_free(dev_list, TRUE);
		return 1;
	}
	TC_PRT("g pointer arrary count : [%d]", dev_list->len);

	if (dev_list->len == 0) {
		TC_PRT("No paired device found");
		g_ptr_array_free(dev_list, TRUE);
		return 1;
	}

	for (i = 0; i < dev_list->len; i++) {
		bluetooth_device_address_t device_address;
		ptr = g_ptr_array_index(dev_list, i);
		if (ptr == NULL)
			continue;
		TC_PRT("[%d] Unbond %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", i + 1,
			ptr->device_address.addr[0], ptr->device_address.addr[1],
			ptr->device_address.addr[2], ptr->device_address.addr[3],
			ptr->device_address.addr[4], ptr->device_address.addr[5]);
			bluetooth_unbond_device(&ptr->device_address);

	}
	g_ptr_array_foreach(dev_list, (GFunc)g_free, NULL);
	g_ptr_array_free(dev_list, TRUE);
	return 0;

}
static int  __bt_get_last_bonded_device(bluetooth_device_address_t *device_address)
{
	int ret;
	int i;
	bluetooth_device_info_t *ptr;

	GPtrArray *dev_list = NULL;
	dev_list = g_ptr_array_new();
	TC_PRT("g pointer arrary count : [%d]", dev_list->len);

	ret = bluetooth_get_bonded_device_list(&dev_list);
	if (ret < 0) {
		TC_PRT("failed bluetooth_get_bonded_device_list");
		g_ptr_array_free(dev_list, TRUE);
		return 1;
	}
	TC_PRT("g pointer arrary count : [%d]", dev_list->len);

	if (dev_list->len == 0) {
		TC_PRT("No paired device found");
		g_ptr_array_free(dev_list, TRUE);
		return 1;
	}

	for (i = 0; i < dev_list->len; i++) {
		ptr = g_ptr_array_index(dev_list, i);
		if (ptr == NULL)
			continue;
		TC_PRT("[%d] %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", i + 1,
			ptr->device_address.addr[0], ptr->device_address.addr[1],
			ptr->device_address.addr[2], ptr->device_address.addr[3],
			ptr->device_address.addr[4], ptr->device_address.addr[5]);
		memcpy(device_address->addr, ptr->device_address.addr,
				sizeof(bluetooth_device_address_t));
	}
	g_ptr_array_foreach(dev_list, (GFunc)g_free, NULL);
	g_ptr_array_free(dev_list, TRUE);
	return 0;

}

int test_input_callback(void *data)
{
	int ret;
	int test_id = (int)data;

	switch (test_id) {
		case 0x00ff:
			TC_PRT("Finished");
			g_main_loop_quit(main_loop);
			break;

		case 1:
			bluetooth_hf_init(hf_event_handler, NULL);
			break;
		case 2:
			bluetooth_hf_deinit();
			break;
		case 3:
			bluetooth_hf_answer_call();
			break;
		case 4:
			bluetooth_hf_terminate_call();
			break;
		case 5:
			ret = bluetooth_hf_initiate_call(TEST_NUMBER);
			TC_PRT("ret = %d", ret);
			break;
		case 6:
			bluetooth_hf_initiate_call(NULL);
			break;
		case 7:
		{	bluetooth_device_address_t device_address = { {0} };
			ret = __bt_get_last_bonded_device(&device_address);
			if (ret != 0) {
				TC_PRT("Error in getting last bonded device.....");
				return FALSE;
			}

			bluetooth_hf_connect(&device_address);
			break;
		}
		case 8:
		{
			bluetooth_device_address_t device_address = { {0} };
			ret = __bt_get_last_bonded_device(&device_address);
			if (ret != 0) {
				TC_PRT("Error in getting last bonded device.....");
				return FALSE;
			}

			bluetooth_hf_disconnect(&device_address);
			break;
		}
		case 9:
			bluetooth_hf_voice_recognition(1);
			break;

		case 10:
			bluetooth_hf_voice_recognition(0);
			break;
		case 11:
			bluetooth_hf_audio_disconnect();
			break;
		case 12:
			bluetooth_hf_set_speaker_gain(BLUETOOTH_HF_SPEAKER_GAIN);
			break;
		case 13:
			bluetooth_hf_send_dtmf("1");
			break;
		case 14:
			/* get the Call Time from AG for DC lauch */
			bluetooth_hf_send_xsat_cmd(11, "Q_CT,1,01025561613");
			break;
		case 15:
			bluetooth_hf_release_all_call();
			break;
		case 16:
			bluetooth_hf_release_and_accept();
			break;
		case 17:
			bluetooth_hf_swap_call();
			break;
		case 18:
			bluetooth_hf_join_call();
			break;
		case 19:
			system("dbus-send --system --print-reply --dest=org.bluez.hf_agent  /org/bluez/handsfree_agent org.tizen.HfApp.SendAtCmd string:AT+BCC");
			break;
		case 20:
		{
			ret = bluetooth_register_callback(bt_event_callback, NULL);
			ret = __bt_unbond_all_bonded_devices();
			if (ret != 0) {
				TC_PRT("Error in getting last bonded device.....");
				return FALSE;
			}

			break;
		}
		case 21:
		{
			unsigned int current_codec;
			bluetooth_hf_get_codec(&current_codec);
			if (current_codec == BLUETOOTH_CODEC_ID_CVSD)
				TC_PRT("current_codec is CVSD");
			else
				TC_PRT("current_codec is. MSBC");
			break;
		}
		case 22:
		{
			int i;
			bt_hf_call_list_s * call_list = NULL;
			bt_hf_call_status_info_t **call_info = NULL;
			bluetooth_hf_request_call_list(&call_list);
			if(call_list == NULL) {
				TC_PRT("call_list is NULL");
				break;
			}
			TC_PRT("call_list length : %d ", call_list->count);
			call_info = g_malloc0(sizeof(bt_hf_call_status_info_t *) *
						call_list->count);
			bluetooth_hf_get_call_list(call_list->list, call_info);

			for (i= 0; i < call_list->count; i++) {
				TC_PRT("Phone Number : %s ", call_info[i]->number);
				TC_PRT("Direction (in -1, out 0 : %d ", call_info[i]->direction);
				TC_PRT("Call status : %d ", call_info[i]->status);
				TC_PRT("MultyParty : %d ", call_info[i]->mpart);
				TC_PRT("Call ID : %d ", call_info[i]->idx);
			}
			g_free(call_info);
			bluetooth_hf_free_call_list(call_list);
			break;
		}
		case 23:
		{
			unsigned int sco_audio_connected;
			bluetooth_hf_get_audio_connected(&sco_audio_connected);
			if (sco_audio_connected == BLUETOOTH_HF_AUDIO_CONNECTED)
				TC_PRT("SCO Audio is connected");
			else
				TC_PRT("SCO Audio is disconnected");
			break;
		}
		case 24:
		{
			gboolean hf_connected;
			bluetooth_hf_is_hf_connected(&hf_connected);
			if (hf_connected == BLUETOOTH_HF_AUDIO_CONNECTED)
				TC_PRT("HF is connected");
			else
				TC_PRT("HF is disconnected");
			break;
		}
		default:
			break;
	}

	return 0;
}

void startup()
{
	TC_PRT("bluetooth framework TC startup");

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	dbus_g_thread_init();

	g_type_init();
	main_loop = g_main_loop_new(NULL, FALSE);
}

void cleanup()
{
	TC_PRT("bluetooth framework TC cleanup");
	if (main_loop != NULL) {
		g_main_loop_unref(main_loop);
	}
}

int timeout_callback(void *data)
{
	TC_PRT("timeout callback");
	timeout_status = -1;

	g_main_loop_quit(main_loop);

	return FALSE;
}

static gboolean key_event_cb(GIOChannel *chan, GIOCondition cond ,
							gpointer data)
{
	char buf[10] = {0};

	unsigned int len = 0;
	int test_id;
	memset(buf, 0, sizeof(buf));

	if (g_io_channel_read(chan, buf, sizeof(buf), &len) !=
					G_IO_ERROR_NONE) {

		printf("IO Channel read error");
		return FALSE;

	}
	printf("%s\n", buf);
	tc_usage_print();

	test_id = atoi(buf);

	if (test_id)
		g_idle_add(test_input_callback, (void *)test_id);

	return TRUE;
}

int main()
{
	startup();

	GIOChannel *key_io;
	key_io = g_io_channel_unix_new(fileno(stdin));

	g_io_add_watch(key_io, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			key_event_cb, NULL);
	g_io_channel_unref(key_io);


	g_main_loop_run(main_loop);

	cleanup();
	return 0;
}
