/*
 * Bluetooth-telephony
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:	Hocheol Seo <hocheol.seo@samsung.com>
 *		Jaganath K <jaganath.k@samsung.com>
 *		Chanyeol Park <chanyeol.park@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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
#include <dbus/dbus.h>
#include <dbus/dbus-glib-bindings.h>
#include <unistd.h>
#include <vconf.h>
#include <vconf-keys.h>

#include "bt-common.h"
#include "bluetooth-telephony-api.h"
#include "marshal.h"

#define BT_SCO_TIMEOUT 3000

#define BT_CVSD_CODEC_ID 1
#define BT_MSBC_CODEC_ID 2

typedef struct {
	GDBusConnection *conn;
	GDBusProxy *proxy;
	GDBusProxy *dbus_proxy;
	GDBusProxy *manager_proxy;
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


char *src_addr = NULL;


#define BLUETOOTH_TELEPHONY_ERROR (__bluetooth_telephony_error_quark())

#define BLUEZ_SERVICE_NAME "org.bluez"
#define BLUEZ_HEADSET_INTERFACE "org.bluez.Headset"

#define BLUEZ_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"
#define BLUEZ_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define HFP_AGENT_SERVICE "org.bluez.ag_agent"


#define HFP_AGENT_PATH "/org/bluez/hfp_agent"
#define HFP_AGENT_INTERFACE "Org.Hfp.App.Interface"

#define TELEPHONY_APP_INTERFACE "org.tizen.csd.Call.Instance"
#define CSD_CALL_APP_PATH "/org/tizen/csd/%d"
#define HFP_NREC_STATUS_CHANGE "NrecStatusChanged"
#define HFP_ANSWER_CALL "Answer"
#define HFP_REJECT_CALL "Reject"
#define HFP_RELEASE_CALL "Release"
#define HFP_THREEWAY_CALL "Threeway"

#define DEFAULT_ADAPTER_OBJECT_PATH "/org/bluez/hci0"

/*Below Inrospection data is exposed to bluez from agent*/
static const gchar bt_telephony_introspection_xml[] =
"<node name='/'>"
" <interface name='org.tizen.csd.Call.Instance'>"
"     <method name='SendDtmf'>"
"          <arg type='s' name='dtmf' direction='in'/>"
"     </method>"
"     <method name='VendorCmd'>"
"          <arg type='s' name='vendor' direction='in'/>"
"     </method>"
" </interface>"
"</node>";

#define BT_TELEPHONY_CHECK_ENABLED() \
	do { \
		if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED) \
		{ \
			BT_ERR("BT is not enabled"); \
			return BLUETOOTH_TELEPHONY_ERROR_NOT_ENABLED; \
		} \
	} while (0)

static gboolean is_initialized = FALSE;
#define BT_TELEPHONY_CHECK_INITIALIZED() \
	do { \
		if (is_initialized == FALSE) \
		{ \
			BT_ERR("Bluetooth telephony not initilized"); \
			return BLUETOOTH_TELEPHONY_ERROR_NOT_INITIALIZED; \
		} \
	} while (0)

static void __bt_telephony_adapter_filter(GDBusConnection *connection,
					const gchar *sender_name,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *signal_name,
					GVariant *parameters,
					gpointer user_data);

static int __bt_telephony_get_src_addr(GVariant *value);

static bt_telephony_info_t telephony_info;
static telephony_dbus_info_t telephony_dbus_info;
static gboolean is_active = FALSE;

/*Function Declaration*/
static int __bt_telephony_get_error(const char *error_message);
static void __bt_telephony_event_cb(int event, int result, void *param_data);
static GQuark __bluetooth_telephony_error_quark(void);
static GVariant *__bluetooth_telephony_dbus_method_send(const char *path,
		const char *interface, const char *method,
		GError **err, GVariant *parameters);
static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id, const char *ph_number);
static void __bluetooth_telephony_error(GDBusMethodInvocation *invocation,
		bluetooth_telephony_error_t error, const char *err_msg);

static void __bluetooth_telephony_event_filter(GDBusConnection *connection,
						 const gchar *sender_name,
						 const gchar *object_path,
						 const gchar *interface_name,
						 const gchar *signal_name,
						 GVariant *parameters,
						 gpointer user_data);

static int __bluetooth_telephony_proxy_init(void);
static void __bluetooth_telephony_proxy_deinit(void);
static int __bluetooth_telephony_register(void);
static int __bluetooth_telephony_unregister(void);

static gboolean __bluetooth_telephony_is_headset(uint32_t device_class);
static int __bluetooth_telephony_get_connected_device(void);
static GDBusProxy *__bluetooth_telephony_get_connected_device_proxy(void);

/*Function Definition*/
static void __bt_telephony_method(GDBusConnection *connection,
			const gchar *sender,
			const gchar *object_path,
			const gchar *interface_name,
			const gchar *method_name,
			GVariant *parameters,
			GDBusMethodInvocation *invocation,
			gpointer user_data)
{
	FN_START;

	BT_INFO("method %s", method_name);
	BT_INFO("object_path %s", object_path);

	if (g_strcmp0(method_name, "SendDtmf") == 0) {
		gchar *dtmf;
		gsize len;
		telephony_event_dtmf_t call_data = { 0, };

		dtmf = (gchar *)g_variant_get_string(parameters, &len);

		if (dtmf == NULL) {
			BT_ERR("Number dial failed");
			__bluetooth_telephony_error(invocation,
					BLUETOOTH_TELEPHONY_ERROR_INVALID_DTMF,
					"Invalid dtmf");
		} else {
			DBG_SECURE("Dtmf = %s", dtmf);

			call_data.dtmf = g_strdup(dtmf);
			__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);

			g_free(call_data.dtmf);

			g_dbus_method_invocation_return_value(invocation, NULL);
		}
	} else if (g_strcmp0(method_name, "VendorCmd") == 0) {
		gchar *at_cmd;
		gsize len;

		at_cmd = (gchar *)g_variant_get_string(parameters, &len);
		BT_INFO("Vendor %s", at_cmd);
		if (at_cmd == NULL) {
			BT_ERR("Vendor command is NULL\n");
			__bluetooth_telephony_error(invocation,
					BLUETOOTH_TELEPHONY_ERROR_APPLICATION,
					"Invalid at vendor cmd");
		} else {
			DBG_SECURE("Vendor AT cmd = %s", at_cmd);

			__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_VENDOR_AT_CMD,
				BLUETOOTH_TELEPHONY_ERROR_NONE, at_cmd);

			g_dbus_method_invocation_return_value(invocation, NULL);
		}
	}

	BT_INFO("-");
}

static const GDBusInterfaceVTable method_table = {
	__bt_telephony_method,
	NULL,
	NULL,
};

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
	else if (g_strrstr(error_message, BT_ACCESS_DENIED_MSG))
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	else
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
}

static int __bt_telephony_check_privilege(void)
{
	GVariant *reply;
	GError *err = NULL;
	int ret;

	FN_START;
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"CheckPrivilege", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	g_variant_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
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

static GVariant *__bluetooth_telephony_dbus_method_send(const char *path,
		const char *interface, const char *method,
		GError **err, GVariant *parameters)
{
#ifdef TIZEN_WEARABLE
	int timeout = 4000;
#else
	int timeout = -1;
#endif
	GVariant *reply;
	GDBusProxy *proxy;
	GDBusConnection *conn;

	FN_START;

	conn = telephony_dbus_info.conn;
	retv_if(conn == NULL, NULL);

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE,
			NULL, HFP_AGENT_SERVICE, path, interface, NULL, err);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy");
		return NULL;
	}

	reply = g_dbus_proxy_call_sync(proxy, method, parameters,
				G_DBUS_CALL_FLAGS_NONE, timeout, NULL, err);

	g_object_unref(proxy);
	FN_END;
	return reply;
}

static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id, const char *ph_number)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	char *path = g_strdup(telephony_info.call_path);
	char *phone_number;
	int ret;

	FN_START;

	if (NULL == ph_number)
		phone_number = g_strdup("");
	else
		phone_number = g_strdup(ph_number);

	param = g_variant_new("(ssii)", path, phone_number,
			call_status, call_id);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"ChangeCallStatus", &err, param);

	g_free(path);
	g_free(phone_number);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);

	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_error(GDBusMethodInvocation *invocation,
		bluetooth_telephony_error_t error, const char *err_msg)
{
	g_dbus_method_invocation_return_error(invocation,
			BLUETOOTH_TELEPHONY_ERROR, error,
			err_msg, NULL);
}

static void __bluetooth_telephony_answer_call(GVariant *var)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;

	g_variant_get(var, "(u)", &callid);
	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_release_call(GVariant *var)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;

	g_variant_get(var, "(u)", &callid);
	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_reject_call(GVariant *var)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;

	g_variant_get(var, "(u)", &callid);
	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_REJECT_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void  *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_threeway_call(GVariant *var)
{
	int event = 0;
	unsigned int chld_value;

	FN_START;

	g_variant_get(var, "(u)", &chld_value);
	BT_DBG("chld value  = [%d]", chld_value);

	switch (chld_value) {
	case 0:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_0_RELEASE_ALL_HELD_CALL;
		break;
	case 1:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_1_RELEASE_ALL_ACTIVE_CALL;
		break;
	case 2:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_2_ACTIVE_HELD_CALL;
		break;
	case 3:
		event = BLUETOOTH_EVENT_TELEPHONY_CHLD_3_MERGE_CALL;
		break;
	default:
		BT_ERR("Invalid CHLD command");
		return;
	}

	__bt_telephony_event_cb(event,
			BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
	FN_END;
}

static void __bluetooth_handle_nrec_status_change(GVariant *var)
{
	gboolean status = FALSE;

	g_variant_get(var, "(b)", &status);
	BT_INFO("NREC status = %d", status);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED,
		BLUETOOTH_TELEPHONY_ERROR_NONE, (void *)&status);

}

static void __bluetooth_telephony_event_filter(GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{
	BT_DBG("+");

	if (strcasecmp(interface_name, HFP_AGENT_SERVICE) == 0) {
		if (strcasecmp(signal_name, HFP_NREC_STATUS_CHANGE) == 0) {
			__bluetooth_handle_nrec_status_change(parameters);
		} else if (strcasecmp(signal_name, HFP_ANSWER_CALL) == 0) {
			__bluetooth_telephony_answer_call(parameters);
		} else if (strcasecmp(signal_name, HFP_REJECT_CALL) == 0) {
			__bluetooth_telephony_reject_call(parameters);
		} else if (strcasecmp(signal_name, HFP_RELEASE_CALL) == 0) {
			__bluetooth_telephony_release_call(parameters);
		} else if (strcasecmp(signal_name, HFP_THREEWAY_CALL) == 0) {
			__bluetooth_telephony_threeway_call(parameters);
		}
	} else if (strcasecmp(interface_name, BLUEZ_HEADSET_INTERFACE) == 0) {
		if (strcasecmp(signal_name, "PropertyChanged") == 0) {
			GVariant *values;
			gchar *property;

			/*Parse (sv)*/
			BT_DBG("Type of Parameters= %s", g_variant_get_type_string(parameters));

			g_variant_get(parameters, "(sv)", &property, &values);
			BT_DBG("Property: %s", property);

			if (strcasecmp(property, "State") == 0) {
				gchar *state;
				gsize len;
				BT_DBG("Type of values %s", g_variant_get_type_string(values));
				state = (gchar *)g_variant_get_string(values, &len);

				if (NULL == state) {
					BT_ERR("State is null");
					return;
				}
				BT_DBG("state: %s", state);
				if (g_strcmp0(state, "connected") == 0) {
					telephony_info.headset_state = BLUETOOTH_STATE_CONNECTED;
				} else if (g_strcmp0(state, "playing") == 0) {
					telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
				} else if (g_strcmp0(state, "disconnected") == 0) {
					/* Headset state: playing -> disconnected */
					if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING) {
						__bt_telephony_event_cb(
								BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
								BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
					}

					telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;
				}
			} else if (strcasecmp(property, "Connected") == 0) {
				gboolean connected = FALSE;
				char *dev_addr = NULL;
				BT_DBG("Type of values %s", g_variant_get_type_string(values));
				connected = g_variant_get_boolean(values);
				BT_INFO("connected %d", connected);
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

						if (telephony_dbus_info.proxy != NULL) {
							g_object_unref(telephony_dbus_info.proxy);
							telephony_dbus_info.proxy = NULL;
						}

						telephony_dbus_info.proxy =
								__bluetooth_telephony_get_connected_device_proxy();

						BT_INFO("Headset Connected");

						__bt_telephony_event_cb(
								BLUETOOTH_EVENT_TELEPHONY_HFP_CONNECTED,
								BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
					}
				} else { /*Device disconnected*/
					memset(telephony_info.address, 0x00,
							sizeof(telephony_info.address));
					telephony_info.headset_state =
							BLUETOOTH_STATE_DISCONNETED;

					if (telephony_dbus_info.proxy != NULL) {
						g_object_unref(telephony_dbus_info.proxy);
						telephony_dbus_info.proxy = NULL;
					}

					BT_INFO("Headset Disconnected");

					__bt_telephony_event_cb(
							BLUETOOTH_EVENT_TELEPHONY_HFP_DISCONNECTED,
							BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
				}
			} else if (strcasecmp(property, "SpeakerGain") == 0) {
				unsigned int spkr_gain;
				guint16 gain;

				BT_DBG("Type of values %s", g_variant_get_type_string(values));
				gain = g_variant_get_uint16(values);
				spkr_gain = (unsigned int)gain;
				BT_DBG("spk_gain[%d]", spkr_gain);

				__bt_telephony_event_cb(
						BLUETOOTH_EVENT_TELEPHONY_SET_SPEAKER_GAIN,
						BLUETOOTH_TELEPHONY_ERROR_NONE,
						(void *)&spkr_gain);
			} else if (strcasecmp(property, "MicrophoneGain") == 0) {
				unsigned int mic_gain;
				guint16 gain;

				BT_DBG("Type of values %s", g_variant_get_type_string(values));
				gain = g_variant_get_uint16(values);
				mic_gain = (unsigned int)gain;
				BT_DBG("mic_gain[%d]", mic_gain);

				__bt_telephony_event_cb(
						BLUETOOTH_EVENT_TELEPHONY_SET_MIC_GAIN,
						BLUETOOTH_TELEPHONY_ERROR_NONE,
						(void *)&mic_gain);
			} else if (strcasecmp(property, "Playing") == 0) {
				gboolean audio_sink_playing = FALSE;

				BT_DBG("Type of values %s", g_variant_get_type_string(values));
				audio_sink_playing = g_variant_get_boolean(values);

				if (audio_sink_playing) {
					telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
					__bt_telephony_event_cb(
							BLUETOOTH_EVENT_TELEPHONY_AUDIO_CONNECTED,
							BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
				} else {
					telephony_info.headset_state =
							BLUETOOTH_STATE_CONNECTED;
					__bt_telephony_event_cb(
							BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
							BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
				}
			}

			g_variant_unref(values);
		}
	}
	BT_DBG("-");
}

static GDBusNodeInfo *__bt_telephony_create_method_node_info
					(const gchar *introspection_data)
{
	GError *err = NULL;
	GDBusNodeInfo *node_info = NULL;

	if (introspection_data == NULL)
		return NULL;

	node_info = g_dbus_node_info_new_for_xml(introspection_data, &err);

	if (err) {
		BT_ERR("Unable to create node: %s", err->message);
		g_clear_error(&err);
	}
	return node_info;
}


int __bluetooth_telephony_register_object(int reg, GDBusNodeInfo *node_info)
{
	static guint bt_tel_id = 0;
	GError *error =  NULL;
	gchar *path;

	if (reg == TRUE) {
		if (node_info == NULL)
			return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

		path = g_strdup(telephony_info.call_path);
		BT_DBG("path is [%s]", path);

		bt_tel_id = g_dbus_connection_register_object(telephony_dbus_info.conn,
				path, node_info->interfaces[0],
				&method_table,
				NULL, NULL, &error);

		g_free(path);
		if (bt_tel_id == 0) {
			BT_ERR("Failed to register: %s", error->message);
			g_error_free(error);
			return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
		}
	} else {
		if (bt_tel_id > 0) {
			g_dbus_connection_unregister_object(telephony_dbus_info.conn,
					bt_tel_id);
			bt_tel_id = 0;
		}
	}

	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static int __bluetooth_telephony_proxy_init(void)
{
	FN_START;
	guint owner_id;
	GDBusNodeInfo *node_info;

	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				TELEPHONY_APP_INTERFACE,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				NULL, NULL, NULL,
				NULL, NULL);
	BT_DBG("owner_id is [%d]", owner_id);

	node_info = __bt_telephony_create_method_node_info(
				bt_telephony_introspection_xml);

	if (node_info == NULL) {
		BT_ERR("node_info NULL");
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;
	}
	if (__bluetooth_telephony_register_object(TRUE, node_info) !=
			BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("Registation of Method Failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_proxy_deinit(void)
{
	FN_START;

	__bluetooth_telephony_register_object(FALSE, NULL);

	FN_END;
	return;
}

static int __bluetooth_telephony_register(void)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	char *path = g_strdup(telephony_info.call_path);
	int ret;

	FN_START;

	param = g_variant_new("(ss)", path, src_addr);
	BT_DBG("Path[%s] Src_Address[%s]", path, src_addr);
	BT_DBG("Variant Type %s", g_variant_get_type_string(param));

	reply =  __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"RegisterApplication", &err, param);

	g_free(path);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	BT_DBG("__bluetooth_telephony_register completed");
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static  int __bluetooth_telephony_unregister(void)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	char *path = g_strdup(telephony_info.call_path);
	int ret;

	FN_START;

	param = g_variant_new("(s)", path);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"UnregisterApplication", &err, param);

	g_free(path);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	BT_DBG("__bluetooth_telephony_unregister completed");
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

#ifndef TIZEN_WEARABLE
static void __bluetooth_telephony_init_headset_state(void)
{
	GVariant *reply;
	GError *err = NULL;
	gboolean status = FALSE;

	FN_START;

	if (telephony_dbus_info.conn == NULL) {
		BT_ERR("Bluetooth telephony not initilized");
		return;
	}

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IsConnected", &err, NULL);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			__bt_telephony_get_error(err->message);
			g_error_free(err);
		}
		return;
	}

	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));
	g_variant_get(reply, "(b)", &status);
	g_variant_unref(reply);

	BT_INFO("Headset Connected Status = [%d]", status);
	if (status)
		telephony_info.headset_state = BLUETOOTH_STATE_CONNECTED;
	else
		return;

	if (bluetooth_telephony_is_sco_connected())
		telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;

	FN_END;
}
#endif

static gboolean __bluetooth_telephony_is_headset(uint32_t device_class)
{
	gboolean flag = FALSE;
	FN_START;

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

	/* Tizen Wearable device */
	case 0x07:
		switch ((device_class & 0xfc) >> 2) {
		case 0x01: /* Wrist Watch */
			flag = TRUE;
			break;
		default:
			break;
		}
		break;
	}
	BT_DBG("[%d]", flag);
	FN_END;
	return flag;
}

static gboolean __bluetooth_telephony_is_headset_by_uuid(gchar **uuids)
{
	int i;
	char **parts;
	unsigned int service = 0;

	FN_START;

	retv_if(uuids == NULL, FALSE);

	for (i = 0; uuids[i] != NULL; i++) {
		parts = g_strsplit(uuids[i], "-", -1);

		if (parts == NULL || parts[0] == NULL) {
			g_strfreev(parts);
			continue;
		}

		service = g_ascii_strtoull(parts[0], NULL, 16);
		g_strfreev(parts);

		if (service == BLUETOOTH_HS_PROFILE_UUID ||
				service == BLUETOOTH_HF_PROFILE_UUID)
			return TRUE;
	}

	FN_END;
	return FALSE;
}


static int __bluetooth_telephony_get_connected_device(void)
{
	GDBusConnection *conn;
	GDBusProxy *headset_agent_proxy = NULL;
	GDBusProxy *manager_proxy = NULL;
	GDBusProxy *proxy = NULL;
	GVariant *reply = NULL;
	GVariant *getall = NULL;
	GVariant *isPlayingReply = NULL;
	GVariant *isConnectedReply = NULL;
	GVariant *param = NULL;
	GVariant *var_path = NULL;
	GVariant *path_values = NULL;
	GVariant *value = NULL;
	GError *error = NULL;
	GVariantIter iter;
	GVariantIter iter_path;
	GVariantIter property_iter;
	int ret = BLUETOOTH_TELEPHONY_ERROR_NONE;

	FN_START;
	conn = _bt_gdbus_get_system_gconn();
	retv_if(conn == NULL, BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

	manager_proxy = g_dbus_proxy_new_sync(
			conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
			BLUEZ_SERVICE_NAME, "/",
			BLUEZ_MANAGER_INTERFACE, NULL, &error);
	if (manager_proxy == NULL) {
		BT_ERR("Unable to allocate new proxy \n");
		ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
		if (error) {
			ret = __bt_telephony_get_error(error->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(error);
		}
		goto done;
	}

	/* Synchronous call */
	reply = g_dbus_proxy_call_sync(manager_proxy, "GetManagedObjects", NULL,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
	g_object_unref(manager_proxy);

	if (!reply) {
		BT_ERR("Can't get managed objects");
		ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
		if (error) {
			ret = __bt_telephony_get_error(error->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(error);
		}
		goto done;
	}

	/* signature of GetManagedObjects:	a{oa{sa{sv}}} */
	g_variant_iter_init(&iter, reply);

	while ((param = g_variant_iter_next_value(&iter))) {
		g_variant_iter_init(&iter_path, param);

		while ((var_path = g_variant_iter_next_value(&iter_path))) {
			gsize len;
			uint32_t device_class = 0;
			gboolean playing = FALSE;
			gboolean connected = FALSE;
			char *object_path = NULL;
			gchar *address = NULL;
			const gchar *key;
			gchar **uuids = NULL;
			GVariant *getall_param = NULL;

			g_variant_get(var_path, "{o*}", &object_path,
					&path_values);
			g_variant_unref(path_values); /* path_values unused*/

			proxy = g_dbus_proxy_new_sync(telephony_dbus_info.conn,
					G_DBUS_PROXY_FLAGS_NONE, NULL,
					BLUEZ_SERVICE_NAME, object_path,
					BLUEZ_PROPERTIES_INTERFACE, NULL, &error);
			if (proxy == NULL) {
				BT_ERR("Unable to allocate new proxy \n");
				ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
				if (error) {
					ret = __bt_telephony_get_error(error->message);
					BT_ERR("Error here %d\n", ret);
					g_error_free(error);
				}
				goto done;
			}


			getall_param = g_variant_new("s", BLUEZ_DEVICE_INTERFACE);
			getall = g_dbus_proxy_call_sync(proxy,
					"GetAll", getall_param,
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
			g_object_unref(proxy);
			g_variant_unref(getall_param);

			if (!getall) {
				BT_ERR("Can't get managed objects");
				ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
				if (error) {
					ret = __bt_telephony_get_error(error->message);
					BT_ERR("Error here %d\n", ret);
					g_error_free(error);
				}
				goto done;
			}

			g_variant_iter_init(&property_iter, getall);

			while (g_variant_iter_loop(&property_iter, "{sv}", &key, &value)) {
				if (!g_strcmp0(key, "Class")) {
					device_class = g_variant_get_uint32(value);
					BT_DBG("Device Class: %d", device_class);
				} else if (!g_strcmp0(key, "UUID")) {
					int i = 0;
					uuids = (gchar **)g_variant_get_strv(value, &len);
					BT_DBG_UUID(uuids, len, i);
				} else if (!g_strcmp0(key, "Address")) {
					address = (gchar *)g_variant_get_string(value, &len);
					BT_DBG("Device Class: %s", address);
				}
				g_variant_unref(value);
			}
			g_variant_unref(getall);

			if (device_class == 0) {
				BT_DBG("COD is NULL (maybe paired by nfc)...  Checking UUIDs");
				if (!__bluetooth_telephony_is_headset_by_uuid(uuids)) {
					BT_DBG("UUID checking completed. None HF device");
					continue;
				}
				BT_DBG("UUID checking completed. HF device");
			} else {
				if (!__bluetooth_telephony_is_headset(device_class)) {
					continue;
				}
			}

			/* this is headset; Check for Connection */
			headset_agent_proxy = g_dbus_proxy_new_sync(telephony_dbus_info.conn,
					G_DBUS_PROXY_FLAGS_NONE, NULL,
					HFP_AGENT_SERVICE, object_path,
					HFP_AGENT_INTERFACE, NULL, &error);
			if (headset_agent_proxy == NULL) {
				BT_ERR("Unable to allocate new headset_agent_proxy");
				ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
				if (error) {
					ret = __bt_telephony_get_error(error->message);
					BT_ERR("Error here %d\n", ret);
					g_error_free(error);
				}
				goto done;
			}

			isConnectedReply = g_dbus_proxy_call_sync(headset_agent_proxy,
					"IsConnected", NULL,
					G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);


			if (!isConnectedReply) {
				BT_ERR("Can't get managed objects");
				ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
				if (error) {
					ret = __bt_telephony_get_error(error->message);
					BT_ERR("Error here %d\n", ret);
					g_error_free(error);
				}
				goto done;
			} else {
				connected = g_variant_get_boolean(isConnectedReply);
				g_variant_unref(isConnectedReply);

				if (connected) {
					g_strlcpy(telephony_info.address,
							address,
							sizeof(telephony_info.address));

					isPlayingReply = g_dbus_proxy_call_sync(headset_agent_proxy,
							"IsPlaying", NULL,
							G_DBUS_CALL_FLAGS_NONE,
							-1, NULL, &error);
					if (!isPlayingReply) {
						BT_ERR("Can't get managed objects");
						ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
						if (error) {
							ret = __bt_telephony_get_error(error->message);
							BT_ERR("Error here %d\n", ret);
							g_error_free(error);
						}
					} else {
						playing = g_variant_get_boolean(isPlayingReply);
						g_variant_unref(isPlayingReply);

						if (playing)
							telephony_info.headset_state =
								BLUETOOTH_STATE_PLAYING;
						else
							telephony_info.headset_state =
								BLUETOOTH_STATE_CONNECTED;
					}

					goto done;
				}
			}

			g_object_unref(headset_agent_proxy);
			if (var_path) {
				g_variant_unref(var_path);
				var_path = NULL;
			}
		}

		if (param) {
			g_variant_unref(param);
			param = NULL;
		}

	}

done:
	if (headset_agent_proxy)
		g_object_unref(headset_agent_proxy);
	if (reply)
		g_variant_unref(reply);
	if (var_path)
		g_variant_unref(var_path);
	if (param)
		g_variant_unref(param);
	FN_END;
	return ret;
}

static GDBusProxy *__bluetooth_telephony_get_connected_device_proxy(void)
{
	GDBusProxy *proxy = NULL;
	GError *error = NULL;
	int ret;
	FN_START;

	if (strlen(telephony_info.address) == 0)
		__bluetooth_telephony_get_connected_device();

	if (strlen(telephony_info.address) == 0)
		return NULL;

	proxy = g_dbus_proxy_new_sync(telephony_dbus_info.conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			HFP_AGENT_SERVICE, HFP_AGENT_PATH,
			HFP_AGENT_INTERFACE, NULL, &error);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy");
		if (error) {
			ret = __bt_telephony_get_error(error->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(error);
		}
		return NULL;
	}

	FN_END;
	return proxy;
}

int __bt_telephony_subscribe_adapter_signal(GDBusConnection *conn,
		int subscribe)
{
	if (conn == NULL)
		return -1;

	static int subscribe_adapter_id = -1 ;
	if (subscribe == TRUE) {
		if (subscribe_adapter_id == -1) {
			subscribe_adapter_id = g_dbus_connection_signal_subscribe(conn,
					NULL, "org.freedesktop.DBus.ObjectManager",
					"InterfacesAdded", NULL, NULL, 0,
					__bt_telephony_adapter_filter,
					NULL, NULL);
		}
		return BLUETOOTH_TELEPHONY_ERROR_NONE;
	} else {
		if (subscribe_adapter_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_adapter_id);
			subscribe_adapter_id = -1;
		}
		return BLUETOOTH_TELEPHONY_ERROR_NONE;
	}
}

int __bt_telephony_event_subscribe_signal(GDBusConnection *conn,
		int subscribe)
{
	if (conn == NULL)
		return -1;

	static int subscribe_event1_id = -1 ;
	static int subscribe_event2_id = -1 ;
	static int subscribe_event3_id = -1 ;
	static int subscribe_event4_id = -1 ;
	static int subscribe_event5_id = -1 ;
	static int subscribe_event6_id = -1 ;
	if (subscribe == TRUE) {
		if (subscribe_event1_id == -1) {
			subscribe_event1_id = g_dbus_connection_signal_subscribe(conn,
					NULL, BLUEZ_HEADSET_INTERFACE,
					"PropertyChanged", NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}
		if (subscribe_event2_id == -1) {
			subscribe_event2_id = g_dbus_connection_signal_subscribe(conn,
					NULL, HFP_AGENT_SERVICE,
					HFP_NREC_STATUS_CHANGE, NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}

		if (subscribe_event3_id == -1) {
			subscribe_event3_id = g_dbus_connection_signal_subscribe(conn,
					NULL, HFP_AGENT_SERVICE,
					HFP_ANSWER_CALL, NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}
		if (subscribe_event4_id == -1) {
			subscribe_event4_id = g_dbus_connection_signal_subscribe(conn,
					NULL, HFP_AGENT_SERVICE,
					HFP_REJECT_CALL, NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}
		if (subscribe_event5_id == -1) {
			subscribe_event5_id = g_dbus_connection_signal_subscribe(conn,
					NULL, HFP_AGENT_SERVICE,
					HFP_RELEASE_CALL, NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}
		if (subscribe_event6_id == -1) {
			subscribe_event6_id = g_dbus_connection_signal_subscribe(conn,
					NULL, HFP_AGENT_SERVICE,
					HFP_THREEWAY_CALL, NULL, NULL, 0,
					__bluetooth_telephony_event_filter,
					NULL, NULL);
		}

		return BLUETOOTH_TELEPHONY_ERROR_NONE;
	} else {
		if (subscribe_event1_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event1_id);
			subscribe_event1_id = -1;
		}
		if (subscribe_event2_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event2_id);
			subscribe_event2_id = -1;
		}
		if (subscribe_event3_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event3_id);
			subscribe_event3_id = -1;
		}
		if (subscribe_event4_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event4_id);
			subscribe_event4_id = -1;
		}
		if (subscribe_event5_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event5_id);
			subscribe_event5_id = -1;
		}
		if (subscribe_event6_id != -1) {
			g_dbus_connection_signal_unsubscribe(conn,
					subscribe_event6_id);
			subscribe_event6_id = -1;
		}
		return BLUETOOTH_TELEPHONY_ERROR_NONE;
	}
}

BT_EXPORT_API int bluetooth_telephony_init(bt_telephony_func_ptr cb,
							void  *user_data)
{
	bluetooth_device_address_t loc_address = { {0} };
	char src_address[BT_ADDRESS_STRING_SIZE] = { 0 };
	int ret = BLUETOOTH_TELEPHONY_ERROR_NONE;
	GError *error = NULL;

	FN_START;

	if (is_initialized == TRUE) {
		BT_ERR("Bluetooth telephony already initilized");
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_INITIALIZED;
	}

	is_initialized = TRUE;

	telephony_dbus_info.conn = _bt_gdbus_init_system_gconn();
	if (!telephony_dbus_info.conn) {
		is_initialized = FALSE;
		BT_ERR("Could not get DBus Connection");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	/* Call Path */
	snprintf(telephony_info.call_path, sizeof(telephony_info.call_path),
					CSD_CALL_APP_PATH, getpid());
	BT_DBG("Call Path = %s", telephony_info.call_path);
	memset(telephony_info.address, 0x00, sizeof(telephony_info.address));

	if (__bluetooth_telephony_proxy_init()) {
		BT_ERR("__bluetooth_telephony_proxy_init failed\n");
		telephony_dbus_info.conn = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	telephony_dbus_info.manager_proxy = g_dbus_proxy_new_sync(
			telephony_dbus_info.conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BLUEZ_SERVICE_NAME, "/",
			BLUEZ_MANAGER_INTERFACE, NULL, &error);
	if (telephony_dbus_info.manager_proxy == NULL) {
		BT_ERR("Could not create a manager proxy");
		__bluetooth_telephony_proxy_deinit();
		telephony_dbus_info.conn = NULL;
		is_initialized = FALSE;
		if (error) {
			ret = __bt_telephony_get_error(error->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(error);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	telephony_dbus_info.dbus_proxy = g_dbus_proxy_new_sync(
			telephony_dbus_info.conn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
			DBUS_INTERFACE_DBUS, NULL, &error);
	if (NULL == telephony_dbus_info.dbus_proxy) {
		__bluetooth_telephony_proxy_deinit();
		telephony_dbus_info.conn = NULL;
		g_object_unref(telephony_dbus_info.manager_proxy);
		telephony_dbus_info.manager_proxy = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (__bt_telephony_subscribe_adapter_signal(telephony_dbus_info.conn, TRUE) != 0) {
		BT_ERR("Fail to Subscribe Adapter Signal");
		goto fail;
	}

	/*Callback and user applicaton data*/
	telephony_info.cb = cb;
	telephony_info.user_data = user_data;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

	if (__bt_telephony_event_subscribe_signal(telephony_dbus_info.conn, TRUE) != 0) {
		BT_ERR("Fail to Subscribe telephony event Signal");
		goto fail;
	}

	if (bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED)
		return BLUETOOTH_TELEPHONY_ERROR_NONE;

	/*Bluetooth is active, therefore set the flag */
	is_active = TRUE;
	if (!src_addr) {
		ret = bluetooth_get_local_address(&loc_address);
		if (ret != BLUETOOTH_ERROR_NONE) {
			BT_ERR("Fail to get local address\n");
			ret = BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			goto fail;
		}
		_bt_convert_addr_type_to_string(src_address, loc_address.addr);
		src_addr = g_strdup(src_address);
	}
	ret = __bluetooth_telephony_register();
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("__bluetooth_telephony_register failed\n");
		goto fail;
	}

#ifndef TIZEN_WEARABLE
	__bluetooth_telephony_init_headset_state();
#endif

	FN_END;
	return ret;
fail:
	bluetooth_telephony_deinit();
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_deinit(void)
{
	FN_START;
	BT_TELEPHONY_CHECK_INITIALIZED();

	is_initialized = FALSE;

	if (__bt_telephony_event_subscribe_signal(telephony_dbus_info.conn, FALSE) != 0)
		BT_ERR("Fail to UnSubscribe telephony event Signal");

	if (bluetooth_check_adapter() != BLUETOOTH_ADAPTER_DISABLED ||
		bluetooth_check_adapter_le() != BLUETOOTH_ADAPTER_LE_DISABLED)
		__bluetooth_telephony_unregister();

	__bluetooth_telephony_proxy_deinit();

	telephony_info.cb = NULL;
	telephony_info.user_data = NULL;
	telephony_info.call_count = 0;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

	/* Remove BT enabled signal */
	if (__bt_telephony_subscribe_adapter_signal(telephony_dbus_info.conn, FALSE) != 0)
		BT_ERR("Fail to UnSubscribe Adapter event Signal");

	g_free(src_addr);
	src_addr = NULL;

	if (telephony_dbus_info.manager_proxy != NULL) {
		g_object_unref(telephony_dbus_info.manager_proxy);
		telephony_dbus_info.manager_proxy = NULL;
	}

	if (telephony_dbus_info.conn != NULL) {
		telephony_dbus_info.conn = NULL;
	}

	if (telephony_dbus_info.dbus_proxy != NULL) {
		g_object_unref(telephony_dbus_info.dbus_proxy);
		telephony_dbus_info.dbus_proxy = NULL;
	}

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API gboolean bluetooth_telephony_is_sco_connected(void)
{
	GVariant *reply;
	GError *err = NULL;
	gboolean status = FALSE;

	FN_START;

	retv_if(is_initialized == FALSE, FALSE);
	retv_if(bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED, FALSE);

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IsPlaying", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			__bt_telephony_get_error(err->message);
			g_error_free(err);
		}
		return FALSE;
	}
	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));
	g_variant_get(reply, "(b)", &status);
	g_variant_unref(reply);

#ifdef TIZEN_WEARABLE
	if (status == TRUE && telephony_info.headset_state != BLUETOOTH_STATE_PLAYING)
		telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
#endif

	BT_INFO("SCO Connected Status = [%d]", status);
	return status;
}

BT_EXPORT_API int bluetooth_telephony_is_nrec_enabled(gboolean *status)
{
	GVariant *reply;
	GError *err = NULL;
	GVariantIter iter;
	GVariant *param_inner;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (status == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	if (telephony_info.headset_state == BLUETOOTH_STATE_DISCONNETED)
		return BLUETOOTH_TELEPHONY_ERROR_AUDIO_NOT_CONNECTED;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetProperties", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			BT_DBG("Error message = %s", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));

	g_variant_iter_init(&iter, reply);
	while ((param_inner = g_variant_iter_next_value(&iter))) {
		GVariant *value;

		BT_INFO("Variant Type: %s", g_variant_get_type_string(param_inner));
		value = g_variant_lookup_value(param_inner,
					"nrec", G_VARIANT_TYPE_BOOLEAN);
		if (value) {
			BT_DBG("Property NREC Found");
			*status = g_variant_get_boolean(value);
			BT_DBG("NREC status = [%d]", *status);
			g_variant_unref(value);
			g_variant_unref(param_inner);
			break;
		}
		g_variant_unref(param_inner);
	}
	BT_DBG("NREC status = [%d]", *status);
	g_variant_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_is_wbs_mode(gboolean *status)
{
	GVariant *reply;
	GError *err = NULL;
	unsigned int codec;
	GVariantIter iter;
	GVariant *param_inner;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (status == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	*status = FALSE;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetProperties", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (err) {
			BT_ERR("Error message = %s", err->message);
			g_error_free(err);
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));

	g_variant_iter_init(&iter, reply);
	while ((param_inner = g_variant_iter_next_value(&iter))) {
		GVariant *value;

		value = g_variant_lookup_value(param_inner,
					"codec", G_VARIANT_TYPE_UINT32);

		if (value) {
			BT_DBG("Property CODEC Found");
			codec = g_variant_get_uint32(value);
			g_variant_unref(value);
			BT_DBG("Codec = [%d]", codec);

			*status = codec == BT_MSBC_CODEC_ID ? TRUE : FALSE;
			BT_DBG("NREC status = [%d]", *status);
			g_variant_unref(value);
			g_variant_unref(param_inner);
			break;
		}


		g_variant_unref(param_inner);
	}

	g_variant_unref(reply);
	BT_DBG("MSBC status = [%d]", *status);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_send_vendor_cmd(const char *cmd)
{
	GError *error = NULL;
	GVariant *reply, *parameters;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	BT_DBG("Send Vendor %s", cmd);

	if (telephony_dbus_info.proxy == NULL)
		telephony_dbus_info.proxy =
			__bluetooth_telephony_get_connected_device_proxy();

	if (telephony_dbus_info.proxy == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;

	parameters = g_variant_new("s", cmd);
	reply = g_dbus_proxy_call_sync(telephony_dbus_info.proxy,
			"SendVendorAtCmd", parameters,
				G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	g_variant_unref(parameters);
	g_variant_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_start_voice_recognition(void)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	int ret;
	gboolean state = TRUE;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	param = g_variant_new("(b)", &state);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetVoiceDial", &err, param);

	g_variant_unref(param);
	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_stop_voice_recognition(void)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	int ret;
	gboolean state = FALSE;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	param = g_variant_new("(b)", &state);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetVoiceDial", &err, param);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_sco_start_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *value;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		if (error != NULL) {
			BT_ERR("sco_close_cb error. errCode[%x],message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("SCo Start Failed");
		}
	}

	BT_DBG("sco_start_cb : -");
	g_object_unref(proxy);
	g_variant_unref(value);
}

BT_EXPORT_API int bluetooth_telephony_audio_open(void)
{
	GDBusConnection *conn;
	GDBusProxy *proxy;
	GError *err = NULL;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	/* Because this API is async call, so can't use dbus SMACK */
	if (__bt_telephony_check_privilege() ==
				BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	}

	conn = _bt_gdbus_get_system_gconn();
	if (!conn) {
		BT_DBG("No System Bus found\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING)
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_CONNECTED;

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
			HFP_AGENT_SERVICE, HFP_AGENT_PATH,
			HFP_AGENT_INTERFACE, NULL, &err);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy \n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_dbus_proxy_call(proxy, "Play", NULL, G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, (GAsyncReadyCallback)__bluetooth_telephony_sco_start_cb, NULL);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_sco_close_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	GVariant *value;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		if (error != NULL) {
			BT_ERR("sco_close_cb error. errCode[%x],message[%s]",
					error->code, error->message);
			g_clear_error(&error);
		} else {
			BT_ERR("SCo close Failed");
		}
	}

	BT_DBG("sco_close_cb : -");
	g_object_unref(proxy);
	g_variant_unref(value);
}
BT_EXPORT_API int bluetooth_telephony_audio_close(void)
{
	GDBusConnection *conn;
	GDBusProxy *proxy;
	GError *err = NULL;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	/* Because this API is async call, so can't use dbus SMACK */
	if (__bt_telephony_check_privilege() ==
				BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	}

	conn = _bt_gdbus_get_system_gconn();
	if (!conn) {
		BT_DBG("No System Bus found\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state != BLUETOOTH_STATE_PLAYING)
		return BLUETOOTH_TELEPHONY_ERROR_NOT_CONNECTED;

	proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
			HFP_AGENT_SERVICE, HFP_AGENT_PATH,
			HFP_AGENT_INTERFACE, NULL, &err);
	if (proxy == NULL) {
		BT_ERR("Unable to allocate new proxy");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			BT_ERR("Error here %d\n", ret);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_dbus_proxy_call(proxy, "Stop", NULL, G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, (GAsyncReadyCallback)__bluetooth_telephony_sco_close_cb, NULL);

	telephony_info.headset_state = BLUETOOTH_STATE_CONNECTED;

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_call_remote_ringing(unsigned int call_id)
{
	int ret;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	FN_START;
	BT_DBG("call_id = [%d]", call_id);

	/*Make sure SCO is already connected */
	ret = __bluetooth_telephony_send_call_status(
				CSD_CALL_STATUS_MO_ALERTING, call_id, NULL);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_call_answered(unsigned int call_id,
							unsigned int bt_audio)
{
	int ret;

	FN_START;
	BT_DBG("call_id = [%d]", call_id);

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_ACTIVE,
								call_id, NULL);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return ret;
	}

	if (bt_audio) {
		if (!bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_open();
			if (ret != 0) {
				BT_ERR("Audio connection call Failed[%d]", ret);
				return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			}
		}
	}

	FN_END;
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_end(unsigned int call_id)
{
	int ret;

	FN_START;
	BT_DBG("call_id = [%d]", call_id);

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (telephony_info.call_count > 0)
		telephony_info.call_count = telephony_info.call_count - 1;

	if (telephony_info.call_count  == 0) {
		if (bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_close();
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
				BT_ERR(" Failed = [%d]", ret);
		}
	}

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_MT_RELEASE,
								call_id, NULL);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("send call status Failed = [%d]", ret);
		return ret;
	}

	FN_END;
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_held(unsigned int call_id)
{
	int ret;

	FN_START;
	BT_DBG("call_id = [%d]", call_id);

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_HOLD,
								call_id, NULL);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
		BT_ERR("send call status Failed = [%d]", ret);

	FN_END;
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_retrieved(unsigned int call_id)
{
	int ret;

	FN_START;
	BT_DBG("call_id = [%d]", call_id);

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	ret = __bluetooth_telephony_send_call_status(CSD_CALL_STATUS_ACTIVE,
								call_id, NULL);
	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
		BT_ERR("send call status Failed = [%d]", ret);

	FN_END;
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_call_swapped(void *call_list,
				unsigned int call_count)
{
	int i;
	int ret;
	GList *list = call_list;
	bt_telephony_call_status_info_t *call_status;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == list) {
		BT_ERR("call_list is invalid");
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;
	}

	/* Because this API is async call, so can't use dbus SMACK */
	if (__bt_telephony_check_privilege() ==
				BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	}

	BT_DBG(" call_count = [%d]", call_count);

	for (i = 0; i < call_count; i++) {
		call_status = g_list_nth_data(list, i);

		if (NULL == call_status)
			continue;

		BT_DBG(" %d : Call id [%d] status[%d]", i,
					call_status->call_id,
					call_status->call_status);

		if (NULL != call_status->phone_number)
			DBG_SECURE("Number [%s]", call_status->phone_number);

		switch (call_status->call_status) {
		case BLUETOOTH_CALL_STATE_HELD:
			ret = __bluetooth_telephony_send_call_status(
						CSD_CALL_STATUS_HOLD,
						call_status->call_id,
						call_status->phone_number);
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
				BT_ERR("Failed = %d", ret);
				return ret;
			}
		break;

		case BLUETOOTH_CALL_STATE_CONNECTED:
			ret = __bluetooth_telephony_send_call_status(
					CSD_CALL_STATUS_ACTIVE,
					call_status->call_id,
					call_status->phone_number);
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

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_set_call_status(void *call_list,
				unsigned int call_count)
{
	int ret;

	FN_START;

	ret = bluetooth_telephony_call_swapped(call_list, call_count);

	if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE) {
		BT_ERR("Failed = [%d]", ret);
		return ret;
	}

	telephony_info.call_count = call_count;

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_indicate_outgoing_call(
			const char *ph_number, unsigned int call_id,
			unsigned int bt_audio)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	const char *path = telephony_info.call_path;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	param = g_variant_new("(ssi)", path, ph_number, call_id);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"OutgoingCall", &err, param);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);

	telephony_info.call_count++;
	BT_DBG(" ag_info.ag_call_count = [%d]", telephony_info.call_count);

	if (bt_audio) {
		if (!bluetooth_telephony_is_sco_connected()) {
			ret = bluetooth_telephony_audio_open();
			if (ret != 0) {
				BT_ERR(" Audio connection Failed = %d", ret);
				return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			}
		}
	}

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_indicate_incoming_call(
		const char *ph_number, unsigned int call_id)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	const char *path = telephony_info.call_path;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	param = g_variant_new("(ssi)", path, ph_number, call_id);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IncomingCall", &err, param);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	telephony_info.call_count++;
	BT_DBG("telephony_info.call_count = [%d]", telephony_info.call_count);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_set_speaker_gain(
						unsigned short speaker_gain)
{
	GVariant *reply;
	GVariant *param;
	GError *err = NULL;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	BT_DBG("set speaker_gain= [%d]", speaker_gain);

	param = g_variant_new("(q)", speaker_gain);
	BT_DBG("Type of param: %s", g_variant_get_type_string(param));
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetSpeakerGain", &err, param);
	g_variant_unref(param);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	g_variant_unref(reply);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_get_headset_volume(
						unsigned int *speaker_gain)
{
	GVariant *reply;
	GError *err = NULL;
	int ret;
	guint16 gain;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetSpeakerGain", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));
	g_variant_get(reply, "(q)", &gain);
	*speaker_gain = gain;
	BT_DBG("Get speaker_gain= [%d]", *speaker_gain);

	g_variant_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_is_connected(gboolean *ag_connected)
{
	GVariant *reply;
	GError *err = NULL;
	int ret;
	gboolean ag_connected_from_bt_agent;

	BT_CHECK_ENABLED(return);

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IsConnected", &err, NULL);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (err) {
			ret = __bt_telephony_get_error(err->message);
			g_error_free(err);
			return ret;
		}
		return BLUETOOTH_ERROR_INTERNAL;
	}
	BT_INFO("Variant Type: %s", g_variant_get_type_string(reply));
	g_variant_get(reply, "(b)", &ag_connected_from_bt_agent);
	*ag_connected = ag_connected_from_bt_agent;

	BT_DBG("Conn Status: %s", *ag_connected ? "Connected" : "Disconnected");

	g_variant_unref(reply);

	return BLUETOOTH_ERROR_NONE;
}

static void __bt_telephony_adapter_filter(GDBusConnection *connection,
					const gchar *sender_name,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *signal_name,
					GVariant *parameters,
					gpointer user_data)
{
	FN_START;
	int ret;
	char *path = NULL;

	if (strcasecmp(signal_name, "InterfacesAdded") == 0) {
		GVariant *optional_param = NULL;

		g_variant_get(parameters, "(&o@a{sa{sv}})",
							&path, &optional_param);

		if (!path) {
			BT_ERR("Invalid adapter path");
			return;
		}

		BT_INFO("Adapter Path = [%s]", path);
		if (strcasecmp(path, DEFAULT_ADAPTER_OBJECT_PATH) == 0) {
			if (__bt_telephony_get_src_addr(optional_param))
				BT_ERR("Fail to get the local adapter address");

			ret = __bluetooth_telephony_register();
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
				BT_ERR("__bluetooth_telephony_register failed");
		}
	}

	FN_END;
}

static int __bt_telephony_get_src_addr(GVariant *value)
{
	GVariantIter iter;
	GVariant *param = NULL;
	FN_START;

	BT_DBG("Type: %s", g_variant_get_type_string(value));
	/* signature a{sa{sv}} */
	g_variant_iter_init(&iter, value);
	while ((param = g_variant_iter_next_value (&iter))) {
		char *interface_name;
		GVariant *interface_var = NULL;
		GVariant *param_inner = NULL;

		BT_DBG("Type: %s", g_variant_get_type_string(param));

		g_variant_get(param, "{s*}", &interface_name, &interface_var);
		g_variant_unref(param);

		BT_DBG("Type: %s", g_variant_get_type_string(interface_var));
		BT_DBG("interface_name: %s", interface_name);
		/* format of interface_var: a{sv}*/
		if (strcasecmp(interface_name, BLUEZ_ADAPTER_INTERFACE) == 0) {
			GVariantIter iter_inner;

			g_variant_iter_init(&iter_inner, interface_var);
			while ((param_inner = g_variant_iter_next_value(&iter_inner))) {
				char *property_name;
				GVariant *property_var;

				BT_DBG("Type: %s", g_variant_get_type_string(param_inner));

				g_variant_get(param_inner, "{sv}",
						&property_name,
						&property_var);
				g_variant_unref(param_inner);

				BT_DBG("Type: %s", g_variant_get_type_string(property_var));

				if (strcasecmp(property_name, "Address") == 0) {
					gsize len;
					const gchar *bd_addr;

					bd_addr = g_variant_get_string(property_var,
							&len);
					src_addr = g_strdup(bd_addr);
					BT_DBG("Address: %s", src_addr);

					g_variant_unref(interface_var);
					g_variant_unref(property_var);
					goto done;
				}
				g_variant_unref(property_var);
			}
		}
		g_variant_unref(interface_var);
	}

done:
	return BLUETOOTH_ERROR_NONE;
}
