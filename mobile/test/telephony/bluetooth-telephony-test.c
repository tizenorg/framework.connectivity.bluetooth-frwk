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

/**
 * @file       bluetooth-telephony-test.c
 * @brief      This is the source file for bluetooth telephony test suite.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <pthread.h>

#include "bluetooth-telephony-api.h"


#define PRT(format, args...) printf("%s:%d() "format, \
			__FUNCTION__, __LINE__, ##args)
#define TC_PRT(format, args...) PRT(format"\n", ##args)

GMainLoop *main_loop = NULL;
static int timeout_status = 0;
#define DEFAULT_CALL_ID 1
/*Change this number with Testing SIM*/
#define TEST_NUMBER "9986008917"

typedef struct {
	const char *tc_name;
	int tc_code;
} tc_table_t;

tc_table_t tc_table[] = {
	/*Telephony Application*/
	{"bluetooth_telephony_init", 70},
	{"bluetooth_telephony_deinit", 71},
	{"Indicate Outgoing call", 72},
	{"Indicate Incoming call", 73},
	{"Speaker to Headphone", 74},
	{"Headphone to Speaker ", 75},
	{"Call End/Release", 76},
	{"Call Hold", 77},
	{"bluetooth_telephony_call_remote_ringing", 78},
	{"Call Swap", 79},
	{"Call Reject", 80},
	{"Call Answer", 81},
	{"Is SCO channel connected", 82},
	{"Voice Recognition Start", 83},
	{"Voice Recognition Stop", 84},
	{"NREC Status", 85},
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

void telephony_event_handler(int event, void *data, void *user_data)
{
	telephony_event_param_t *bt_event;

	if (data == NULL)
		return;
	bt_event = data;

	TC_PRT("AG event : [0x%04x]", event);

	switch (event) {
	case BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL");
		bluetooth_telephony_call_answered(DEFAULT_CALL_ID, TRUE);
		break;

	case BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL");
		bluetooth_telephony_call_end(DEFAULT_CALL_ID);
		break;

	case BLUETOOTH_EVENT_TELEPHONY_REJECT_CALL:
		bluetooth_telephony_call_end(DEFAULT_CALL_ID);
		break;

	case BLUETOOTH_EVENT_TELEPHONY_CHLD_0_RELEASE_ALL_HELD_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_CHLD_0_RELEASE_ALL_HELD_CALL");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_CHLD_1_RELEASE_ALL_ACTIVE_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_CHLD_1_RELEASE_ALL_ACTIVE_CALL");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_CHLD_3_MERGE_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_CHLD_3_MERGE_CALL");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_CHLD_2_ACTIVE_HELD_CALL:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_CHLD_2_ACTIVE_HELD_CALL");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_CHLD_4_EXPLICIT_CALL_TRANSFER:
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_CHLD_4_EXPLICIT_CALL_TRANSFER");
		break;

	case BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED: {
		gboolean *nrec;
		TC_PRT("BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED");
		nrec = bt_event->param_data;
		TC_PRT("NREC status = [%d]", *nrec);
		break;
	}

	default:
		break;
	}
}

int test_input_callback(void *data)
{
	int test_id = (int)data;

	switch (test_id) {
		case 0x00ff:
			TC_PRT("Finished");
			g_main_loop_quit(main_loop);
			break;

		case 70:
			bluetooth_telephony_init(telephony_event_handler, NULL);
			break;
		case 71:
			bluetooth_telephony_deinit();
			break;

		case 72:
			bluetooth_telephony_indicate_outgoing_call(
					TEST_NUMBER, DEFAULT_CALL_ID, TRUE);
			break;
		case 73:
			bluetooth_telephony_indicate_incoming_call(
					TEST_NUMBER, TRUE);
			break;
		case 74:
			bluetooth_telephony_audio_open();
			break;
		case 75:
			bluetooth_telephony_audio_close();
			break;
		case 76:
			bluetooth_telephony_call_end(DEFAULT_CALL_ID);
			break;
		case 77:
			bluetooth_telephony_call_held(DEFAULT_CALL_ID);
			break;
		case 78:
			bluetooth_telephony_call_remote_ringing(
							DEFAULT_CALL_ID);
			break;
		case 79:
			TC_PRT("bluetooth_telephony_call_swapped  \n");
			break;
		case 80:
			bluetooth_telephony_call_answered(
							DEFAULT_CALL_ID, FALSE);
			break;
		case 81:
			bluetooth_telephony_call_answered(
							DEFAULT_CALL_ID, TRUE);
			break;

		case 82: {
			int state;

			state = bluetooth_telephony_is_sco_connected();

			TC_PRT("State = %d \n", state);
			break;
		}

		case 83: {
			int ret = 0;

			TC_PRT("**********************\n");
			TC_PRT("           PLEASE SPEAK          \n");
			TC_PRT("**********************\n");

			ret = bluetooth_telephony_start_voice_recognition();

			if (ret == BLUETOOTH_TELEPHONY_ERROR_NONE) {
				TC_PRT("No error\n");
				bluetooth_telephony_audio_open();
			}
			break;
		}

		case 84: {
			TC_PRT("Rcognition finished \n");
			bluetooth_telephony_audio_close();
			bluetooth_telephony_stop_voice_recognition();
			break;
		}

		case 85: {
			int ret;
			gboolean status = FALSE;

			ret = bluetooth_telephony_is_nrec_enabled(&status);

			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
				TC_PRT("Error getting NREC Status\n");

			TC_PRT("NREC status = %d\n", status);
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
