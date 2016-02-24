/*
 * Bluetooth-httpproxy-service
 *
 * Copyright (c) 2015 - 2016 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  C S Bhargava <cs.bhargava@samsung.com>
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

#include <dlog.h>
#include <gio/gio.h>

#include <stdio.h>
#include <privilege-control.h>

#include "bt-httpproxy.h"
#include "bluetooth-api.h"

#include <libsoup/soup.h>

#ifdef HPS_FEATURE

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_HPS"

#define BT_INFO(fmt, arg...) SLOGI(fmt, ##arg)
#define BT_ERR(fmt, arg...) SLOGE(fmt, ##arg)
#define BT_DBG(fmt, arg...) SLOGD(fmt, ##arg)

char *hps_obj_path = NULL;
char *http_uri_obj_path = NULL;
char *http_hdr_obj_path = NULL;
char *http_entity_obj_path = NULL;
char *http_cp_obj_path = NULL;
char *http_status_obj_path = NULL;
char *http_status_desc_obj_path = NULL;
char *http_security_obj_path = NULL;

static GMainLoop *main_loop;
static int property_sub_id = -1;
static int adapter_sub_id = -1;
static http_request_state req_state;

#ifdef	HPS_GATT_DB
struct hps_notify_read_info {
	gchar *char_path;
	guint  read_status;
	guint  offset_status;
	int  https_status;
};

struct hps_char_info {
	gchar *char_path;
	gchar *char_value;
	int value_length;
};

static GSList *hps_notify_read_list = NULL;
static GSList *hps_char_list = NULL;
#endif

static GDBusConnection *conn;
static GDBusConnection *g_conn;
static guint g_owner_id = 0;
GDBusNodeInfo *hps_node_info = NULL;

char *g_uri = NULL;
char *g_header = NULL;
char *g_entity = NULL;

static SoupSession *hps_soup_session = NULL;
static SoupMessage *hps_soup_msg = NULL;


static const gchar hps_introspection_xml[] =
"<node name='/'>"
"	<interface name='org.projectx.httpproxy_service'>"
"		<method name='enable'>"
"			<arg type='y' name='status' direction='out'/>"
"		</method>"
"		<method name='disable'>"
"			<arg type='y' name='status' direction='out'/>"
"		</method>"
"	</interface>"
"</node>";

#ifdef	HPS_GATT_DB
static void _bt_hps_set_char_value(const char *obj_path, const char* value, int value_length);

static void _hps_convert_address_to_hex(bluetooth_device_address_t *addr_hex, const char *addr_str)
{
	int i = 0;
	unsigned int addr[BLUETOOTH_ADDRESS_LENGTH] = { 0, };

	if (addr_str == NULL || addr_str[0] == '\0')
		return;

	i = sscanf(addr_str, "%X:%X:%X:%X:%X:%X", &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if (i != BLUETOOTH_ADDRESS_LENGTH) {
		BT_ERR("Invalid format string - [%s]", addr_str);
	}

	for (i = 0; i < BLUETOOTH_ADDRESS_LENGTH; i++) {
		addr_hex->addr[i] = (unsigned char)addr[i];
	}
}

static void _bt_hps_send_status_notification(unsigned short http_status,
			unsigned char data_status,
			bluetooth_device_address_t *unicast_address)
{
	char status[3] = {0x00};

	BT_DBG("");

	status[0] = http_status & 0x0F;
	status[1] = (http_status >> 8 )& 0x0F;
	status[2] = data_status;

	/* Store the status value */
	_bt_hps_set_char_value(http_status_obj_path, status, 3);

	/* Send nunicast notification */
	bluetooth_gatt_notify_characteristics_value_change(http_status_obj_path,
						status, 3, unicast_address);
}
#endif

static void _bt_httpproxy_method(GDBusConnection *connection,
		const gchar *sender,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *method_name,
		GVariant *parameters,
		GDBusMethodInvocation *invocation,
		gpointer user_data)
{
	int status = 0;

	BT_DBG("Method[%s] Object Path[%s] Interface Name[%s]",
			method_name, object_path, interface_name);

	if (g_strcmp0(method_name, "enable") == 0) {
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(y)", status));
	} else if (g_strcmp0(method_name, "disable") == 0) {
		_bt_hps_exit();
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(y)", status));
	}

	return;
}

static const GDBusInterfaceVTable hps_method_table = {
	_bt_httpproxy_method,
	NULL,
	NULL,
};

static void _bt_hps_on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	guint object_id;
	GError *error = NULL;

	BT_DBG("");

	g_conn = connection;

	object_id = g_dbus_connection_register_object(connection, BT_HPS_OBJECT_PATH,
						hps_node_info->interfaces[0],
						&hps_method_table,
						NULL, NULL, &error);
	if (object_id == 0) {
		BT_ERR("Failed to register method table: %s", error->message);
		g_error_free(error);
		g_dbus_node_info_unref(hps_node_info);
	}

	return;
}

static void _bt_hps_on_name_acquired (GDBusConnection *connection,
					const gchar	*name,
					gpointer		 user_data)
{
	BT_DBG("");
	return;
}

static void _bt_hps_on_name_lost (GDBusConnection *connection,
				const gchar	*name,
				gpointer		 user_data)
{
	BT_DBG("");
	g_object_unref(g_conn);
	g_conn = NULL;
	g_dbus_node_info_unref(hps_node_info);
	g_bus_unown_name(g_owner_id);

	return;
}

int _bt_hps_register_interface(void)
{
	GError *error = NULL;
	guint owner_id;

	BT_DBG("");

	hps_node_info = g_dbus_node_info_new_for_xml (hps_introspection_xml, &error);
	if (!hps_node_info) {
		BT_ERR("Failed to install: %s", error->message);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				BT_HPS_SERVICE_NAME,
				G_BUS_NAME_OWNER_FLAGS_NONE,
				_bt_hps_on_bus_acquired, _bt_hps_on_name_acquired, _bt_hps_on_name_lost,
				NULL, NULL);
	g_owner_id = owner_id;
	BT_DBG("owner_id is [%d]\n", owner_id);

	return BLUETOOTH_ERROR_NONE;
}

void _bt_hps_unregister_interface(void)
{
	BT_DBG("");

	g_object_unref(g_conn);
	g_conn = NULL;
	g_dbus_node_info_unref(hps_node_info);
	g_bus_unown_name(g_owner_id);

	return;
}

#ifdef	HPS_GATT_DB
static struct hps_char_info *hps_get_char_value(const char *path)
{
	GSList *tmp = NULL;

	for (tmp = hps_char_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_char_info *char_info = tmp->data;
			if(!g_strcmp0(char_info->char_path, path))
				return char_info;
		}
	}
	return NULL;
}

static int char_info_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct hps_char_info *attrib1 = a1;
	const struct hps_char_info *attrib2 = a2;

	return g_strcmp0(attrib1->char_path, attrib2->char_path);
}

static int notify_info_cmp(gconstpointer a1, gconstpointer a2)
{
	const struct hps_notify_read_info *attrib1 = a1;
	const struct hps_notify_read_info *attrib2 = a2;

	return g_strcmp0(attrib1->char_path, attrib2->char_path);
}

static void _bt_hps_set_char_value(const char *obj_path, const char* value, int value_length)
{
	GSList *tmp = NULL;
	if (!value)
		return;

	for (tmp = hps_char_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_char_info *char_info = tmp->data;
			if(!g_strcmp0(char_info->char_path, obj_path)) {
				char_info->char_value = g_try_realloc(char_info->char_value, value_length);
				if (char_info->char_value) {
					memcpy(char_info->char_value, value, value_length);
					char_info->value_length = value_length;
					hps_char_list = g_slist_insert_sorted (hps_char_list,
									char_info, char_info_cmp);
				}
				return;
			}
		}
	}
	return;
}

static void _bt_hps_set_notify_read_status(const char *obj_path,
			guint offset_status, guint read_status, int https_status)
{
	struct hps_notify_read_info *notify_read_info = NULL;
	GSList *tmp = NULL;

	for (tmp = hps_notify_read_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			notify_read_info = tmp->data;
			if(!g_strcmp0(notify_read_info->char_path, obj_path)) {
				notify_read_info->read_status = read_status;
				notify_read_info->offset_status = offset_status;
				notify_read_info->https_status = https_status;
				hps_notify_read_list = g_slist_insert_sorted (hps_notify_read_list,
								notify_read_info, notify_info_cmp);
				return;
			}
		}
	}

	if (!hps_notify_read_list) {
		/* Store Notification information */
		notify_read_info = g_new0(struct hps_notify_read_info, 1);
		if (notify_read_info) {
			notify_read_info->char_path = g_strdup(obj_path);
			notify_read_info->read_status = read_status;
			notify_read_info->offset_status = offset_status;
			notify_read_info->https_status = https_status;
			hps_notify_read_list = g_slist_append(hps_notify_read_list, notify_read_info);
		}
		return;
	} else {
		/* Store Notification information */
		notify_read_info = g_new0(struct hps_notify_read_info, 1);
		if (notify_read_info) {
			notify_read_info->char_path = g_strdup(obj_path);
			notify_read_info->read_status = read_status;
			notify_read_info->offset_status = offset_status;
			notify_read_info->https_status = https_status;
			hps_notify_read_list = g_slist_append(hps_notify_read_list, notify_read_info);
		}
		return;
	}
}

static struct hps_notify_read_info *_bt_hps_get_notify_read_status(const char *obj_path)
{
	GSList *tmp = NULL;

	for (tmp = hps_notify_read_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_notify_read_info *notify_read_info = tmp->data;
			if(!g_strcmp0(notify_read_info->char_path, obj_path)) {
				return notify_read_info;
			}
		}
	}

	return NULL;
}

static void delete_all_characterisitc(void)
{
	GSList *tmp = NULL;
	for (tmp = hps_char_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_char_info *char_info = tmp->data;
			if (char_info->char_path)
				g_free(char_info->char_path);
			if (char_info->char_value)
				g_free(char_info->char_value);
			hps_char_list = g_slist_delete_link(hps_char_list, tmp->data);
		}
	}
	g_slist_free(hps_char_list);
	hps_char_list = NULL;
}

static void delete_all_notify_read_status(void)
{
	GSList *tmp = NULL;
	for (tmp = hps_notify_read_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_notify_read_info *notify_read_info = tmp->data;
			if (notify_read_info->char_path)
				g_free(notify_read_info->char_path);
			hps_notify_read_list = g_slist_delete_link(hps_notify_read_list, tmp->data);
		}
	}
	g_slist_free(hps_notify_read_list);
	hps_notify_read_list = NULL;
}

static void delete_notify_read_status(const char *obj_path)
{
	GSList *tmp = NULL;
	for (tmp = hps_notify_read_list; tmp != NULL; tmp = tmp->next) {
		if (tmp->data) {
			struct hps_notify_read_info *notify_read_info = tmp->data;
			if(!g_strcmp0(notify_read_info->char_path, obj_path)) {
				if (notify_read_info->char_path)
					g_free(notify_read_info->char_path);
				hps_notify_read_list = g_slist_delete_link(hps_notify_read_list, tmp->data);
				return;
			}
		}
	}
}
#endif

int _bt_hps_uri_write_cb(char *uri, int len)
{
	if((len < 1) || (len > MAX_URI_LENGTH)) {
		BT_ERR("Wrong URI length %d", len);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* g_uri will be used commonly for all HTTP methods whereever applicable */
	if (g_uri)
		g_free(g_uri);
	g_uri = g_strndup(uri, len);
#ifdef	HPS_GATT_DB
	_bt_hps_set_char_value(http_uri_obj_path, g_uri, len);
#endif
	return BLUETOOTH_ERROR_NONE;
}

int _bt_hps_http_header_write_cb(char *header, int len)
{
	if((len < 1) || (len > MAX_HEADER_LENGTH)) {
		BT_ERR("Wrong Header length %d", len);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* g_header will be used commonly for all HTTP methods where ever applicable
	   general-header, request-header, entity-header
	*/
	if (g_header)
		g_free(g_header);
	g_header = g_strndup(header, len);
#ifdef	HPS_GATT_DB
	_bt_hps_set_char_value(http_hdr_obj_path, g_header, len);
#endif

	return BLUETOOTH_ERROR_NONE;
}

int _bt_hps_entity_body_write_cb(char *entity, int len)
{
	if((len < 1) || (len > MAX_ENTITY_LENGTH)) {
		BT_ERR("Wrong Entity length %d", len);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	/* g_entity will be used commonly for all HTTP methods whereever applicable */
	if (g_entity)
		g_free(g_entity);
	g_entity = g_strndup(entity, len);
#ifdef	HPS_GATT_DB
	_bt_hps_set_char_value(http_entity_obj_path, g_entity, len);
#endif

	return BLUETOOTH_ERROR_NONE;
}

#ifdef	HPS_GATT_DB
int _bt_hps_read_cb(const char *obj_path, char **value, int *len)
{
	struct hps_char_info *info = NULL;
	struct hps_notify_read_info *notify_read_info = NULL;
	guint data_status = -1;
	guint offset = 0;
	gboolean is_header = FALSE;

	if(!obj_path) {
		BT_ERR("Wrong Obj path");
		return FALSE;
	}

	if (!g_strcmp0(http_hdr_obj_path, obj_path))
		is_header = TRUE;

	info = hps_get_char_value(obj_path);
	if (info) {

		if (info->char_value == NULL || info->value_length == 0)
			return data_status;

		notify_read_info = _bt_hps_get_notify_read_status(obj_path);
		if (notify_read_info && notify_read_info->read_status != DS_BODY_RECEIVED &&
				notify_read_info->read_status != DS_HEADER_RECEIVED) {
			offset = notify_read_info->offset_status;
			if ((info->value_length - offset) > 0 &&
				(info->value_length - offset) > MAX_ENTITY_LENGTH)  {
				if (is_header)
					data_status = DS_HEADER_TRUNCATED;
				else
					data_status = DS_BODY_TRUNCATED;
				_bt_hps_set_notify_read_status(obj_path, offset + MAX_ENTITY_LENGTH,
								data_status, notify_read_info->https_status);
				*value = g_strdup(&info->char_value[offset]);
				*len = info->value_length;
			} else if ((info->value_length - offset) > 0 &&
				(info->value_length - offset) <= MAX_ENTITY_LENGTH) {
				if (is_header)
					data_status = DS_HEADER_RECEIVED;
				else
					data_status = DS_BODY_RECEIVED;
				_bt_hps_set_notify_read_status(obj_path, offset, data_status, notify_read_info->https_status);
				*value = g_strdup(&info->char_value[offset]);
				*len = info->value_length;
			}
		} else if (notify_read_info && (notify_read_info->read_status == DS_BODY_RECEIVED ||
						notify_read_info->read_status == DS_HEADER_RECEIVED)) {
				if (is_header)
					data_status = DS_HEADER_RECEIVED;
				else
					data_status = DS_BODY_RECEIVED;
				delete_notify_read_status(obj_path);
				*value = g_strdup(&info->char_value[offset]);
				*len = info->value_length;
		}
	}

	return data_status;
}
#endif

void _bt_hps_head_response_cb(SoupSession *session,
			SoupMessage *msg, gpointer user_data)
{
	unsigned short http_status = 0x00;
#ifndef HPS_GATT_DB
	unsigned char status[3] = {0x00};
#else
	const char *device_address = user_data;
	bluetooth_device_address_t addr_hex = { {0,} };
	unsigned char data_status = DS_NONE;
	_hps_convert_address_to_hex(&addr_hex, device_address);
#endif

	if(hps_soup_session != session) {
		BT_ERR("Wrong Session");
		return;
	}

	if(msg == NULL) {
		BT_ERR("Wrong Message");
		return;
	}
	hps_soup_msg = NULL;

	req_state = HTTP_REQ_STATE_EXECUTED;

	http_status = msg->status_code;

	// Process Header in Response Body
	if(msg->response_headers) {

		const char *content = NULL;
		const char *length = NULL;
		guint hdr_len = 0;

		length = soup_message_headers_get_one (msg->request_headers,
								"Content-Length");
		// Check "Content-MD5" is the right name to get header content
		content = soup_message_headers_get_one (msg->response_headers,
								"Content-MD5");
		if (content == NULL || length == NULL) {
			BT_ERR("Wrong Response Header");
			_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
			return;
		}

		hdr_len = soup_message_headers_get_content_length(msg->response_headers);

		// Write Data to Header Characteristic
#ifdef	HPS_GATT_DB
		_bt_hps_set_char_value(http_hdr_obj_path, content, hdr_len);
#else
		bluetooth_gatt_set_characteristic_value(http_hdr_obj_path, content, hdr_len);
#endif
		// TODO : Handle Truncated Header

		// Write Data to Status Code Characteristic
#ifdef	HPS_GATT_DB
		data_status = (hdr_len > MAX_ENTITY_LENGTH ) ? DS_HEADER_TRUNCATED : DS_HEADER_RECEIVED;
		if (data_status == DS_BODY_TRUNCATED && SOUP_STATUS_IS_SUCCESSFUL(http_status)) {
			_bt_hps_set_notify_read_status(http_hdr_obj_path, data_status, 0, http_status);
		}
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#else
		status[0] = http_status & 0x0F;
		status[1] = (http_status >> 8 )& 0x0F;
		status[2] = (hdr_len > MAX_HEADER_LENGTH ) ? DS_HEADER_TRUNCATED : DS_HEADER_RECEIVED;

		bluetooth_gatt_set_characteristic_value(http_status_obj_path, status, 3);
#endif
	}else {
		BT_ERR("HEAD Response is NULL");
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
	}

	return;
}

void _bt_hps_http_response_cb(SoupSession *session,
			SoupMessage *msg, gpointer user_data)
{
	unsigned short http_status = 0x00;
#ifndef	HPS_GATT_DB
	unsigned char status[3] = {0x00};
#else
	const char *device_address = user_data;
	bluetooth_device_address_t addr_hex = { {0,} };
	unsigned char data_status = DS_NONE;
	_hps_convert_address_to_hex(&addr_hex, device_address);
#endif

	if(hps_soup_session != session) {
		BT_ERR("Wrong Session");
		return;
	}

	if(msg == NULL) {
		BT_ERR("Wrong Message");
		return;
	}

	hps_soup_msg = NULL;

	req_state = HTTP_REQ_STATE_EXECUTED;

	http_status = msg->status_code;

	// Write Data to Status Code Characteristic
#ifndef	HPS_GATT_DB
	status[0] = http_status & 0x0F;
	status[1] = (http_status >> 8 )& 0x0F;
	status[2] = DS_HEADER_RECEIVED;
	bluetooth_gatt_set_characteristic_value(http_status_obj_path, status, 3);
#else
	data_status = DS_HEADER_RECEIVED;
	_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#endif

	return;
}

void _bt_hps_get_response_cb(SoupSession *session,
			SoupMessage *msg, gpointer user_data)
{
	SoupBuffer *body = NULL;
	unsigned short http_status = 0x00;
#ifndef HPS_GATT_DB
	unsigned char status[3] = {0x00};
#else
	const char *device_address = user_data;
	bluetooth_device_address_t addr_hex = { {0,} };
	unsigned char data_status = DS_NONE;
	_hps_convert_address_to_hex(&addr_hex, device_address);
#endif

	if(hps_soup_session != session) {
		BT_ERR("Wrong Session");
		return;
	}

	if(msg == NULL) {
		BT_ERR("Wrong Message");
		return;
	}

	hps_soup_msg = NULL;

	req_state = HTTP_REQ_STATE_EXECUTED;

	http_status = msg->status_code;

	// Process Entity Body in Response Message
	if(msg->response_body) {

		body = soup_message_body_flatten (msg->response_body);
		if (body == NULL) {
			BT_ERR("Wrong Response Body");
#ifdef HPS_GATT_DB
			_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#endif
			return;
		}
		if (body->data == NULL || body->length <= 0) {
			BT_ERR("Wrong Response");
			soup_buffer_free(body);
#ifdef HPS_GATT_DB
			_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#endif
			return;
		}
		// Write Data to Entity Body Characteristic
#ifdef	HPS_GATT_DB
		_bt_hps_set_char_value(http_entity_obj_path, body->data, body->length);
#else
		bluetooth_gatt_set_characteristic_value(http_entity_obj_path, body->data, body->length);
#endif
		// TODO : Handle Truncated Entiry Body

		// Write Data to Status Code Characteristic
#ifdef	HPS_GATT_DB
		data_status = (body->length > MAX_ENTITY_LENGTH ) ? DS_BODY_TRUNCATED : DS_BODY_RECEIVED;
		if (data_status == DS_BODY_TRUNCATED && SOUP_STATUS_IS_SUCCESSFUL(http_status)) {
			_bt_hps_set_notify_read_status(http_entity_obj_path, data_status, 0, http_status);
		}
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);

#else
		status[0] = http_status & 0x0F;
		status[1] = (http_status >> 8 )& 0x0F;
		status[2] = (body->length > MAX_HEADER_LENGTH ) ? DS_BODY_TRUNCATED : DS_BODY_TRUNCATED;

		bluetooth_gatt_set_characteristic_value(http_status_obj_path, status, 3);
#endif
		soup_buffer_free(body);
	}else {
		BT_ERR("GET Response Body is NULL");
#ifdef HPS_GATT_DB
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#endif
	}

	// Process Header in Response Body
	if(msg->response_headers) {

		const char *content = NULL;
		const char *length = NULL;
		guint hdr_len = 0;

		length = soup_message_headers_get_one (msg->request_headers,
								"Content-Length");
		// Check "Content-MD5" is the right name to get header content
		content = soup_message_headers_get_one (msg->response_headers,
								"Content-MD5");
		if (content == NULL || length == NULL) {
			BT_ERR("Wrong Response Header");
			data_status = DS_NONE;
			_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
			return;
		}

		hdr_len = soup_message_headers_get_content_length(msg->response_headers);
		// Write Data to Header Characteristic
#ifdef	HPS_GATT_DB
		_bt_hps_set_char_value(http_hdr_obj_path, content, hdr_len);
#else
		bluetooth_gatt_set_characteristic_value(http_hdr_obj_path, content, hdr_len);
#endif
		// TODO : Handle Truncated Header

		// Write Data to Status Code Characteristic
#ifdef	HPS_GATT_DB
		data_status = (hdr_len > MAX_HEADER_LENGTH ) ? DS_HEADER_TRUNCATED : DS_HEADER_RECEIVED;
		if (data_status == DS_HEADER_TRUNCATED && SOUP_STATUS_IS_SUCCESSFUL(http_status)) {
			_bt_hps_set_notify_read_status(http_hdr_obj_path, data_status, 0, http_status);
		}
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#else
		status[0] = http_status & 0x0F;
		status[1] = (http_status >> 8 )& 0x0F;
		status[2] = (hdr_len > MAX_HEADER_LENGTH ) ? DS_HEADER_TRUNCATED : DS_HEADER_RECEIVED;

		bluetooth_gatt_set_characteristic_value(http_status_obj_path, status, 3);
#endif
	}else {
		BT_ERR("GET Response Header is NULL");
#ifdef HPS_GATT_DB
		_bt_hps_send_status_notification(http_status, data_status, &addr_hex);
#endif
	}

	return;
}

#ifdef	HPS_GATT_DB
int _bt_hps_control_point_write_cb(const char *value, int len, char *addr)
#else
int _bt_hps_control_point_write_cb(char *value, int len)
#endif
{
	int opcode = *value;
	GTlsCertificate *cert = NULL;
	GTlsCertificateFlags flags;
	gboolean https_status = FALSE;
	int result = BLUETOOTH_ERROR_NONE;
	BT_INFO("Opcode %0x", opcode);

#ifdef	HPS_GATT_DB
	_bt_hps_set_char_value(http_cp_obj_path, value, len);
#endif

	switch(opcode) {
		case HTTP_GET_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("GET", g_uri);
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_get_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_get_response_cb, NULL);
#endif
			} else {
				BT_ERR("HTTP GET request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTP_POST_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("POST", g_uri);
				if(hps_soup_msg == NULL || g_entity == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
				soup_message_set_request (hps_soup_msg, "text/xml", SOUP_MEMORY_COPY,
							  g_entity, strlen (g_entity));
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);
#endif
			} else {
				BT_ERR("HTTP POST request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTP_HEAD_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("HEAD", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_head_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_head_response_cb, NULL);
#endif
			} else {
				BT_ERR("HTTP HEAD request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTP_PUT_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				SoupBuffer *buf;
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("PUT", g_uri);
				if(hps_soup_msg == NULL  || g_entity == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
				buf = soup_buffer_new (SOUP_MEMORY_TAKE, g_entity, strlen (g_entity));
				soup_message_body_append_buffer (hps_soup_msg->request_body, buf);
				soup_message_body_set_accumulate (hps_soup_msg->request_body, FALSE);
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);
#endif

			} else {
				BT_ERR("HTTP PUT request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTP_DELETE_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("DELETE", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);
#endif
			} else {
				BT_ERR("HTTP DELETE request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTPS_GET_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("GET", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_get_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_get_response_cb, NULL);
#endif
				https_status = soup_message_get_https_status (hps_soup_msg, &cert, &flags);
#ifdef	HPS_GATT_DB
				_bt_hps_set_char_value(http_security_obj_path, (const char *)&https_status, 1);
#else
				bluetooth_gatt_set_characteristic_value(http_security_obj_path, (char *)&https_status, 1);
#endif
			} else {
				BT_ERR("HTTPS GET request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTPS_HEAD_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("HEAD", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_head_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_head_response_cb, NULL);
#endif
				https_status = soup_message_get_https_status (hps_soup_msg, &cert, &flags);
#ifdef	HPS_GATT_DB
				_bt_hps_set_char_value(http_security_obj_path, (const char *)&https_status, 1);
#else
				bluetooth_gatt_set_characteristic_value(http_security_obj_path, (char *)&https_status, 1);
#endif
			} else {
				BT_ERR("HTTPS HEAD request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTPS_POST_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("POST", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
				soup_message_set_request (hps_soup_msg, "text/xml", SOUP_MEMORY_STATIC,
							  g_entity, strlen (g_entity));
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);

#endif
				https_status = soup_message_get_https_status (hps_soup_msg, &cert, &flags);
#ifdef	HPS_GATT_DB
				_bt_hps_set_char_value(http_security_obj_path, (const char *)&https_status, 1);
#else
				bluetooth_gatt_set_characteristic_value(http_security_obj_path, (char *)&https_status, 1);
#endif
			} else {
				BT_ERR("HTTPS POST request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTPS_PUT_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				SoupBuffer *buf;
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("PUT", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
				buf = soup_buffer_new (SOUP_MEMORY_TAKE, g_entity, strlen (g_entity));
				soup_message_body_append_buffer (hps_soup_msg->request_body, buf);
				soup_message_body_set_accumulate (hps_soup_msg->request_body, FALSE);
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);
#endif
				https_status = soup_message_get_https_status (hps_soup_msg, &cert, &flags);
#ifdef	HPS_GATT_DB
				_bt_hps_set_char_value(http_security_obj_path, (const char *)&https_status, 1);
#else
				bluetooth_gatt_set_characteristic_value(http_security_obj_path, (char *)&https_status, 1);
#endif
			} else {
				BT_ERR("HTTPS PUT request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTPS_DELETE_REQUEST:
			if(req_state == HTTP_REQ_STATE_EXECUTED) {
				req_state = HTTP_REQ_STATE_INPROGRESS;
				hps_soup_msg = soup_message_new("DELETE", g_uri);
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
#ifdef	HPS_GATT_DB
				g_object_ref (hps_soup_msg);
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, addr);
#else
				soup_session_queue_message(hps_soup_session, hps_soup_msg, _bt_hps_http_response_cb, NULL);
#endif

				https_status = soup_message_get_https_status (hps_soup_msg, &cert, &flags);
#ifdef	HPS_GATT_DB
				_bt_hps_set_char_value(http_security_obj_path, (const char *)&https_status, 1);
#else
				bluetooth_gatt_set_characteristic_value(http_security_obj_path, (char *)&https_status, 1);
#endif
			} else {
				BT_ERR("HTTPS DELETE request in progress, message dropped");
				result = BLUETOOTH_ERROR_INTERNAL;
			}
			break;

		case HTTP_REQUEST_CANCEL:
			/* Cancel the outstanding request */
			if(req_state == HTTP_REQ_STATE_INPROGRESS) {
				req_state = HTTP_REQ_STATE_IDLE;
				if(hps_soup_msg == NULL) {
					BT_ERR("Soup Message NULL");
					result = BLUETOOTH_ERROR_INTERNAL;
					req_state = HTTP_REQ_STATE_EXECUTED;
					break;
				}
				soup_session_cancel_message (hps_soup_session, hps_soup_msg, SOUP_STATUS_CANCELLED);
				hps_soup_msg = NULL;
			}
			break;

		default:
			BT_ERR("Unknown opcode %0x", opcode);
			result = BLUETOOTH_ERROR_INTERNAL;
			break;
	}

	return result;
}

void _bt_hps_security_read_cb (char *value, int len)
{
	BT_INFO("HPS Client Read the value");
	return;
}

#ifdef	HPS_GATT_DB
void _bt_hps_gatt_char_property_changed_event(GVariant *msg,
				const char *path)
{
	int result = BLUETOOTH_ERROR_NONE;
	GVariantIter value_iter;
	const char *property = NULL;
	const char * char_path = NULL;
	const char * svc_handle = NULL;
	GVariant *var = NULL;
	GVariant *val = NULL;
	g_variant_iter_init (&value_iter, msg);

	while ((g_variant_iter_loop(&value_iter, "{sv}", &property, &var))) {

		if(property == NULL) {
			BT_ERR("Property NULL");
			return;
		}

		if (!g_strcmp0(property, "WriteValue")) {
			int len = 0;
			BT_INFO("WriteValue");
			BT_INFO("Type '%s'\n", g_variant_get_type_string (var));

			if (var) {
				gchar *addr = NULL;
				guint8 req_id = 1;
				guint16 offset = 0;
				char *value = NULL;
				g_variant_get(var, "(&s&s&syq@ay)", &char_path,
						&svc_handle, &addr, &req_id, &offset, &val);

				len = g_variant_get_size(val);

				BT_DBG("Len = %d", len);

				value = (char *) g_variant_get_data(val);

				if (len != 0) {
					if(!g_strcmp0(char_path, http_uri_obj_path)) {
						/* Retrive URI */
						result = _bt_hps_uri_write_cb(value, len);
					} else if(!g_strcmp0(char_path, http_hdr_obj_path)) {
						/* Retrive HEADER */
						result = _bt_hps_http_header_write_cb(value, len);
					} else if(!g_strcmp0(char_path, http_entity_obj_path)) {
						/* Retrive ENTITY BODY */
						result = _bt_hps_entity_body_write_cb(value, len);
					} else if(!g_strcmp0(char_path, http_cp_obj_path)) {
						result = _bt_hps_control_point_write_cb(value, len, addr);
					} else {
						BT_ERR("Wrong Object Path %s", char_path);
						result = BLUETOOTH_ERROR_INTERNAL;
					}
				bluetooth_gatt_send_response(req_id, BLUETOOTH_GATT_ATT_REQUEST_TYPE_WRITE, result, 0, NULL, 0);
				} else {
					BT_ERR("Array Len 0");
				}
			} else {
				BT_ERR("var==NULL");
			}
		} else if (!g_strcmp0(property, "ReadValue")) {
			gchar *addr = NULL;
			guint8 req_id = 1;
			guint16 offset = 0;
			char *value = NULL;
			int len = 0;
			int data_status = -1;
			BT_INFO("ReadValue");
			BT_INFO("Type '%s'\n", g_variant_get_type_string (var));

			g_variant_get(var, "(&s&s&syq)", &char_path, &svc_handle,
								&addr, &req_id, &offset);

			data_status = _bt_hps_read_cb(char_path, &value, &len);
			if (data_status >= DS_NONE) {
				struct hps_notify_read_info *notify_read_info = NULL;
				bluetooth_device_address_t addr_hex = { {0,} };
				_hps_convert_address_to_hex(&addr_hex, addr);
				bluetooth_gatt_send_response(req_id, BLUETOOTH_GATT_ATT_REQUEST_TYPE_READ,
								BLUETOOTH_ERROR_NONE, offset, value, len);
				notify_read_info = _bt_hps_get_notify_read_status(char_path);
				if (notify_read_info) {
					_bt_hps_send_status_notification(notify_read_info->https_status,
									data_status, &addr_hex);
				} else {
					if (data_status == DS_BODY_RECEIVED ||
						data_status == DS_HEADER_RECEIVED) {
						_bt_hps_set_char_value(char_path, NULL, 0);
					}
				}
				if (value)
					g_free(value);
			} else {
				BT_ERR("ReadValue failed %s", char_path);
				bluetooth_gatt_send_response(req_id, BLUETOOTH_GATT_ATT_REQUEST_TYPE_READ,
								BLUETOOTH_ERROR_INTERNAL, offset, NULL, 0);
			}
		}
	}
	return;
}
#else
void _bt_hps_gatt_char_property_changed_event(GVariant *msg,
				const char *path)
{
	GVariantIter value_iter;
	char *property = NULL;
	char * char_handle = NULL;
	GVariant *val = NULL;
	int result = BLUETOOTH_ERROR_NONE;
	GVariant *param = NULL;
	g_variant_iter_init (&value_iter, msg);
	char_handle = g_strdup(path);

	while ((g_variant_iter_loop(&value_iter, "{sv}", &property, &val))) {

		if(property == NULL) {
			BT_ERR("Property NULL");
			return;
		}

		if (strcasecmp(property, "ChangedValue") == 0) {

			int len = 0;
			GByteArray *gp_byte_array = NULL;
			BT_INFO("Type '%s'\n", g_variant_get_type_string (val));

			if (val) {
				gp_byte_array = g_byte_array_new();
				len = g_variant_get_size(val);
				BT_DBG("Len = %d", len);
				g_byte_array_append (gp_byte_array,
					(const guint8 *) g_variant_get_data(val), len);
				if (gp_byte_array->len != 0) {
					GVariant *byte_array = NULL;
					byte_array = g_variant_new_from_data(
								G_VARIANT_TYPE_BYTESTRING,
								gp_byte_array->data,
								gp_byte_array->len,
								TRUE, NULL, NULL);
					param = g_variant_new("(is@ay)", result, char_handle,
								byte_array);

					if(strcmp(path, http_uri_obj_path)) {
						//Retrive URI
						_bt_hps_uri_write_cb(NULL, len);
					} else if(strcmp(path, http_hdr_obj_path)) {
						//Retrive HEADER
						_bt_hps_http_header_write_cb(NULL, len);
					} else if(strcmp(path, http_entity_obj_path)) {
						//Retrive ENTITY BODY
						_bt_hps_entity_body_write_cb(NULL, len);
					} else if(strcmp(path, http_cp_obj_path)) {
						_bt_hps_control_point_write_cb(NULL, len);
					} else if(strcmp(path, http_security_obj_path)) {
						_bt_hps_security_read_cb(NULL, len);
					} else {
						BT_ERR("Wrong Object Path %s", path);
					}
				} else {
					BT_ERR("Array Len 0");
				}
				g_byte_array_free(gp_byte_array, TRUE);
			}else {
				BT_ERR("val==NULL");
			}
		}
	}
	g_free(char_handle);

	return;
}
#endif

void _bt_hps_property_event_filter(GDBusConnection *connection,
					const gchar *sender_name,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *signal_name,
					GVariant *parameters,
					gpointer user_data)
{
	GVariant *value;

	if (signal_name == NULL) {
		BT_ERR("Wrong Signal");
		return;
	}

#ifdef	HPS_GATT_DB
	if (g_strcmp0(signal_name, PROPERTIES_CHANGED) == 0) {

		g_variant_get(parameters, "(@a{sv}@as)", &value, NULL);

		_bt_hps_gatt_char_property_changed_event(value, object_path);
#else
	if (g_strcmp0(interface_name, BT_HPS_PROPERTIES_INTERFACE) == 0) {

		g_variant_get(parameters, "(&s@a{sv}@as)", &interface_name, &value, NULL);

		_bt_hps_gatt_char_property_changed_event(value, object_path);
#endif
	} else {
		//BT_ERR("Wrong Interface %s", interface_name);
	}

	return;
}


void _bt_hps_adapter_event_filter(GDBusConnection *connection,
					const gchar *sender_name,
					const gchar *object_path,
					const gchar *interface_name,
					const gchar *signal_name,
					GVariant *parameters,
					gpointer user_data)
{
	int result = BLUETOOTH_ERROR_NONE;
	GVariant *value;

	if (signal_name == NULL) {
		BT_ERR("Wrong Signal");
		return;
	}

	BT_INFO("Interface %s, Signal %s", interface_name, signal_name);

	if (g_strcmp0(interface_name, BT_HPS_INTERFACE_NAME) == 0) {

		g_variant_get(parameters, "(&s@a{sv}@as)", &interface_name, &value, NULL);

		if (strcasecmp(signal_name, BLE_ENABLED) == 0) {
			g_variant_get(parameters, "(i)", &result);

			if (_bt_hps_prepare_httpproxy() != BLUETOOTH_ERROR_NONE) {
				BT_ERR("Fail to prepare HTTP Proxy");
				return;
			}

			if (_bt_hps_set_advertising_data() != BLUETOOTH_ERROR_NONE) {
				BT_ERR("Fail to set advertising data");
				return;
			}

		} else {
			BT_ERR("Wrong Signal %s", signal_name);
		}
	}

	return;
}

int _bt_hps_init_event_receiver()
{
	GError *error = NULL;

	BT_DBG("");

	if (conn == NULL) {
		conn =	g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (error != NULL) {
			BT_ERR("ERROR: Can't get on system bus [%s]", error->message);
			g_clear_error(&error);
		}
	}

	property_sub_id = g_dbus_connection_signal_subscribe(conn,
		NULL, BT_HPS_INTERFACE_NAME,
		PROPERTIES_CHANGED, BT_HPS_OBJECT_PATH, NULL, 0,
		_bt_hps_property_event_filter,
		NULL, NULL);

	adapter_sub_id = g_dbus_connection_signal_subscribe(conn,
		NULL, BT_HPS_INTERFACE_NAME,
		BLE_ENABLED, BT_HPS_OBJECT_PATH, NULL, 0,
		_bt_hps_adapter_event_filter,
		NULL, NULL);

	return 0;
}

void _bt_hps_deinit_event_receiver(void)
{
	BT_DBG("");
	g_dbus_connection_signal_unsubscribe(conn, property_sub_id);
	g_dbus_connection_signal_unsubscribe(conn, adapter_sub_id);
	conn = NULL;
	return;
}

int _bt_hps_set_advertising_data(void)
{
	int ret;
	BT_DBG("");

    // Temporary UUID is used. SIG have not yet defined the UUID yet
	guint8 data[4]	= {0x03, 0x02, 0x00, 0x19};
	bluetooth_advertising_data_t adv;

	BT_DBG("%x %x %x %x", data[0], data[1], data[2], data[3]);
	memcpy(adv.data, data, sizeof(data));
	ret = bluetooth_set_advertising_data(0, &adv, sizeof(data));
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to set ADV data %d", ret);
		return ret;
	}

	ret = bluetooth_set_advertising(0, TRUE);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to set ADV %d", ret);
		return ret;
	}

	return 0;
}

int _bt_hps_prepare_httpproxy(void)
{
	int ret = BLUETOOTH_ERROR_NONE;
	char *char_uuid;
	char *service_uuid;
	char *desc_uuid;
	bt_gatt_characteristic_property_t props;
#ifdef	HPS_GATT_DB
	char value[MAX_URI_LENGTH] = { 0 };
	struct hps_char_info *char_info = NULL;
	char cp = 0x00;
	char status[3] = { 0 };
#endif

	BT_DBG("");

	ret = bluetooth_gatt_init();
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to Init GATT %d", ret);
		goto fail;
	}

	service_uuid = g_strdup(HPS_UUID);
	ret = bluetooth_gatt_add_service(service_uuid, &hps_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add service %d", ret);
		goto fail;
	}

	/* Characteristic URI */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	char_uuid = g_strdup(HTTP_URI_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_uri_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_uri_obj_path, value, MAX_URI_LENGTH);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

	/* Store requets information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_uri_obj_path);
	_bt_hps_set_char_value(http_uri_obj_path, value, MAX_URI_LENGTH);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	/* Characteristic HTTP Headers */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	char_uuid = g_strdup(HTTP_HDR_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_hdr_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_hdr_obj_path, value, MAX_HEADER_LENGTH);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

	/* Store Characterisitc information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_hdr_obj_path);
	_bt_hps_set_char_value(http_hdr_obj_path, value, MAX_HEADER_LENGTH);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	/* Characteristic HTTP Entity Body */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	char_uuid = g_strdup(HTTP_ENTITY_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_entity_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_entity_obj_path, value, MAX_ENTITY_LENGTH);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

	/* Store Characterisitc information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_entity_obj_path);
	_bt_hps_set_char_value(http_entity_obj_path, value, MAX_ENTITY_LENGTH);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	/* Characteristic HTTP Control Point */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	char_uuid = g_strdup(HTTP_CP_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_cp_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_cp_obj_path, &cp, 1);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

	/* Store Characterisitc information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_cp_obj_path);
	_bt_hps_set_char_value(http_cp_obj_path, &cp, 1);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	/* Characteristic HTTP Status Code */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_NOTIFY;
	char_uuid = g_strdup(HTTP_STATUS_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_status_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_status_obj_path, status, 3);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#endif
	desc_uuid = g_strdup(HTTP_STATUS_CCC_DESC_UUID);
	ret = bluetooth_gatt_add_descriptor(http_status_obj_path, desc_uuid, &http_status_desc_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char descriptor %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	/* Store Characterisitc information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_status_obj_path);
	_bt_hps_set_char_value(http_status_obj_path, status, 3);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	/* Characteristic HTTPS Security */
	props = BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_READ |
			BLUETOOTH_GATT_CHARACTERISTIC_PROPERTY_WRITE;
	char_uuid = g_strdup(HTTP_SECURITY_UUID);
	ret = bluetooth_gatt_add_new_characteristic(hps_obj_path, char_uuid, props, &http_security_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}
#ifdef	HPS_GATT_DB
	ret = bluetooth_gatt_set_characteristic_value(http_security_obj_path, &cp, 1);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to add new char %d", ret);
		goto fail;
	}

	/* Store Characterisitc information */
	char_info = g_new0(struct hps_char_info, 1);
	char_info->char_path = g_strdup(http_security_obj_path);
	_bt_hps_set_char_value(http_security_obj_path, &cp, 1);
	hps_char_list = g_slist_append(hps_char_list, char_info);
#endif

	ret = bluetooth_gatt_register_service(hps_obj_path);
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to register service %d", ret);
		goto fail;
	}

	return ret;

fail:
#ifdef	HPS_GATT_DB
	delete_all_characterisitc();
	delete_all_notify_read_status();
#endif
	return ret;
}


static void _bt_hps_sig_handler(int sig)
{
	BT_DBG("");

	switch(sig) {
		case SIGTERM:
			BT_DBG("caught signal - sigterm\n");
			break;
		case SIGINT:
			BT_DBG("caught signal - sigint\n");
			break;
		case SIGKILL:
			BT_DBG("caught signal - sigkill\n");
			break;
		default:
			BT_DBG("caught signal %d and ignored\n",sig);
			break;
	}
}

void _bt_hps_exit(void)
{
	int ret;
	BT_DBG("");

	if(g_uri != NULL) {
		g_free(g_uri);
		g_uri = NULL;
	}

	if(g_header != NULL) {
		g_free(g_header);
		g_header = NULL;
	}

	if(g_entity != NULL) {
		g_free(g_entity);
		g_entity = NULL;
	}

	soup_session_abort(hps_soup_session);
	g_assert_cmpint(G_OBJECT (hps_soup_session)->ref_count, ==, 1);
	g_object_unref(hps_soup_session);

#ifdef	HPS_GATT_DB
	delete_all_characterisitc();
#endif

	ret = bluetooth_gatt_deinit();
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to Deinit GATT %d", ret);
	}

	ret = bluetooth_unregister_callback();
	if(ret != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Failed to Unregister callback %d", ret);
	}

	_bt_hps_deinit_event_receiver();

	_bt_hps_unregister_interface();

	if (main_loop != NULL) {
		g_main_loop_quit(main_loop);
	}
}

void bt_hps_event_callback(int event, bluetooth_event_param_t* param,
							void *user_data)
{
	BT_DBG("HPS event %d", event);
	return;
}

/* HTTP Proxy Service Main loop */
int main(void)
{
	struct sigaction sa;

	BT_ERR("Starting the bt-httpproxy daemon");

	/* Values taken from http://www.browserscope.org/  following
	  * the rule "Do What Every Other Modern Browser Is Doing". They seem
	  * to significantly improve page loading time compared to soup's
	  * default values.
	  * Change MAX_CONNECTIONS_PER_HOST value 6 -> 12, and maxConnections is changed from 35 to 60.
	  * Enhanced network loading speed apply tunning value. */
	static const int maxConnections = 60;
	static const int maxConnectionsPerHost = 12;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = _bt_hps_sig_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGKILL, &sa, NULL);

	// g_type_init is deprecated glib 2.36 onwards, current version id 2.15
	g_type_init();

	if (perm_app_set_privilege("bluetooth-httpproxy-service", NULL, NULL) != PC_OPERATION_SUCCESS) {
		BT_ERR("Failed to set app privilege");
		return -2;
	}

#ifndef	HPS_GATT_DB
	if(bluetooth_register_callback(bt_hps_event_callback, NULL) != BLUETOOTH_ERROR_NONE) {
		BT_ERR("bluetooth_register_callback returned failiure");
		return -3;
	}
#endif

	if (_bt_hps_register_interface() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to register http proxy service");
		return -4;
	}

	if (_bt_hps_init_event_receiver() != BLUETOOTH_ERROR_NONE) {
		BT_ERR("Fail to init event reciever");
		return -5;
	}

	hps_soup_session = soup_session_async_new();
	if (hps_soup_session == NULL) {
		BT_ERR("Failed to soup_session_async_new");
		return -6;
	}
	/* Set Soup Session Fetures */
	g_object_set(hps_soup_session,
			SOUP_SESSION_MAX_CONNS, maxConnections,
			SOUP_SESSION_MAX_CONNS_PER_HOST, maxConnectionsPerHost,
			SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_DECODER,
			SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_SNIFFER,
			SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_PROXY_RESOLVER_DEFAULT,
			SOUP_SESSION_USE_THREAD_CONTEXT, TRUE,
			NULL);

	main_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(main_loop);

	BT_DBG("g_main_loop_quit called!");

	if (main_loop != NULL) {
		g_main_loop_unref(main_loop);
	}

	return 0;
}

#endif
