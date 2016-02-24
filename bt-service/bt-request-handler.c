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

#include <string.h>
#include <glib.h>
#include <dlog.h>
#include <security-server.h>
#include <gio/gio.h>
#include <sys/smack.h>

#include "bluetooth-api.h"
#include "bt-service-common.h"
#include "bt-service-util.h"
#include "bt-service-event.h"
#include "bt-service-adapter.h"
#include "bt-service-adapter-le.h"
#include "bt-service-device.h"
#include "bt-service-hid.h"
#include "bt-service-network.h"
#include "bt-service-audio.h"
#include "bt-service-oob.h"
#include "bt-service-avrcp.h"
#include "bt-service-avrcp-controller.h"
#include "bt-service-opp-client.h"
#include "bt-service-obex-server.h"
#include "bt-service-rfcomm-client.h"
#include "bt-service-rfcomm-server.h"
#include "bt-request-handler.h"
#include "bt-service-pbap.h"

static GDBusConnection *bt_service_conn;
static guint owner_id = 0;

static const gchar bt_service_introspection_xml[] =
"<node name='/org/projectx/bt_service'>"
"	<interface name='org.projectx.bt'>"
"		<method name='service_request'>"
			/* Input Parameters */
"			<arg type='i' name='service_type' direction='in' />"
"			<arg type='i' name='service_function' direction='in' />"
"			<arg type='i' name='request_type' direction='in' />"
"			<arg type='ay' name='input_param1' direction='in' />"
"			<arg type='ay' name='input_param2' direction='in' />"
"			<arg type='ay' name='input_param3' direction='in' />"
"			<arg type='ay' name='input_param4' direction='in' />"
"			<arg type='ay' name='input_param5' direction='in' />"
			/* Return Parameters */
"			<arg type='i' name='output_param1' direction='out' />"
"			<arg type='v' name='output_param2' direction='out' />"
"		</method>"
"	</interface>"
"</node>";

GDBusNodeInfo *node_info = NULL;

static void __bt_service_method(GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data);

int __bt_bluez_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GArray **out_param1);
int __bt_obexd_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GVariant *in_param5,
		GArray **out_param1);
int __bt_agent_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GArray **out_param1);
int __bt_core_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1);

gboolean __bt_service_check_privilege(int function_name,
					int service_type,
					GVariant *param);

int __bt_service_check_file_path_security(char **file_path,
					int file_count, GVariant *param, char *type);

/* Function definitions*/
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

static void __bt_service_get_parameters(GVariant *in_param,
		void *value, int size)
{
	void *buf = NULL;
	buf = (void *)g_variant_get_data(in_param);
	memcpy(value, buf, size);
}

static void __bt_service_method(GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	FN_START;
	BT_DBG("Method[%s] Object Path[%s] Interface Name[%s]",
			method_name, object_path, interface_name);

	if (g_strcmp0(method_name, "service_request") == 0) {
		int service_type;
		int service_function;
		int request_type;
		GVariant *param1 = NULL;
		GVariant *param2 = NULL;
		GVariant *param3 = NULL;
		GVariant *param4 = NULL;
		GVariant *param5 = NULL;
		GArray *out_param1 = NULL;
		GVariant *out_var = NULL;
		int result = 0;
		int request_id = -1;

		g_variant_get(parameters, "(iii@ay@ay@ay@ay@ay)", &service_type,
				&service_function, &request_type,
				&param1, &param2, &param3, &param4, &param5);

		out_param1 = g_array_new(FALSE, FALSE, sizeof(gchar));

		if (service_type == BT_CORE_SERVICE) {
			BT_DBG("No need to check privilege from bt-core");
		} else if (__bt_service_check_privilege(service_function,
					service_type, param5) == FALSE) {
			result = BLUETOOTH_ERROR_PERMISSION_DEINED;
			BT_ERR("Priviledge Check Failed");

			goto fail;
		}

		if (request_type == BT_ASYNC_REQ
				|| service_function == BT_OBEX_SERVER_ACCEPT_CONNECTION) {
			/* Set the timer */
			request_id = _bt_assign_request_id();
			BT_DBG("Request ID: %d", request_id);

			if (request_id < 0) {
				BT_ERR("Fail to assign the request id");
				result = BLUETOOTH_ERROR_INTERNAL;

				goto fail;
			}
		}

		BT_DBG("SERVICE TYPE [%d] SERVICE FUNC [%d]",
				service_type, service_function);
		switch (service_type) {
		case BT_BLUEZ_SERVICE:
			result = __bt_bluez_request(service_function,
					request_type, request_id,
					invocation, param1, param2,
					param3, param4, &out_param1);
			break;
		case BT_OBEX_SERVICE:
			result = __bt_obexd_request(service_function,
					request_type, request_id,
					invocation, param1,
					param2, param3,
					param4, param5, &out_param1);
			break;
		case BT_AGENT_SERVICE:
			result = __bt_agent_request(service_function,
					request_type, request_id,
					invocation, param1,
					param2, param3,
					param4, &out_param1);
			break;
		case BT_CORE_SERVICE:
			result = __bt_core_request(service_function,
					request_type, request_id,
					invocation, param1);
			break;
		case BT_CHECK_PRIVILEGE:
			result = BLUETOOTH_ERROR_NONE;
			break;
		default:
			BT_ERR("Unknown service type");
			result = BLUETOOTH_ERROR_INTERNAL;
			goto fail;
		}

		if (result != BLUETOOTH_ERROR_NONE) {
			goto fail;
		}

		if ((request_type == BT_ASYNC_REQ ||
			service_function == BT_OBEX_SERVER_ACCEPT_CONNECTION) &&
			service_function != BT_OPP_PUSH_FILES) {
			BT_DBG("INSERT INTO REQ LIST");
			_bt_insert_request_list(request_id, service_function,
						NULL, invocation);
		} else {
			/* Return result */
			if (service_type == BT_CHECK_PRIVILEGE ||
					service_function != BT_OPP_PUSH_FILES) {
				out_var = g_variant_new_from_data((const GVariantType *)"ay",
						out_param1->data, out_param1->len,
						TRUE, NULL, NULL);

				GVariant *temp = g_variant_new("(iv)", result, out_var);
				g_dbus_method_invocation_return_value(invocation, temp);

				g_array_free(out_param1, TRUE);
				out_param1 = NULL;
			}
		}

		g_variant_unref(param1);
		g_variant_unref(param2);
		g_variant_unref(param3);
		g_variant_unref(param4);
		g_variant_unref(param5);
		FN_END;
		return;
fail:
		BT_ERR_C("Request is failed [%s] [%x]",
				_bt_convert_error_to_string(result), result);

		out_var = g_variant_new_from_data((const GVariantType *)"ay",
				out_param1->data, out_param1->len,
				TRUE, NULL, NULL);

		GVariant *temp = g_variant_new("(iv)", result, out_var);
		g_dbus_method_invocation_return_value(invocation, temp);

		g_array_free(out_param1, TRUE);
		out_param1 = NULL;

		if (request_type == BT_ASYNC_REQ)
			_bt_delete_request_id(request_id);

		g_variant_unref(param1);
		g_variant_unref(param2);
		g_variant_unref(param3);
		g_variant_unref(param4);
		g_variant_unref(param5);
	}

	FN_END;
	return;
}


static const GDBusInterfaceVTable method_table = {
	__bt_service_method,
	NULL,
	NULL,
};

int __bt_bluez_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GArray **out_param1)
{
	int result = BLUETOOTH_ERROR_NONE;

	switch (function_name) {
	case BT_ENABLE_ADAPTER:
		result = _bt_enable_adapter();
		break;
	case BT_DISABLE_ADAPTER:
		result = _bt_disable_adapter();
		break;
	case BT_RECOVER_ADAPTER:
		result = _bt_recover_adapter();
		break;
	case BT_ENABLE_ADAPTER_LE:
		result = _bt_enable_adapter_le();
		break;
	case BT_DISABLE_ADAPTER_LE:
		result = _bt_disable_adapter_le();
		break;
	case BT_RESET_ADAPTER:
		result = _bt_reset_adapter();
		break;
	case BT_CHECK_ADAPTER: {
		int enabled = BT_ADAPTER_DISABLED;

		result = _bt_check_adapter(&enabled);

		g_array_append_vals(*out_param1, &enabled,
				sizeof(int));
		break;
	}
	case BT_GET_LOCAL_ADDRESS: {
		bluetooth_device_address_t local_address = { {0} };
		result = _bt_get_local_address(&local_address);

		g_array_append_vals(*out_param1, &local_address,
				sizeof(bluetooth_device_address_t));
		break;
	}
	case BT_GET_LOCAL_VERSION: {
		bluetooth_version_t ver = { {0} };
		result = _bt_get_local_version(&ver);

		g_array_append_vals(*out_param1, &ver,
				sizeof(bluetooth_version_t));
		break;
	}
	case BT_GET_LOCAL_NAME: {
		bluetooth_device_name_t local_name = { {0} };
		result = _bt_get_local_name(&local_name);

		g_array_append_vals(*out_param1, &local_name,
				sizeof(bluetooth_device_name_t));

		break;
	}
	case BT_SET_LOCAL_NAME: {
		bluetooth_device_name_t local_name = { {0} };
		__bt_service_get_parameters(in_param1,
				&local_name, sizeof(bluetooth_device_name_t));

		result = _bt_set_local_name(local_name.name);

		break;
	}
	case BT_IS_SERVICE_USED: {
		char *uuid;
		gboolean used = FALSE;

		uuid = (char *)g_variant_get_data(in_param1);

		result = _bt_is_service_used(uuid, &used);

		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &used,
						sizeof(gboolean));
		}
		break;
	}
	case BT_GET_DISCOVERABLE_MODE: {
		int mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;

		result = _bt_get_discoverable_mode(&mode);

		g_array_append_vals(*out_param1, &mode, sizeof(int));
		break;
	}
	case BT_SET_DISCOVERABLE_MODE: {
		int mode = BLUETOOTH_DISCOVERABLE_MODE_CONNECTABLE;
		int time = 0;

		__bt_service_get_parameters(in_param1, &mode, sizeof(int));
		__bt_service_get_parameters(in_param2, &time, sizeof(int));

		result = _bt_set_discoverable_mode(mode, time);
		break;
	}
	case BT_GET_DISCOVERABLE_TIME: {
		int timeout = 0;

		result = _bt_get_timeout_value(&timeout);

		g_array_append_vals(*out_param1, &timeout, sizeof(int));
		break;
	}
	case BT_START_DISCOVERY:
		result = _bt_start_discovery();

		break;
	case BT_START_CUSTOM_DISCOVERY: {
		bt_discovery_role_type_t role;

		__bt_service_get_parameters(in_param1,
				&role, sizeof(bt_discovery_role_type_t));
		result = _bt_start_custom_discovery(role);

		break;
	}
	case BT_CANCEL_DISCOVERY:
		result = _bt_cancel_discovery();
		break;
	case BT_IS_DISCOVERYING: {
		gboolean discovering = FALSE;
		discovering = _bt_is_discovering();
		g_array_append_vals(*out_param1,
				&discovering, sizeof(gboolean));
		break;
	}
	case BT_START_LE_DISCOVERY: {
		char *sender = NULL;

		sender = (char *)g_dbus_method_invocation_get_sender(context);
		result = _bt_start_le_scan(sender);

		break;
	}
	case BT_STOP_LE_DISCOVERY: {
		char *sender = NULL;

		sender = (char *)g_dbus_method_invocation_get_sender(context);
		result = _bt_stop_le_scan(sender);

		break;
	}
	case BT_IS_LE_DISCOVERYING: {
		gboolean le_discovering = FALSE;

		le_discovering = _bt_is_le_scanning();
		g_array_append_vals(*out_param1,
				&le_discovering, sizeof(gboolean));

		break;
	}
	case BT_REGISTER_SCAN_FILTER: {
		char *sender = NULL;
		bluetooth_le_scan_filter_t scan_filter;
		int slot_id;

		sender = (char *)g_dbus_method_invocation_get_sender(context);
		__bt_service_get_parameters(in_param1, &scan_filter,
				sizeof(bluetooth_le_scan_filter_t));
		BT_DBG("bluetooth_le_scan_filter_t [features : %.2x]",
				scan_filter.added_features);

		result = _bt_register_scan_filter(sender,
				&scan_filter, &slot_id);

		g_array_append_vals(*out_param1, &slot_id, sizeof(int));
		break;
	}
	case BT_UNREGISTER_SCAN_FILTER:{
		char *sender = NULL;
		int slot_id;

		sender = (char *)g_dbus_method_invocation_get_sender(context);
		__bt_service_get_parameters(in_param1, &slot_id, sizeof(int));
		BT_DBG("Remove scan filter [Slot ID : %d]", slot_id);

		result = _bt_unregister_scan_filter(sender, slot_id);

		break;
	}
	case BT_UNREGISTER_ALL_SCAN_FILTERS:{
		char *sender = NULL;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		BT_DBG("Remove all scan filters [Sender : %s]", sender);

		result = _bt_unregister_all_scan_filters(sender);

		break;
	}
	case BT_ENABLE_RSSI: {
		bluetooth_device_address_t bd_addr;
		int link_type;
		bt_rssi_threshold_t rssi_threshold;
		int low_threshold;
		int in_range_threshold;
		int high_threshold;

		BT_DBG("Enable RSSI");

		__bt_service_get_parameters(in_param1,
				&bd_addr, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&link_type, sizeof(int));
		__bt_service_get_parameters(in_param3,
				&rssi_threshold, sizeof(bt_rssi_threshold_t));

		low_threshold = rssi_threshold.low_threshold;
		in_range_threshold = rssi_threshold.in_range_threshold;
		high_threshold = rssi_threshold.high_threshold;

		result = _bt_enable_rssi(&bd_addr,
				link_type, low_threshold,
				in_range_threshold, high_threshold);
		break;
	}
	case BT_GET_RSSI: {
		int link_type;
		bluetooth_device_address_t bd_addr;

		BT_DBG("Get RSSI Strength");

		__bt_service_get_parameters(in_param1,
				&bd_addr, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&link_type, sizeof(int));

		result = _bt_get_rssi_strength(&bd_addr, link_type);
		break;
	}
	case BT_IS_CONNECTABLE: {
		gboolean is_connectable;

		is_connectable = _bt_is_connectable();
		g_array_append_vals(*out_param1,
				&is_connectable, sizeof(gboolean));
		break;
	}
	case BT_SET_CONNECTABLE: {
		gboolean is_connectable;

		__bt_service_get_parameters(in_param1,
				&is_connectable, sizeof(gboolean));
		result = _bt_set_connectable(is_connectable);
		break;
	}
	case BT_SET_ADVERTISING: {
		char *sender = NULL;
		int adv_handle;
		gboolean enable = FALSE;
		gboolean use_reserved_slot = FALSE;

		__bt_service_get_parameters(in_param1,
				&adv_handle, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&enable, sizeof(gboolean));
		__bt_service_get_parameters(in_param3,
				&use_reserved_slot, sizeof(gboolean));

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		result = _bt_set_advertising(sender, adv_handle,
				enable, use_reserved_slot);
		break;
	}
	case BT_SET_CUSTOM_ADVERTISING: {
		char *sender = NULL;
		int adv_handle;
		gboolean enable = FALSE;
		bluetooth_advertising_params_t adv_params;
		gboolean use_reserved_slot = FALSE;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		__bt_service_get_parameters(in_param1, &adv_handle,
				sizeof(int));
		__bt_service_get_parameters(in_param2, &enable,
				sizeof(gboolean));
		__bt_service_get_parameters(in_param3, &adv_params,
				sizeof(bluetooth_advertising_params_t));
		__bt_service_get_parameters(in_param4, &use_reserved_slot,
				sizeof(gboolean));

		BT_DBG("bluetooth_advertising_params_t [%f %f %d %d]",
				adv_params.interval_min, adv_params.interval_max,
				adv_params.filter_policy, adv_params.type);
		result = _bt_set_custom_advertising(sender, adv_handle,
				enable, &adv_params, use_reserved_slot);
		break;
	}
	case BT_GET_ADVERTISING_DATA: {
		bluetooth_advertising_data_t adv = { {0} };
		int length = 0;

		result = _bt_get_advertising_data(&adv, &length);
		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, adv.data, length);
		}
		break;
	}
	case BT_SET_ADVERTISING_DATA: {
		char *sender = NULL;
		int adv_handle;
		bluetooth_advertising_data_t adv = { {0} };
		int length;
		gboolean use_reserved_slot = FALSE;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		__bt_service_get_parameters(in_param1,
				&adv_handle, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&adv, sizeof(bluetooth_advertising_data_t));
		__bt_service_get_parameters(in_param3,
				&length, sizeof(int));
		__bt_service_get_parameters(in_param4,
				&use_reserved_slot, sizeof(gboolean));

		result = _bt_set_advertising_data(sender, adv_handle,
				&adv, length, use_reserved_slot);
		break;
	}
	case BT_GET_SCAN_RESPONSE_DATA: {
		bluetooth_scan_resp_data_t rsp = { {0} };
		int length = 0;

		result = _bt_get_scan_response_data(&rsp, &length);
		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, rsp.data, length);
		}

		break;
	}
	case BT_SET_SCAN_RESPONSE_DATA: {
		char *sender = NULL;
		int adv_handle;
		bluetooth_scan_resp_data_t rsp = { {0} };
		int length;
		gboolean use_reserved_slot = FALSE;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		__bt_service_get_parameters(in_param1,
				&adv_handle, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&rsp, sizeof(bluetooth_scan_resp_data_t));
		__bt_service_get_parameters(in_param3,
				&length, sizeof(int));
		__bt_service_get_parameters(in_param4,
				&use_reserved_slot, sizeof(gboolean));

		result = _bt_set_scan_response_data(sender, adv_handle,
				&rsp, length, use_reserved_slot);

		break;
	}
	case BT_SET_MANUFACTURER_DATA: {
		bluetooth_manufacturer_data_t m_data = { 0 };
		__bt_service_get_parameters(in_param1,
				&m_data, sizeof(bluetooth_manufacturer_data_t));

		result = _bt_set_manufacturer_data(&m_data);
		break;
	}
	case BT_SET_SCAN_PARAMETERS: {
		bluetooth_le_scan_params_t scan_params;
		__bt_service_get_parameters(in_param1, &scan_params,
				sizeof(bluetooth_le_scan_params_t));

		BT_DBG("bluetooth_le_scan_params_t [%f %f %d]",
				scan_params.interval, scan_params.window,
				scan_params.type);

		result = _bt_set_scan_parameters(&scan_params);
		break;
	}
	case BT_LE_CONN_UPDATE: {
		bluetooth_device_address_t local_address = { {0} };
		bluetooth_le_connection_param_t parameters = {0};

		__bt_service_get_parameters(in_param1, &local_address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &parameters,
				sizeof(bluetooth_le_connection_param_t));

		result =  _bt_le_conn_update(local_address.addr,
					parameters.interval_min,
					parameters.interval_max,
					parameters.latency,
					parameters.timeout);
		break;
	}
	case BT_IS_ADVERTISING: {
		gboolean advertising = FALSE;
		advertising = _bt_is_advertising();

		g_array_append_vals(*out_param1, &advertising,
				sizeof(gboolean));
		break;
	}
	case BT_ADD_WHITE_LIST: {
		bluetooth_device_address_t address = { {0} };
		int addr_type = 0;

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &addr_type,
				sizeof(int));

		result = _bt_add_white_list(&address, addr_type);
		break;
	}
	case BT_REMOVE_WHITE_LIST: {
		bluetooth_device_address_t address = { {0} };
		int addr_type = 0;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&addr_type, sizeof(int));

		result = _bt_remove_white_list(&address, addr_type);
		break;
	}
	case BT_CLEAR_WHITE_LIST: {
		result = _bt_clear_white_list();
		break;
	}
	case BT_GET_BONDED_DEVICES: {
		result = _bt_get_bonded_devices(out_param1);
		break;
	}
	case BT_GET_BONDED_DEVICE: {
		bluetooth_device_address_t address = { {0} };
		bluetooth_device_info_t dev_info;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		memset(&dev_info, 0x00, sizeof(bluetooth_device_info_t));
		result = _bt_get_bonded_device_info(&address, &dev_info);

		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &dev_info,
					sizeof(bluetooth_device_info_t));
		}
		break;
	}
	case BT_BOND_DEVICE: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_bond_device(request_id, &address,
				BLUETOOTH_DEV_CONN_DEFAULT, out_param1);
		break;
	}
	case BT_BOND_DEVICE_BY_TYPE: {
		bluetooth_device_address_t address = { {0} };
		unsigned short conn_type = 0;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&conn_type, sizeof(unsigned short));

		result = _bt_bond_device(request_id, &address,
				conn_type, out_param1);
		break;
	}
	case BT_CANCEL_BONDING: {
		result = _bt_cancel_bonding();
		break;
	}
	case BT_PASSKEY_REPLY: {
		const char *passkey = NULL;
		gboolean authentication_reply = FALSE;

		passkey = g_variant_get_data(in_param1);
		__bt_service_get_parameters(in_param2,
			&authentication_reply, sizeof(gboolean));
		result = _bt_passkey_reply(passkey, authentication_reply);
		break;
	}
	case BT_PASSKEY_CONFIRMATION_REPLY: {
		gboolean confirmation_reply = FALSE;

		__bt_service_get_parameters(in_param1,
			&confirmation_reply, sizeof(gboolean));
		result = _bt_passkey_confirmation_reply(confirmation_reply);
		break;
	}
	case BT_UNBOND_DEVICE: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_unbond_device(request_id, &address, out_param1);

		break;
	}
	case BT_SET_ALIAS: {
		bluetooth_device_address_t address = { {0} };
		const char *local_name;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		local_name = (const char *)g_variant_get_data(in_param2);

		result = _bt_set_alias(&address, local_name);
		break;
	}
	case BT_SEARCH_SERVICE: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_search_device(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_CANCEL_SEARCH_SERVICE: {
		result = _bt_cancel_search_device();
		break;
	}
	case BT_SET_AUTHORIZATION: {
		bluetooth_device_address_t address = { {0} };
		gboolean authorize;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&authorize, sizeof(gboolean));

		result = _bt_set_authorization(&address, authorize);
		break;
	}
	case BT_IS_DEVICE_CONNECTED: {
		bluetooth_device_address_t address = { {0} };
		int type;
		gboolean connected = FALSE;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&type, sizeof(int));

		result = _bt_is_device_connected(&address, type, &connected);
		BT_DBG("is_connected: %d", connected);
		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &connected,
						sizeof(gboolean));
		}

		break;
	}
	case BT_GET_CONNECTED_LINK_TYPE: {
		bluetooth_device_address_t address = { {0} };
		bluetooth_connected_link_t connected = BLUETOOTH_CONNECTED_LINK_NONE;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_get_connected_link(&address, &connected);

		if (result == BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &connected,
					sizeof(guint));
		}

		break;
	}
	case BT_SET_PIN_CODE: {
		bluetooth_device_address_t address = { {0} };
		bluetooth_device_pin_code_t pin_code = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&pin_code, sizeof(bluetooth_device_pin_code_t));

		result = _bt_set_pin_code(&address, &pin_code);
		break;
	}
	case BT_UNSET_PIN_CODE: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_unset_pin_code(&address);
		break;
	}
	case BT_UPDATE_LE_CONNECTION_MODE: {
		bluetooth_device_address_t remote_address = { { 0 } };
		bluetooth_le_connection_param_t param = { 0 };
		bluetooth_le_connection_mode_t mode = BLUETOOTH_LE_CONNECTION_MODE_BALANCED;

		__bt_service_get_parameters(in_param1, &remote_address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &mode,
				sizeof(bluetooth_le_connection_mode_t));

		result = _bt_get_le_connection_parameter(mode, &param);
		if (result != BLUETOOTH_ERROR_NONE)
			break;

		result = _bt_le_conn_update(remote_address.addr,
				param.interval_min,
				param.interval_max,
				param.latency,
				param.timeout);
		break;
	}

	case BT_HID_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_hid_connect(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_HID_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_hid_disconnect(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_NETWORK_ACTIVATE:
		result = _bt_network_activate();
		break;
	case BT_NETWORK_DEACTIVATE:
		result = _bt_network_deactivate();
		break;
	case BT_NETWORK_CONNECT: {
		bluetooth_device_address_t address = { {0} };
		int role;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&role, sizeof(int));

		result = _bt_network_connect(request_id, role, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_NETWORK_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_network_disconnect(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_NETWORK_SERVER_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_network_server_disconnect(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}

	case BT_AUDIO_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_connect(request_id, BT_AUDIO_ALL,
					&address, out_param1);
		break;
	}
	case BT_AUDIO_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_disconnect(request_id, BT_AUDIO_ALL,
					&address, out_param1);
		break;
	}
	case BT_AG_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_connect(request_id, BT_AUDIO_HSP,
					&address, out_param1);
		break;
	}
	case BT_AG_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_disconnect(request_id, BT_AUDIO_HSP,
					&address, out_param1);
		break;
	}
	case BT_AV_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_connect(request_id, BT_AUDIO_A2DP,
					&address, out_param1);
		break;
	}
	case BT_AV_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_disconnect(request_id, BT_AUDIO_A2DP,
					&address, out_param1);
		break;
	}
	case BT_AVRCP_CONTROL_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_connect(request_id, BT_AVRCP,
					&address, out_param1);
		break;
	}
	case BT_AVRCP_CONTROL_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_audio_disconnect(request_id, BT_AVRCP,
					&address, out_param1);
		break;
	}
	case BT_AV_SOURCE_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_audio_connect(request_id, BT_AUDIO_A2DP_SOURCE,
					&address, out_param1);
		break;
	}
	case BT_AV_SOURCE_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_audio_disconnect(request_id, BT_AUDIO_A2DP_SOURCE,
					&address, out_param1);
		break;
	}
	case BT_HF_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_hf_connect(request_id, &address, out_param1);
		break;
	}
	case BT_HF_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_hf_disconnect(request_id, &address, out_param1);
		break;
	}
	case BT_SET_CONTENT_PROTECT: {
		gboolean status;

		__bt_service_get_parameters(in_param1,
				&status, sizeof(gboolean));

		result = _bt_audio_set_content_protect(status);

		break;
	}
	case BT_OOB_READ_LOCAL_DATA: {
		bt_oob_data_t local_oob_data;

		memset(&local_oob_data, 0x00, sizeof(bt_oob_data_t));
		result = _bt_oob_read_local_data(&local_oob_data);

		g_array_append_vals(*out_param1, &local_oob_data,
				sizeof(bt_oob_data_t));

		break;
	}
	case BT_OOB_ADD_REMOTE_DATA: {
		bluetooth_device_address_t address = { {0} };
		bt_oob_data_t local_oob_data;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&local_oob_data, sizeof(bt_oob_data_t));

		result = _bt_oob_add_remote_data(&address, &local_oob_data);

		break;
	}
	case BT_OOB_REMOVE_REMOTE_DATA: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		result = _bt_oob_remove_remote_data(&address);

		break;
	}
	case BT_AVRCP_SET_TRACK_INFO: {
		media_metadata_t data;
		media_metadata_attributes_t meta_data;

		memset(&data, 0x00, sizeof(media_metadata_t));
		memset(&meta_data, 0x00, sizeof(media_metadata_attributes_t));

		__bt_service_get_parameters(in_param1,
				&data, sizeof(media_metadata_t));

		meta_data.title = g_strdup(data.title);
		meta_data.artist = g_strdup(data.artist);
		meta_data.album = g_strdup(data.album);
		meta_data.genre = g_strdup(data.genre);
		meta_data.total_tracks = data.total_tracks;
		meta_data.number = data.number;
		meta_data.duration = (int64_t) data.duration;

		result = _bt_avrcp_set_track_info(&meta_data);

		g_free((gpointer)meta_data.title);
		g_free((gpointer)meta_data.artist);
		g_free((gpointer)meta_data.album);
		g_free((gpointer)meta_data.genre);

		break;
	}
	case BT_AVRCP_SET_PROPERTY: {
		int type;
		unsigned int value;

		__bt_service_get_parameters(in_param1,
				&type, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&value, sizeof(unsigned int));

		result = _bt_avrcp_set_property(type, value);

		break;
	}
	case BT_AVRCP_SET_PROPERTIES: {
		media_player_settings_t properties;

		memset(&properties, 0x00, sizeof(media_player_settings_t));
		__bt_service_get_parameters(in_param1,
				&properties, sizeof(media_player_settings_t));

		result = _bt_avrcp_set_properties(&properties);

		break;
	}
	case BT_AVRCP_HANDLE_CONTROL: {
		int type;

		__bt_service_get_parameters(in_param1, &type, sizeof(int));

		result = _bt_avrcp_control_cmd(type);

		break;
	}
	case BT_AVRCP_CONTROL_SET_PROPERTY: {
		int type;
		unsigned int value;

		__bt_service_get_parameters(in_param1,
				&type, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&value, sizeof(unsigned int));

		result = _bt_avrcp_control_set_property(type, value);

		break;
	}
	case BT_AVRCP_CONTROL_GET_PROPERTY: {
		int type;
		unsigned int value;

		__bt_service_get_parameters(in_param1, &type, sizeof(int));

		result = _bt_avrcp_control_get_property(type, &value);
		g_array_append_vals(*out_param1, &value, sizeof(int));

		break;
	}
	case BT_AVRCP_GET_TRACK_INFO: {
		media_metadata_t meta_data;
		media_metadata_attributes_t metadata;

		memset(&meta_data, 0x00, sizeof(media_metadata_t));
		memset(&metadata, 0x00, sizeof(media_metadata_attributes_t));

		result = _bt_avrcp_control_get_track_info(&metadata);

		if (BLUETOOTH_ERROR_NONE != result)
			break;

		if (_bt_copy_utf8_string(meta_data.title, metadata.title,
							BT_META_DATA_MAX_LEN))
			BT_ERR("Error in copying Title\n");
		if (_bt_copy_utf8_string(meta_data.artist, metadata.artist,
							BT_META_DATA_MAX_LEN))
			BT_ERR("Error in copying Artist\n");
		if (_bt_copy_utf8_string(meta_data.album, metadata.album,
							BT_META_DATA_MAX_LEN))
			BT_ERR("Error in copying Album\n");
		if (_bt_copy_utf8_string(meta_data.genre, metadata.genre,
							BT_META_DATA_MAX_LEN))
			BT_ERR("Error in copying Genre\n");

		if (_bt_utf8_validate(meta_data.title) == FALSE)
			meta_data.title[0] = '\0';

		if (_bt_utf8_validate(meta_data.artist) == FALSE)
			meta_data.artist[0] = '\0';

		if (_bt_utf8_validate(meta_data.album) == FALSE)
			meta_data.album[0] = '\0';

		if (_bt_utf8_validate(meta_data.genre) == FALSE)
			meta_data.genre[0] = '\0';

		meta_data.total_tracks = metadata.total_tracks;
		meta_data.number = metadata.number;
		meta_data.duration = metadata.duration;

		g_free((gpointer)metadata.title);
		g_free((gpointer)metadata.artist);
		g_free((gpointer)metadata.album);
		g_free((gpointer)metadata.genre);

		g_array_append_vals(*out_param1, &meta_data,
					sizeof(media_metadata_t));
		break;
	}
	case BT_RFCOMM_CLIENT_CONNECT: {
#ifdef RFCOMM_DIRECT
		result = BLUETOOTH_ERROR_NONE;
#else
		bluetooth_device_address_t address = { {0} };
		char *input_string;
		int connect_type;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));

		input_string = &g_array_index(in_param2, char, 0);

		connect_type = g_array_index(in_param3, int, 0);

		if (connect_type == BT_RFCOMM_UUID) {
			result = _bt_rfcomm_connect_using_uuid(request_id,
							&address, input_string);
		} else {
			result = _bt_rfcomm_connect_using_channel(request_id,
							&address, input_string);
		}

		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
#endif
		break;
	}
	case BT_RFCOMM_CLIENT_CANCEL_CONNECT:
		result = _bt_rfcomm_cancel_connect();
		break;
	case BT_RFCOMM_CLIENT_IS_CONNECTED: {
		gboolean connected = FALSE;
		result = _bt_rfcomm_is_connected(&connected);

		g_array_append_vals(*out_param1, &connected, sizeof(gboolean));
		break;
	}
	case BT_RFCOMM_SOCKET_DISCONNECT: {
#ifdef RFCOMM_DIRECT
		result = BLUETOOTH_ERROR_NONE;
#else
		int socket_fd;

		socket_fd = g_array_index(in_param1, int, 0);

		result = _bt_rfcomm_disconnect(socket_fd);
#endif
		break;
	}
	case BT_RFCOMM_SOCKET_WRITE: {
#ifdef RFCOMM_DIRECT
		result = BLUETOOTH_ERROR_NONE;
#else
		int socket_fd;
		int length;
		char *buffer;

		socket_fd = g_array_index(in_param1, int, 0);
		length = g_array_index(in_param2, int, 0);
		buffer = &g_array_index(in_param3, char, 0);

		result = _bt_rfcomm_write(socket_fd, buffer, length);
#endif
		break;
	}
	case BT_RFCOMM_CREATE_SOCKET: {
#ifdef RFCOMM_DIRECT
		result = BLUETOOTH_ERROR_NONE;
#else
		char *sender;
		char *uuid;
		int socket_fd = -1;

		sender = dbus_g_method_get_sender(context);
		uuid = &g_array_index(in_param1, char, 0);

		result = _bt_rfcomm_create_socket(sender, uuid);

		if (result > 0) {
			socket_fd = result;
			result = BLUETOOTH_ERROR_NONE;
		}

		g_array_append_vals(*out_param1, &socket_fd, sizeof(int));

		g_free(sender);
#endif
		break;
	}
	case BT_RFCOMM_REMOVE_SOCKET: {
#ifdef RFCOMM_DIRECT
		result = BLUETOOTH_ERROR_NONE;
#else
		int socket_fd;

		socket_fd = g_array_index(in_param1, int, 0);

		result = _bt_rfcomm_remove_socket(socket_fd);
#endif
		break;
	}
	case BT_RFCOMM_LISTEN: {
		int socket_fd;
		int pending;
		gboolean is_native;

		__bt_service_get_parameters(in_param1, &socket_fd,
				sizeof(int));
		__bt_service_get_parameters(in_param2, &pending,
				sizeof(int));
		__bt_service_get_parameters(in_param3, &is_native,
				sizeof(gboolean));

		result = _bt_rfcomm_listen(socket_fd, pending, is_native);
		break;
	}
	case BT_RFCOMM_IS_UUID_AVAILABLE: {
		gboolean available = TRUE;
		char *uuid;

		uuid = (char *)g_variant_get_data(in_param1);

		result = _bt_rfcomm_is_uuid_available(uuid, &available);

		g_array_append_vals(*out_param1, &available, sizeof(gboolean));
		break;
	}
	case BT_RFCOMM_ACCEPT_CONNECTION: {
		int socket_fd;

		__bt_service_get_parameters(in_param1, &socket_fd, sizeof(int));
		BT_DBG(" socket fd %d", socket_fd);
		result = _bt_rfcomm_accept_connection();
		break;
	}
	case BT_RFCOMM_REJECT_CONNECTION: {
		int socket_fd;

		__bt_service_get_parameters(in_param1, &socket_fd, sizeof(int));
		BT_DBG(" socket fd %d", socket_fd);
		result = _bt_rfcomm_reject_connection();
		break;
	}
	case BT_RFCOMM_CREATE_SOCKET_EX: {
		result = BLUETOOTH_ERROR_NONE;
		break;
	}
	case BT_RFCOMM_REMOVE_SOCKET_EX: {
		result = BLUETOOTH_ERROR_NONE;
		break;
	}
	case BT_CONNECT_LE: {
		bluetooth_device_address_t address = { {0} };
		gboolean auto_connect;

		__bt_service_get_parameters(in_param1,
				&address, sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&auto_connect, sizeof(gboolean));

		result = _bt_connect_le_device(request_id, &address, auto_connect);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_DISCONNECT_LE: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_disconnect_le_device(request_id, &address);
		if (result != BLUETOOTH_ERROR_NONE) {
			g_array_append_vals(*out_param1, &address,
					sizeof(bluetooth_device_address_t));
		}
		break;
	}
	case BT_SET_LE_PRIVACY: {
		gboolean set_privacy;

		__bt_service_get_parameters(in_param1, &set_privacy,
				sizeof(gboolean));

		result = _bt_set_le_privacy(set_privacy);

		break;
	}
	case BT_HDP_CONNECT:
	case BT_HDP_DISCONNECT:
	case BT_HDP_SEND_DATA:

	case BT_GATT_GET_PRIMARY_SERVICES:
	case BT_GATT_DISCOVER_CHARACTERISTICS:
	case BT_GATT_SET_PROPERTY_REQUEST:
	case BT_GATT_READ_CHARACTERISTIC:
	case BT_GATT_DISCOVER_CHARACTERISTICS_DESCRIPTOR:
		/* Just call to check the privilege */
		break;
	case BT_GATT_WATCH_CHARACTERISTIC: {
		char *sender = NULL;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		result = _bt_insert_gatt_client_sender(sender);

		break;
	}
	case BT_GATT_UNWATCH_CHARACTERISTIC: {
		char *sender = NULL;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		result = _bt_delete_gatt_client_sender(sender);

		break;
	}
	case BT_LE_IPSP_INIT:
		result = _bt_initialize_ipsp();
		break;
	case BT_LE_IPSP_DEINIT:
		result = _bt_deinitialize_ipsp();
		break;
	case BT_LE_IPSP_CONNECT: {
		bluetooth_device_address_t address = { {0} };
		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_connect_le_ipsp_device(&address);
		break;
	}
	case BT_LE_IPSP_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_disconnect_le_ipsp_device(&address);
		break;
	}
	case BT_LE_READ_MAXIMUM_DATA_LENGTH: {
		bluetooth_le_read_maximum_data_length_t max_le_datalength = {0};

		result = _bt_le_read_maximum_data_length(&max_le_datalength);

		g_array_append_vals(*out_param1, &max_le_datalength,
			sizeof(bluetooth_le_read_maximum_data_length_t));
		break;
	}
	case BT_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH: {
		unsigned int def_tx_Octects = 0;
		unsigned int def_tx_Time = 0;

		__bt_service_get_parameters(in_param1,
				&def_tx_Octects, sizeof(int));
		__bt_service_get_parameters(in_param2,
				&def_tx_Time, sizeof(int));

		result = _bt_le_write_host_suggested_default_data_length(
						def_tx_Octects, def_tx_Time);
		break;
	}
	case BT_LE_READ_HOST_SUGGESTED_DATA_LENGTH: {
		bluetooth_le_read_host_suggested_data_length_t def_data_length = {0};

		result = _bt_le_read_host_suggested_default_data_length(&def_data_length);

		g_array_append_vals(*out_param1, &def_data_length,
				sizeof(bluetooth_le_read_host_suggested_data_length_t));

		break;
	}
	case BT_LE_SET_DATA_LENGTH: {
		int max_tx_Octets = 0;
		int max_tx_Time = 0;
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2,
				&max_tx_Octets, sizeof(int));
		__bt_service_get_parameters(in_param3,
				&max_tx_Time, sizeof(int));

		result = _bt_le_set_data_length(&address, max_tx_Octets, max_tx_Time);
		break;
	}
	default:
		result = BLUETOOTH_ERROR_INTERNAL;
		break;
	}

	return result;
}

int __bt_obexd_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GVariant *in_param5,
		GArray **out_param1)
{
	BT_DBG("+");

	int result;

	BT_DBG("function_name : %x", function_name);

	switch (function_name) {
	case BT_OPP_PUSH_FILES: {
		BT_DBG("BT_OPP_PUSH_FILES");
		int i;
		bluetooth_device_address_t address = { {0} };
		bt_file_path_t path;
		char **file_path;
		int file_count;
		GDBusProxy *process_proxy;
		guint owner_pid = 0;
		int opp_server_pid = 0;
		gchar *owner_sender_name;
		GDBusConnection *owner_connection = NULL;
		GVariant *val_get = NULL;
		GError *error_connection = NULL;
		GError *errro_proxy = NULL;
		GArray *param2;

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param3, &file_count,
				sizeof(int));

		file_path = g_new0(char *, file_count + 1);

		param2 = g_array_new(TRUE, TRUE, sizeof(gchar));
		__bt_fill_garray_from_variant(in_param2, param2);

		for (i = 0; i < file_count; i++) {
			path = g_array_index(param2, bt_file_path_t, i);
			file_path[i] = g_strdup(path.path);
		}

		owner_connection = g_dbus_method_invocation_get_connection(context);
		owner_sender_name = g_dbus_method_invocation_get_sender(context);

		BT_DBG("sender = %s", owner_sender_name);

		process_proxy = g_dbus_proxy_new_sync(owner_connection,
						  G_DBUS_PROXY_FLAGS_NONE,
						  NULL,
						  "org.freedesktop.DBus",
						  "/org/freedesktop/DBus",
						  "org.freedesktop.DBus",
						  NULL, &error_connection);

		if(process_proxy == NULL)
			BT_DBG("Fail to get process_proxy");

		if (error_connection) {
			BT_DBG("Fail to get proxy : %s", error_connection->message);
			g_error_free(error_connection);
			error_connection = NULL;
		}

		if (process_proxy) {
			val_get = g_dbus_proxy_call_sync(process_proxy,
							"GetConnectionUnixProcessID",
							g_variant_new("(s)", owner_sender_name),
							G_DBUS_CALL_FLAGS_NONE,
							-1,	NULL,
							&errro_proxy);

			if (val_get == NULL) {
				BT_DBG("Fail to get pid");
			} else {
				g_variant_get(val_get, "(u)", &owner_pid);
				BT_DBG("request is from pid %d\n", owner_pid);
			}

			if (errro_proxy) {
				g_error("Unable to get PID for %s: %s",
						  owner_sender_name, errro_proxy->message);
				g_error_free(errro_proxy);
				errro_proxy = NULL;
			}
		} else {
			BT_DBG("fail to get proxy");
		}

		opp_server_pid = _bt_obex_get_native_pid();

		BT_DBG("owner_pid, agent_info.native_server->app_pid = %d, %d",
					owner_pid, opp_server_pid);
		if (opp_server_pid == owner_pid) {
			BT_DBG("The exception case : _bt_opp_client_push_files");
			result = _bt_opp_client_push_files(request_id, context,
								&address, file_path,
								file_count);
		} else {
			if(__bt_service_check_file_path_security(file_path, file_count,
							in_param5, "r") == BLUETOOTH_ERROR_NONE) {
						BT_DBG("_bt_opp_client_push_files");
						result = _bt_opp_client_push_files(request_id, context,
										&address, file_path,
										file_count);
			} else {
				result = BLUETOOTH_ERROR_PERMISSION_DEINED;
			}
		}

		for (i = 0; i < file_count; i++) {
			g_free(file_path[i]);
		}
		g_free(file_path);
		g_array_free(param2, TRUE);
		if (process_proxy)
			g_object_unref(process_proxy);
		break;
	}
	case BT_OPP_CANCEL_PUSH: {
		result = _bt_opp_client_cancel_push();

		break;
	}
	case BT_OPP_IS_PUSHING_FILES: {
		gboolean is_sending = FALSE;

		result = _bt_opp_client_is_sending(&is_sending);

		g_array_append_vals(*out_param1, &is_sending,
				sizeof(gboolean));
		break;
	}
	case BT_OBEX_SERVER_ALLOCATE: {
		int app_pid;
		gboolean is_native;
		char *path;
		char *sender;

		sender = (char *)g_dbus_method_invocation_get_sender(context);

		path = (char *)g_variant_get_data(in_param1);
		__bt_service_get_parameters(in_param2, &is_native,
				sizeof(gboolean));
		__bt_service_get_parameters(in_param3, &app_pid,
				sizeof(int));
		result = _bt_obex_server_allocate(sender,
				path, app_pid, is_native);

		break;
	}
	case BT_OBEX_SERVER_DEALLOCATE: {
		int app_pid;
		gboolean is_native;

		__bt_service_get_parameters(in_param1, &is_native,
				sizeof(gboolean));
		__bt_service_get_parameters(in_param2, &app_pid,
				sizeof(int));

		result = _bt_obex_server_deallocate(app_pid, is_native);
		break;
	}
	case BT_OBEX_SERVER_IS_ACTIVATED: {
		gboolean is_activated = FALSE;

		result = _bt_obex_server_is_activated(&is_activated);

		g_array_append_vals(*out_param1, &is_activated,
				sizeof(gboolean));

		break;
	}
	case BT_OBEX_SERVER_ACCEPT_CONNECTION: {
		result = _bt_obex_server_accept_connection(request_id);

		break;
	}
	case BT_OBEX_SERVER_REJECT_CONNECTION: {
		result = _bt_obex_server_reject_connection();

		break;
	}
	case BT_OBEX_SERVER_ACCEPT_FILE: {
		char *file_name;

		file_name = (char *)g_variant_get_data(in_param1);
		result = _bt_obex_server_accept_authorize(file_name, TRUE);

		break;
	}
	case BT_OBEX_SERVER_REJECT_FILE: {
		result = _bt_obex_server_reject_authorize();

		break;
	}
	case BT_OBEX_SERVER_SET_PATH: {
		gboolean is_native;
		char *destination_path;

		destination_path = (char *)g_variant_get_data(in_param1);
		__bt_service_get_parameters(in_param2, &is_native,
				sizeof(gboolean));

		if(__bt_service_check_file_path_security(&destination_path,
				1, in_param5, "x") == BLUETOOTH_ERROR_NONE)
			result = _bt_obex_server_set_destination_path(
					destination_path, is_native);
		else
			result = BLUETOOTH_ERROR_PERMISSION_DEINED;

		break;
	}
	case BT_OBEX_SERVER_SET_ROOT: {
		char *root;

		root = (char *)g_variant_get_data(in_param1);

		if(__bt_service_check_file_path_security(&root,
				1, in_param5, "x") == BLUETOOTH_ERROR_NONE)
			result = _bt_obex_server_set_root(root);
		else
			result = BLUETOOTH_ERROR_PERMISSION_DEINED;

		break;
	}
	case BT_OBEX_SERVER_CANCEL_TRANSFER: {
		int transfer_id;

		__bt_service_get_parameters(in_param1, &transfer_id,
				sizeof(int));

		result = _bt_obex_server_cancel_transfer(transfer_id);

		break;
	}
	case BT_OBEX_SERVER_CANCEL_ALL_TRANSFERS: {
		result = _bt_obex_server_cancel_all_transfers();

		break;
	}
	case BT_OBEX_SERVER_IS_RECEIVING: {
		gboolean is_receiving = FALSE;

		result = _bt_obex_server_is_receiving(&is_receiving);

		g_array_append_vals(*out_param1, &is_receiving,
				sizeof(gboolean));
		break;
	}
	case BT_PBAP_CONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_pbap_connect(&address);
		break;
	}
	case BT_PBAP_DISCONNECT: {
		bluetooth_device_address_t address = { {0} };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));

		result = _bt_pbap_disconnect(&address);
		break;
	}
	case BT_PBAP_GET_PHONEBOOK_SIZE: {
		bluetooth_device_address_t address = { {0} };
		bt_pbap_folder_t folder = { 0, };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &folder,
				sizeof(bt_pbap_folder_t));

		result = _bt_pbap_get_phonebook_size(&address,
				folder.addressbook, folder.folder_type);
		break;
	}
	case BT_PBAP_GET_PHONEBOOK: {
		bluetooth_device_address_t address = { {0} };
		bt_pbap_folder_t folder = { 0, };
		bt_pbap_pull_parameters_t app_param = { 0, };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &folder,
				sizeof(bt_pbap_folder_t));
		__bt_service_get_parameters(in_param3, &app_param,
				sizeof(bt_pbap_pull_parameters_t));

		result = _bt_pbap_get_phonebook(&address, folder.addressbook,
				folder.folder_type, &app_param);
		break;
	}
	case BT_PBAP_GET_LIST: {
		bluetooth_device_address_t address = { {0} };
		bt_pbap_folder_t folder = { 0, };
		bt_pbap_list_parameters_t app_param = { 0, };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &folder,
				sizeof(bt_pbap_folder_t));
		__bt_service_get_parameters(in_param3, &app_param,
				sizeof(bt_pbap_list_parameters_t));

		result = _bt_pbap_get_list(&address, folder.addressbook,
				folder.folder_type, &app_param);
		break;
	}
	case BT_PBAP_PULL_VCARD: {
		bluetooth_device_address_t address = { {0} };
		bt_pbap_folder_t folder = { 0, };
		bt_pbap_pull_vcard_parameters_t app_param = { 0, };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &folder,
				sizeof(bt_pbap_folder_t));
		__bt_service_get_parameters(in_param3, &app_param,
				sizeof(bt_pbap_pull_vcard_parameters_t));

		result = _bt_pbap_pull_vcard(&address, folder.addressbook,
				folder.folder_type, &app_param);
		break;
	}
	case BT_PBAP_PHONEBOOK_SEARCH: {
		bluetooth_device_address_t address = { {0} };
		bt_pbap_folder_t folder = { 0, };
		bt_pbap_search_parameters_t app_param = { 0, };

		__bt_service_get_parameters(in_param1, &address,
				sizeof(bluetooth_device_address_t));
		__bt_service_get_parameters(in_param2, &folder,
				sizeof(bt_pbap_folder_t));
		__bt_service_get_parameters(in_param3, &app_param,
				sizeof(bt_pbap_search_parameters_t));

		result = _bt_pbap_phonebook_search(&address, folder.addressbook,
				folder.folder_type, &app_param);
		break;
	}

	default:
		BT_ERR("Unknown function!");
		result = BLUETOOTH_ERROR_INTERNAL;
		break;
	}

	FN_END;

	return result;
}

int __bt_agent_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1,
		GVariant *in_param2,
		GVariant *in_param3,
		GVariant *in_param4,
		GArray **out_param1)
{
	int result;
	switch (function_name) {
	case BT_SET_AUTHORIZATION: {
		int type;
		char *uuid;
		char *path;
		int fd;

		__bt_service_get_parameters(in_param1, &type, sizeof(int));
		uuid = (char *)g_variant_get_data(in_param2);
		path = (char *)g_variant_get_data(in_param3);
		__bt_service_get_parameters(in_param4, &fd, sizeof(int));

		result = _bt_register_osp_server_in_agent(type, uuid, path, fd);
		break;
	}
	case BT_UNSET_AUTHORIZATION: {
		int type;
		char *uuid;

		__bt_service_get_parameters(in_param1, &type, sizeof(int));
		uuid = (char *)g_variant_get_data(in_param2);

		result = _bt_unregister_osp_server_in_agent(type, uuid);
		break;
	}
	default:
		BT_ERR("Unknown function!");
		result = BLUETOOTH_ERROR_INTERNAL;
		break;
	}

	return result;
}

int __bt_core_request(int function_name,
		int request_type,
		int request_id,
		GDBusMethodInvocation *context,
		GVariant *in_param1)
{
	int result;

	switch (function_name) {
	case BT_ENABLE_ADAPTER:
	{
		bt_status_t status;

		status = _bt_adapter_get_status();

		if (status == BT_ACTIVATING) {
			BT_DBG("Enabling in progress");
			result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (status == BT_ACTIVATED) {
			BT_DBG("Already enabled");
			result = BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED;
		} else {
			_bt_adapter_set_status(BT_ACTIVATING);
			_bt_adapter_start_enable_timer();
			result = BLUETOOTH_ERROR_NONE;
		}

		break;
	}
	case BT_DISABLE_ADAPTER:
	{
		bt_status_t status;

		status = _bt_adapter_get_status();
		if (status == BT_DEACTIVATING) {
				BT_DBG("Disabling in progress");
				result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (status == BT_DEACTIVATED) {
				BT_DBG("Already disabled");
				result = BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
		} else {
			_bt_adapter_set_status(BT_DEACTIVATING);
			result = BLUETOOTH_ERROR_NONE;
		}

		break;
	}
	case BT_ENABLE_ADAPTER_LE:
	{
		bt_le_status_t le_status;

		le_status = _bt_adapter_get_le_status();
		if (le_status == BT_LE_ACTIVATING) {
			BT_DBG("Enabling in progress");
			result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (le_status == BT_LE_ACTIVATED) {
			BT_DBG("Already enabled");
			result = BLUETOOTH_ERROR_DEVICE_ALREADY_ENABLED;
		} else {
			_bt_adapter_set_le_status(BT_LE_ACTIVATING);
			_bt_adapter_start_le_enable_timer();
			result = BLUETOOTH_ERROR_NONE;
		}

		break;
	}
	case BT_DISABLE_ADAPTER_LE:
	{
		bt_le_status_t le_status;

		le_status = _bt_adapter_get_le_status();
		if (le_status == BT_LE_DEACTIVATING) {
				BT_DBG("Disabling in progress");
				result = BLUETOOTH_ERROR_IN_PROGRESS;
		} else if (le_status == BT_LE_DEACTIVATED) {
				BT_DBG("Already disabled");
				result = BLUETOOTH_ERROR_DEVICE_NOT_ENABLED;
		} else {
			_bt_adapter_set_le_status(BT_LE_DEACTIVATING);
			result = BLUETOOTH_ERROR_NONE;
		}

		break;
	}
	default:
		BT_ERR("Unknown function!");
		result = BLUETOOTH_ERROR_INTERNAL;
		break;
	}

	return result;
}

gboolean __bt_service_check_privilege(int function_name,
					int service_type,
					GVariant *param)
{
	char *cookie = NULL;
	int ret_val;
	int len;
	gboolean result = TRUE;

	len = g_variant_get_size(param);
	if (len > 0)
		cookie = (char *)g_variant_get_data(param);

	retv_if(cookie == NULL, FALSE);

	switch (function_name) {
	case BT_SET_LOCAL_NAME:
	case BT_START_DISCOVERY:
	case BT_START_CUSTOM_DISCOVERY:
	case BT_CANCEL_DISCOVERY:
	case BT_OOB_ADD_REMOTE_DATA:
	case BT_OOB_REMOVE_REMOTE_DATA:
	case BT_SET_ADVERTISING:
	case BT_SET_CUSTOM_ADVERTISING:
	case BT_SET_ADVERTISING_PARAMETERS:
	case BT_START_LE_DISCOVERY:
	case BT_STOP_LE_DISCOVERY:

	case BT_BOND_DEVICE:
	case BT_CANCEL_BONDING:
	case BT_UNBOND_DEVICE:
	case BT_SET_ALIAS:
	case BT_SET_AUTHORIZATION:
	case BT_UNSET_AUTHORIZATION:
	case BT_SEARCH_SERVICE:
	case BT_PASSKEY_CONFIRMATION_REPLY:
	case BT_PASSKEY_REPLY:

	case BT_RFCOMM_CLIENT_CONNECT:
	case BT_RFCOMM_CLIENT_CANCEL_CONNECT:
	case BT_RFCOMM_SOCKET_DISCONNECT:
	case BT_RFCOMM_SOCKET_WRITE:
	case BT_RFCOMM_CREATE_SOCKET:
	case BT_RFCOMM_REMOVE_SOCKET:

	case BT_OPP_PUSH_FILES:
	case BT_OPP_CANCEL_PUSH:

	case BT_OBEX_SERVER_ACCEPT_CONNECTION:
	case BT_OBEX_SERVER_REJECT_CONNECTION:
	case BT_OBEX_SERVER_ACCEPT_FILE:
	case BT_OBEX_SERVER_REJECT_FILE:
	case BT_OBEX_SERVER_SET_PATH:
	case BT_OBEX_SERVER_SET_ROOT:
	case BT_OBEX_SERVER_CANCEL_TRANSFER:
	case BT_OBEX_SERVER_CANCEL_ALL_TRANSFERS:

	case BT_AUDIO_CONNECT:
	case BT_AUDIO_DISCONNECT:
	case BT_AG_CONNECT:
	case BT_AG_DISCONNECT:
	case BT_AV_CONNECT:
	case BT_AV_DISCONNECT:
	case BT_AVRCP_CONTROL_CONNECT:
	case BT_AVRCP_CONTROL_DISCONNECT:
	case BT_AV_SOURCE_CONNECT:
	case BT_AV_SOURCE_DISCONNECT:
	case BT_HF_CONNECT:
	case BT_HF_DISCONNECT:

	case BT_HID_CONNECT:
	case BT_HID_DISCONNECT:

	case BT_CONNECT_LE:
	case BT_DISCONNECT_LE:

	case BT_SET_ADVERTISING_DATA:
	case BT_SET_SCAN_RESPONSE_DATA:

	case BT_HDP_CONNECT:
	case BT_HDP_DISCONNECT:
	case BT_HDP_SEND_DATA:

	case BT_NETWORK_ACTIVATE:
	case BT_NETWORK_DEACTIVATE:
	case BT_NETWORK_CONNECT:
	case BT_NETWORK_DISCONNECT:
	case BT_NETWORK_SERVER_DISCONNECT:

	case BT_GATT_GET_PRIMARY_SERVICES:
	case BT_GATT_DISCOVER_CHARACTERISTICS:
	case BT_GATT_SET_PROPERTY_REQUEST:
	case BT_GATT_READ_CHARACTERISTIC:
	case BT_GATT_DISCOVER_CHARACTERISTICS_DESCRIPTOR:
	case BT_GATT_WATCH_CHARACTERISTIC:
	case BT_GATT_UNWATCH_CHARACTERISTIC:

		ret_val = security_server_check_privilege_by_cookie(cookie,
						BT_PRIVILEGE_PUBLIC, "w");
		if (ret_val == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			BT_ERR("[SMACK] Fail to access: %s", BT_PRIVILEGE_PUBLIC);
			result = FALSE;
		}
	break;

	case BT_ENABLE_ADAPTER:
	case BT_DISABLE_ADAPTER:
	case BT_RESET_ADAPTER:
	case BT_RECOVER_ADAPTER:
	case BT_ENABLE_ADAPTER_LE:
	case BT_DISABLE_ADAPTER_LE:
	case BT_SET_CONNECTABLE:
	case BT_SET_DISCOVERABLE_MODE:
	case BT_ADD_WHITE_LIST:
	case BT_REMOVE_WHITE_LIST:
	case BT_CLEAR_WHITE_LIST:
	case BT_SET_MANUFACTURER_DATA:
	case BT_SET_SCAN_PARAMETERS:

	case BT_CANCEL_SEARCH_SERVICE:
	case BT_ENABLE_RSSI:
	case BT_SET_PIN_CODE:
	case BT_UNSET_PIN_CODE:

	case BT_HID_DEVICE_ACTIVATE:
	case BT_HID_DEVICE_DEACTIVATE:
	case BT_HID_DEVICE_CONNECT:
	case BT_HID_DEVICE_DISCONNECT:
	case BT_HID_DEVICE_SEND_MOUSE_EVENT:
	case BT_HID_DEVICE_SEND_KEY_EVENT:
	case BT_HID_DEVICE_SEND_REPLY_TO_REPORT:

	case BT_RFCOMM_ACCEPT_CONNECTION:
	case BT_RFCOMM_REJECT_CONNECTION:
	case BT_RFCOMM_LISTEN:
	case BT_RFCOMM_CREATE_SOCKET_EX:
	case BT_RFCOMM_REMOVE_SOCKET_EX:

	case BT_PBAP_CONNECT:
	case BT_PBAP_DISCONNECT:
	case BT_PBAP_GET_PHONEBOOK_SIZE:
	case BT_PBAP_GET_PHONEBOOK:
	case BT_PBAP_GET_LIST:
	case BT_PBAP_PULL_VCARD:
	case BT_PBAP_PHONEBOOK_SEARCH:

	case BT_AVRCP_SET_TRACK_INFO:
	case BT_AVRCP_SET_PROPERTY:
	case BT_AVRCP_SET_PROPERTIES:
	case BT_AVRCP_HANDLE_CONTROL:
	case BT_AVRCP_CONTROL_SET_PROPERTY:
	case BT_AVRCP_CONTROL_GET_PROPERTY:
	case BT_AVRCP_GET_TRACK_INFO:

	case BT_SET_CONTENT_PROTECT:
	case BT_BOND_DEVICE_BY_TYPE:
	case BT_SET_LE_PRIVACY:
	case BT_LE_IPSP_INIT:
	case BT_LE_IPSP_DEINIT:
	case BT_LE_CONN_UPDATE:
	case BT_LE_IPSP_CONNECT:
	case BT_LE_IPSP_DISCONNECT:
	case BT_UPDATE_LE_CONNECTION_MODE:
	case BT_LE_READ_MAXIMUM_DATA_LENGTH:
	case BT_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH:
	case BT_LE_READ_HOST_SUGGESTED_DATA_LENGTH:
	case BT_LE_SET_DATA_LENGTH:

		ret_val = security_server_check_privilege_by_cookie(cookie,
						BT_PRIVILEGE_PLATFORM, "w");

		if (ret_val == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			BT_ERR("[SMACK] Fail to access: %s", BT_PRIVILEGE_PLATFORM);
			result = FALSE;
		}
	break;

	case BT_CHECK_ADAPTER:
	case BT_GET_RSSI:

	case BT_GET_LOCAL_NAME:
	case BT_GET_LOCAL_ADDRESS:
	case BT_GET_LOCAL_VERSION:
	case BT_IS_SERVICE_USED:
	case BT_GET_DISCOVERABLE_MODE:
	case BT_GET_DISCOVERABLE_TIME:
	case BT_IS_DISCOVERYING:
	case BT_IS_LE_DISCOVERYING:
	case BT_IS_CONNECTABLE:
	case BT_GET_BONDED_DEVICES:
	case BT_GET_BONDED_DEVICE:
	case BT_IS_DEVICE_CONNECTED:
	case BT_GET_CONNECTED_LINK_TYPE:
	case BT_GET_SPEAKER_GAIN:
	case BT_SET_SPEAKER_GAIN:
	case BT_OOB_READ_LOCAL_DATA:
	case BT_RFCOMM_CLIENT_IS_CONNECTED:
	case BT_RFCOMM_IS_UUID_AVAILABLE:
	case BT_GET_ADVERTISING_DATA:
	case BT_GET_SCAN_RESPONSE_DATA:
	case BT_IS_ADVERTISING:
	case BT_REGISTER_SCAN_FILTER:
	case BT_UNREGISTER_SCAN_FILTER:
	case BT_UNREGISTER_ALL_SCAN_FILTERS:

	case BT_OBEX_SERVER_ALLOCATE:
	case BT_OBEX_SERVER_DEALLOCATE:
	case BT_OBEX_SERVER_IS_ACTIVATED:

		/* Non-privilege control */
		break;
	default:
		BT_ERR("Unknown function!");
		result = FALSE;
		break;
	}

	return result;
}

GDBusNodeInfo *__bt_service_create_method_node_info
					(const gchar *introspection_data)
{
	GError *err = NULL;
	GDBusNodeInfo *node_info = NULL;

	if (introspection_data == NULL) {
		ERR("Introspection XML not present");
		return NULL;
	}

	node_info = g_dbus_node_info_new_for_xml(introspection_data, &err);

	if (err) {
		ERR("Unable to create node: %s", err->message);
		g_clear_error(&err);
	}
	return node_info;
}

int __bt_service_register_object(GDBusConnection *conn,
		GDBusNodeInfo *node_info, gboolean reg)
{
	static guint service_id = 0;
	GError *error = NULL;

	if (reg) {
		if (node_info == NULL)
			return -1;

		service_id = g_dbus_connection_register_object(conn,
				BT_SERVICE_PATH,
				node_info->interfaces[0],
				&method_table,
				NULL, NULL, &error);
		if (service_id == 0)
			return -1;
	} else {
		if (service_id > 0) {
			g_dbus_connection_unregister_object(conn,
					service_id);
			service_id = 0;
		}
	}

	return 0;
}

int _bt_service_register(void)
{
	GDBusConnection *conn;
	GError *err = NULL;
	int result;

	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
	retv_if(conn == NULL, BLUETOOTH_ERROR_INTERNAL);

	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				BT_SERVICE_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				NULL, NULL, NULL,
				NULL, NULL);
	BT_DBG("owner_id is [%d]", owner_id);
	if (owner_id == 0)
		goto fail;

	node_info = __bt_service_create_method_node_info(
			bt_service_introspection_xml);

	if (node_info == NULL)
		goto fail;

	result = __bt_service_register_object(conn, node_info, TRUE);
	g_dbus_node_info_unref(node_info);
	node_info = NULL;

	if (result != BLUETOOTH_ERROR_NONE)
		goto fail;

	bt_service_conn = conn;

	return BLUETOOTH_ERROR_NONE;

fail:
	if (bt_service_conn) {
		g_object_unref(bt_service_conn);
		bt_service_conn = NULL;
	}

	return BLUETOOTH_ERROR_INTERNAL;
}

void _bt_service_unregister(void)
{
	if (bt_service_conn) {
		__bt_service_register_object(bt_service_conn, NULL, FALSE);
		if (bt_service_conn) {
			g_object_unref(bt_service_conn);
			bt_service_conn = NULL;
		}
		if (node_info) {
			g_dbus_node_info_unref(node_info);
			node_info = NULL;
		}
		if (owner_id > 0) {
			g_bus_unown_name(owner_id);
			owner_id = 0;
		}
	}
}

int __bt_service_check_file_path_security(char **file_path,
					int file_count, GVariant *param, char *type)
{
	int i;
	int len;
	int ret_val;
	char *path_label = NULL;
	char *cookie = NULL;
	char *dir_path = NULL;

	len = g_variant_get_size(param);

	if (len > 0)
		cookie = (char *)g_variant_get_data(param);
	retv_if(cookie == NULL, BLUETOOTH_ERROR_ACCESS_DENIED);

	/* security check for file and directory */
	for (i = 0; i < file_count; i++) {
		/* current path check */
		if (smack_getlabel(file_path[i], &path_label,
								SMACK_LABEL_ACCESS) != 0) {
			BT_DBG("Fail to get \"%s\"'s smack label",file_path[i]);
			return BLUETOOTH_ERROR_ACCESS_DENIED;
		}

		ret_val = security_server_check_privilege_by_cookie(cookie,
								path_label, type);
		if (ret_val == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			BT_ERR("[SMACK] Fail to access: %s", path_label);
			return BLUETOOTH_ERROR_ACCESS_DENIED;
		}

		if (g_strcmp0(type, "x") == 0)
			break;

		/* parent path check */
		dir_path = strrchr(file_path[i], '/');
		if (dir_path == NULL) {
			return BLUETOOTH_ERROR_INVALID_PARAM;
		}

		len = strlen(file_path[i]) - strlen(dir_path);
		dir_path = g_malloc0(len + 1);
		if (dir_path == NULL) {
			return BLUETOOTH_ERROR_INTERNAL;
		}
		memcpy(dir_path, file_path[i], len);

		if (smack_getlabel(dir_path, &path_label,
								SMACK_LABEL_ACCESS) != 0) {
			BT_DBG("Fail to get \"%s\"'s smack label",dir_path);
			g_free(dir_path);
			return BLUETOOTH_ERROR_ACCESS_DENIED;
		}
		g_free(dir_path);

		ret_val = security_server_check_privilege_by_cookie(cookie,
								path_label, "x");
		if (ret_val == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
			BT_ERR("[SMACK] Fail to access: %s", path_label);
			return BLUETOOTH_ERROR_ACCESS_DENIED;
		}
	}
	return BLUETOOTH_ERROR_NONE;
}
