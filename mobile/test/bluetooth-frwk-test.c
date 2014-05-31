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
 * @file       bluetooth-frwk-test.c
 * @brief      This is the source file for bluetooth framework test suite.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <pthread.h>

#include "bluetooth-api.h"
#include "bluetooth-hid-api.h"
#include "bluetooth-audio-api.h"


bluetooth_device_address_t searched_device = {{0}};

#define TC_TIMEOUT	30000

#define BT_DEFAULT_DEV_NAME "SLP-BT-TEST-TARGET"
#define DISCOVER_TIMEOUT 20
#define DISCOVER_CANCEL_INTERVAL 3

#define PRT(format, args...) printf("%s:%d() "format, __FUNCTION__, __LINE__, ##args)
#define TC_PRT(format, args...) PRT(format"\n", ##args)

#define TC_PASS 1
#define TC_FAIL 0
int client_fd = 0;
int server_fd = 0;
int g_ret_client_fd1 = -1, g_ret_client_fd2 = -1;
const char *g_hdp_app_handle1 =  NULL;
const char *g_hdp_app_handle2 =  NULL;
int selection;
int hdp_manual_mode =  1;
const char * rfcomm_test_uuid_spp ="00001101-0000-1000-8000-00805F9B34FB";
const char * rfcomm_test_uuid_dun = "00001103-0000-1000-8000-00805F9B34FB";
const char * rfcomm_test_uuid_custom ="26b2831b-2c2d-4f9c-914a-c0ab142351b7";


GMainLoop *main_loop = NULL;
static int timeout_status = 0;

int current_transfer_id = 0;

typedef struct {
	bluetooth_device_address_t address;
	bt_oob_data_t oob_data;
} oob_data_t;

oob_data_t g_local_oob_data;
oob_data_t g_remote_oob_data;


typedef struct
{
	const char *tc_name;
	int tc_code;
} tc_table_t;

void bt_event_callback(int event, bluetooth_event_param_t* param, void *user_data);
void bt_hid_event_callback(int event, hid_event_param_t* param, void *user_data);
void bt_audio_event_callback(int event, bt_audio_event_param_t* param, void *user_data);


tc_table_t tc_table[] =
{
	{"bluetooth_register_callback"		, 1},
	{"bluetooth_unregister_callback"	, 2},
	{"bluetooth_enable_adapter"		, 3},
	{"bluetooth_disable_adapter"		, 4},
	{"bluetooth_check_adapter"		, 5},
	{"bluetooth_get_local_address"		, 6},
	{"bluetooth_get_local_name"		, 7},
	{"bluetooth_set_local_name"		, 8},
	{"bluetooth_is_service_used"		, 9},
	{"bluetooth_get_discoverable_mode"	, 10},
	{"bluetooth_set_discoverable_mode(CONNECTABLE)"			, 11},
	{"bluetooth_set_discoverable_mode(GENERAL_DISCOVERABLE)"	, 12},
	{"bluetooth_set_discoverable_mode(TIME_LIMITED_DISCOVERABLE)"	, 13},
	{"bluetooth_start_discovery"		, 14},
	{"bluetooth_cancel_discovery"		, 15},
	{"bluetooth_is_discovering"		, 16},
	{"bluetooth_get_bonded_device_list"	, 17},
	{"bluetooth_bond_device"		, 18},
	{"bluetooth_cancel_bonding"		, 19},
	{"bluetooth_unbond_device"	, 20},
	{"bluetooth_get_bonded_device"	, 21},
	{"bluetooth_set_alias"	, 22},
	{"bluetooth_authorize_device (TRUE)"	, 23},
	{"bluetooth_authorize_device (FALSE)"	, 24},
	{"bluetooth_search_service"	, 25},
	{"bluetooth_cancel_service_search"	, 26},
	{"bluetooth_is_device_connected"	, 27},

	{"bluetooth_audio_init"	, 29},
	{"bluetooth_audio_deinit" , 30},
	{"bluetooth_audio_connect"	, 31},
	{"bluetooth_audio_disconnect"	, 32},
	{"bluetooth_ag_connect"	, 33},
	{"bluetooth_ag_disconnect" , 34},
	{"bluetooth_av_connect"	, 35},
	{"bluetooth_av_disconnect"	, 36},
	{"bluetooth_ag_get_headset_volume" , 37},
	{"bluetooth_ag_set_speaker_gain" , 38},

	{"bluetooth_oob_read_local_data"	, 39},
	{"bluetooth_oob_add_remote_data" , 40},
	{"bluetooth_oob_remove_remote_data" , 41},

	{"bluetooth_opc_init"	, 42},
	{"bluetooth_opc_deinit" , 43},
	{"bluetooth_opc_push_files" , 44},
	{"bluetooth_opc_cancel_push" , 45},
	{"bluetooth_opc_session_is_exist" , 46},

	{"bluetooth_network_activate_server"	, 47},
	{"bluetooth_network_deactivate_server" , 48},
	{"bluetooth_network_connect" , 49},
	{"bluetooth_network_disconnect" , 50},

	{"bluetooth_obex_server_init"	, 51},
	{"bluetooth_obex_server_deinit"	, 52},
	{"bluetooth_obex_server_init_without_agent"	, 53},
	{"bluetooth_obex_server_deinit_without_agent"	, 54},
	{"bluetooth_obex_server_is_activated"	, 55},
	{"bluetooth_obex_server_accept_connection"	, 56},
	{"bluetooth_obex_server_reject_connection"	, 57},
	{"bluetooth_obex_server_accept_authorize"	, 58},
	{"bluetooth_obex_server_reject_authorize"	, 59},
	{"bluetooth_obex_server_set_destination_path"	, 60},
	{"bluetooth_obex_server_set_root"		, 61},
	{"bluetooth_obex_server_cancel_transfer"	, 62},
	{"bluetooth_obex_server_cancel_all_transfers"	, 63},

	{"bluetooth_hid_init"	, 65},
	{"bluetooth_hid_deinit"	, 66},
	{"bluetooth_hid_connect"	, 67},
	{"bluetooth_hid_disconnect"	, 68},

	{"bluetooth_rfcomm_connect"	, 70},
	{"bluetooth_rfcomm_disconnect (cancel)"	, 71},
	{"bluetooth_rfcomm_disconnect"	, 72},
	{"bluetooth_rfcomm_write"	, 73},
	{"bluetooth_rfcomm_is_client_connected"	, 74},

	{"bluetooth_rfcomm_create_socket"	, 80},
	{"bluetooth_rfcomm_create_socket (Custom UUID)"	, 81},
	{"bluetooth_rfcomm_remove_socket"	, 82},
	{"bluetooth_rfcomm_listen_and_accept"	, 83},
	{"bluetooth_rfcomm_listen (OSP)"	, 84},
	{"bluetooth_rfcomm_server_disconnect"	, 85},
	{"bluetooth_rfcomm_is_server_uuid_available"	, 86},
	{"bluetooth_rfcomm_accept_connection"	, 87},
	{"bluetooth_rfcomm_reject_connection"	, 88},


#if 0
	{"bluetooth_rfcomm_is_server_uuid_available"	, 26},

	{"bluetooth_hdp_activate"	, 30},
	{"bluetooth_hdp_deactivate"	, 31},
	{"bluetooth_hdp_connect"	, 32},
	{"bluetooth_hdp_disconnect"	, 33},
	{"bluetooth_hdp_send_data"	, 34},

	{"bluetooth_opc_init"		, 35},
	{"bluetooth_opc_push_file"	, 36},
	{"bluetooth_opc_cancel_push"	, 37},
	{"bluetooth_opc_deinit"		, 38},
	{"bluetooth_obex_server_init"	, 39},
	{"bluetooth_obex_server_deinit"	, 40},
	{"bluetooth_obex_server_accept_authorize"	, 41},
	{"bluetooth_obex_server_reject_authorize"	, 42},
	{"bluetooth_is_supported"	, 43},
	{"bluetooth_opc_session_is_exist"	, 46},
	{"bluetooth_obex_server_is_activated"	, 47},
	{"bluetooth_obex_server_cancel_transfer"	, 48},

	{"bluetooth_oob_read_local_data"	, 50},
	{"bluetooth_oob_add_remote_data"	, 51},
	{"bluetooth_oob_remove_remote_data"	, 52},

	{"bluetooth_network_activate_server"	, 60},
	{"bluetooth_network_deactivate_server"	, 61},
	{"bluetooth_network_connect"	, 62},
	{"bluetooth_network_disconnect"	, 63},

	{"bluetooth_gatt_discover_primary_services", 64},
	{"bluetooth_gatt_discover_service_characteristics",	65},
	{"bluetooth_gatt_get_service_property",		66},
	{"bluetooth_gatt_get_characteristics_value",	67},
	{"bluetooth_gatt_set_characteristics_value",	68},
#endif
	/* -----------------------------------------*/
	{"Finish"							, 0x00ff},
	{NULL								, 0x0000},

};

#define tc_result(success, tc_index) \
	TC_PRT("Test case [%d - %s] %s", tc_table[tc_index].tc_code, tc_table[tc_index].tc_name, ((success == TC_PASS)?"Success":"Failed"));

bluetooth_device_info_t bond_dev;
int is_bond_device = FALSE;

void tc_usage_print(void)
{
	int i = 0;

	while (tc_table[i].tc_name) {
		if (tc_table[i].tc_code != 0x00ff) {
			TC_PRT("Key %d : usage %s", tc_table[i].tc_code, tc_table[i].tc_name);
		} else {
			TC_PRT("Key %d : usage %s\n\n", 0x00ff, tc_table[i].tc_name);
		}

		i++;
	}
}

int test_input_callback(void *data)
{
	int ret = 0;
	int test_id = (int)data;

	switch (test_id)
	{
		case 0x00ff:
			TC_PRT("Finished");
			g_main_loop_quit(main_loop);
			break;

		case 1:
			TC_PRT("TC : %s", tc_table[0].tc_name);
			bluetooth_register_callback(bt_event_callback, NULL);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[0].tc_name, ret);
				tc_result(TC_FAIL, 1);
			}
			break;

		case 2:
			TC_PRT("TC : %s", tc_table[1].tc_name);
			bluetooth_unregister_callback();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[1].tc_name, ret);
				tc_result(TC_FAIL, 1);
			}
			break;

		case 3:
			TC_PRT("TC : %s", tc_table[2].tc_name);
			ret = bluetooth_enable_adapter();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[2].tc_name, ret);
				tc_result(TC_FAIL, 1);
			}
			break;

		case 4:
			TC_PRT("TC : %s", tc_table[3].tc_name);
			ret = bluetooth_disable_adapter();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[3].tc_name, ret);
				tc_result(TC_FAIL, 2);
			}
			break;

		case 5:
		{
			TC_PRT("TC : %s", tc_table[4].tc_name);
			ret = bluetooth_check_adapter();
			TC_PRT("state: %d", ret);
			break;
		}

		case 6:
		{
			bluetooth_device_address_t address = {{0}};

			TC_PRT("TC : %s", tc_table[5].tc_name);
			ret = bluetooth_get_local_address(&address);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[5].tc_name, ret);
			} else {
				TC_PRT("dev [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", \
					address.addr[0], address.addr[1], address.addr[2], \
					address.addr[3], address.addr[4], address.addr[5]);
			}
			break;
		}

		case 7:
		{
			bluetooth_device_name_t local_name = {{0}};

			TC_PRT("TC : %s", tc_table[6].tc_name);
			ret = bluetooth_get_local_name(&local_name);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[6].tc_name, ret);
			} else {
				TC_PRT("name: %s", local_name.name);
			}
			break;
		}

		case 8:
		{
			bluetooth_device_name_t local_name = {{0}};
			snprintf(local_name.name, sizeof(local_name.name),
					"bt-frwk-pid-%d", getpid());

			TC_PRT("TC : %s", tc_table[7].tc_name);
			ret = bluetooth_set_local_name(&local_name);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[7].tc_name, ret);
			}
			break;
		}

		case 9:
		{
			gboolean used = FALSE;			

			TC_PRT("TC : %s", tc_table[8].tc_name);
			ret = bluetooth_is_service_used(rfcomm_test_uuid_spp, &used);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[8].tc_name, ret);
			} else {
				TC_PRT("used: %d", used);
			}
			break;
		}

	        case 10:
		{
			bluetooth_discoverable_mode_t mode;
			TC_PRT("TC : %s", tc_table[9].tc_name);
			ret = bluetooth_get_discoverable_mode(&mode);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[9].tc_name, ret);
			}
			else
			{
				TC_PRT("BT Get Discoverable mode [%d]", mode);
			}
			break;
		}

	        case 11:
		{
			bluetooth_discoverable_mode_t mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;
			TC_PRT("TC : %s", tc_table[10].tc_name);
			ret = bluetooth_set_discoverable_mode(mode, 0);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[10].tc_name, ret);
			}
			else
			{
				TC_PRT("BT Set Discoverable mode [%d]", mode);
			}
			break;
		}

	        case 12:
		{
			bluetooth_discoverable_mode_t mode = BLUETOOTH_DISCOVERABLE_MODE_GENERAL_DISCOVERABLE;
			TC_PRT("TC : %s", tc_table[11].tc_name);
			ret = bluetooth_set_discoverable_mode(mode, 0);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[11].tc_name, ret);
			}
			else
			{
				TC_PRT("BT Set Discoverable mode [%d]", mode);
			}
			break;
		}

	        case 13:
		{
			bluetooth_discoverable_mode_t mode = BLUETOOTH_DISCOVERABLE_MODE_TIME_LIMITED_DISCOVERABLE;
			TC_PRT("TC : %s", tc_table[12].tc_name);
			ret = bluetooth_set_discoverable_mode(mode, 5);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[12].tc_name, ret);
			}
			else
			{
				TC_PRT("BT Set Discoverable mode [%d]", mode);
			}
			break;
		}

		case 14:
			TC_PRT("TC : %s", tc_table[13].tc_name);
			ret = bluetooth_start_discovery(0,0,0);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[13].tc_name, ret);
			}
			break;

		case 15:
			TC_PRT("TC : %s", tc_table[14].tc_name);
			ret = bluetooth_cancel_discovery();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[14].tc_name, ret);
			}
			break;

		case 16:
		{
			TC_PRT("TC : %s", tc_table[15].tc_name);
			ret = bluetooth_is_discovering();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[15].tc_name, ret);
			}
			else
			{
				TC_PRT("Discovering [%d]", ret);
			}
			break;
		}

		case 17: /*Get paired device list */
		{

			TC_PRT("TC : %s", tc_table[16].tc_name);

			GPtrArray *devinfo = NULL;
			devinfo = g_ptr_array_new();
			TC_PRT("g pointer arrary count : [%d]", devinfo->len);

			ret = bluetooth_get_bonded_device_list(&devinfo);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[16].tc_name, ret);
			}
			else
			{
				int i;
				bluetooth_device_info_t *ptr;
				TC_PRT("g pointer arrary count : [%d]", devinfo->len);

				for(i=0; i<devinfo->len;i++)
				{
					ptr = g_ptr_array_index(devinfo, i);
					if(ptr != NULL)
					{
						TC_PRT("Name [%s]", ptr->device_name.name);
						TC_PRT("Major Class [%d]", ptr->device_class.major_class);
						TC_PRT("Minor Class [%d]", ptr->device_class.minor_class);
						TC_PRT("Service Class [%d]", ptr->device_class.service_class);
						TC_PRT("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", ptr->device_address.addr[0], ptr->device_address.addr[1], ptr->device_address.addr[2], ptr->device_address.addr[3], ptr->device_address.addr[4], ptr->device_address.addr[5]);
						TC_PRT("\n");
					}
				}
			}
			g_ptr_array_free(devinfo, TRUE);

			break;
		}

		case 18:
		{
			/* Apple wireless keyboard */
			//bluetooth_device_address_t device_address={{0xE8,0x06,0x88,0x3B,0x18,0xBA}};
			//bluetooth_device_address_t device_address={{0x00,0x19,0x0E,0x01,0x61,0x17}}; /* DO-DH79-PYUN04 */
			//bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}}; /* DO-DH79-PYUN03 */
			//bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}}; /* MW600 */
			bluetooth_device_address_t device_address={{0x00,0x0D,0xFD,0x24,0x5E,0xFF}}; /* Motorola S9 */

			TC_PRT("TC : %s", tc_table[17].tc_name);

			TC_PRT("dev [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", \
				device_address.addr[0], device_address.addr[1], device_address.addr[2], \
				device_address.addr[3], device_address.addr[4], device_address.addr[5]);

			ret = bluetooth_bond_device(&device_address);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[17].tc_name, ret);
			}
			break;
		}


		case 19: /*Cancel bonding */
		{

			TC_PRT("TC : %s", tc_table[18].tc_name);
			ret = bluetooth_cancel_bonding();
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[18].tc_name, ret);
			}

			break;
		}

		case 20: /*unbonding */
		{
			bluetooth_device_address_t device_address={{0x00,0x19,0x0E,0x01,0x61,0x17}}; /* DO-DH79-PYUN04 */
			//bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}};

			TC_PRT("TC : %s", tc_table[19].tc_name);
			ret = bluetooth_unbond_device(&device_address);
			if (ret < 0)
			{
				TC_PRT("%s failed with [0x%04x]", tc_table[19].tc_name, ret);
			}

			break;
		}

		case 21: /*Get paired device */
		{
			bluetooth_device_info_t devinfo;
			bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}};

			memset(&devinfo, 0x00, sizeof(bluetooth_device_info_t));

			ret = bluetooth_get_bonded_device(&device_address, &devinfo);
			if (ret != BLUETOOTH_ERROR_NONE)
			{
				TC_PRT("bluetooth_get_bonded_device failed with [%d]",ret);
			}
			else
			{
				TC_PRT("Name [%s]", devinfo.device_name.name);
				TC_PRT("Major Class [%d]", devinfo.device_class.major_class);
				TC_PRT("Minor Class [%d]", devinfo.device_class.minor_class);
				TC_PRT("Service Class [%d]", devinfo.device_class.service_class);
				TC_PRT("%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", devinfo.device_address.addr[0], devinfo.device_address.addr[1], devinfo.device_address.addr[2], devinfo.device_address.addr[3], devinfo.device_address.addr[4], devinfo.device_address.addr[5]);
			}

			break;
		}

		case 22: /*set alias for bonded device */
		{
			bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}};

			TC_PRT("TC : %s", tc_table[21].tc_name);

			ret = bluetooth_set_alias(&device_address, "Renamed device");
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[21].tc_name, ret);
			break;
		}

		case 23:
		{
			bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}};

			TC_PRT("TC : %s", tc_table[22].tc_name);

			ret = bluetooth_authorize_device(&device_address, TRUE);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[22].tc_name, ret);
			break;
		}
		case 24:
		{
			bluetooth_device_address_t device_address={{0x00,0x16,0x38,0xC3,0x1F,0xD2}};

			TC_PRT("TC : %s", tc_table[23].tc_name);

			ret = bluetooth_authorize_device(&device_address, FALSE);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[23].tc_name, ret);
			break;
		}
		case 25:
		{
			bluetooth_device_address_t device_address={{0x00,0x19,0x0E,0x01,0x61,0x17}}; /* DO-DH79-PYUN04 */

			TC_PRT("TC : %s", tc_table[24].tc_name);

			ret = bluetooth_search_service(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[24].tc_name, ret);
			break;
		}
		case 26:
		{
			TC_PRT("TC : %s", tc_table[25].tc_name);

			ret = bluetooth_cancel_service_search();
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[24].tc_name, ret);
			break;
		}
		case 27:
		{
			gboolean connected = FALSE;
			bluetooth_device_address_t device_address={{0x00,0x1B,0x66,0x01,0x23,0x1C}}; /* Gennheiser PX210BT */

			TC_PRT("TC : %s", tc_table[26].tc_name);

			ret = bluetooth_is_device_connected(&device_address, BLUETOOTH_A2DP_SERVICE, &connected);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[24].tc_name, ret);

			TC_PRT("connected : %d", connected);
			break;
		}
		case 28:
		{
			TC_PRT("TC : %s", tc_table[27].tc_name);

			ret = bluetooth_reset_adapter();
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[27].tc_name, ret);
			break;
		}

		case 29:
		{
			TC_PRT("TC : %s", tc_table[28].tc_name);

			ret = bluetooth_audio_init(bt_audio_event_callback, NULL);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[28].tc_name, ret);
			break;
		}
		case 30:
		{
			TC_PRT("TC : %s", tc_table[29].tc_name);

			ret = bluetooth_audio_deinit();
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[29].tc_name, ret);
			break;
		}
		case 31:
		{
			/* MW600 */
			//bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};
			bluetooth_device_address_t device_address={{0x00,0x0D,0xFD,0x24,0x5E,0xFF}}; /* Motorola S9 */

			TC_PRT("TC : %s", tc_table[30].tc_name);

			ret = bluetooth_audio_connect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[30].tc_name, ret);
			break;
		}
		case 32:
		{
			/* MW600 */
			//bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};
			bluetooth_device_address_t device_address={{0x00,0x0D,0xFD,0x24,0x5E,0xFF}}; /* Motorola S9 */

			TC_PRT("TC : %s", tc_table[31].tc_name);

			ret = bluetooth_audio_disconnect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[31].tc_name, ret);
			break;
		}
		case 33:
		{
			/* MW600 */
			bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};

			TC_PRT("TC : %s", tc_table[32].tc_name);

			ret = bluetooth_ag_connect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[32].tc_name, ret);
			break;
		}
		case 34:
		{
			/* MW600 */
			bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};

			TC_PRT("TC : %s", tc_table[33].tc_name);

			ret = bluetooth_ag_disconnect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[33].tc_name, ret);
			break;
		}
		case 35:
		{
			/* MW600 */
			bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};

			TC_PRT("TC : %s", tc_table[34].tc_name);

			ret = bluetooth_av_connect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[34].tc_name, ret);
			break;
		}
		case 36:
		{
			/* MW600 */
			bluetooth_device_address_t device_address={{0x58,0x17,0x0C,0xEC,0x6A,0xF3}};

			TC_PRT("TC : %s", tc_table[35].tc_name);

			ret = bluetooth_av_disconnect(&device_address);
			if (ret < 0)
				TC_PRT("%s failed with [0x%04x]", tc_table[35].tc_name, ret);
			break;
		}
		case 37:
		{
			unsigned int volume = 0;

			ret = bluetooth_ag_get_headset_volume(&volume);
			if (ret < 0)
				TC_PRT("failed with [0x%04x]", ret);

			TC_PRT("volume: %d", volume);
			break;
		}
		case 38:
		{
			ret = bluetooth_ag_set_speaker_gain(10);
			if (ret < 0)
				TC_PRT("failed with [0x%04x]", ret);
			break;
		}

		case 39:
		{
			if (bluetooth_oob_read_local_data(&g_local_oob_data.oob_data))
				TC_PRT("ERROR in bluetooth_oob_read_local_data\n");
			else {
				TC_PRT("SUCESS in bluetooth_oob_read_local_data\n");
				TC_PRT("hash = [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
					"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]\n",
					g_local_oob_data.oob_data.hash[0],
					g_local_oob_data.oob_data.hash[1],
					g_local_oob_data.oob_data.hash[2],
					g_local_oob_data.oob_data.hash[3],
					g_local_oob_data.oob_data.hash[4],
					g_local_oob_data.oob_data.hash[5],
					g_local_oob_data.oob_data.hash[6],
					g_local_oob_data.oob_data.hash[7],
					g_local_oob_data.oob_data.hash[8],
					g_local_oob_data.oob_data.hash[9],
					g_local_oob_data.oob_data.hash[10],
					g_local_oob_data.oob_data.hash[11],
					g_local_oob_data.oob_data.hash[12],
					g_local_oob_data.oob_data.hash[13],
					g_local_oob_data.oob_data.hash[14],
					g_local_oob_data.oob_data.hash[15]);

				TC_PRT("randomizer = [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X"
					"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]\n",
					g_local_oob_data.oob_data.randomizer[0],
					g_local_oob_data.oob_data.randomizer[1],
					g_local_oob_data.oob_data.randomizer[2],
					g_local_oob_data.oob_data.randomizer[3],
					g_local_oob_data.oob_data.randomizer[4],
					g_local_oob_data.oob_data.randomizer[5],
					g_local_oob_data.oob_data.randomizer[6],
					g_local_oob_data.oob_data.randomizer[7],
					g_local_oob_data.oob_data.randomizer[8],
					g_local_oob_data.oob_data.randomizer[9],
					g_local_oob_data.oob_data.randomizer[10],
					g_local_oob_data.oob_data.randomizer[11],
					g_local_oob_data.oob_data.randomizer[12],
					g_local_oob_data.oob_data.randomizer[13],
					g_local_oob_data.oob_data.randomizer[14],
					g_local_oob_data.oob_data.randomizer[15]);

				TC_PRT("hash_len: %d\n", g_local_oob_data.oob_data.hash_len);
				TC_PRT("randomizer_len: %d\n", g_local_oob_data.oob_data.randomizer_len);
			}
			break;
		}
		case 40:
		{
			TC_PRT("hash_len: %d\n", g_local_oob_data.oob_data.hash_len);
			TC_PRT("randomizer_len: %d\n", g_local_oob_data.oob_data.randomizer_len);

			if (bluetooth_oob_add_remote_data(&g_local_oob_data.address,
				&g_local_oob_data.oob_data))
				TC_PRT("ERROR in bluetooth_oob_add_remote_data\n");
			else
				TC_PRT(" bluetooth_oob_add_remote_data SUCCESS\n");
			break;
		}

		case 41:
		{
			if (bluetooth_oob_remove_remote_data(&g_local_oob_data.address))
				TC_PRT("ERROR in bluetooth_oob_remove_remote_data\n");
			else
				TC_PRT(" bluetooth_oob_remove_remote_data SUCCESS\n");
			break;
		}
		case 42:
		{
			bluetooth_opc_init();
			break;
		}
		case 43:
		{
			bluetooth_opc_deinit();
			break;
		}
		case 44:
		{
			bluetooth_device_address_t remote_address = {{0}};

			/* Grey dongle */
			remote_address.addr[0] = 0x00; remote_address.addr[1] = 0x02; remote_address.addr[2] = 0x70;
			remote_address.addr[3] = 0x2B; remote_address.addr[4] = 0xD3;  remote_address.addr[5]= 0xAF;

			char *files[5] = {NULL};

			files[0] = "/opt/media/Images/image1.jpg";
//			files[1] = "/opt/media/Images/image2.jpg";
//			files[2] = "/opt/media/Images/image3.jpg";
			bluetooth_opc_push_files(&remote_address, files);
			break;
		}
		case 45:
		{
			bluetooth_opc_cancel_push();
			break;
		}
		case 46:
		{
			gboolean exist;
			exist = bluetooth_opc_session_is_exist();
			TC_PRT("exist: %d", exist);
			break;
		}
		case 47:
		{
			bluetooth_network_activate_server();
			break;
		}
		case 48:
		{
			bluetooth_network_deactivate_server();
			break;
		}
		case 49:
		{
			bluetooth_device_address_t device_address = {{0x00, 0x02, 0xA2, 0x14, 0x40, 0x51}};
			bluetooth_network_connect(&device_address, BLUETOOTH_NETWORK_NAP_ROLE, NULL);
			break;
		}
		case 50:
		{
			bluetooth_device_address_t device_address = {{0x00, 0x02, 0xA2, 0x14, 0x40, 0x51}};
			bluetooth_network_disconnect(&device_address);
			break;
		}
		case 51:
		{
			bluetooth_obex_server_init("/opt/media/Downloads");
			break;
		}
		case 52:
		{
			bluetooth_obex_server_deinit();
			break;
		}
		case 53:
		{
			bluetooth_obex_server_init_without_agent("/opt/media/Downloads");
			break;
		}
		case 54:
		{
			bluetooth_obex_server_deinit_without_agent();
			break;
		}
		case 55:
		{
			bluetooth_obex_server_is_activated();
			break;
		}
		case 56:
		{
			bluetooth_obex_server_accept_connection();
			TC_PRT(" bluetooth_obex_server_accept_connection SUCCESS\n");
			break;
		}
		case 57:
		{
			bluetooth_obex_server_reject_connection();
			break;
		}
		case 58:
		{
			bluetooth_obex_server_accept_authorize("abc");
			break;
		}
		case 59:
		{
			bluetooth_obex_server_reject_authorize();
			break;
		}
		case 60:
		{
			bluetooth_obex_server_set_destination_path("/opt/media");
			break;
		}
		case 61:
		{
			bluetooth_obex_server_set_root("/opt/media");
			break;
		}
		case 62:
		{
			bluetooth_obex_server_cancel_transfer(0);
			break;
		}
		case 63:
		{
			bluetooth_obex_server_cancel_all_transfers();
			break;
		}

		case 65:
		{
			ret = bluetooth_hid_init(bt_hid_event_callback, NULL);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 66:
		{
			ret = bluetooth_hid_deinit();
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 67:
		{
			/* Apple wireless keyboard */
			hid_device_address_t device_address={{0xE8,0x06,0x88,0x3B,0x18,0xBA}};

			ret = bluetooth_hid_connect(&device_address);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 68:
		{
			/* Apple wireless keyboard */
			hid_device_address_t device_address={{0xE8,0x06,0x88,0x3B,0x18,0xBA}};

			ret = bluetooth_hid_disconnect(&device_address);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}

		case 70:
		{
			bluetooth_device_address_t device_address = {{0x00, 0x02, 0x2F, 0x92, 0x7B, 0xF5}};

			ret = bluetooth_rfcomm_connect(&device_address, rfcomm_test_uuid_spp);
			//ret = bluetooth_rfcomm_connect(&device_address, "1");

			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 71:
		{
			ret = bluetooth_rfcomm_disconnect(-1);

			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 72:
		{
			ret = bluetooth_rfcomm_disconnect(g_ret_client_fd1);

			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 73:
		{
			ret = bluetooth_rfcomm_write(g_ret_client_fd1, "123456789 12345", 20);

			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 74:
		{
			gboolean is_connected;

			is_connected = bluetooth_rfcomm_is_client_connected();

			TC_PRT("Connected: %d", is_connected);
			break;
		}
		case 80:
		{
			ret = bluetooth_rfcomm_create_socket(rfcomm_test_uuid_spp);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);

			TC_PRT("Returned FD = %d", ret);
			server_fd = ret;
			break;
		}
		case 81:
		{
			ret = bluetooth_rfcomm_create_socket(rfcomm_test_uuid_custom);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);

			TC_PRT("Returned FD = %d", ret);
			server_fd = ret;
			break;
		}
		case 82:
		{
			ret = bluetooth_rfcomm_remove_socket(server_fd);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 83: /*Listen and accept */
		{

			ret = bluetooth_rfcomm_listen_and_accept(server_fd, 1);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);

			TC_PRT("result = %d", ret);
			break;
		}
		case 84: /*Listen */
		{

			ret = bluetooth_rfcomm_listen(server_fd, 1);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);

			TC_PRT("result = %d", ret);
			break;
		}
		case 85:
		{
			ret = bluetooth_rfcomm_server_disconnect(client_fd);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			break;
		}
		case 86:
		{
			gboolean available;

			available = bluetooth_rfcomm_is_server_uuid_available(rfcomm_test_uuid_spp);

			TC_PRT("available: %d", available);
			break;
		}
		case 87:
		{
			ret = bluetooth_rfcomm_accept_connection(server_fd, &client_fd);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
			TC_PRT("client fd: %d", client_fd);
			break;
		}
		case 88:
		{
			ret = bluetooth_rfcomm_reject_connection(server_fd);
			if (ret < 0)
				TC_PRT("Failed with [0x%04x]", ret);
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

	if(!g_thread_supported())
	{
		g_thread_init(NULL);
	}

	dbus_g_thread_init();

	g_type_init();
	main_loop = g_main_loop_new(NULL, FALSE);
}

void cleanup()
{
	TC_PRT("bluetooth framework TC cleanup");
	if( main_loop!= NULL)
	{
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


void bt_hid_event_callback(int event, hid_event_param_t* param, void *user_data)
{
	TC_PRT(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	TC_PRT("bt event callback 0x%04x", event);



}

void bt_audio_event_callback(int event, bt_audio_event_param_t* param, void *user_data)
{
	TC_PRT(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	TC_PRT("bt event callback 0x%04x", event);



}

void bt_event_callback(int event, bluetooth_event_param_t* param, void *user_data)
{
	TC_PRT(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
	TC_PRT("bt event callback 0x%04x", event);
	switch(event)
	{
		case BLUETOOTH_EVENT_ENABLED:
			TC_PRT("BLUETOOTH_EVENT_ENABLED, result [0x%04x]", param->result);
			break;

		case BLUETOOTH_EVENT_DISABLED:
			TC_PRT("BLUETOOTH_EVENT_DISABLED, result [0x%04x]", param->result);
			break;

		case BLUETOOTH_EVENT_LOCAL_NAME_CHANGED:
			TC_PRT("BLUETOOTH_EVENT_LOCAL_NAME_CHANGED, result [0x%04x]", param->result);
			if (param->result == BLUETOOTH_ERROR_NONE)
			{
				bluetooth_device_name_t *local_name = (bluetooth_device_name_t *)param->param_data;
				tc_result(TC_PASS, 6);
				TC_PRT("Changed Name : [%s]", local_name->name);
			}
			else
			{
				tc_result(TC_FAIL, 6);
			}
			break;


		case BLUETOOTH_EVENT_DISCOVERY_STARTED:
			TC_PRT("BLUETOOTH_EVENT_DISCOVERY_STARTED, result [0x%04x]", param->result);
			break;

		case BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND:
		{
			bluetooth_device_info_t *device_info = NULL;
			TC_PRT("BLUETOOTH_EVENT_REMOTE_DEVICE_FOUND, result [0x%04x]", param->result);
			device_info  = (bluetooth_device_info_t *)param->param_data;
			memcpy(&searched_device, &device_info->device_address, sizeof(bluetooth_device_address_t));
			TC_PRT("dev [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", \
				device_info->device_address.addr[0], device_info->device_address.addr[1], device_info->device_address.addr[2], \
				device_info->device_address.addr[3], device_info->device_address.addr[4], device_info->device_address.addr[5]);
			break;
		}

		case BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED:
		{
			bluetooth_device_info_t *device_info = NULL;
			TC_PRT("BLUETOOTH_EVENT_REMOTE_DEVICE_NAME_UPDATED, result [0x%04x]", param->result);
			device_info  = (bluetooth_device_info_t *)param->param_data;
			memcpy(&searched_device, &device_info->device_address, sizeof(bluetooth_device_address_t));
			TC_PRT("dev [%s] [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X]", device_info->device_name.name, \
				device_info->device_address.addr[0], device_info->device_address.addr[1], device_info->device_address.addr[2], \
				device_info->device_address.addr[3], device_info->device_address.addr[4], device_info->device_address.addr[5]);
			break;
		}

		case BLUETOOTH_EVENT_DISCOVERY_FINISHED:
			TC_PRT("BLUETOOTH_EVENT_DISCOVERY_FINISHED, result [0x%04x]", param->result);
			break;

		case BLUETOOTH_EVENT_DISCOVERABLE_MODE_CHANGED:
		{
			int *mode = (int *)param->param_data;
			TC_PRT("BT_DISCOVERABLE_MODE_CHANGED, result [0x%04x]", param->result);
			TC_PRT("mode [%d]", *mode);
			break;
		}
		case BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED:
		{
			int *timeout = (int *)param->param_data;
			TC_PRT("BLUETOOTH_EVENT_DISCOVERABLE_TIMEOUT_CHANGED, result [0x%04x]", param->result);
			TC_PRT("timeout [%d]", *timeout);
			break;
		}
		case BLUETOOTH_EVENT_BONDING_FINISHED:
		{
			TC_PRT("BLUETOOTH_EVENT_BONDING_FINISHED, result [0x%04x]", param->result);
			if (param->result >= BLUETOOTH_ERROR_NONE)
			{
				bluetooth_device_info_t *device_info = NULL;
				tc_result(TC_PASS, 12);
				device_info  = (bluetooth_device_info_t *)param->param_data;
				if (device_info == NULL)
					break;

				TC_PRT("dev [%s] [%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X] mjr[%#x] min[%#x] srv[%#x]", device_info->device_name.name, \
					device_info->device_address.addr[0], device_info->device_address.addr[1], device_info->device_address.addr[2], \
					device_info->device_address.addr[3], device_info->device_address.addr[4], device_info->device_address.addr[5], \
					device_info->device_class.major_class, device_info->device_class.minor_class, device_info->device_class.service_class);
			}
			else
			{
				tc_result(TC_FAIL, 12);
			}
			break;
		}

		case BLUETOOTH_EVENT_BONDED_DEVICE_FOUND:
		{
			// bluetooth_get_bonded_device_list is changed as synchronous API. This event is not used any more.
			// 2011.01.06
#if 0
//			int i = 0;
			TC_PRT("BLUETOOTH_EVENT_BONDED_DEVICE_FOUND, result [0x%04x]", param->result);
			if (param->result >= BLUETOOTH_ERROR_NONE)
			{
				///tc_result(TC_PASS, 15);
				bluetooth_device_info_t * bt_dev_info= (bluetooth_device_info_t*)param->param_data;

				TC_PRT("Dev Name = %s, Dev add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X, COD (major,minor,service)= 0x%x:%x:%x\n", bt_dev_info->device_name.name,
					bt_dev_info->device_address.addr[0], bt_dev_info->device_address.addr[1], bt_dev_info->device_address.addr[2], \
					bt_dev_info->device_address.addr[3], bt_dev_info->device_address.addr[4], bt_dev_info->device_address.addr[5], \
					bt_dev_info->device_class.major_class, bt_dev_info->device_class.minor_class, bt_dev_info->device_class.service_class);

			}
			else
			if(param->result == BLUETOOTH_ERROR_END_OF_DEVICE_LIST) /*End of the Device found indication*/
			{
				tc_result(TC_PASS, 15);
				TC_PRT("*****<<No more BLUETOOTH_EVENT_BONDED_DEVICE_FOUND indication>>***** ");
			}
			else
			{
				tc_result(TC_FAIL, 15);
				TC_PRT("*****API failed ***** ");
			}
			break;
#endif
		}
		case BLUETOOTH_EVENT_SERVICE_SEARCHED:
		{
			int i = 0;
			TC_PRT("BLUETOOTH_EVENT_SERVICE_SEARCHED, result [0x%04x]", param->result);
			if (param->result >= BLUETOOTH_ERROR_NONE)
			{
				tc_result(TC_PASS, 18);
				bt_sdp_info_t * bt_sdp_info=param->param_data;

				TC_PRT("Dev add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
					bt_sdp_info->device_addr.addr[0], bt_sdp_info->device_addr.addr[1], bt_sdp_info->device_addr.addr[2], \
					bt_sdp_info->device_addr.addr[3], bt_sdp_info->device_addr.addr[4], bt_sdp_info->device_addr.addr[5]);

					TC_PRT("Supported service list:\n");
					for(i=0; i<bt_sdp_info->service_index; i++)
						TC_PRT("[%#x]\n", bt_sdp_info->service_list_array[i]);

			}
			else
			{
				tc_result(TC_FAIL, 18);
			}
			break;
		}
		case BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED:
		{
			TC_PRT("BLUETOOTH_EVENT_RFCOMM_DATA_RECEIVED, result [0x%04x]", param->result);
			bluetooth_rfcomm_received_data_t *rx_data = param->param_data;
			printf("\n FD = %d \nBuffer len = %d ", rx_data->socket_fd, rx_data->buffer_size);
			if (rx_data->buffer_size < sizeof(oob_data_t))
				TC_PRT("Complete oob data is not recivedn");
			else
				memcpy(&g_remote_oob_data,rx_data->buffer, sizeof(oob_data_t));
			//tc_result(TC_PASS, 21);
			break;
		}
		case BLUETOOTH_EVENT_RFCOMM_CONNECTED:
		{
			bluetooth_rfcomm_connection_t *con_ind = (bluetooth_rfcomm_connection_t *)param->param_data;
			TC_PRT("BLUETOOTH_EVENT_RFCOMM_CONNECTED, result [0x%04x], fd = %d, device add = 0x%X:%X:%X:%X:%X:%X, Role = %s", param->result,
								con_ind->socket_fd,
								con_ind->device_addr.addr[0], con_ind->device_addr.addr[1], con_ind->device_addr.addr[2],
								con_ind->device_addr.addr[3], con_ind->device_addr.addr[4], con_ind->device_addr.addr[5],
								(con_ind->device_role == RFCOMM_ROLE_SERVER)? "SERVER":"CLIENT");
			//tc_result(TC_PASS, 22);
			if((con_ind->device_role == RFCOMM_ROLE_CLIENT) && (con_ind->socket_fd > 0))
			{
				g_ret_client_fd1 = con_ind->socket_fd;
			}

			if((con_ind->device_role == RFCOMM_ROLE_SERVER) && (con_ind->socket_fd > 0))
			{
				client_fd = con_ind->socket_fd;
			}

			break;
		}
		case BLUETOOTH_EVENT_RFCOMM_DISCONNECTED:
		{
			bluetooth_rfcomm_disconnection_t *disconnection_ind = (bluetooth_rfcomm_disconnection_t *)param->param_data;;
			TC_PRT("BLUETOOTH_EVENT_RFCOMM_DISCONNECTED, result [0x%04x] Fd = %d, device add = 0x%X:%X:%X:%X:%X:%X\n", param->result, disconnection_ind->socket_fd,
														disconnection_ind->device_addr.addr[0], disconnection_ind->device_addr.addr[1], disconnection_ind->device_addr.addr[2],
														disconnection_ind->device_addr.addr[3], disconnection_ind->device_addr.addr[4], disconnection_ind->device_addr.addr[5] );
			//tc_result(TC_PASS, 22);
			break;
		}
		case BLUETOOTH_EVENT_NETWORK_SERVER_CONNECTED:
		{
			bluetooth_network_device_info_t *dev_info = (bluetooth_network_device_info_t *)param->param_data;

			TC_PRT("BLUETOOTH_EVENT_RFCOMM_DISCONNECTED, result [0x%04x]", param->result);
			TC_PRT("interface name: %s", dev_info->interface_name);

			TC_PRT("device add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
								dev_info->device_address.addr[0], dev_info->device_address.addr[1], dev_info->device_address.addr[2],
								dev_info->device_address.addr[3], dev_info->device_address.addr[4], dev_info->device_address.addr[5] );
			break;
		}
		case BLUETOOTH_EVENT_NETWORK_SERVER_DISCONNECTED:
		{
			bluetooth_network_device_info_t *dev_info = (bluetooth_network_device_info_t *)param->param_data;

			TC_PRT("BLUETOOTH_EVENT_RFCOMM_DISCONNECTED, result [0x%04x]", param->result);
			TC_PRT("interface name: %s", dev_info->interface_name);

			TC_PRT("device add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
								dev_info->device_address.addr[0], dev_info->device_address.addr[1], dev_info->device_address.addr[2],
								dev_info->device_address.addr[3], dev_info->device_address.addr[4], dev_info->device_address.addr[5] );
			break;
		}

		case BLUETOOTH_EVENT_HDP_CONNECTED:
		{
			bt_hdp_connected_t *conn_ind = (bt_hdp_connected_t *)param->param_data;

			TC_PRT("BLUETOOTH_EVENT_HDP_CONNECTED, Result = %s\n", (param->result == 0)? "BLUETOOTH_ERROR_NONE": "BLUETOOTH_ERROR_XXXX");
			TC_PRT("App handler = %s, channel id = %d, type = %s", conn_ind->app_handle, conn_ind->channel_id, (conn_ind->type == HDP_QOS_RELIABLE)? "Reliable":"Streaming");
			TC_PRT("device add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n",
								conn_ind->device_address.addr[0], conn_ind->device_address.addr[1], conn_ind->device_address.addr[2],
								conn_ind->device_address.addr[3], conn_ind->device_address.addr[4], conn_ind->device_address.addr[5] );

			g_ret_client_fd1 = conn_ind->channel_id;
			break;
		}

		case BLUETOOTH_EVENT_HDP_DISCONNECTED:
		{
			bt_hdp_disconnected_t *dis_ind = (bt_hdp_disconnected_t *)param->param_data;

			TC_PRT("BLUETOOTH_EVENT_HDP_DISCONNECTED, Result = %s\n", (param->result == 0)? "BLUETOOTH_ERROR_NONE": "BLUETOOTH_ERROR_XXXX");
			TC_PRT("Channel = %d, Add = device add = %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\n", dis_ind->channel_id,
								dis_ind->device_address.addr[0], dis_ind->device_address.addr[1], dis_ind->device_address.addr[2],
								dis_ind->device_address.addr[3], dis_ind->device_address.addr[4], dis_ind->device_address.addr[5]);
			break;
		}

		case BLUETOOTH_EVENT_HDP_DATA_RECEIVED:
		{
			bt_hdp_data_ind_t *data_ind = (bt_hdp_data_ind_t *)param->param_data;

			TC_PRT("BLUETOOTH_EVENT_HDP_DATA_RECEIVED, Result = %s\n", (param->result == 0)? "BLUETOOTH_ERROR_NONE": "BLUETOOTH_ERROR_XXXX");
			TC_PRT("Data received from channel id = %d and  size =%d, buff =[%s]\n",  data_ind->channel_id, data_ind->size, data_ind->buffer);

			break;
		}

		case BLUETOOTH_EVENT_OPC_CONNECTED:
			TC_PRT("BLUETOOTH_EVENT_OPC_CONNECTED");
			break;

		case BLUETOOTH_EVENT_OPC_DISCONNECTED:
			TC_PRT("BLUETOOTH_EVENT_OPC_DISCONNECTED");
			break;
		case BLUETOOTH_EVENT_OPC_TRANSFER_STARTED:
		{
			TC_PRT("BLUETOOTH_EVENT_OPC_TRANSFER_STARTED");
			if (param->param_data) {
				bt_opc_transfer_info_t *info = param->param_data;
				TC_PRT("file %s", info->filename);
				TC_PRT("size %ld", info->size);
			}
			break;
		}
		case BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS:
		{
			TC_PRT("BLUETOOTH_EVENT_OPC_TRANSFER_PROGRESS");
			break;
		}
		case BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE:
		{
			TC_PRT("BLUETOOTH_EVENT_OPC_TRANSFER_COMPLETE");
			bt_opc_transfer_info_t *info = param->param_data;
			TC_PRT("file %s", info->filename);
			TC_PRT("size %ld", info->size);
			break;
		}

		case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE:
			TC_PRT("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_AUTHORIZE");
			break;

		case BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE:
			TC_PRT("BLUETOOTH_EVENT_OBEX_SERVER_CONNECTION_AUTHORIZE");
			break;

		case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED:
			TC_PRT("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_STARTED");
			bt_obex_server_transfer_info_t *info = param->param_data;
			current_transfer_id = info->transfer_id;
			break;

		case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS:
			TC_PRT("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_PROGRESS");
			break;

		case BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED:
			TC_PRT("BLUETOOTH_EVENT_OBEX_SERVER_TRANSFER_COMPLETED");
			break;

#if 0
		case BLUETOOTH_EVENT_GATT_PRIM_SVC_DISCOVERED:
		{
			TC_PRT("BLUETOOTH_EVENT_GATT_PRIM_SVC_DISCOVERED");
			bt_gatt_handle_info_t *prim_svc = param->param_data;
			int i;

			if (prim_svc == NULL) {
				TC_PRT("prim_svc is NULL");
				break;
			}

			for (i = 0; i < prim_svc->count; i++) {
				TC_PRT("prim_svc[%d] = %s\n", i, prim_svc->handle[i]);
			}

			break;
		}

		case BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED:
		{
			TC_PRT("BLUETOOTH_EVENT_GATT_SVC_CHAR_DISCOVERED");
			bt_gatt_discovered_char_t *svc_char = param->param_data;
			int i = 0;

			if (svc_char == NULL) {
				TC_PRT("svc_char is NULL");
				break;
			}

			if (svc_char->service_handle != NULL) {
				TC_PRT("svc_char->service_handle %s \n", svc_char->service_handle);
			}

			for (i = 0; i < svc_char->handle_info.count; i++) {
				TC_PRT("svc_char.handle_info.handle[%d] = %s\n", i, svc_char->handle_info.handle[i]);
			}

			break;
		}

		case BLUETOOTH_EVENT_GATT_SVC_PROP_RECEIVED:
		{
			TC_PRT("BLUETOOTH_EVENT_GATT_SVC_PROP_RECEIVED");
			bt_gatt_service_property_t *svc_pty = param->param_data;
			int i;

			if (svc_pty == NULL) {
				TC_PRT("char_pty is NULL \n");
				break;
			}

			if (svc_pty->service_handle != NULL) {
				TC_PRT("svc_pty->service_handle %s \n", svc_pty->service_handle);
			}

			if (svc_pty->uuid != NULL) {
				TC_PRT("svc_pty->uuid %s \n", svc_pty->uuid);
			}

			for (i = 0; i < svc_pty->handle_info.count; i++) {
				TC_PRT("svc_char[%d] = %s\n", i, svc_pty->handle_info.handle[i]);
			}

			break;
		}

		case BLUETOOTH_EVENT_GATT_CHAR_PROP_RECEIVED:
		{
			TC_PRT("BLUETOOTH_EVENT_GATT_CHAR_PROP_RECEIVED");
			bt_gatt_char_property_t *char_pty = param->param_data;
			int i = 0;

			if (char_pty->char_handle != NULL) {
				TC_PRT("char_pty->char_handle %s \n", char_pty->char_handle);
			}

			if (char_pty->uuid != NULL) {
				TC_PRT("char_pty->uuid %s \n", char_pty->uuid);
			}

			if (char_pty == NULL) {
				TC_PRT("char_pty is NULL \n");
				break;
			}

			if (char_pty->name != NULL) {
				TC_PRT("char_pty->name %s \n", char_pty->name);
			}

			if (char_pty->description != NULL) {
				TC_PRT("char_pty->description %s \n", char_pty->description);
			}

			if (char_pty->val != NULL) {
				TC_PRT("char_pty->val_len %d \n", char_pty->val_len);

				for (i = 0; i < char_pty->val_len; i ++)
					TC_PRT("char_pty->val %02x \n", char_pty->val[i]);
			}

			break;
		}
#endif
		default:
			TC_PRT("received event [0x%04x]", event);
			break;
	}
	TC_PRT("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

static gboolean key_event_cb(GIOChannel * chan, GIOCondition cond , gpointer data)
{
	char buf[10] = {0};

	unsigned int len=0;
	int test_id;
	memset(buf, 0, sizeof(buf));

	if(g_io_channel_read_chars(chan, buf, sizeof(buf),
				&len, NULL) == G_IO_STATUS_ERROR) {
		TC_PRT("IO Channel read error");
		return FALSE;
	}

	printf("%s\n",buf);
	tc_usage_print();

	test_id=atoi(buf);

	if(test_id)
		g_idle_add(test_input_callback, (void*)test_id);

	return TRUE;
}

int main(void)
{
	startup();

	GIOChannel *key_io;
	key_io=g_io_channel_unix_new(fileno(stdin));

	g_io_channel_set_encoding(key_io, NULL, NULL);
	g_io_channel_set_flags(key_io, G_IO_FLAG_NONBLOCK, NULL);

	g_io_add_watch(key_io, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
			key_event_cb, NULL);
	g_io_channel_unref(key_io);

	g_main_loop_run(main_loop);

	cleanup();
	return 0;
}

