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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <vconf.h>
#include <vconf-keys.h>
#include "bt-common.h"
#include "bluetooth-telephony-api.h"
#include "marshal.h"

typedef struct {
	guint monitor_filter_id_bluez_headset;
	guint monitor_filter_id_hfp_agent;
	guint monitor_filter_id_bluez_manager;
} telephony_dbus_info_t;

typedef struct {
	bt_telephony_func_ptr cb;
	unsigned int call_count;
	char *obj_path;
	char address[BT_ADDRESS_STR_LEN];
	char call_path[BT_AUDIO_CALL_PATH_LEN];
	bluetooth_headset_state_t headset_state;
	void *user_data;
} bt_telephony_info_t;

#define BLUETOOTH_TELEPHONY_ERROR (__bluetooth_telephony_error_quark())

#define BLUEZ_SERVICE_NAME "org.bluez"
#define BLUEZ_HEADSET_INTERFACE "org.bluez.Headset"
#define BLUEZ_MANAGER_INTERFACE "org.bluez.Manager"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device"

#define HFP_AGENT_SERVICE "org.bluez.hfp_agent"
#define HFP_AGENT_PATH "/org/bluez/hfp_agent"
#define HFP_AGENT_INTERFACE "Org.Hfp.App.Interface"

#define CSD_CALL_APP_PATH "/org/tizen/csd/%d"
#define HFP_NREC_STATUS_CHANGE "NrecStatusChanged"

#define BT_TELEPHONY_CHECK_ENABLED() \
	do { \
		if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED) {\
			BT_ERR("BT is not enabled"); \
			return BLUETOOTH_TELEPHONY_ERROR_NOT_ENABLED; \
		} \
	} while (0)

static gboolean is_initialized = FALSE;
#define BT_TELEPHONY_CHECK_INITIALIZED() \
	do { \
		if (is_initialized == FALSE) { \
			BT_ERR("Bluetooth telephony not initilized"); \
			return BLUETOOTH_TELEPHONY_ERROR_NOT_INITIALIZED; \
		} \
	} while (0)

#include "bt-telephony-glue.h"

OrgTizenCsdCallInstance *bluetooth_telephony_obj = NULL;
GDBusConnection *gdbus_conn;
int owner_id;

static bt_telephony_info_t telephony_info;
static telephony_dbus_info_t telephony_dbus_info;
static gboolean is_active = FALSE;

/*Function Declaration*/
static int __bt_telephony_get_error(const char *error_message);
static void __bt_telephony_event_cb(int event, int result, void *param_data);
static GQuark __bluetooth_telephony_error_quark(void);
static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id);
static GError *__bluetooth_telephony_error(bluetooth_telephony_error_t error,
			const char *err_msg);

static void __bluetooth_telephony_adapter_added_cb(GDBusConnection *connection,
						const gchar     *sender_name,
						const gchar     *object_path,
						const gchar     *interface_name,
						const gchar     *signal_name,
						GVariant        *parameters,
						gpointer         user_data);

static void __bluetooth_telephony_bluez_headset_event_filter(
						GDBusConnection *connection,
						const gchar     *sender_name,
						const gchar     *object_path,
						const gchar     *interface_name,
						const gchar     *signal_name,
						GVariant        *parameters,
						gpointer         user_data);

static void __bluetooth_telephony_hfp_event_filter(GDBusConnection *connection,
						const gchar     *sender_name,
						const gchar     *object_path,
						const gchar     *interface_name,
						const gchar     *signal_name,
						GVariant        *parameters,
						gpointer         user_data);

static int __bluetooth_telephony_init(void);
static int __bluetooth_telephony_register(void);
static int __bluetooth_telephony_unregister(void);
static int __bluetooth_get_default_adapter_path(char *path);
static gboolean __bluetooth_telephony_is_headset(uint32_t device_class);
static int __bluetooth_telephony_get_connected_device(void);
static gboolean __bluetooth_telephony_get_connected_device_path(void);
/*Function Definition*/
static int __bt_telephony_get_error(const char *error_message)
{
	if (error_message == NULL) {
		BT_ERR("Error message NULL");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_ERR("Error message = %s", error_message);
	if (g_strcmp0(error_message, "NotAvailable") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_NOT_AVAILABLE;
	else if (g_strcmp0(error_message, "NotConnected") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_NOT_CONNECTED;
	else if (g_strcmp0(error_message, "InProgress") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_BUSY;
	else if (g_strcmp0(error_message, "InvalidArguments") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;
	else if (g_strcmp0(error_message, "AlreadyExists") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_EXSIST;
	else if (g_strcmp0(error_message, "Already Connected") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_CONNECTED;
	else if (g_strcmp0(error_message, "No memory") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_NO_MEMORY;
	else if (g_strcmp0(error_message, "I/O error") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_I_O_ERROR;
	else if (g_strcmp0(error_message,
				"Operation currently not available") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_OPERATION_NOT_AVAILABLE;
	else
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
}

static void __bt_telephony_event_cb(int event, int result, void *param_data)
{
	telephony_event_param_t bt_event = { 0, };

	bt_event.event = event;
	bt_event.result = result;
	bt_event.param_data = param_data;

	ret_if(telephony_info.cb == NULL);
	telephony_info.cb(bt_event.event, &bt_event, telephony_info.user_data);
	return;
}

static GQuark __bluetooth_telephony_error_quark(void)
{
	static GQuark quark = 0;

	quark = g_quark_from_static_string("telephony");

	return quark;
}

static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id)
{
	GVariant *reply = NULL;
	GError *error = NULL;;
	char *path = g_strdup(telephony_info.call_path);
	GDBusConnection *connection = NULL;
	int ret;
	BT_DBG("+");

	connection = _bt_init_system_gdbus_conn();
	BT_DBG("path is %s", path);
	BT_DBG("call_status is %d", call_status);
	BT_DBG("call_id is %d", call_id);
	reply = g_dbus_connection_call_sync(connection,
						HFP_AGENT_SERVICE,
						HFP_AGENT_PATH,
						HFP_AGENT_INTERFACE,
						"ChangeCallStatus",
						g_variant_new("(sii)",
							path,
							call_status,
							call_id),
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&error);
	if (error) {
		BT_ERR("ChangeCallStatus GDBus error: %s", error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	g_variant_unref(reply);
	g_free(path);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static GError *__bluetooth_telephony_error(bluetooth_telephony_error_t error,
					const char *err_msg)
{
	return g_error_new(BLUETOOTH_TELEPHONY_ERROR, error, err_msg, NULL);
}

static gboolean bluetooth_telephony_method_answer(
					OrgTizenCsdCallInstance *object,
					GDBusMethodInvocation *context,
					guint callid,
					gpointer user_data)
{
	telephony_event_callid_t call_data = { 0, };

	BT_DBG("+");
	BT_DBG("call_id = [%d]", callid);

	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);

	g_dbus_method_invocation_return_value(context, NULL);
	BT_DBG("-");
	return TRUE;
}

static gboolean bluetooth_telephony_method_release(
				OrgTizenCsdCallInstance *object,
				GDBusMethodInvocation *context,
				guint callid,
				gpointer user_data)
{
	telephony_event_callid_t call_data = { 0, };

	BT_DBG("+");
	BT_DBG("call_id = [%d]\n", callid);

	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);
	g_dbus_method_invocation_return_value(context, NULL);
	BT_DBG("-");
	return TRUE;
}

static gboolean bluetooth_telephony_method_reject(
				OrgTizenCsdCallInstance *object,
				GDBusMethodInvocation *context,
				guint callid,
				gpointer user_data)
{
	telephony_event_callid_t call_data = { 0, };

	BT_DBG("+");
	BT_DBG("call_id = [%d]", callid);

	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_REJECT_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void  *)&call_data);
	g_dbus_method_invocation_return_value(context, NULL);
	BT_DBG("-");
	return TRUE;
}

static gboolean bluetooth_telephony_method_threeway(
				OrgTizenCsdCallInstance *object,
				GDBusMethodInvocation *context,
				guint value,
				gpointer user_data)
{
	int event = 0;
	GError *err;

	BT_DBG("+");
	BT_DBG("chld value  = [%d]", value);

	switch (value) {
	case 0:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_0_RELEASE_ALL_HELD_CALL;
		break;
	case 1:
		event =
		BLUETOOTH_EVENT_TELEPHONY_CHLD_1_RELEASE_ALL_ACTIVE_CALL;
		break;
	case 2:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_2_ACTIVE_HELD_CALL;
		break;
	case 3:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_3_MERGE_CALL;
		break;
	default:
		BT_ERR("Invalid CHLD command");
		err = __bluetooth_telephony_error(
			BLUETOOTH_TELEPHONY_ERROR_INVALID_CHLD_INDEX,
			"Invalid chld command");
		g_dbus_method_invocation_return_error(context,
						G_DBUS_ERROR,
						G_DBUS_ERROR_FAILED,
						err->message);
		if (err != NULL)
			g_clear_error(&err);
		return FALSE;
	}

	BT_DBG("event  = [%d]", event);

	__bt_telephony_event_cb(event,
			BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
	g_dbus_method_invocation_return_value(context, NULL);
	BT_DBG("-");
	return TRUE;
}

static gboolean bluetooth_telephony_method_send_dtmf(
				OrgTizenCsdCallInstance *object,
				GDBusMethodInvocation *context,
				gchar *dtmf,
				gpointer user_data)
{
	telephony_event_dtmf_t call_data = { 0, };
	GError *err;

	BT_DBG("+");

	if (dtmf == NULL) {
		BT_DBG("Number dial failed\n");
		err = __bluetooth_telephony_error(
			BLUETOOTH_TELEPHONY_ERROR_INVALID_DTMF,
			"Invalid dtmf");
		g_dbus_method_invocation_return_error(context,
						G_DBUS_ERROR,
						G_DBUS_ERROR_FAILED,
						err->message);
		if (err != NULL)
			g_clear_error(&err);
		return FALSE;
	}

	BT_DBG("Dtmf = %s \n", dtmf);

	call_data.dtmf = g_strdup(dtmf);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF,
		BLUETOOTH_TELEPHONY_ERROR_NONE, (void *)&call_data);

	g_dbus_method_invocation_return_value(context, NULL);
	g_free(call_data.dtmf);
	BT_DBG("-");
	return TRUE;
}

static void __bluetooth_handle_nrec_status_change(GVariant *parameters)
{
	gboolean status = FALSE;
	g_variant_get(parameters, "b", &status);
	if (parameters)
		g_variant_unref(parameters);

	BT_DBG("NREC status = %d\n", status);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED,
		BLUETOOTH_TELEPHONY_ERROR_NONE, (void *)&status);

}

static void __bluetooth_telephony_bluez_headset_event_filter(
					GDBusConnection *connection,
					const gchar     *sender_name,
					const gchar     *object_path,
					const gchar     *interface_name,
					const gchar     *signal_name,
					GVariant        *parameters,
					gpointer         user_data)
{
	char *dev_addr = NULL;
	const char *property = NULL;
	GVariant *gVal;
	GVariantIter iter;
	GVariantIter value_iter;
	g_variant_iter_init(&iter, parameters);
	g_variant_iter_next(&iter, "s", &property);
	/*g_variant_unref (parameters);*/
	BT_DBG("Property (%s)\n", property);
	if (g_strcmp0(property, "State") == 0) {
		char *state = NULL;
		gVal = g_variant_iter_next_value(&iter);
		g_variant_iter_init(&value_iter, gVal);
		g_variant_iter_next(&value_iter, "s", &state);
		if (NULL == state) {
			BT_ERR("State is null\n");
			return;
		}
		BT_DBG("State %s\n", state);

		if (g_strcmp0(state, "connected") == 0) {
			telephony_info.headset_state =
				BLUETOOTH_STATE_CONNECTED;
		} else if (g_strcmp0(state, "playing") == 0) {
			telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
			__bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		}
		return;
	}
	if (g_strcmp0(property, "Connected") == 0) {
		gboolean connected = FALSE;
		gVal = g_variant_iter_next_value(&iter);
		g_variant_iter_init(&value_iter, gVal);
		g_variant_iter_next(&value_iter, "b", &connected);

		BT_DBG("Connected %d\n", connected);

		if (connected) {
			/*Get device address*/
			if (object_path != NULL)
				dev_addr = strstr(object_path, "dev_");

			if (dev_addr != NULL) {
				dev_addr += 4;
				g_strlcpy(telephony_info.address,
					dev_addr,
					sizeof(telephony_info.address));
				g_strdelimit(telephony_info.address, "_", ':');
				BT_DBG("address is %s",
					telephony_info.address);

				telephony_info.headset_state =
						BLUETOOTH_STATE_CONNECTED;

				__bluetooth_telephony_get_connected_device_path();

				BT_DBG("Headset Connected");

				 __bt_telephony_event_cb(
					BLUETOOTH_EVENT_TELEPHONY_HFP_CONNECTED,
					BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
			}
		} else { /*Device disconnected*/
			memset(telephony_info.address, 0x00,
					sizeof(telephony_info.address));
			telephony_info.headset_state =
						BLUETOOTH_STATE_DISCONNETED;

			BT_DBG("Headset Disconnected");

			 __bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_HFP_DISCONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		}
		return;
	}
	if (g_strcmp0(property, "SpeakerGain") == 0) {
		unsigned int spkr_gain;
		guint16 gain;
		gVal = g_variant_iter_next_value(&iter);
		g_variant_iter_init(&value_iter, gVal);
		g_variant_iter_next(&value_iter, "q", &gain);

		spkr_gain = (unsigned int)gain;
		BT_DBG("spk_gain[%d]\n", spkr_gain);

		__bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_SET_SPEAKER_GAIN,
				BLUETOOTH_TELEPHONY_ERROR_NONE,
				(void *)&spkr_gain);

		return;
	}
	if (g_strcmp0(property, "MicrophoneGain") == 0) {
		unsigned int mic_gain;
		guint16 gain;
		gVal = g_variant_iter_next_value(&iter);
		g_variant_iter_init(&value_iter, gVal);
		g_variant_iter_next(&value_iter, "q", &gain);
		mic_gain = (unsigned int)gain;
		BT_DBG("mic_gain[%d]\n", mic_gain);

		__bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_SET_MIC_GAIN,
				BLUETOOTH_TELEPHONY_ERROR_NONE,
				(void *)&mic_gain);

		return;
	}
	if (g_strcmp0(property, "Playing") == 0) {
		gboolean audio_sink_playing = FALSE;
		gVal = g_variant_iter_next_value(&iter);
		g_variant_iter_init(&value_iter, gVal);
		g_variant_iter_next(&value_iter, "b", &audio_sink_playing);

		if (audio_sink_playing) {
			if (!vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, TRUE)) {
				BT_DBG("SVCONFKEY_BT_HEADSET_SCO -"
					"Set to TRUE\n");
			} else {
				BT_DBG("vconf_set_bool - Failed\n");
			}
			telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
			 __bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_AUDIO_CONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		} else {
			if (!vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, FALSE)) {
				BT_DBG("SVCONFKEY_BT_HEADSET_SCO -"
						"Set to FALSE\n");
			} else {
				BT_DBG("vconf_set_bool - Failed\n");
			}
			telephony_info.headset_state =
						BLUETOOTH_STATE_CONNECTED;
			__bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		}
	}
	return;
}


static void __bluetooth_telephony_hfp_event_filter(GDBusConnection *connection,
						const gchar     *sender_name,
						const gchar     *object_path,
						const gchar     *interface_name,
						const gchar     *signal_name,
						GVariant        *parameters,
						gpointer         user_data)
{
	if (g_strcmp0(signal_name, HFP_NREC_STATUS_CHANGE) != 0) {
		BT_ERR("signal name is not matching... \n");
		return;
	} else if (g_strcmp0(interface_name, HFP_AGENT_SERVICE) != 0) {
		BT_ERR("interface name is not matching... \n");
		return;
	}
	__bluetooth_handle_nrec_status_change(parameters);
}

static void __bluetooth_telephony_adapter_added_cb(GDBusConnection *connection,
						const gchar     *sender_name,
						const gchar     *object_path,
						const gchar     *interface_name,
						const gchar     *signal_name,
						GVariant        *parameters,
						gpointer         user_data)
{
	int ret;
	BT_DBG("+");
	char *adapter_path = NULL;
	g_variant_get(parameters, "(o)", &adapter_path);
	BT_DBG("Adapter added [%s] \n", adapter_path);
	if (parameters)
		g_variant_unref(parameters);

	if (adapter_path == NULL) {
		return;
	}

	if (strstr(adapter_path, "hci0")) {
		BT_DBG("BlueZ is Activated and flag need to be reset");
		BT_DBG("Send enabled to application\n");

		ret = __bluetooth_telephony_register();
		if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
			BT_DBG("__bluetooth_telephony_register failed\n");
			return;
		}
	}
	g_free(adapter_path);
	BT_DBG("-");
}

static void __bt_telephony_gdbus_init(void)
{
	GDBusConnection *g_conn;
	GError *g_err = NULL;
	BT_DBG("+");

	ret_if(bluetooth_telephony_obj != NULL);

	g_conn = _bt_init_system_gdbus_conn();
	ret_if(g_conn == NULL);

	bluetooth_telephony_obj = org_tizen_csd_call_instance_skeleton_new();

	g_signal_connect(bluetooth_telephony_obj,
			"handle-answer",
			G_CALLBACK(bluetooth_telephony_method_answer),
			NULL);

	g_signal_connect(bluetooth_telephony_obj,
			"handle-release",
			G_CALLBACK(bluetooth_telephony_method_release),
			NULL);

	g_signal_connect(bluetooth_telephony_obj,
			"handle-reject",
			G_CALLBACK(bluetooth_telephony_method_reject),
			NULL);

	g_signal_connect(bluetooth_telephony_obj,
			"handle-threeway",
			G_CALLBACK(bluetooth_telephony_method_threeway),
			NULL);

	g_signal_connect(bluetooth_telephony_obj,
			"handle-send-dtmf",
			G_CALLBACK(bluetooth_telephony_method_send_dtmf),
			NULL);

	if (g_dbus_interface_skeleton_export(
			G_DBUS_INTERFACE_SKELETON(bluetooth_telephony_obj),
			g_conn,
			"/",
			&g_err) == FALSE) {
			if (g_err) {
				BT_ERR("Export error: %s", g_err->message);
				g_clear_error(&g_err);
			}
			g_object_unref(bluetooth_telephony_obj);
			bluetooth_telephony_obj = NULL;
			g_dbus_connection_close_sync(gdbus_conn, NULL, NULL);
			gdbus_conn = NULL;
			return;
	}
	gdbus_conn = g_conn;
	BT_DBG("-");
}

static void __on_bus_acquired(GDBusConnection *connection,
			const gchar *path,
			gpointer user_data)
{
	BT_DBG("bus path : %s", path);
	__bt_telephony_gdbus_init();
}

static void __on_name_acquired(GDBusConnection *connection,
			const gchar *name,
			gpointer user_data)
{
	BT_DBG("name : %s", name);
}

static void __on_name_lost(GDBusConnection *connnection,
			const gchar *name,
			gpointer user_data)
{
	BT_DBG("name : %s", name);
}

static int __bluetooth_telephony_init(void){
	int id;
	BT_DBG("+");
	id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
		"org.tizen.csd.Call.Instance",
		G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
		G_BUS_NAME_OWNER_FLAGS_REPLACE,
		__on_bus_acquired,
		__on_name_acquired,
		__on_name_lost,
		NULL,
		NULL);
	if (id <= 0) {
		BT_DBG("telephony: bus acquired failed... \n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	owner_id = id;
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static int __bluetooth_telephony_register(void)
{
	GError *err = NULL;
	char *path = g_strdup(telephony_info.call_path);
	int ret;
	GDBusConnection *connection;

	BT_DBG("+");
	connection = _bt_init_system_gdbus_conn();
	g_dbus_connection_call_sync(connection,
					HFP_AGENT_SERVICE,
					HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE,
					"RegisterApplication",
					g_variant_new("(s)",
						path),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);

	g_free(path);
	if (err) {
		BT_ERR("Error returned in method call\n");
		ret = __bt_telephony_get_error(err->message);
		g_clear_error(&err);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static  int __bluetooth_telephony_unregister(void)
{
	GError *err = NULL;
	char *path = g_strdup(telephony_info.call_path);
	BT_DBG("path is .. %s \n", path);
	int ret;
	GDBusConnection *conn;
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	g_dbus_connection_call_sync(conn,
					HFP_AGENT_SERVICE,
					HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE,
					"UnregisterApplication",
					g_variant_new("(s)",
							path),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);


	g_free(path);
	if (err) {
		ret = __bt_telephony_get_error(err->message);
		g_clear_error(&err);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_DBG("+");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static int __bluetooth_get_default_adapter_path(char *path)
{
	GError *err = NULL;
	GVariant *result;
	gchar *adapter_path = NULL;
	GDBusConnection *conn;
	BT_DBG("+");

	conn = _bt_init_system_gdbus_conn();
	result = g_dbus_connection_call_sync(conn,
					BLUEZ_SERVICE_NAME,
					"/",
					BLUEZ_MANAGER_INTERFACE,
					"DefaultAdapter",
					NULL,
					G_VARIANT_TYPE("(o)"),
					/*G_VARIANT_TYPE_OBJECT_PATH,*/
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);

	if (err != NULL) {
		BT_ERR("Getting DefaultAdapter failed: [%s]",
						err->message);
		g_clear_error(&err);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	g_variant_get(result, "(o)", &adapter_path);
	if (result)
		g_variant_unref(result);
	if (adapter_path == NULL) {
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (strlen(adapter_path) >= BT_ADAPTER_PATH_LEN) {
		BT_ERR("Path too long.\n");
		g_free(adapter_path);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_DBG("path = %s", adapter_path);

	g_strlcpy(path, adapter_path, BT_ADAPTER_PATH_LEN);
	g_free(adapter_path);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static gboolean __bluetooth_telephony_is_headset(uint32_t device_class)
{
	gboolean flag = FALSE;
	BT_DBG("+");

	switch ((device_class & 0x1f00) >> 8) {
	case 0x04:
		switch ((device_class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			flag = TRUE;
			break;
		case 0x06:
			flag = TRUE;
			break;
		case 0x0b:
		case 0x0c:
		case 0x0d:
			break;
		default:
			flag = TRUE;
			break;
		}
		break;
	}
	BT_DBG("-");
	return flag;
}

static int __bluetooth_telephony_get_connected_device(void)
{
	GError *error = NULL;
	gchar *gp_path = NULL;
	GDBusConnection *conn;
	GHashTable *list_hash;
	GHashTable *device_hash;
	uint32_t device_class;
	gboolean playing = FALSE;
	gboolean connected = FALSE;
	const gchar *address;
	char object_path[BT_ADAPTER_PATH_LEN] = {0};
	GVariant *result;
	GVariantIter *iter;
	GVariantIter *prop_iter;
	char *key;
	GVariant *value;
	BT_DBG("+");

	/*Get default adapter path*/
	if (__bluetooth_get_default_adapter_path(object_path) < 0)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	BT_DBG("adapter path is %s \n", object_path);
	/*Get List of All devices*/
	conn = _bt_init_system_gdbus_conn();
	result = g_dbus_connection_call_sync(conn,
					BLUEZ_SERVICE_NAME,
					object_path,
					BLUEZ_ADAPTER_INTERFACE,
					"ListDevices",
					NULL,
					G_VARIANT_TYPE("(ao)"),
					/*G_VARIANT_TYPE_OBJECT_PATH_ARRAY,*/
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

	if (error != NULL) {
		BT_ERR("Getting DefaultAdapter failed: [%s]",
						error->message);
		g_clear_error(&error);
		goto done;
	}

	if (result == NULL) {
		BT_DBG("ListDevices result is NULL \n");
		goto done;
	}
	/*Check for headset devices*/
	g_variant_get(result, "(ao)", &iter);
	while (g_variant_iter_next(iter, "o", &gp_path)) {
		list_hash = NULL;
		device_hash = NULL;

		if (gp_path == NULL) {
			BT_DBG("gp_path result is NULL \n");
			goto done;
		}
		BT_DBG("gp_path is %s \n", gp_path);
		result = g_dbus_connection_call_sync(conn,
						BLUEZ_SERVICE_NAME,
						gp_path,
						BLUEZ_DEVICE_INTERFACE,
						"GetProperties",
						NULL,
						G_VARIANT_TYPE("(a{sv})"),
						/*G_VARIANT_TYPE_DICTIONARY,*/
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&error);
		if (error != NULL) {
			 BT_ERR("GetProperties failed: [%s]",
						error->message);
			g_clear_error(&error);
			goto done;
		}
		list_hash = g_hash_table_new_full(g_str_hash,
						g_str_equal,
						NULL,
						NULL);
		g_variant_get(result, "(a{sv})", &prop_iter);
		while (g_variant_iter_next(prop_iter, "{sv}", &key, &value)) {
			g_hash_table_insert(list_hash, key, value);
		}

		if (list_hash == NULL) {
			BT_DBG("list_hash result is NULL \n");
			goto done;
		}

		value = g_hash_table_lookup(list_hash, "Class");
		if (value == NULL)
			BT_DBG("hashtable look up is null ... \n");
		device_class = value ? g_variant_get_uint32(value) : 0;
		BT_DBG("device class is %d \n", device_class);
		if (!__bluetooth_telephony_is_headset(device_class)) {
			BT_DBG("__bluetooth_telephony_is_headset failed... \n");
			g_free(gp_path);
			gp_path = NULL;
			g_hash_table_destroy(list_hash);
			goto done;
		}
		result = g_dbus_connection_call_sync(conn,
						BLUEZ_SERVICE_NAME,
						gp_path,
						BLUEZ_DEVICE_INTERFACE,
						"GetProperties",
						NULL,
						G_VARIANT_TYPE("(a{sv})"),
						/*G_VARIANT_TYPE_DICTIONARY,*/
						G_DBUS_CALL_FLAGS_NONE,
						-1, /* 3 secs */
						NULL,
						&error);

		if (error == NULL) {
			device_hash = g_hash_table_new_full(g_str_hash,
							g_str_equal,
							NULL,
							NULL);
			g_variant_get(result, "(a{sv})", &prop_iter);
			while (g_variant_iter_next(prop_iter,
						"{sv}",
						&key,
						&value))
				g_hash_table_insert(device_hash, key, value);
			value = g_hash_table_lookup(device_hash,
							"Connected");
			connected = value ? g_variant_get_boolean(
					value) : FALSE;
			if (connected) {
				value = g_hash_table_lookup(list_hash,
							"Address");
				g_variant_get(value, "s", &address);
				g_strlcpy(telephony_info.address, address,
					sizeof(telephony_info.address));
				value = g_hash_table_lookup(device_hash,
							"Playing");
				playing = value ? g_variant_get_boolean(
							value) : FALSE;
				if (playing)
					telephony_info.headset_state =
						BLUETOOTH_STATE_PLAYING;
				else
					telephony_info.headset_state =
						BLUETOOTH_STATE_CONNECTED;
				g_hash_table_destroy(device_hash);
				g_hash_table_destroy(list_hash);
				goto done;
			}
			g_hash_table_destroy(device_hash);
		} else {
			BT_ERR("GetProperties 2 failed: [%s]",
						error->message);
			g_clear_error(&error);
		}
		g_hash_table_destroy(list_hash);
		g_free(gp_path);
		gp_path = NULL;
	}
done:
	if (value)
		g_variant_unref(value);
	if (result)
		g_variant_unref(result);
	if (iter)
		g_variant_iter_free(iter);
	if (prop_iter)
		g_variant_iter_free(prop_iter);
	g_free(gp_path);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
}

static gboolean __bluetooth_telephony_get_connected_device_path(void)
{
	gboolean flag = TRUE;
	char object_path[BT_ADAPTER_PATH_LEN] = {0};
	GVariant *result;
	GError *error = NULL;
	GDBusConnection *conn;
	BT_DBG("+");

	/*Get default adapter path*/
	if (__bluetooth_get_default_adapter_path(object_path) < 0)
		flag = FALSE;

	if (strlen(telephony_info.address) == 0)
		__bluetooth_telephony_get_connected_device();

	if (strlen(telephony_info.address) == 0) {
		flag = FALSE;
	}

	if (telephony_info.obj_path) {
		g_free(telephony_info.obj_path);
		telephony_info.obj_path = NULL;
	}
	BT_DBG("telephony addess is %s \n", telephony_info.address);
	BT_DBG("adapter object path is %s \n", object_path);
	conn = _bt_init_system_gdbus_conn();
	result = g_dbus_connection_call_sync(conn,
					BLUEZ_SERVICE_NAME,
					object_path,
					BLUEZ_ADAPTER_INTERFACE,
					"FindDevice",
					g_variant_new("(s)",
					telephony_info.address),
					G_VARIANT_TYPE("(o)"),
					/*G_VARIANT_TYPE_OBJECT_PATH,*/
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

	if (error) {
		BT_ERR("FindDevice failed: [%s]",
				error->message);
		if (result)
			g_variant_unref(result);
		flag = FALSE;
		g_clear_error(&error);
		return flag;
	}
	g_variant_get(result, "(o)", &telephony_info.obj_path);
	BT_DBG("object path is %s \n", telephony_info.obj_path);
	if (result)
		g_variant_unref(result);
	BT_DBG("-");
	return flag;
}

BT_EXPORT_API int bluetooth_telephony_init(bt_telephony_func_ptr cb,
					void  *user_data)
{
	int ret = BLUETOOTH_TELEPHONY_ERROR_NONE;
	GDBusConnection *conn = NULL;
	char object_path[BT_ADAPTER_PATH_LEN] = {0};
	BT_DBG("+");

	g_type_init();
	if (is_initialized == TRUE) {
		BT_ERR("Bluetooth telephony already initilized");
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_INITIALIZED;
	}

	is_initialized = TRUE;

	conn = _bt_init_system_gdbus_conn();
	if (!conn) {
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	/* Call Path */
	snprintf(telephony_info.call_path, sizeof(telephony_info.call_path),
					CSD_CALL_APP_PATH, getpid());
	BT_DBG("Call Path = %s", telephony_info.call_path);
	memset(telephony_info.address, 0x00, sizeof(telephony_info.address));

	if (__bluetooth_telephony_init()) {
		BT_ERR("__bluetooth_telephony_init failed\n");
		g_object_unref(conn);
		conn = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	telephony_dbus_info.monitor_filter_id_bluez_manager = 0;
	telephony_dbus_info.monitor_filter_id_bluez_manager =
				g_dbus_connection_signal_subscribe(conn,
				BLUEZ_SERVICE_NAME,
				BLUEZ_MANAGER_INTERFACE,  /* any interface */
				"AdapterAdded",  /* any member */
				"/",
				NULL,  /* arg0 */
				G_DBUS_SIGNAL_FLAGS_NONE,
				__bluetooth_telephony_adapter_added_cb,
				NULL,  /* user_data */
				NULL); /* user_data destroy notify */

	if (0 == telephony_dbus_info.monitor_filter_id_bluez_manager) {
		g_object_unref(conn);
		BT_ERR("AdapterAdded signal subscribe failed... \n");
		conn = NULL;
		is_initialized = FALSE;
		goto fail;
	}
	/*Callback and user applicaton data*/
	telephony_info.cb = cb;
	telephony_info.user_data = user_data;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

	telephony_dbus_info.monitor_filter_id_bluez_headset = 0;
	telephony_dbus_info.monitor_filter_id_bluez_headset =
			g_dbus_connection_signal_subscribe(conn,
			NULL,
			BLUEZ_HEADSET_INTERFACE,  /* any interface */
			"PropertyChanged",  /* any member */
			NULL,
			NULL,  /* arg0 */
			G_DBUS_SIGNAL_FLAGS_NONE,
			__bluetooth_telephony_bluez_headset_event_filter,
			NULL,  /* user_data */
			NULL); /* user_data destroy notify */
	if (0 == telephony_dbus_info.monitor_filter_id_bluez_headset) {
		BT_ERR("PropertyChanged signal subscribe failed... \n");
		g_object_unref(conn);
		conn = NULL;
		is_initialized = FALSE;
		goto fail;
	}

	telephony_dbus_info.monitor_filter_id_hfp_agent = 0;
	telephony_dbus_info.monitor_filter_id_hfp_agent =
				g_dbus_connection_signal_subscribe(conn,
				NULL,
				HFP_AGENT_SERVICE,  /* any interface */
				HFP_NREC_STATUS_CHANGE,  /* any member */
				NULL,
				NULL,  /* arg0 */
				G_DBUS_SIGNAL_FLAGS_NONE,
				__bluetooth_telephony_hfp_event_filter,
				NULL,  /* user_data */
				NULL); /* user_data destroy notify */

	if (0 == telephony_dbus_info.monitor_filter_id_hfp_agent) {
		BT_ERR("filter added subscribe failed... \n");
		g_object_unref(conn);
		conn = NULL;
		is_initialized = FALSE;
		goto fail;
	}

	/*Check for BT status*/
	ret = __bluetooth_get_default_adapter_path(object_path);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
		return BLUETOOTH_TELEPHONY_ERROR_NONE;

	/*Bluetooth is active, therefore set the flag */
	is_active = TRUE;

	ret = __bluetooth_telephony_register();
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("__bluetooth_telephony_register failed\n");
		goto fail;
	}

	BT_DBG("-");
	return ret;
fail:
	bluetooth_telephony_deinit();
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_deinit(void)
{
	GDBusConnection *conn = NULL;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();

	is_initialized = FALSE;

	conn = _bt_init_system_gdbus_conn();
	if (!conn) {
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_dbus_connection_signal_unsubscribe(conn,
		telephony_dbus_info.monitor_filter_id_bluez_headset);

	g_dbus_connection_signal_unsubscribe(conn,
		telephony_dbus_info.monitor_filter_id_hfp_agent);

	if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_ENABLED)
		__bluetooth_telephony_unregister();

	/*__bluetooth_telephony_proxy_deinit();*/
	telephony_info.cb = NULL;
	telephony_info.user_data = NULL;
	telephony_info.call_count = 0;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

	g_dbus_connection_signal_unsubscribe(conn,
		telephony_dbus_info.monitor_filter_id_bluez_manager);

	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API gboolean bluetooth_telephony_is_sco_connected(void)
{
	BT_DBG("+");
	GDBusConnection *conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("Bluetooth telephony not initilized");
		return FALSE;
	}

	/* To get the headset state */
	if (!__bluetooth_telephony_get_connected_device_path()) {
		return FALSE;
	}
	if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING)
		return TRUE;

	BT_DBG("-");
	return FALSE;
}

BT_EXPORT_API int bluetooth_telephony_is_nrec_enabled(gboolean *status)
{
	GVariant *reply = NULL;
	GError *err = NULL;
	GVariantIter *reply_iter;
	GDBusConnection *connection;
	GVariant *value;
	gchar *key;
	GHashTable *list_hash = NULL;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();
	connection = _bt_init_system_gdbus_conn();
	if (status == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	reply = g_dbus_connection_call_sync(connection,
					HFP_AGENT_SERVICE,
					HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE,
					"GetProperties",
					NULL,
					G_VARIANT_TYPE("(a{sv})"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);
	if (err) {
		BT_ERR("GetProperties GDBus error: %s", err->message);
		g_clear_error(&err);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	g_variant_get(reply, "(a{sv})", &reply_iter);
	list_hash = g_hash_table_new_full(g_str_hash,
					g_str_equal,
					NULL,
					NULL);

	while (g_variant_iter_next(reply_iter, "{sv}", &key, &value)) {
		BT_DBG("key is %s", key);
		g_hash_table_insert(list_hash, key, value);
	}

	value = g_hash_table_lookup(list_hash, "nrec");
	if (value != NULL) {
		*status = g_variant_get_boolean(value);
		BT_DBG("NREC status = [%d]", *status);
	}
	g_hash_table_destroy(list_hash);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_start_voice_recognition(void)
{
	GError *error = NULL;
	int ret;
	GDBusConnection *conn = NULL;
	BT_DBG("+");

	conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("Bluetooth telephony not initilized");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (!__bluetooth_telephony_get_connected_device_path()) {
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_dbus_connection_call_sync(conn,
				BLUEZ_SERVICE_NAME,
				telephony_info.obj_path,
				BLUEZ_HEADSET_INTERFACE,
				"SetVoiceDial",
				g_variant_new("(b)", TRUE),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
		BT_ERR("Getting DefaultAdapter failed: [%s]",
						error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_stop_voice_recognition(void)
{
	GError *error = NULL;
	int ret;
	GDBusConnection *conn = NULL;
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("Bluetooth telephony not initilized");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (!__bluetooth_telephony_get_connected_device_path()) {
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_dbus_connection_call_sync(conn,
				BLUEZ_SERVICE_NAME,
				telephony_info.obj_path,
				BLUEZ_HEADSET_INTERFACE,
				"SetVoiceDial",
				g_variant_new("(b)", FALSE),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
		BT_ERR("GDBus call : Play failed: [%s]",
						error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_audio_open(void)
{
	GError *error = NULL;
	int ret;
	GDBusConnection *conn = NULL;
	BT_DBG("+");

	conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("GDBus connection failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (!__bluetooth_telephony_get_connected_device_path()) {
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING)
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_CONNECTED;

	g_dbus_connection_call_sync(conn,
				BLUEZ_SERVICE_NAME,
				telephony_info.obj_path,
				BLUEZ_HEADSET_INTERFACE,
				"Play",
				NULL,
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
		BT_ERR("GDBus call : Play failed: [%s]",
					error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_audio_close(void)
{
	GError *error = NULL;
	int ret;
	GDBusConnection *conn = NULL;
	BT_DBG("+");
	conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("GDBus connection failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (!__bluetooth_telephony_get_connected_device_path()) {
		BT_DBG("__bluetooth_telephony_get_connected_device_path error");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state != BLUETOOTH_STATE_PLAYING) {
		BT_DBG("headset state not playing");
		return BLUETOOTH_TELEPHONY_ERROR_NOT_CONNECTED;
	}

	g_dbus_connection_call_sync(conn,
				BLUEZ_SERVICE_NAME,
				telephony_info.obj_path,
				BLUEZ_HEADSET_INTERFACE,
				"Stop",
				NULL,
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
		BT_ERR("GDBus call : Stop failed: [%s]",
					error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_call_remote_ringing(unsigned int call_id)
{
	int ret;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	BT_DBG("+");

	/*Make sure SCO is already connected */
	ret = __bluetooth_telephony_send_call_status(
				CSD_CALL_STATUS_MO_ALERTING, call_id);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_call_answered(unsigned int call_id,
							unsigned int bt_audio)
{
	int ret;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_ACTIVE,
								call_id);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return ret;
	}

	if (bt_audio) {
		if (!bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_open();
			if (ret != 0) {
				BT_ERR("Audio connection call Failed = %d",
								ret);
				return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			}
		}
	}

	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_end(unsigned int call_id)
{
	int ret;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(
			CSD_CALL_STATUS_MT_RELEASE,
			call_id);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return ret;
	}
	if (telephony_info.call_count > 0)
		telephony_info.call_count = telephony_info.call_count - 1;

	if (telephony_info.call_count  == 0) {
		if (bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_close();
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
				BT_ERR(" Failed = [%d]", ret);
				return ret;
			}
		}
	}
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_held(unsigned int call_id)
{
	int ret;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_HOLD,
								call_id);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
	}
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_retrieved(unsigned int call_id)
{
	int ret;
	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_ACTIVE,
								call_id);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
	}
	BT_DBG("-");
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_swapped(void *call_list,
				unsigned int call_count)
{
	int i;
	int ret;
	GList *list = call_list;
	bt_telephony_call_status_info_t *call_status;

	BT_DBG("+");

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == list) {
		BT_ERR("call_list is invalid");
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;
	}

	BT_DBG(" call_count = [%d]", call_count);

	for (i = 0; i < call_count; i++) {
		call_status = g_list_nth_data(list, i);

		if (NULL == call_status)
			continue;

		BT_DBG(" %d : Call id [%d] status[%d]", i,
					call_status->call_id,
					call_status->call_status);

		switch (call_status->call_status) {
		case BLUETOOTH_CALL_STATE_HELD:
			ret = __bluetooth_telephony_send_call_status(
						CSD_CALL_STATUS_HOLD,
						call_status->call_id);
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
				BT_ERR("Failed = %d", ret);
				return ret;
			}
		break;

		case BLUETOOTH_CALL_STATE_CONNECTED:
			ret = __bluetooth_telephony_send_call_status(
					CSD_CALL_STATUS_ACTIVE,
					call_status->call_id);
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
				BT_ERR("Failed = [%d]", ret);
				return ret;
			}
		break;

		default:
			if ((call_status->call_status <
				BLUETOOTH_CALL_STATE_NONE) ||
				(call_status->call_status >=
				BLUETOOTH_CALL_STATE_ERROR)) {
				BT_ERR("Unknown Call state");
				return BLUETOOTH_TELEPHONY_ERROR_NOT_AVAILABLE;
			}
		}
	}

	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_set_call_status(void *call_list,
				unsigned int call_count)
{
	int ret;

	BT_DBG("+");

	ret = bluetooth_telephony_call_swapped(call_list, call_count);

	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("Failed = [%d]", ret);
		return ret;
	}

	telephony_info.call_count = call_count;

	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_indicate_outgoing_call(
			const char *ph_number, unsigned int call_id,
			unsigned int bt_audio)
{
	GVariant *reply = NULL;
	GError *err = NULL;
	const char *path = telephony_info.call_path;
	int ret;
	GDBusConnection *conn;
	BT_DBG("+");

	conn  = _bt_init_system_gdbus_conn();

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	reply = g_dbus_connection_call_sync(conn,
					HFP_AGENT_SERVICE,
					HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE,
					"OutgoingCall",
					g_variant_new("(ssi)",
					path,
					ph_number,
					call_id),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_clear_error(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_free(reply);

	telephony_info.call_count++;
	BT_DBG(" ag_info.ag_call_count = [%d]", telephony_info.call_count);

	if (bt_audio) {
		if (!bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_open();
			if (ret != 0) {
				BT_ERR(" Audio connection call Failed = %d",
									ret);
				return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			}
		}
	}

	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_indicate_incoming_call(
		const char *ph_number, unsigned int call_id)
{
	GVariant *reply;
	GError *err = NULL;
	const char *path = telephony_info.call_path;
	int ret;
	GDBusConnection *conn;
	BT_DBG("+");

	conn = _bt_init_system_gdbus_conn();

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	reply = g_dbus_connection_call_sync(conn,
					HFP_AGENT_SERVICE,
					HFP_AGENT_PATH,
					HFP_AGENT_INTERFACE,
					"IncomingCall",
					g_variant_new("(ssi)",
							path,
							ph_number,
							call_id),
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&err);


	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_clear_error(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_free(reply);

	telephony_info.call_count++;
	BT_DBG("telephony_info.call_count = [%d]", telephony_info.call_count);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_set_speaker_gain(
			unsigned short speaker_gain)
{
	GDBusConnection *conn = NULL;
	GVariantBuilder *builder;
	char *spkr_gain_str = "SpeakerGain";
	int ret = BLUETOOTH_TELEPHONY_ERROR_NONE;
	GError *error = NULL;
	GVariant *value;
	BT_DBG("+");
	BT_DBG("set speaker_gain= [%d]", speaker_gain);

	conn = _bt_init_system_gdbus_conn();
	if (conn == NULL) {
		BT_ERR("GDBus connection failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.obj_path == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;

	builder = g_variant_builder_new(G_VARIANT_TYPE_INT16);
	g_variant_builder_add(builder, "q", speaker_gain);
	value = g_variant_builder_end(builder);
	g_variant_builder_unref(builder);
	g_dbus_connection_call_sync(conn,
				BLUEZ_SERVICE_NAME,
				telephony_info.obj_path,
				BLUEZ_HEADSET_INTERFACE,
				"SetProperty",
				g_variant_new("(sv)",
					spkr_gain_str,
					value),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				&error);

	if (error != NULL) {
		BT_ERR("GDBus call : SetProperty failed: [%s]",
						error->message);
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_get_headset_volume(
				unsigned int *speaker_gain)
{
	GHashTable *hash = NULL;
	GVariant *result;
	GError *error = NULL;
	int ret;
	char *key;
	GVariant *value;
	GVariantIter iter;
	GDBusConnection *conn = NULL;
	conn = _bt_init_system_gdbus_conn();

	if (conn == NULL) {
		BT_ERR("GDBus connection failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_DBG("+");
	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (!__bluetooth_telephony_get_connected_device_path()) {
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	result = g_dbus_connection_call_sync(conn,
					BLUEZ_SERVICE_NAME,
					telephony_info.obj_path,
					BLUEZ_HEADSET_INTERFACE,
					"GetProperties",
					NULL,
					G_VARIANT_TYPE("(a{sv})"),
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);
	if (error != NULL) {
		ret = __bt_telephony_get_error(error->message);
		g_clear_error(&error);
		return ret;
	}
	hash = g_hash_table_new_full(g_str_hash,
					g_str_equal,
					NULL,
					NULL);
	g_variant_iter_init(&iter, result);
	while (g_variant_iter_next(&iter, "{sv}", &key, &value))
		g_hash_table_insert(hash, key, value);

	if (hash == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	value = g_hash_table_lookup(hash, "SpeakerGain");
	g_variant_get(value, "q", &speaker_gain);

	g_hash_table_destroy(hash);
	if (result)
		g_variant_unref(result);
	g_variant_iter_free(&iter);
	if (value)
		g_variant_unref(value);
	BT_DBG("-");
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}
