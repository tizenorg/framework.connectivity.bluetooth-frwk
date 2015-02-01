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

static GDBusConnection *service_gconn;
static GDBusProxy *service_gproxy;

static GDBusProxy *__bt_gdbus_init_service_proxy(void)
{
	GDBusProxy *proxy;
	GError *err = NULL;

	g_type_init();

	if (service_gconn == NULL)
		service_gconn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);

	if (!service_gconn) {
		if (err) {
			BT_ERR("Unable to connect to dbus: %s", err->message);
			g_clear_error(&err);
		}
		return NULL;
	}

	proxy =  g_dbus_proxy_new_sync(service_gconn,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			BT_DBUS_NAME,
			BT_SERVICE_PATH,
			BT_DBUS_NAME,
			NULL, &err);
	if (!proxy) {
		if (err) {
			 BT_ERR("Unable to create proxy: %s", err->message);
			 g_clear_error(&err);
		}

		g_object_unref(service_gconn);
		service_gconn = NULL;
		return NULL;
	}

	service_gproxy = proxy;

	return proxy;
}

static GDBusProxy *__bt_gdbus_get_service_proxy(void)
{
	return (service_gproxy) ? service_gproxy : __bt_gdbus_init_service_proxy();
}

void _bt_gdbus_deinit_proxys(void)
{
	if (service_gproxy) {
		g_object_unref(service_proxy);
		service_proxy = NULL;
	}

	if (service_gconn) {
		g_object_unref(service_gconn);
		service_gconn = NULL;
	}
}

static void __bt_get_event_info(int service_function, GArray *output,
			int *event, int *event_type, void **param_data)
{
	ret_if(event == NULL);

	BT_DBG("service_function : %x", service_function);
	switch (service_function) {
	case BT_BOND_DEVICE:
	case BT_BOND_DEVICE_BY_TYPE:
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
	case BT_HF_CONNECT:
		*event_type = BT_HF_AGENT_EVENT;
		*event = BLUETOOTH_EVENT_HF_CONNECTED;
		ret_if(output == NULL);
		*param_data = &g_array_index(output, char, 0);
		break;
	case BT_HF_DISCONNECT:
		*event_type = BT_HF_AGENT_EVENT;
		*event = BLUETOOTH_EVENT_HF_DISCONNECTED;
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
static void __bt_fill_garray_from_variant(GVariant *var, GArray *param)
{
	char *data;
	int size;

	size = g_variant_get_size(var);
	if (size > 0) {
		data = (char *)g_variant_get_data(var);
		if (data)
			param = g_array_append_vals(param, data, size);

	}
}

static void __send_request_cb(GDBusProxy *proxy,
                     GAsyncResult *res,
                     gpointer      user_data)
{
	bluetooth_event_param_t bt_event;
	bt_req_info_t *cb_data = user_data;
	int result = BLUETOOTH_ERROR_NONE;
	int event_type = BT_ADAPTER_EVENT;
	int request_id;
	GError *error = NULL;
	GVariant *value;
	GVariant *param1;
	GVariant *param2;
	GArray *out_param1 = NULL;
	GArray *out_param2 = NULL;

	BT_DBG("+");
	memset(&bt_event, 0x00, sizeof(bluetooth_event_param_t));

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		if (error) {
			/* dBUS gives error cause */
			BT_ERR("D-Bus API failure: message[%s]",
							error->message);
			g_clear_error(&error);
		}
		result = BLUETOOTH_ERROR_TIMEOUT;

		ret_if(cb_data == NULL);

		__bt_get_event_info(cb_data->service_function, NULL,
				&bt_event.event, &event_type,
				&bt_event.param_data);
	} else {
		g_variant_get(value, "(@ay@ay)", &param1, &param2);
		g_variant_unref(value);

		if (param1) {
			out_param1 = g_array_new(TRUE, TRUE, sizeof(gchar));
			__bt_fill_garray_from_variant(param1, out_param1);
			g_variant_unref(param1);
		}

		if (param2) {
			out_param2 = g_array_new(TRUE, TRUE, sizeof(gchar));
			__bt_fill_garray_from_variant(param2, out_param2);
			result = g_array_index(out_param2, int, 0);
			g_variant_unref(param2);
			g_array_free(out_param2, TRUE);
		} else {
			result = BLUETOOTH_ERROR_INTERNAL;
		}

		ret_if(cb_data == NULL);

		__bt_get_event_info(cb_data->service_function, out_param1,
				&bt_event.event, &event_type,
				&bt_event.param_data);

		if (result == BLUETOOTH_ERROR_NONE && out_param1) {
			if (cb_data->service_function == BT_OPP_PUSH_FILES) {
				request_id = g_array_index(out_param1, int, 0);
				BT_DBG("request_id : %d", request_id);
				_bt_add_push_request_id(request_id);
			}

			goto done;
		}

	}

	if (cb_data->cb == NULL)
		goto done;

	/* Only if fail case, call the callback function*/
	bt_event.result = result;
	BT_INFO("event_type[%d], result=[%d]", event_type, result);

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
	} else if (event_type == BT_HF_AGENT_EVENT) {
		((bt_audio_func_ptr)cb_data->cb)(bt_event.event,
				(bt_audio_event_param_t *)&bt_event,
				cb_data->user_data);
	} else {
		BT_INFO("Not handled event type : %d", event_type);
	}
done:
	if (out_param1)
		g_array_free(out_param1, TRUE);

	sending_requests = g_slist_remove(sending_requests, (void *)cb_data);

	g_free(cb_data);
	BT_DBG("-");
}

int _bt_sync_send_request(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;
	char *cookie;
	GError *error = NULL;
	GArray *in_param5 = NULL;
	GArray *out_param2 = NULL;

	GDBusProxy  *proxy;
	GVariant *ret;
	GVariant *param1;
	GVariant *param2;
	GVariant *param3;
	GVariant *param4;
	GVariant *param5;

	switch (service_type) {
	case BT_BLUEZ_SERVICE:
	case BT_OBEX_SERVICE:
	case BT_AGENT_SERVICE:
		proxy = __bt_gdbus_get_service_proxy();
		if (!proxy)
			return BLUETOOTH_ERROR_INTERNAL;

		in_param5 = g_array_new(TRUE, TRUE, sizeof(gchar));

		cookie = _bt_get_cookie();

		if (cookie) {
			g_array_append_vals(in_param5, cookie,
					_bt_get_cookie_size());
		}

		param1 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param1->data, in_param1->len,
					TRUE, NULL, NULL);
		param2 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param2->data, in_param2->len,
					TRUE, NULL, NULL);
		param3 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param3->data, in_param3->len,
					TRUE, NULL, NULL);
		param4 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param4->data, in_param4->len,
					TRUE, NULL, NULL);
		param5 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param5->data, 	in_param5->len,
					TRUE, NULL, NULL);

		ret = g_dbus_proxy_call_sync(proxy, "service_request",
					g_variant_new("(iii@ay@ay@ay@ay@ay)",
						service_type, service_function,
						BT_SYNC_REQ, param1,
						param2, param3,
						param4, param5),
					G_DBUS_CALL_FLAGS_NONE, -1,
					NULL, &error);

		g_array_free(in_param5, TRUE);

		if (ret == NULL) {
			/* dBUS-RPC is failed */
			BT_ERR("dBUS-RPC is failed");

			if (error != NULL) {
				/* dBUS gives error cause */
				BT_ERR("D-Bus API failure: errCode[%x], message[%s]",
				       error->code, error->message);

				g_clear_error(&error);
			} else {
				/* dBUS does not give error cause dBUS-RPC is failed */
				BT_ERR("error returned was NULL");
			}

			return BLUETOOTH_ERROR_INTERNAL;
		}

		param1 = NULL;
		param2 = NULL;

		g_variant_get(ret, "(@ay@ay)", &param1, &param2);

		if (param1) {
			*out_param1 = g_array_new(TRUE, TRUE, sizeof(gchar));
			__bt_fill_garray_from_variant(param1, *out_param1);
			g_variant_unref(param1);
		}

		if (param2) {
			out_param2 = g_array_new(TRUE, TRUE, sizeof(gchar));
			__bt_fill_garray_from_variant(param2, out_param2);
			result = g_array_index(out_param2, int, 0);
			g_variant_unref(param2);
			g_array_free(out_param2, TRUE);
		} else {
			result = BLUETOOTH_ERROR_INTERNAL;
		}

		g_variant_unref(ret);
		break;
	default:
		BT_ERR("Unknown service type");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return result;
}

int _bt_async_send_request(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			void *callback, void *user_data)
{
	GArray* in_param5 = NULL;
	bt_req_info_t *cb_data;
	char *cookie;

	GDBusProxy *proxy;
	int timeout;
	GVariant *param1;
	GVariant *param2;
	GVariant *param3;
	GVariant *param4;
	GVariant *param5;

	BT_DBG("service_function : %x", service_function);

	cb_data = g_new0(bt_req_info_t, 1);

	cb_data->service_function = service_function;
	cb_data->cb = callback;
	cb_data->user_data = user_data;

	switch (service_type) {
	case BT_BLUEZ_SERVICE:
	case BT_OBEX_SERVICE:
		proxy =  __bt_gdbus_get_service_proxy();
		if (!proxy) {
			g_free(cb_data);
			return BLUETOOTH_ERROR_INTERNAL;
		}

		/* Do not timeout the request in certain cases. Sometime the
		 * request may take undeterministic time to reponse.
		 * (for ex: pairing retry) */
		if (service_function == BT_BOND_DEVICE ||
			service_function == BT_BOND_DEVICE_BY_TYPE)
			timeout = INT_MAX;
		else
			timeout = BT_DBUS_TIMEOUT_MAX;

		in_param5 = g_array_new(TRUE, TRUE, sizeof(gchar));

		cookie = _bt_get_cookie();

		if (cookie) {
			g_array_append_vals(in_param5, cookie,
					_bt_get_cookie_size());
		}
		param1 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param1->data, in_param1->len,
					TRUE, NULL, NULL);
		param2 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param2->data, in_param2->len,
					TRUE, NULL, NULL);
		param3 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param3->data, in_param3->len,
					TRUE, NULL, NULL);
		param4 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param4->data, in_param4->len,
					TRUE, NULL, NULL);
		param5 = g_variant_new_from_data((const GVariantType *)"ay",
					in_param5->data, in_param5->len,
					TRUE, NULL, NULL);

		g_dbus_proxy_call(proxy, "service_request",
					g_variant_new("(iii@ay@ay@ay@ay@ay)",
						service_type, service_function,
						BT_ASYNC_REQ, param1, param2,
						param3, param4, param5),
					G_DBUS_CALL_FLAGS_NONE,
					timeout, NULL,
					(GAsyncReadyCallback)__send_request_cb,
					(gpointer)cb_data);
		sending_requests = g_slist_append(sending_requests, cb_data);

		g_array_free(in_param5, TRUE);
		break;
	}

	return BLUETOOTH_ERROR_NONE;
}

