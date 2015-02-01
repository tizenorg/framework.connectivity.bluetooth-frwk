/*
 * Bluetooth-telephony
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:	Hocheol Seo <hocheol.seo@samsung.com>
 * 		GirishAshok Joshi <girish.joshi@samsung.com>
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
	DBusGConnection *conn;
	DBusGProxy *proxy;
	DBusGProxy *dbus_proxy;
	DBusGProxy *manager_proxy;
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

#define CSD_CALL_APP_PATH "/org/tizen/csd/%d"
#define HFP_NREC_STATUS_CHANGE "NrecStatusChanged"
#define HFP_ANSWER_CALL "Answer"
#define HFP_REJECT_CALL "Reject"
#define HFP_RELEASE_CALL "Release"
#define HFP_THREEWAY_CALL "Threeway"

#define DEFAULT_ADAPTER_OBJECT_PATH "/org/bluez/hci0"


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

#define BLUETOOTH_TELEPHONY_METHOD (bluetooth_telephony_method_get_type())
#define BLUETOOTH_TELEPHONY_METHOD_GET_OBJECT(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
		BLUETOOTH_TELEPHONY_METHOD, BluetoothTelephonyMethod))

#define BLUETOOTH_TELEPHONY_METHOD_IS_OBJECT(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
		BLUETOOTH_TELEPHONY_METHOD))

#define BLUETOOTH_TELEPHONY_METHOD_CLASS(class) \
		(G_TYPE_CHECK_CLASS_CAST((class), \
		BLUETOOTH_TELEPHONY_METHOD, BluetoothTelephonyMethodClass))

#define BLUETOOTH_TELEPHONY_METHOD_GET_AGENT_CLASS(obj) \
		(G_TYPE_INSTANCE_GET_CLASS((obj), \
		BLUETOOTH_TELEPHONY_METHOD, BluetoothTelephonyMethodClass))

#define BLUETOOTH_TELEPHONY_METHOD_IS_AGENT_CLASS(class) \
		(G_TYPE_CHECK_CLASS_TYPE((class), BLUETOOTH_TELEPHONY_METHOD))

#define BLUETOOTH_TELEPHONY_METHOD_AGENT_GET_PRIVATE(obj) \
		(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
		BLUETOOTH_TELEPHONY_METHOD, BluetoothTelephonyMethodPrivate))

typedef struct _BluetoothTelephonyMethod BluetoothTelephonyMethod;
typedef struct _BluetoothTelephonyMethodClass BluetoothTelephonyMethodClass;

struct _BluetoothTelephonyMethod {
	GObject parent;
};

struct _BluetoothTelephonyMethodClass {
	GObjectClass parent_class;
};

BluetoothTelephonyMethod *bluetooth_telephony_method_new(void);
GType bluetooth_telephony_method_get_type(void);

G_DEFINE_TYPE(BluetoothTelephonyMethod, bluetooth_telephony_method, G_TYPE_OBJECT)


static DBusHandlerResult __bt_telephony_adapter_filter(DBusConnection *conn,
			DBusMessage *msg, void *data);

static int __bt_telephony_get_object_path(DBusMessage *msg, char **path);

static int __bt_telephony_get_src_addr(DBusMessage *msg);

static gboolean bluetooth_telephony_method_send_dtmf(
				BluetoothTelephonyMethod *object,
				gchar *dtmf, DBusGMethodInvocation *context);

static gboolean bluetooth_telephony_method_vendor_cmd(
				BluetoothTelephonyMethod *object,
				gchar *at_cmd, DBusGMethodInvocation *context);
#include "bt-telephony-glue.h"

static GObject *object;
static bt_telephony_info_t telephony_info;
static telephony_dbus_info_t telephony_dbus_info;
static gboolean is_active = FALSE;

/*Function Declaration*/
static int __bt_telephony_get_error(const char *error_message);
static void __bt_telephony_event_cb(int event, int result, void *param_data);
static GQuark __bluetooth_telephony_error_quark(void);
static DBusMessage* __bluetooth_telephony_dbus_method_send(const char *path,
			const char *interface, const char *method, DBusError *err,  int type, ...);
static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id, const char *ph_number);
static GError *__bluetooth_telephony_error(bluetooth_telephony_error_t error,
					const char *err_msg);

static DBusHandlerResult __bluetooth_telephony_event_filter(
						DBusConnection *conn,
						DBusMessage *msg, void *data);

static int __bluetooth_telephony_proxy_init(void);
static void __bluetooth_telephony_proxy_deinit(void);
static int __bluetooth_telephony_register(void);
static int __bluetooth_telephony_unregister(void);

static gboolean __bluetooth_telephony_is_headset(uint32_t device_class);
static int __bluetooth_telephony_get_connected_device(void);
static DBusGProxy *__bluetooth_telephony_get_connected_device_proxy(void);

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
	else if (g_strcmp0(error_message, "Operation currently not available") == 0)
		return BLUETOOTH_TELEPHONY_ERROR_OPERATION_NOT_AVAILABLE;
	else if (g_strrstr(error_message, BT_ACCESS_DENIED_MSG))
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	else
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
}

static int __bt_telephony_check_privilege(void)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError err;
	DBusConnection *conn;
	int ret;

	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE,
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"CheckPrivilege");
	if (!msg) {
		BT_ERR("Unable to allocate new D-Bus message \n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn,
						msg, -1, &err);

	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			BT_ERR("Error here %d\n", ret);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

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

static DBusMessage* __bluetooth_telephony_dbus_method_send(const char *path,
			const char *interface, const char *method, DBusError *err,  int type, ...)
{
	DBusMessage *msg;
	DBusMessage *reply;
	va_list args;
#ifdef TIZEN_WEARABLE
	int timeout = 4000;
#else
	int timeout = -1
#endif

	FN_START;

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE,
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

	reply = dbus_connection_send_with_reply_and_block(
		dbus_g_connection_get_connection(telephony_dbus_info.conn),
		msg, timeout, err);
	dbus_message_unref(msg);

	FN_END;
	return reply;
}

static int __bluetooth_telephony_send_call_status(
			bt_telephony_call_status_t call_status,
			unsigned int call_id, const char *ph_number)
{
	DBusMessage *reply;
	DBusError err;
	char *path = g_strdup(telephony_info.call_path);
	char *phone_number;
	int ret;

	FN_START;

	if (NULL == ph_number)
		phone_number = g_strdup("");
	else
		phone_number = g_strdup(ph_number);

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"ChangeCallStatus", &err, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_STRING, &phone_number,
			DBUS_TYPE_INT32, &call_status,
			DBUS_TYPE_INT32, &call_id, DBUS_TYPE_INVALID);

	g_free(path);
	g_free(phone_number);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static GError *__bluetooth_telephony_error(bluetooth_telephony_error_t error,
					const char *err_msg)
{
	return g_error_new(BLUETOOTH_TELEPHONY_ERROR, error, err_msg, NULL);
}

static void bluetooth_telephony_method_init(BluetoothTelephonyMethod *object)
{
	FN_START;
	BT_DBG("agent %p\n", object);
	FN_END;
}

static void __bluetooth_telephony_method_finalize(
					BluetoothTelephonyMethod *object)
{
	FN_START;
	G_OBJECT_CLASS(bluetooth_telephony_method_parent_class)->finalize((
							GObject *)object);
	FN_END;
}

static BluetoothTelephonyMethod *__bluetooth_telephony_method_new(void)
{
	BluetoothTelephonyMethod *obj;

	FN_START;
	obj = g_object_new(BLUETOOTH_TELEPHONY_METHOD, NULL);
	FN_END;

	return obj;
}

static void bluetooth_telephony_method_class_init(
					BluetoothTelephonyMethodClass *klass)
{
	GObjectClass *object_class = NULL;
	FN_START;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = (void *)__bluetooth_telephony_method_finalize;

	/*Registration of the Framework methods */
	dbus_g_object_type_install_info(BLUETOOTH_TELEPHONY_METHOD,
			&dbus_glib_bluetooth_telephony_method_object_info);
	FN_END;
	return;
}

static void __bluetooth_telephony_answer_call(DBusMessage *msg)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;
	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &callid,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Error Getting parameters");
		return;
	}

	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_release_call(DBusMessage *msg)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;
	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &callid,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Error Getting parameters");
		return;
	}

	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_reject_call(DBusMessage *msg)
{
	telephony_event_callid_t call_data = { 0, };
	unsigned int callid;

	FN_START;
	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &callid,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Error Getting parameters");
		return;
	}

	BT_DBG("call_id = [%d]", callid);
	call_data.callid = callid;

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_REJECT_CALL,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void  *)&call_data);
	FN_END;
}

static void __bluetooth_telephony_threeway_call(DBusMessage *msg)
{
	int event = 0;
	unsigned int chld_value;

	FN_START;
	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &chld_value,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Error Getting parameters");
		return;
	}

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

static gboolean bluetooth_telephony_method_send_dtmf(
				BluetoothTelephonyMethod *object,
				gchar *dtmf, DBusGMethodInvocation *context)
{
	telephony_event_dtmf_t call_data = { 0, };
	GError *err;

	FN_START;

	if (dtmf == NULL) {
		BT_ERR("Number dial failed");
		err = __bluetooth_telephony_error(
				BLUETOOTH_TELEPHONY_ERROR_INVALID_DTMF,
				"Invalid dtmf");
		dbus_g_method_return_error(context, err);
		g_error_free(err);
		return FALSE;
	}

	DBG_SECURE("Dtmf = %s", dtmf);

	call_data.dtmf = g_strdup(dtmf);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF,
		BLUETOOTH_TELEPHONY_ERROR_NONE, (void *)&call_data);

	dbus_g_method_return(context);
	g_free(call_data.dtmf);
	FN_END;
	return TRUE;
}

static gboolean bluetooth_telephony_method_vendor_cmd(
				BluetoothTelephonyMethod *object,
				gchar *at_cmd, DBusGMethodInvocation *context)
{
	GError *err;

	FN_START;

	if (at_cmd == NULL) {
		BT_ERR("Vendor command is NULL\n");
		err = __bluetooth_telephony_error(
				BLUETOOTH_TELEPHONY_ERROR_APPLICATION,
				"Invalid at vendor cmd");
		dbus_g_method_return_error(context, err);
		g_error_free(err);
		return FALSE;
	}

	DBG_SECURE("Vendor AT cmd = %s", at_cmd);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_VENDOR_AT_CMD,
		BLUETOOTH_TELEPHONY_ERROR_NONE, at_cmd);

	dbus_g_method_return(context);
	FN_END;
	return TRUE;
}

static void __bluetooth_handle_nrec_status_change(DBusMessage *msg)
{
	gboolean status = FALSE;

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_BOOLEAN, &status,
				DBUS_TYPE_INVALID)) {
		BT_ERR("Error Getting parameters");
		return;
	}
	BT_INFO("NREC status = %d", status);

	__bt_telephony_event_cb(BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED,
		BLUETOOTH_TELEPHONY_ERROR_NONE, (void *)&status);

}

static DBusHandlerResult __bluetooth_telephony_event_filter(
						DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	const char *path = dbus_message_get_path(msg);
	char *dev_addr = NULL;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	const char *property;

	/* Check NREC status change Signal*/
	if (dbus_message_is_signal(msg, HFP_AGENT_SERVICE,
				HFP_NREC_STATUS_CHANGE)) {
		__bluetooth_handle_nrec_status_change(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_is_signal(msg, HFP_AGENT_SERVICE,
				HFP_ANSWER_CALL)) {
		__bluetooth_telephony_answer_call(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_is_signal(msg, HFP_AGENT_SERVICE,
				HFP_REJECT_CALL)) {
		__bluetooth_telephony_reject_call(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_is_signal(msg, HFP_AGENT_SERVICE,
				HFP_RELEASE_CALL)) {
		__bluetooth_telephony_release_call(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (dbus_message_is_signal(msg, HFP_AGENT_SERVICE,
				HFP_THREEWAY_CALL)) {
		__bluetooth_telephony_threeway_call(msg);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (!dbus_message_has_interface(msg, BLUEZ_HEADSET_INTERFACE))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_iter_init(msg, &item_iter);
	if (dbus_message_iter_get_arg_type(&item_iter) != DBUS_TYPE_STRING) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_iter_get_basic(&item_iter, &property);

	if (property == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	BT_DBG("Property (%s)", property);

	if (g_strcmp0(property, "State") == 0) {
		char *state = NULL;
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &state);
		if (NULL == state) {
			BT_ERR("State is null");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
		BT_DBG("State : %s", state);

		if (g_strcmp0(state, "connected") == 0) {
			telephony_info.headset_state = BLUETOOTH_STATE_CONNECTED;
		} else if (g_strcmp0(state, "playing") == 0) {
			telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
		} else if (g_strcmp0(state, "disconnected") == 0) {
			/* Headset state: playing -> disconnected */
			if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING) {
				if (!vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, FALSE)) {
					BT_DBG("SVCONFKEY_BT_HEADSET_SCO - Set to FALSE");
				} else {
					 BT_ERR("vconf_set_bool - Failed");
				}

				__bt_telephony_event_cb(
					 BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
					 BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
			}

			telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;
		}

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0(property, "Connected") == 0) {
		gboolean connected = FALSE;
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &connected);
		BT_DBG("Connected : %d", connected);

		if (connected) {
			/*Get device address*/
			if (path != NULL)
				dev_addr = strstr(path, "dev_");

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
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0(property, "SpeakerGain") == 0) {
		unsigned int spkr_gain;
		guint16 gain;
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &gain);

		spkr_gain = (unsigned int)gain;
		BT_DBG("spk_gain[%d]", spkr_gain);

		__bt_telephony_event_cb(
					BLUETOOTH_EVENT_TELEPHONY_SET_SPEAKER_GAIN,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&spkr_gain);

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0(property, "MicrophoneGain") == 0) {
		unsigned int mic_gain;
		guint16 gain;
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &gain);

		mic_gain = (unsigned int)gain;
		BT_DBG("mic_gain[%d]", mic_gain);

		__bt_telephony_event_cb(
					BLUETOOTH_EVENT_TELEPHONY_SET_MIC_GAIN,
					BLUETOOTH_TELEPHONY_ERROR_NONE,
					(void *)&mic_gain);

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	if (g_strcmp0(property, "Playing") == 0) {
		gboolean audio_sink_playing = FALSE;
		dbus_message_iter_next(&item_iter);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		dbus_message_iter_get_basic(&value_iter, &audio_sink_playing);

		if (audio_sink_playing) {
			if (!vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, TRUE)) {
				BT_DBG("SVCONFKEY_BT_HEADSET_SCO -"
					"Set to TRUE");
			} else {
				BT_ERR("vconf_set_bool - Failed");
			}
			telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
			 __bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_AUDIO_CONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		} else {
			if (!vconf_set_bool(VCONFKEY_BT_HEADSET_SCO, FALSE)) {
				BT_DBG("SVCONFKEY_BT_HEADSET_SCO -"
						"Set to FALSE");
			} else {
				BT_ERR("vconf_set_bool - Failed");
			}
			telephony_info.headset_state =
						BLUETOOTH_STATE_CONNECTED;
			__bt_telephony_event_cb(
				BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
				BLUETOOTH_TELEPHONY_ERROR_NONE, NULL);
		}

		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static int __bluetooth_telephony_proxy_init(void)
{
	FN_START;

	object = (GObject *)__bluetooth_telephony_method_new();

	if (NULL == object)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;

	BT_DBG("telephony_object = %x", object);

	dbus_g_connection_register_g_object(telephony_dbus_info.conn,
			telephony_info.call_path, G_OBJECT(object));

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_proxy_deinit(void)
{
	FN_START;

	dbus_g_connection_unregister_g_object(telephony_dbus_info.conn,
				G_OBJECT(object));

	g_object_unref(object);
	object = NULL;

	FN_END;
	return;
}

static int __bluetooth_telephony_register(void)
{
	DBusMessage *reply;
	DBusError err;
	char *path = g_strdup(telephony_info.call_path);
	int ret;

	FN_START;

	reply =  __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"RegisterApplication", &err, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_STRING, &src_addr,
			DBUS_TYPE_INVALID);

	g_free(path);
	if (!reply) {
		BT_ERR("Error returned in method call");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			BT_ERR("Error here %d\n", ret);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);
	BT_DBG("__bluetooth_telephony_register completed");
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static  int __bluetooth_telephony_unregister(void)
{
	DBusMessage *reply;
	DBusError err;
	char *path = g_strdup(telephony_info.call_path);
	int ret;

	FN_START;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"UnregisterApplication", &err, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_INVALID);

	g_free(path);
	if (!reply) {
		BT_ERR("Error returned in method call");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);
	BT_DBG("__bluetooth_telephony_unregister completed");
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

#ifndef TIZEN_WEARABLE
static void __bluetooth_telephony_init_headset_state(void)
{
	DBusMessage *reply;
	DBusError err;

	gboolean status = FALSE;

	FN_START;

	if (telephony_dbus_info.conn == NULL) {
		BT_ERR("Bluetooth telephony not initilized");
		return;
	}
	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IsConnected", &err, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			__bt_telephony_get_error(err.message);
			dbus_error_free(&err);
		}
		return;
	}

	if (!dbus_message_get_args(reply, &err,
			DBUS_TYPE_BOOLEAN, &status,
			DBUS_TYPE_INVALID)) {
		BT_ERR("Error to get features");
		if (dbus_error_is_set(&err)) {
			BT_ERR("error message: %s", err.message);
			dbus_error_free(&err);
		}
		dbus_message_unref(reply);
		return;
	}

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

static gboolean __bluetooth_telephony_is_headset_by_uuid(GValue *value)
{
	int i;
	char **uuids;
	char **parts;
	unsigned int service = 0;

	FN_START;

	retv_if(value == NULL, FALSE);

	uuids = g_value_get_boxed(value);
	retv_if(uuids == NULL, FALSE);

	for (i = 0; uuids[i] != NULL; i++) {
		parts = g_strsplit(uuids[i], "-", -1);

		if (parts == NULL || parts[0] == NULL) {
			g_strfreev(parts);
			continue;
		}

		if (service == BLUETOOTH_HS_PROFILE_UUID ||
				service == BLUETOOTH_HF_PROFILE_UUID)
			return TRUE;
	}

	FN_END;
	return FALSE;
}


static int __bluetooth_telephony_get_connected_device(void)
{
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter reply_iter;
	DBusMessageIter value_iter;
	DBusError err;
	DBusConnection *conn;
	DBusGProxy *headset_agent_proxy = NULL;

	GError *error = NULL;
	uint32_t device_class;
	gboolean playing = FALSE;
	gboolean connected = FALSE;
	GHashTable *list_hash = NULL;
	GValue *value = {0};
	char *object_path = NULL;
	DBusGProxy *proxy = NULL;
	const gchar *address;

	FN_START;
	conn = _bt_get_system_conn();
	retv_if(conn == NULL, BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

	msg = dbus_message_new_method_call(BLUEZ_SERVICE_NAME, "/",
						BLUEZ_MANAGER_INTERFACE,
						"GetManagedObjects");

	retv_if(msg == NULL, BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

	/* Synchronous call */
	dbus_error_init(&err);
	reply = dbus_connection_send_with_reply_and_block(
					conn, msg,
					-1, &err);
	dbus_message_unref(msg);

	if (!reply) {
		BT_ERR("Can't get managed objects");

		if (dbus_error_is_set(&err)) {
			BT_ERR("%s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (dbus_message_iter_init(reply, &reply_iter) == FALSE) {
		BT_ERR("Fail to iterate the reply");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_recurse(&reply_iter, &value_iter);

	/* signature of GetManagedObjects:	a{oa{sa{sv}}} */
	while (dbus_message_iter_get_arg_type(&value_iter) ==
						DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter msg_iter;

		dbus_message_iter_recurse(&value_iter, &msg_iter);

		/* Parse the signature:	oa{sa{sv}}} */
		retv_if(dbus_message_iter_get_arg_type(&msg_iter) !=
				DBUS_TYPE_OBJECT_PATH,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

		dbus_message_iter_get_basic(&msg_iter, &object_path);

		if (object_path) {
			proxy = dbus_g_proxy_new_for_name(
						telephony_dbus_info.conn,
						BLUEZ_SERVICE_NAME,
						object_path,
						BLUEZ_PROPERTIES_INTERFACE);
			if (proxy == NULL)
				goto done;

			dbus_g_proxy_call(proxy, "GetAll", &error,
						G_TYPE_STRING,
						BLUEZ_DEVICE_INTERFACE,
						G_TYPE_INVALID,
						dbus_g_type_get_map("GHashTable",
							G_TYPE_STRING,
							G_TYPE_VALUE),
						&list_hash,
						G_TYPE_INVALID);
			if (list_hash == NULL)
				goto done;

			if (error) {
				BT_ERR("error in GetBasicProperties [%s]\n", error->message);
				g_error_free(error);
				goto done;
			}

			value = g_hash_table_lookup(list_hash, "Class");
			device_class = value ? g_value_get_uint(value) : 0;

			if (device_class == 0) {
				BT_DBG("COD is NULL (maybe paired by nfc)...  Checking UUIDs");
				value = g_hash_table_lookup(list_hash, "UUIDs");
				if (!__bluetooth_telephony_is_headset_by_uuid(value)) {
					BT_DBG("UUID checking completed. None HF device");
					g_object_unref(proxy);
					proxy = NULL;
					g_hash_table_destroy(list_hash);
					dbus_message_iter_next(&value_iter);
					continue;
				}
				BT_DBG("UUID checking completed. HF device");
			} else {
				if (!__bluetooth_telephony_is_headset(device_class)) {
					g_object_unref(proxy);
					proxy = NULL;
					g_hash_table_destroy(list_hash);
					dbus_message_iter_next(&value_iter);
					continue;
				}
			}

			/* this is headset; Check for Connection */
			headset_agent_proxy = dbus_g_proxy_new_for_name(
						telephony_dbus_info.conn,
						HFP_AGENT_SERVICE, object_path,
						HFP_AGENT_INTERFACE);

			if (headset_agent_proxy == NULL) {
				g_hash_table_destroy(list_hash);
				goto done;
			}

			dbus_g_proxy_call(headset_agent_proxy, "IsConnected",
					&error, G_TYPE_INVALID,
					G_TYPE_BOOLEAN, &connected,
					G_TYPE_INVALID);

			if (error == NULL) {
				if (connected) {
					value = g_hash_table_lookup(list_hash,
									"Address");
					address = value ? g_value_get_string(
								value) : NULL;

					g_strlcpy(telephony_info.address,
							address,
							sizeof(telephony_info.address));
					dbus_g_proxy_call(headset_agent_proxy,
								"IsPlaying",
								&error,
								G_TYPE_INVALID,
								G_TYPE_BOOLEAN,
								&playing,
								G_TYPE_INVALID);
					if (playing)
						telephony_info.headset_state =
							BLUETOOTH_STATE_PLAYING;
					else
						telephony_info.headset_state =
							BLUETOOTH_STATE_CONNECTED;

					g_hash_table_destroy(list_hash);
					goto done;
				}
			} else {
				g_error_free(error);
			}

			g_hash_table_destroy(list_hash);
			g_object_unref(proxy);
			proxy = NULL;
		} /* end of if(object_path) */

	dbus_message_iter_next(&value_iter);
	} /* end of while */

done:
	if (proxy)
		g_object_unref(proxy);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static DBusGProxy *__bluetooth_telephony_get_connected_device_proxy(void)
{
	DBusGProxy *proxy = NULL;

	FN_START;

	if (strlen(telephony_info.address) == 0)
		__bluetooth_telephony_get_connected_device();

	if (strlen(telephony_info.address) == 0)
		return NULL;

	proxy = dbus_g_proxy_new_for_name(telephony_dbus_info.conn,
			HFP_AGENT_SERVICE, HFP_AGENT_PATH,
			HFP_AGENT_INTERFACE);

	FN_END;
	return proxy;
}


BT_EXPORT_API int bluetooth_telephony_init(bt_telephony_func_ptr cb,
							void  *user_data)
{
	DBusError dbus_error;
	DBusConnection *conn;
	int ret = BLUETOOTH_TELEPHONY_ERROR_NONE;
	GError *error = NULL;

	DBusConnection *dbus_conn;
	bluetooth_device_address_t loc_address = { {0} };
	char src_address[BT_ADDRESS_STRING_SIZE] = { 0 };


	FN_START;
	g_type_init();

	if (is_initialized == TRUE) {
		BT_ERR("Bluetooth telephony already initilized");
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_INITIALIZED;
	}

	is_initialized = TRUE;

	telephony_dbus_info.conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!telephony_dbus_info.conn) {
		if (NULL != error) {
			BT_ERR("dbus_g_bus_get() failed:[%d:%s]\n",
					error->code, error->message);
			g_error_free(error);
		}
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	/* Call Path */
	snprintf(telephony_info.call_path, sizeof(telephony_info.call_path),
					CSD_CALL_APP_PATH, getpid());
	BT_DBG("Call Path = %s", telephony_info.call_path);
	memset(telephony_info.address, 0x00, sizeof(telephony_info.address));

	if (__bluetooth_telephony_proxy_init()) {
		BT_ERR("__bluetooth_telephony_proxy_init failed\n");
		dbus_g_connection_unref(telephony_dbus_info.conn);
		telephony_dbus_info.conn = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	telephony_dbus_info.manager_proxy = dbus_g_proxy_new_for_name(
						telephony_dbus_info.conn,
						BLUEZ_SERVICE_NAME,
						"/", BLUEZ_MANAGER_INTERFACE);

	if (telephony_dbus_info.manager_proxy == NULL) {
		BT_ERR("Could not create a manager proxy\n");
		__bluetooth_telephony_proxy_deinit();
		dbus_g_connection_unref(telephony_dbus_info.conn);
		telephony_dbus_info.conn = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	telephony_dbus_info.dbus_proxy = dbus_g_proxy_new_for_name(
			telephony_dbus_info.conn, DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	if (NULL == telephony_dbus_info.dbus_proxy) {
		__bluetooth_telephony_proxy_deinit();
		dbus_g_connection_unref(telephony_dbus_info.conn);
		telephony_dbus_info.conn = NULL;
		g_object_unref(telephony_dbus_info.manager_proxy);
		telephony_dbus_info.manager_proxy = NULL;
		is_initialized = FALSE;
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_conn = dbus_g_connection_get_connection(telephony_dbus_info.conn);

	/*Add Signal callback for BT enabled*/
	if (!dbus_connection_add_filter(dbus_conn,
				__bt_telephony_adapter_filter,
				NULL, NULL)) {
		BT_ERR("Fail to add filter");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(dbus_conn,
			"type='signal',interface='org.freedesktop.DBus.ObjectManager'"
			",member='InterfacesAdded'",
			&dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add match: %s\n", dbus_error.message);
		dbus_error_free(&dbus_error);
		goto fail;
	}

	/*Callback and user applicaton data*/
	telephony_info.cb = cb;
	telephony_info.user_data = user_data;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

#ifndef TIZEN_WEARABLE
	__bluetooth_telephony_init_headset_state();
#endif

	dbus_error_init(&dbus_error);
	conn = dbus_g_connection_get_connection(telephony_dbus_info.conn);
	dbus_connection_add_filter(conn, __bluetooth_telephony_event_filter,
				NULL, NULL);
	dbus_bus_add_match(conn,
			"type='signal',interface='"BLUEZ_HEADSET_INTERFACE
			"',member='PropertyChanged'", &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		goto fail;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_NREC_STATUS_CHANGE"'" , &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		goto fail;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_ANSWER_CALL"'" , &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		goto fail;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_REJECT_CALL"'" , &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		goto fail;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_RELEASE_CALL"'" , &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
		goto fail;
	}

	dbus_error_init(&dbus_error);
	dbus_bus_add_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_THREEWAY_CALL"'" , &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		BT_ERR("Fail to add dbus filter signal\n");
		dbus_error_free(&dbus_error);
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

	FN_END;
	return ret;
fail:
	bluetooth_telephony_deinit();
	return ret;
}

BT_EXPORT_API int bluetooth_telephony_deinit(void)
{
	DBusConnection *conn;
	DBusError error;

	FN_START;
	BT_TELEPHONY_CHECK_INITIALIZED();

	is_initialized = FALSE;

	conn = dbus_g_connection_get_connection(telephony_dbus_info.conn);

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"BLUEZ_HEADSET_INTERFACE
			"',member='PropertyChanged'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
		/* Need to re initilize before use */
		dbus_error_init(&error);
	}

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_NREC_STATUS_CHANGE"'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_ANSWER_CALL"'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_REJECT_CALL"'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_RELEASE_CALL"'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='"HFP_AGENT_SERVICE
			"',member='"HFP_THREEWAY_CALL"'", &error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_connection_remove_filter(conn,
				__bluetooth_telephony_event_filter,
				NULL);

	if (bluetooth_check_adapter() != BLUETOOTH_ADAPTER_DISABLED ||
		bluetooth_check_adapter_le() != BLUETOOTH_ADAPTER_LE_DISABLED)
		__bluetooth_telephony_unregister();

	__bluetooth_telephony_proxy_deinit();

	telephony_info.cb = NULL;
	telephony_info.user_data = NULL;
	telephony_info.call_count = 0;
	telephony_info.headset_state = BLUETOOTH_STATE_DISCONNETED;

	/* Remove BT enabled signal */
	dbus_error_init(&error);
	dbus_bus_remove_match(conn,
			"type='signal',interface='org.freedesktop.DBus.ObjectManager'"
			",member='InterfacesAdded'",
			&error);
	if (dbus_error_is_set(&error)) {
		BT_ERR("Fail to remove dbus filter signal\n");
		dbus_error_free(&error);
	}

	dbus_connection_remove_filter(dbus_g_connection_get_connection(
				telephony_dbus_info.conn),
				__bt_telephony_adapter_filter,
				NULL);
	g_free(src_addr);
	src_addr = NULL;

	if (telephony_dbus_info.manager_proxy != NULL) {
		g_object_unref(telephony_dbus_info.manager_proxy);
		telephony_dbus_info.manager_proxy = NULL;
	}

	if (telephony_dbus_info.conn != NULL) {
		dbus_g_connection_unref(telephony_dbus_info.conn);
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
	DBusMessage *reply;
	DBusError err;

	gboolean status = FALSE;

	FN_START;

	retv_if(is_initialized == FALSE, FALSE);
	retv_if(bluetooth_check_adapter() == BLUETOOTH_ADAPTER_DISABLED, FALSE);

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IsPlaying", &err, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			__bt_telephony_get_error(err.message);
			dbus_error_free(&err);
		}
		return FALSE;
	}

	if (!dbus_message_get_args(reply, &err,
			DBUS_TYPE_BOOLEAN, &status,
			DBUS_TYPE_INVALID)) {
		BT_ERR("Error to get features");
		if (dbus_error_is_set(&err)) {
			BT_ERR("error message: %s", err.message);
			dbus_error_free(&err);
		}
		dbus_message_unref(reply);
		return FALSE;
	}

#ifdef TIZEN_WEARABLE
	if (status == TRUE && telephony_info.headset_state != BLUETOOTH_STATE_PLAYING)
		telephony_info.headset_state = BLUETOOTH_STATE_PLAYING;
#endif

	BT_INFO("SCO Connected Status = [%d]", status);
	return status;
}

BT_EXPORT_API int bluetooth_telephony_is_nrec_enabled(gboolean *status)
{
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter reply_iter;
	DBusMessageIter reply_iter_entry;
	const char *property;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (status == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	if (telephony_info.headset_state == BLUETOOTH_STATE_DISCONNETED)
		return BLUETOOTH_TELEPHONY_ERROR_AUDIO_NOT_CONNECTED;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetProperties", &err, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			BT_DBG("Error message = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_init(reply, &reply_iter);

	if (dbus_message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_ARRAY) {
		BT_ERR("Can't get reply arguments - DBUS_TYPE_ARRAY");
		dbus_message_unref(reply);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_recurse(&reply_iter, &reply_iter_entry);

	/*Parse the dict */
	while (dbus_message_iter_get_arg_type(&reply_iter_entry) ==
						DBUS_TYPE_DICT_ENTRY) {

		DBusMessageIter dict_entry, dict_entry_val;
		dbus_message_iter_recurse(&reply_iter_entry, &dict_entry);
		dbus_message_iter_get_basic(&dict_entry, &property);
		BT_DBG("String received = %s", property);

		if (g_strcmp0("nrec", property) == 0) {
			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
						DBUS_TYPE_BOOLEAN)
				continue;

			dbus_message_iter_get_basic(&dict_entry_val, status);
			BT_DBG("NREC status = [%d]", *status);
		}
		dbus_message_iter_next(&reply_iter_entry);
	}
	dbus_message_unref(reply);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_is_wbs_mode(gboolean *status)
{
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter reply_iter;
	DBusMessageIter reply_iter_entry;
	unsigned int codec;
	const char *property;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (status == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	*status = FALSE;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetProperties", &err, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call");
		if (dbus_error_is_set(&err)) {
			BT_ERR("Error message = %s", err.message);
			dbus_error_free(&err);
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_init(reply, &reply_iter);

	if (dbus_message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_ARRAY) {
		BT_ERR("Can't get reply arguments - DBUS_TYPE_ARRAY");
		dbus_message_unref(reply);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_recurse(&reply_iter, &reply_iter_entry);

	/*Parse the dict */
	while (dbus_message_iter_get_arg_type(&reply_iter_entry) ==
						DBUS_TYPE_DICT_ENTRY) {

		DBusMessageIter dict_entry, dict_entry_val;
		dbus_message_iter_recurse(&reply_iter_entry, &dict_entry);
		dbus_message_iter_get_basic(&dict_entry, &property);
		BT_DBG("String received = %s", property);

		if (g_strcmp0("codec", property) == 0) {
			dbus_message_iter_next(&dict_entry);
			dbus_message_iter_recurse(&dict_entry, &dict_entry_val);
			if (dbus_message_iter_get_arg_type(&dict_entry_val) !=
						DBUS_TYPE_UINT32)
				continue;

			dbus_message_iter_get_basic(&dict_entry_val, &codec);
			BT_DBG("Codec = [%d]", codec);
			*status = codec == BT_MSBC_CODEC_ID ? TRUE : FALSE;
		}
		dbus_message_iter_next(&reply_iter_entry);
	}
	dbus_message_unref(reply);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_send_vendor_cmd(const char *cmd)
{
	GError *error = NULL;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	BT_DBG("Send Vendor %s", cmd);

	if (telephony_dbus_info.proxy == NULL)
		telephony_dbus_info.proxy =
			__bluetooth_telephony_get_connected_device_proxy();

	if (telephony_dbus_info.proxy == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;

	if (!dbus_g_proxy_call(telephony_dbus_info.proxy, "SendVendorAtCmd",
			&error,
			G_TYPE_STRING, cmd,
			G_TYPE_INVALID, G_TYPE_INVALID)) {
		if (error != NULL) {
			ret = __bt_telephony_get_error(error->message);
			g_error_free(error);
			return ret;
		}

	}
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_start_voice_recognition(void)
{
	DBusMessage *reply;
	DBusError err;
	int ret;
	gboolean state = TRUE;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetVoiceDial", &err, DBUS_TYPE_BOOLEAN, &state,
			DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_stop_voice_recognition(void)
{
	DBusMessage *reply;
	DBusError err;
	int ret;
	gboolean state = FALSE;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetVoiceDial", &err, DBUS_TYPE_BOOLEAN, &state,
			DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_sco_start_cb(
			DBusPendingCall *call, gpointer user_data)
{
	DBusMessage *reply;
	DBusError derr;

	reply = dbus_pending_call_steal_reply(call);
	dbus_error_init(&derr);

	if (dbus_set_error_from_message(&derr, reply)) {
		BT_ERR("hs_sco_cb error: %s, %s",
			derr.name, derr.message);
		dbus_error_free(&derr);
		goto done;
	}
	dbus_pending_call_unref(call);
done:
	BT_DBG("sco_start_cb : -");
	dbus_message_unref(reply);
}

BT_EXPORT_API int bluetooth_telephony_audio_open(void)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusPendingCall *c;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	/* Because this API is async call, so can't use dbus SMACK */
	if (__bt_telephony_check_privilege() ==
				BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	}

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		BT_DBG("No System Bus found\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state == BLUETOOTH_STATE_PLAYING)
		return BLUETOOTH_TELEPHONY_ERROR_ALREADY_CONNECTED;

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE,
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"Play");

	if (dbus_connection_send_with_reply(conn, msg, &c, -1) == FALSE) {
		BT_DBG("HFP_AGENT: send with reply failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	dbus_pending_call_set_notify(c, __bluetooth_telephony_sco_start_cb,
				NULL, NULL);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static void __bluetooth_telephony_sco_close_cb(DBusPendingCall *call, gpointer user_data)
{
	DBusMessage *reply;
	DBusError derr;

	reply = dbus_pending_call_steal_reply(call);
	dbus_error_init(&derr);

	if (dbus_set_error_from_message(&derr, reply)) {
		BT_ERR("sco_close_cb error: %s, %s",
			derr.name, derr.message);
		dbus_error_free(&derr);
		goto done;
	}

	dbus_pending_call_unref(call);
done:
	BT_DBG("sco_close_cb : -");
	dbus_message_unref(reply);
}
BT_EXPORT_API int bluetooth_telephony_audio_close(void)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusPendingCall *c;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	/* Because this API is async call, so can't use dbus SMACK */
	if (__bt_telephony_check_privilege() ==
				BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED) {
		BT_ERR("Don't have a privilege to use this API");
		return BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED;
	}

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		BT_DBG("No System Bus found\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (telephony_info.headset_state != BLUETOOTH_STATE_PLAYING)
		return BLUETOOTH_TELEPHONY_ERROR_NOT_CONNECTED;

	msg = dbus_message_new_method_call(HFP_AGENT_SERVICE,
				HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
				"Stop");
	if (dbus_connection_send_with_reply(conn, msg, &c, -1) == FALSE) {
		BT_DBG("HFP_AGENT: send with reply failed");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	telephony_info.headset_state = BLUETOOTH_STATE_CONNECTED;

	dbus_pending_call_set_notify(c, __bluetooth_telephony_sco_close_cb,
								NULL, NULL);

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
				BT_ERR("Audio connection call Failed = %d", ret);
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
			DBG_SECURE(" call number [%s]", call_status->phone_number);

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
			if ((call_status->call_status < BLUETOOTH_CALL_STATE_NONE) ||
				(call_status->call_status >= BLUETOOTH_CALL_STATE_ERROR)) {
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
	DBusMessage *reply;
	DBusError err;
	const char *path = telephony_info.call_path;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"OutgoingCall", &err, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_STRING, &ph_number, DBUS_TYPE_INT32,
			&call_id, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

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
	DBusMessage *reply;
	DBusError err;
	const char *path = telephony_info.call_path;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	if (NULL == ph_number)
		return BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM;

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"IncomingCall", &err, DBUS_TYPE_STRING, &path,
			DBUS_TYPE_STRING, &ph_number, DBUS_TYPE_INT32,
			&call_id, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);

	telephony_info.call_count++;
	BT_DBG("telephony_info.call_count = [%d]", telephony_info.call_count);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_set_speaker_gain(
						unsigned short speaker_gain)
{
	DBusMessage *reply;
	DBusError err;
	int ret;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	BT_DBG("set speaker_gain= [%d]", speaker_gain);

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"SetSpeakerGain", &err, DBUS_TYPE_UINT16,
			&speaker_gain, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_unref(reply);
	FN_END;

	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

BT_EXPORT_API int bluetooth_telephony_get_headset_volume(
						unsigned int *speaker_gain)
{
	DBusMessage *reply;
	DBusError err;
	int ret;
	guint16 gain;

	FN_START;

	BT_TELEPHONY_CHECK_INITIALIZED();
	BT_TELEPHONY_CHECK_ENABLED();

	reply = __bluetooth_telephony_dbus_method_send(
			HFP_AGENT_PATH, HFP_AGENT_INTERFACE,
			"GetSpeakerGain", &err, DBUS_TYPE_INVALID);

	if (!reply) {
		BT_ERR("Error returned in method call\n");
		if (dbus_error_is_set(&err)) {
			ret = __bt_telephony_get_error(err.message);
			dbus_error_free(&err);
			return ret;
		}
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	if (!dbus_message_get_args(reply, &err,
			DBUS_TYPE_UINT16, &gain,
			DBUS_TYPE_INVALID)) {
		BT_ERR("Error to get features");
		if (dbus_error_is_set(&err)) {
			BT_ERR("error message: %s", err.message);
			dbus_error_free(&err);
		}
		dbus_message_unref(reply);
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}
	*speaker_gain = gain;
	BT_DBG("Get speaker_gain= [%d]", *speaker_gain);

	dbus_message_unref(reply);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}

static DBusHandlerResult __bt_telephony_adapter_filter(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	int ret;
	char *object_path = NULL;
	const char *member = dbus_message_get_member(msg);
	FN_START;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strcasecmp(member, "InterfacesAdded") == 0) {
		if (__bt_telephony_get_object_path(msg, &object_path)) {
			BT_ERR("Fail to get the path");
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		if (strcasecmp(object_path, DEFAULT_ADAPTER_OBJECT_PATH) == 0) {

			BT_DBG("Adapter added [%s]", object_path);
			BT_DBG("BlueZ is Activated and flag need to be reset");
			BT_DBG("Send enabled to application");

			if (__bt_telephony_get_src_addr(msg)) {
				BT_ERR("Fail to get the local adapter address");
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			}

			ret = __bluetooth_telephony_register();
			if (ret != BLUETOOTH_TELEPHONY_ERROR_NONE)
				BT_ERR("__bluetooth_telephony_register failed");
		}
	}

	FN_END;
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int __bt_telephony_get_object_path(DBusMessage *msg, char **path)
{
	DBusMessageIter item_iter;
	dbus_message_iter_init(msg, &item_iter);
	FN_START;

	if (dbus_message_iter_get_arg_type(&item_iter)
					!= DBUS_TYPE_OBJECT_PATH) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, path);

	if (*path == NULL)
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;

	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}


static int __bt_telephony_get_src_addr(DBusMessage *msg)
{
	char *object_path;
	const char *property = NULL;
	char *interface_name;
	DBusMessageIter item_iter;
	DBusMessageIter value_iter;
	DBusMessageIter msg_iter, dict_iter;
	DBusMessageIter in_iter, in2_iter;
	char *bd_addr;
	FN_START;

	dbus_message_iter_init(msg, &item_iter);

	/* signature of InterfacesAdded signal is oa{sa{sv}} */
	if (dbus_message_iter_get_arg_type(&item_iter)
				!= DBUS_TYPE_OBJECT_PATH) {
		BT_ERR("This is bad format dbus\n");
		return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
	}

	dbus_message_iter_get_basic(&item_iter, &object_path);
	retv_if(object_path == NULL,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

	if (strcasecmp(object_path, DEFAULT_ADAPTER_OBJECT_PATH) == 0) {
		/* get address from here */
		retv_if(dbus_message_iter_next(&item_iter) == FALSE,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

		/* signature a{sa{sv}} */
		retv_if(dbus_message_iter_get_arg_type(&item_iter) !=
				DBUS_TYPE_ARRAY,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);
		dbus_message_iter_recurse(&item_iter, &value_iter);
		while (dbus_message_iter_get_arg_type(&value_iter) ==
					DBUS_TYPE_DICT_ENTRY) {
			dbus_message_iter_recurse(&value_iter, &msg_iter);
			if (dbus_message_iter_get_arg_type(&msg_iter)
					!= DBUS_TYPE_STRING) {
				BT_ERR("This is bad format dbus\n");
				return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
			}

			dbus_message_iter_get_basic(&msg_iter, &interface_name);
			retv_if(interface_name == NULL,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

			BT_DBG("interface name is %s", interface_name);

			if (strcasecmp(interface_name, BLUEZ_ADAPTER_INTERFACE) == 0) {
				retv_if(!dbus_message_iter_next(&msg_iter),
					BLUETOOTH_TELEPHONY_ERROR_INTERNAL);
				dbus_message_iter_recurse(&msg_iter, &in_iter);

				if (dbus_message_iter_get_arg_type(&in_iter)
						!= DBUS_TYPE_DICT_ENTRY) {
					BT_ERR("This is bad format dbus\n");
					return BLUETOOTH_TELEPHONY_ERROR_INTERNAL;
				}

				dbus_message_iter_recurse(&in_iter, &dict_iter);
				dbus_message_iter_get_basic(
							&dict_iter, &property);

				retv_if(property == NULL,
				BLUETOOTH_TELEPHONY_ERROR_INTERNAL);
				retv_if(!dbus_message_iter_next(&dict_iter),
					BLUETOOTH_TELEPHONY_ERROR_INTERNAL);

				if (strcasecmp(property, "Address") == 0) {
					dbus_message_iter_recurse
						(&dict_iter, &in2_iter);
					dbus_message_iter_get_basic
						(&in2_iter, &bd_addr);
					src_addr = g_strdup(bd_addr);
					break;
				}
			}
			dbus_message_iter_next(&value_iter);
		}
	}
	BT_DBG("default adapter address is src_addr = %s", src_addr);
	FN_END;
	return BLUETOOTH_TELEPHONY_ERROR_NONE;
}
