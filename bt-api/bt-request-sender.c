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

#include "bluetooth-api.h"
#include "bluetooth-hid-api.h"
#include "bluetooth-audio-api.h"
#include "bt-internal-types.h"

#include "bt-common.h"
#include "bt-request-sender.h"
#include "bt-event-handler.h"

/* auto generated header by bt-request-service.xml*/
#include "bt-request-service.h"

static GSList *sending_requests;

DBusGConnection *service_conn;
DBusGConnection *system_conn;
DBusGProxy *service_proxy;

static void __bt_remove_all_sending_requests(void)
{
	GSList *l;
	bt_req_info_t *info;

	for (l = sending_requests; l != NULL; l = g_slist_next(l)) {
		info = l->data;

		if (info && info->proxy && info->proxy_call)
			dbus_g_proxy_cancel_call(info->proxy, info->proxy_call);
	}

	g_slist_free(sending_requests);
	sending_requests = NULL;
}

DBusGProxy *_bt_init_service_proxy(void)
{
	DBusGProxy *proxy;

	g_type_init();

	if (service_conn == NULL) {
		service_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
		retv_if(service_conn == NULL, NULL);
	}

	proxy = dbus_g_proxy_new_for_name(service_conn, BT_DBUS_NAME, BT_SERVICE_PATH,
			BT_DBUS_NAME);

	if (proxy == NULL) {
		dbus_g_connection_unref(service_conn);
		service_conn = NULL;
		return NULL;
	}

	service_proxy = proxy;

	return proxy;
}

void _bt_deinit_proxys(void)
{
	__bt_remove_all_sending_requests();

	if (service_proxy) {
		g_object_unref(service_proxy);
		service_proxy = NULL;
	}

	if (service_conn) {
		dbus_g_connection_unref(service_conn);
		service_conn = NULL;
	}

	if (system_conn) {
		dbus_g_connection_unref(system_conn);
		system_conn = NULL;
	}
}

static DBusGProxy *__bt_get_service_proxy(void)
{
	return (service_proxy) ? service_proxy : _bt_init_service_proxy();
}

static void __bt_get_event_info(int service_function, GArray *output,
			int *event, int *event_type, void **param_data)
{
	ret_if(event == NULL);

	switch (service_function) {
	case BT_BOND_DEVICE:
		*event_type = BT_ADAPTER_EVENT;
		*event = BLUETOOTH_EVENT_BONDING_FINISHED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_info_t, 0);
		break;
	case BT_UNBOND_DEVICE:
		*event_type = BT_ADAPTER_EVENT;
		*event = BLUETOOTH_EVENT_BONDED_DEVICE_REMOVED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_info_t, 0);
		break;
	case BT_SEARCH_SERVICE:
		*event_type = BT_ADAPTER_EVENT;
		*event = BLUETOOTH_EVENT_SERVICE_SEARCHED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_info_t, 0);
		break;
	case BT_HID_CONNECT:
		*event_type = BT_HID_EVENT;
		*event = BLUETOOTH_HID_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_address_t, 0);
		break;
	case BT_HID_DISCONNECT:
		*event_type = BT_HID_EVENT;
		*event = BLUETOOTH_HID_DISCONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_address_t, 0);
		break;
	case BT_AUDIO_CONNECT:
	case BT_AG_CONNECT:
		*event_type = BT_HEADSET_EVENT;
		*event = BLUETOOTH_EVENT_AG_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output, char, 0);
		break;
	case BT_AUDIO_DISCONNECT:
	case BT_AG_DISCONNECT:
		*event_type = BT_HEADSET_EVENT;
		*event = BLUETOOTH_EVENT_AG_DISCONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output, char, 0);
		break;
	case BT_AV_CONNECT:
		*event_type = BT_HEADSET_EVENT;
		*event = BLUETOOTH_EVENT_AV_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output, char, 0);
		break;
	case BT_AV_DISCONNECT:
		*event_type = BT_HEADSET_EVENT;
		*event = BLUETOOTH_EVENT_AV_DISCONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output, char, 0);
		break;
	case BT_NETWORK_CONNECT:
		*event_type = BT_ADAPTER_EVENT;
		*event = BLUETOOTH_EVENT_NETWORK_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_address_t, 0);
		break;
	case BT_NETWORK_DISCONNECT:
		*event_type = BT_ADAPTER_EVENT;
		*event = BLUETOOTH_EVENT_NETWORK_DISCONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_device_address_t, 0);
		break;
	case BT_RFCOMM_CLIENT_CONNECT:
		*event_type = BT_RFCOMM_CLIENT_EVENT;
		*event = BLUETOOTH_EVENT_RFCOMM_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output,
				bluetooth_rfcomm_connection_t, 0);

		break;
	default:
		BT_ERR("Unknown function");
		return;
	}
}

/*
out param1: API result
out param2: return paramter
out param3:
*/
void __send_request_cb(DBusGProxy *proxy, GArray *out_param1,
			GArray *out_param2, GError *error,
			gpointer userdata)
{
	bluetooth_event_param_t bt_event;
	bt_req_info_t *cb_data = userdata;
	int result = BLUETOOTH_ERROR_NONE;
	int event_type = BT_ADAPTER_EVENT;
	int request_id;

	memset(&bt_event, 0x00, sizeof(bluetooth_event_param_t));

	if (error != NULL) {
		/* dBUS gives error cause */
		BT_ERR("D-Bus API failure: message[%s]", error->message);
		g_error_free(error);
		result = BLUETOOTH_ERROR_TIMEOUT;

		ret_if(cb_data == NULL);

		__bt_get_event_info(cb_data->service_function, NULL,
				&bt_event.event, &event_type,
				&bt_event.param_data);
	} else {
		ret_if(out_param2 == NULL);

		result = g_array_index(out_param2, int, 0);

		ret_if(cb_data == NULL);

		__bt_get_event_info(cb_data->service_function, out_param1,
				&bt_event.event, &event_type,
				&bt_event.param_data);

		if (result == BLUETOOTH_ERROR_NONE) {
			if (cb_data->service_function == BT_OPP_PUSH_FILES) {
				request_id = g_array_index(out_param1, int, 0);
				_bt_add_push_request_id(request_id);
			}

			if (out_param1)
				g_array_free(out_param1, FALSE);

			if (out_param2)
				g_array_free(out_param2, FALSE);

			goto done;
		}

		if (out_param1)
			g_array_free(out_param1, FALSE);

		if (out_param2)
			g_array_free(out_param2, FALSE);
	}

	if (cb_data->cb == NULL)
		goto done;

	/* Only if fail case, call the callback function*/
	bt_event.result = result;

	if (event_type == BT_ADAPTER_EVENT || event_type == BT_RFCOMM_CLIENT_EVENT) {
		((bluetooth_cb_func_ptr)cb_data->cb)(bt_event.event,
				&bt_event,
				cb_data->user_data);
	} else if (event_type == BT_HID_EVENT) {
		((hid_cb_func_ptr)cb_data->cb)(bt_event.event,
				(hid_event_param_t *)&bt_event,
				cb_data->user_data);
	} else if (event_type == BT_HEADSET_EVENT) {
		((bt_audio_func_ptr)cb_data->cb)(bt_event.event,
				(bt_audio_event_param_t *)&bt_event,
				cb_data->user_data);
	}
done:
	sending_requests = g_slist_remove(sending_requests, (void *)cb_data);

	g_free(cb_data);
}

int _bt_send_request(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	char *cookie;
	gboolean ret;
	GError *error = NULL;
	GArray *in_param5 = NULL;
	GArray *out_param2 = NULL;
	DBusGProxy *proxy;

	switch (service_type) {
	case BT_BLUEZ_SERVICE:
	case BT_OBEX_SERVICE:
		proxy = __bt_get_service_proxy();
		retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		in_param5 = g_array_new(FALSE, FALSE, sizeof(gchar));

		cookie = _bt_get_cookie();

		if (cookie) {
			g_array_append_vals(in_param5, cookie,
					_bt_get_cookie_size());
		}

		ret = org_projectx_bt_service_request(proxy,
					service_type, service_function,
					BT_SYNC_REQ, in_param1, in_param2,
					in_param3, in_param4, in_param5,
					out_param1, &out_param2, &error);

		g_array_free(in_param5, TRUE);
		break;
	default:
		BT_ERR("Unknown service type");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	if (out_param2) {
		result = g_array_index(out_param2, int, 0);
		g_array_free(out_param2, TRUE);
	} else {
		result = BLUETOOTH_ERROR_INTERNAL;
	}

	if (ret != TRUE) {
		/* dBUS-RPC is failed */
		BT_ERR("dBUS-RPC is failed");

		if (error != NULL) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
			       error->code, error->message);

			g_error_free(error);
		}
		else {
			/* dBUS does not give error cause dBUS-RPC is failed */
			BT_ERR("error returned was NULL");
		}

		return result;
	}

	BT_DBG("dBUS RPC is successfully done. type=%d, function=0x%x",
			service_type, service_function);

	return result;
}

int _bt_send_request_async(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			void *callback, void *user_data)
{
	GArray* in_param5 = NULL;
	char *cookie;
	bt_req_info_t *cb_data;
	DBusGProxy *proxy;
	DBusGProxyCall *proxy_call;

	cb_data = g_new0(bt_req_info_t, 1);

	cb_data->service_function = service_function;
	cb_data->cb = callback;
	cb_data->user_data = user_data;

	switch (service_type) {
	case BT_BLUEZ_SERVICE:
	case BT_OBEX_SERVICE:
		proxy = __bt_get_service_proxy();
		retv_if(proxy == NULL, BLUETOOTH_ERROR_INTERNAL);

		dbus_g_proxy_set_default_timeout(proxy, BT_DBUS_TIMEOUT_MAX);

		in_param5 = g_array_new(FALSE, FALSE, sizeof(gchar));

		cookie = _bt_get_cookie();

		if (cookie) {
			g_array_append_vals(in_param5, cookie,
					_bt_get_cookie_size());
		}

		proxy_call = org_projectx_bt_service_request_async(proxy, service_type,
                        service_function, BT_ASYNC_REQ, in_param1, in_param2,
                        in_param3, in_param4, in_param5,
                        (org_projectx_bt_service_request_reply)__send_request_cb,
                        (gpointer)cb_data);

		if (proxy_call == NULL) {

			BT_ERR("dBUS-RPC is failed");
			g_array_free(in_param5, TRUE);
			g_free(cb_data);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		sending_requests = g_slist_append(sending_requests, cb_data);

		g_array_free(in_param5, TRUE);
		break;
	}

	BT_DBG("dBUS RPC is successfully done. type=%d, function=0x%x",
			 service_type, service_function);

	return BLUETOOTH_ERROR_NONE;
}

