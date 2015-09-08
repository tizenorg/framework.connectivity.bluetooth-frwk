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

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <stacktrim.h>
#include <syspopup_caller.h>
#include <vconf.h>

#include "bt-internal-types.h"
#include "bt-service-common.h"
//#include "bt-service-agent.h"
//#include "bt-service-gap-agent.h"
//#include "bt-service-adapter.h"
#include "bt-service-event.h"
//#include "bt-service-rfcomm-server.h"
//#include "bt-service-device.h"
//#include "bt-service-audio.h"
#include "bt-service-pbap.h"
#include <glib.h>
#include <gio/gio.h>

#define	 PBAP_UUID "0000112f-0000-1000-8000-00805f9b34fb"
#define	 PBAP_OBEX_CLIENT_SERVICE "org.bluez.obex"
#define	 PBAP_OBEX_CLIENT_PATH "/org/bluez/obex"
#define	 PBAP_OBEX_CLIENT_INTERFACE "org.bluez.obex.Client1"

#define	 PBAP_SESSION_SERVICE	"org.bluez.obex"
#define	 PBAP_SESSION_INTERFACE	"org.bluez.obex.PhonebookAccess1"
#define PBAP_VCARDLIST_MAXLENGTH 256

typedef enum {
PBAP_FIELD_ALL,
PBAP_FIELD_VERSION,
PBAP_FIELD_FN,
PBAP_FIELD_N,
PBAP_FIELD_PHOTO,
PBAP_FIELD_BDAY,
PBAP_FIELD_ADR,
PBAP_FIELD_LABEL,
PBAP_FIELD_TEL,
PBAP_FIELD_EMAIL,
PBAP_FIELD_MAILER,
PBAP_FIELD_TZ,
PBAP_FIELD_GEO,
PBAP_FIELD_TITLE,
PBAP_FIELD_ROLE,
PBAP_FIELD_LOGO,
PBAP_FIELD_AGENT,
PBAP_FIELD_ORG,
PBAP_FIELD_NOTE,
PBAP_FIELD_REV,
PBAP_FIELD_SOUND,
PBAP_FIELD_URL,
PBAP_FIELD_UID,
PBAP_FIELD_KEY,
PBAP_FIELD_NICKNAME,
PBAP_FIELD_CATEGORIES,
PBAP_FIELD_PROID,
PBAP_FIELD_CLASS,
PBAP_FIELD_SORT_STRING,
PBAP_FIELD_X_IRMC_CALL_DATETIME,
} bt_pbap_field_e;

char *SOURCE[] = {
		"int",	//Phone memory
		"sim"	// SIM memory
};

char *TYPE[] = {
		"pb",	//Phonebook for the saved contacts
		"ich",	//Incoming call history
		"och",	//Outgoing call history
		"mch",	//Missed call history
		"cch",	//Combined Call History cch = ich + och + mch
};

char *FORMAT[] = {
		"vcard21",	// vCard Format 2.1 (Default)
		"vcard30",	// vCard Format 3.0
};

char *ORDER[] = {
		"indexed",		// Index (default)
		"alphanumeric",	// Alphanumeric
		"phonetic",		// Phonetic
};

char *SEARCH_FIELD[] = {
		"name",		// Search by Name(default)
		"number",	// Search by Phone Number
		"sound",	// Search by phonetic sound
};

static char *g_pbap_session_path = NULL;
static char *g_pbap_server_address = NULL;
static DBusGConnection *dbus_connection = NULL;
static DBusGProxy *g_pbap_proxy = NULL;

static struct {
	int type;
	int folder;
} selected_path = { -1, -1};

typedef enum  {
	PBAP_NONE,
	GET_SIZE,
	PULL_ALL,
	GET_LIST,
	GET_VCARD,
	PB_SEARCH,
} bt_pbap_operation_e;

typedef struct  {
	bt_pbap_operation_e operation;
	void *data;
	void *app_param;
} bt_pbap_data_t;

typedef struct {
	char *path;
	char *filename;
	char *remote_device;
	bt_pbap_operation_e operation;
} bt_pbap_transfer_info_t;

static GSList *transfers;

int __bt_pbap_call_get_phonebook_size(DBusGProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_phonebook(DBusGProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_vcards_list(DBusGProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_vcard(DBusGProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_search_phonebook(DBusGProxy *proxy, bt_pbap_data_t *pbap_data);

static void __bt_pbap_free_data(bt_pbap_data_t *pbap_data)
{
	g_free(pbap_data->app_param);
	g_free(pbap_data->data);
	g_free(pbap_data);
}

static bt_pbap_transfer_info_t *__bt_find_transfer_by_path(const char *transfer_path)
{
	GSList *l;
	bt_pbap_transfer_info_t *transfer;

	retv_if(transfer_path == NULL, NULL);

	for (l = transfers; l != NULL; l = l->next) {
		transfer = l->data;

		if (transfer == NULL)
			continue;

		if (g_strcmp0(transfer->path, transfer_path) == 0)
			return transfer;
	}

	return NULL;
}

static void __bt_free_transfer_info(bt_pbap_transfer_info_t *transfer_info)
{
	ret_if(transfer_info == NULL);

	g_free(transfer_info->path);
	g_free(transfer_info->filename);
	g_free(transfer_info->remote_device);
	g_free(transfer_info);
}

void _bt_pbap_obex_transfer_completed(const char *transfer_path, gboolean transfer_status)
{
	bt_pbap_transfer_info_t *transfer_info;
	int result = 0;
	int success = transfer_status;
	BT_DBG("Transfer [%s] Success [%d] \n", transfer_path, success);

	result = (success == TRUE) ? BLUETOOTH_ERROR_NONE
				: BLUETOOTH_ERROR_CANCEL;

	transfer_info = __bt_find_transfer_by_path(transfer_path);
	ret_if(transfer_info == NULL);

	BT_DBG("Remote Device [%s] FileName: [%s] Operation[%d]",
			transfer_info->remote_device, transfer_info->filename,
			transfer_info->operation);

	switch(transfer_info->operation) {
	case PULL_ALL: {
		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_PHONEBOOK_PULL,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &transfer_info->remote_device,
					DBUS_TYPE_STRING, &transfer_info->filename,
					DBUS_TYPE_INT32, &success,
					DBUS_TYPE_INVALID);
		break;
		}
	case GET_VCARD: {
		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_VCARD_PULL,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &transfer_info->remote_device,
					DBUS_TYPE_STRING, &transfer_info->filename,
					DBUS_TYPE_INT32, &success,
					DBUS_TYPE_INVALID);
		break;
		}
	default:
		BT_INFO("Case not handled");
		break;

	}
	transfers = g_slist_remove(transfers, transfer_info);
	__bt_free_transfer_info(transfer_info);
}

void _bt_obex_pbap_client_disconnect(char *path)
{
	if (g_strcmp0(g_pbap_session_path, path) == 0) {
		int result = BLUETOOTH_ERROR_INTERNAL ;

		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_DISCONNECTED,
					DBUS_TYPE_INT32, &result,
					DBUS_TYPE_STRING, &g_pbap_server_address,
					DBUS_TYPE_INVALID);

		g_free(g_pbap_session_path);
		g_free(g_pbap_server_address);
		g_object_unref(g_pbap_proxy);
		selected_path.folder = -1;
		selected_path.type = -1;
		g_pbap_session_path = NULL;
		g_pbap_server_address = NULL;
	}
	BT_DBG("-");
}

void __bt_pbap_connect_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	char *session_path = NULL;
	char *address_string = user_data;
	GError *g_error = NULL;
	int result = BLUETOOTH_ERROR_CANCEL;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
					DBUS_TYPE_G_OBJECT_PATH, &session_path,
					G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		g_pbap_session_path = g_strdup(session_path);
		BT_DBG("Session Path = %s\n", g_pbap_session_path);
		result = BLUETOOTH_ERROR_NONE;
	}

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_CONNECTED,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address_string,
				DBUS_TYPE_INVALID);

	g_pbap_server_address = g_strdup(address_string);
	g_free(address_string);
	g_free(session_path);
	BT_DBG("-");
}

int _bt_pbap_connect(const bluetooth_device_address_t *address)
{
	BT_DBG("+");
	GHashTable *hash;
	GValue *tgt_value;
	GError *error = NULL;
	char address_string[18] = { 0, };
	char *ptr = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if already connected */
	if (g_pbap_session_path)
		return BLUETOOTH_ERROR_ALREADY_CONNECT;

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
	BT_DBG("Address String: %s", address_string);
	dbus_connection = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (error != NULL) {
			BT_ERR("Couldn't connect to system bus[%s]\n", error->message);
			g_error_free(error);
			return EXIT_FAILURE;
	}
	BT_DBG("#2");
	g_pbap_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_OBEX_CLIENT_SERVICE,
			PBAP_OBEX_CLIENT_PATH,
			PBAP_OBEX_CLIENT_INTERFACE);
	if (!g_pbap_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		return -1;
	}
	BT_DBG("#3");
	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
			NULL, (GDestroyNotify)g_free);
	BT_DBG("#4");
	tgt_value = g_new0(GValue, 1);
	g_value_init(tgt_value, G_TYPE_STRING);
	g_value_set_string(tgt_value, "pbap");
	g_hash_table_insert(hash, "Target", tgt_value);
	BT_DBG("#5");

	ptr = g_strdup(address_string);
	if (!dbus_g_proxy_begin_call(g_pbap_proxy, "CreateSession",
			(DBusGProxyCallNotify)__bt_pbap_connect_cb,
			ptr, NULL,
			G_TYPE_STRING, ptr,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			hash, G_TYPE_INVALID)) {
		BT_ERR("Connect Dbus Call Error");
		g_free(ptr);
		g_object_unref(g_pbap_proxy);
		g_hash_table_destroy(hash);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_hash_table_destroy(hash);
	g_free(ptr);

	BT_DBG("-");
	return 0;
}

void __bt_pbap_disconnect_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	char *address_string = user_data;
	GError *g_error = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL ;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error, G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		g_free(g_pbap_session_path);
		g_pbap_session_path = NULL;
		result = BLUETOOTH_ERROR_NONE;
		selected_path.folder = -1;
		selected_path.type = -1;
	}

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
			BLUETOOTH_PBAP_DISCONNECTED,
			DBUS_TYPE_INT32, &result,
			DBUS_TYPE_STRING, &address_string,
			DBUS_TYPE_INVALID);

	g_object_unref(proxy);
	g_free(g_pbap_server_address);
	g_free(address_string);
	g_pbap_server_address = NULL;
	BT_DBG("-");
}

int _bt_pbap_disconnect(const bluetooth_device_address_t *address)
{
	BT_DBG("+");
	char address_string[18] = { 0, };
	char *ptr = NULL;
	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL)
		return BLUETOOTH_ERROR_NOT_CONNECTED;

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
	BT_DBG("Address String: %s", address_string);
	BT_DBG("Session Path: %s", g_pbap_session_path);

	ptr = g_strdup(address_string);
	if (!dbus_g_proxy_begin_call(g_pbap_proxy, "RemoveSession",
			(DBusGProxyCallNotify)__bt_pbap_disconnect_cb,
			ptr, NULL,
			DBUS_TYPE_G_OBJECT_PATH, g_pbap_session_path,
			G_TYPE_INVALID)) {
		g_free(ptr);
		BT_ERR("Disconnect Dbus Call Error");
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return 0;
}

void __bt_pbap_select_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;

	BT_DBG("Address = %s", address_string);
	if (dbus_g_proxy_end_call(proxy, call, &g_error, G_TYPE_INVALID)) {
		switch (pbap_data->operation) {
		case GET_SIZE: {
			__bt_pbap_call_get_phonebook_size(proxy, pbap_data);
			break;
		}
		case PULL_ALL: {
			__bt_pbap_call_get_phonebook(proxy, pbap_data);
			break;
		}
		case GET_LIST: {
			__bt_pbap_call_get_vcards_list(proxy, pbap_data);
			break;
		}
		case GET_VCARD: {
			__bt_pbap_call_get_vcard(proxy, pbap_data);
			break;
		}
		case PB_SEARCH: {
			__bt_pbap_call_search_phonebook(proxy, pbap_data);
			break;
		}
		default: {
			g_object_unref(proxy);
			__bt_pbap_free_data(pbap_data);
		}
		} // End of Case
	} else {
		BT_ERR("dbus_g_proxy_end_call failed");
		g_object_unref(proxy);
		__bt_pbap_free_data(pbap_data);
	}

	BT_DBG("-");
}


void __bt_pbap_get_phonebook_size_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	unsigned int size = 0;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
			G_TYPE_UINT, &size,
			G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		BT_ERR("Success");
		result = BLUETOOTH_ERROR_NONE;
	}
	BT_DBG("Size of Phonebook: %d", size);
	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_PHONEBOOK_SIZE,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address_string,
				DBUS_TYPE_INT32, &size,
				DBUS_TYPE_INVALID);
	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_get_phonebook_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	GHashTable *properties;
	GValue *value = { 0 };
	bt_pbap_transfer_info_t *transfer_info;
	char *transfer = NULL;
	const gchar *filename =  NULL;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
			DBUS_TYPE_G_OBJECT_PATH, &transfer,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			&properties,
			G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		if (properties != NULL) {
			value = g_hash_table_lookup(properties, "Filename");
			filename = value ? g_value_get_string(value) : NULL;
		}

		BT_DBG("Transfer Path: %s", transfer);
		BT_DBG("File Name: %s", filename);
		transfer_info = g_new0(bt_pbap_transfer_info_t, 1);
		transfer_info->path = transfer;
		transfer_info->remote_device = g_strdup(address_string);
		transfer_info->filename = (char *)filename;
		transfer_info->operation = PULL_ALL;
		transfers = g_slist_append(transfers, transfer_info);
	}

	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_get_vcard_list_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	int i;
	int result = BLUETOOTH_ERROR_INTERNAL;
	GPtrArray *vcardlist = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	char **vcard_list = NULL;
	char list_entry[PBAP_VCARDLIST_MAXLENGTH] = { 0, };
	int length = 0;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
			dbus_g_type_get_collection("GPtrArray", dbus_g_type_get_struct("GValueArray",
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID)),
				&vcardlist, G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
		vcard_list = g_new0(char *, length + 1);
	} else {
		BT_DBG("vcardlist len %d", vcardlist->len);
		length = vcardlist->len;
		result = BLUETOOTH_ERROR_NONE;

		vcard_list = g_new0(char *, length + 1);

		GValue *v = g_new0(GValue, 1);//g_ptr_array_index(vcardlist, 0);
		gchar *elname, *elval;
		g_value_init(v, dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID));
		for (i = 0; i < length; i++) {
			g_value_set_boxed(v, g_ptr_array_index(vcardlist, i));
			if (dbus_g_type_struct_get (v, 0, &elname, 1, &elval, G_MAXUINT)) {
				memset(list_entry, 0, PBAP_VCARDLIST_MAXLENGTH);
				g_snprintf (list_entry, PBAP_VCARDLIST_MAXLENGTH - 1,
						"<card handle = \"%s\" name = \"%s\"/>", elname, elval);
				//If possible send as Array of <STRING, STRING>
				BT_DBG("%s", list_entry);
				vcard_list[i] = g_strdup(list_entry);
			}
		}
	}

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_VCARD_LIST,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address_string,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&vcard_list, length,
				DBUS_TYPE_INVALID);

	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_get_vcard_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	GHashTable *properties;
	GValue *value = { 0 };
	bt_pbap_transfer_info_t *transfer_info;
	char *transfer = NULL;
	const gchar *filename =  NULL;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
			DBUS_TYPE_G_OBJECT_PATH, &transfer,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			&properties,
			G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		if (properties != NULL) {
			value = g_hash_table_lookup(properties, "Filename");
			filename = value ? g_value_get_string(value) : NULL;
		}

		BT_DBG("Transfer Path: %s", transfer);
		BT_DBG("File Name: %s", filename);
		transfer_info = g_new0(bt_pbap_transfer_info_t, 1);
		transfer_info->path = transfer;
		transfer_info->remote_device = g_strdup(address_string);
		transfer_info->filename = (char *)filename;
		transfer_info->operation = GET_VCARD;
		transfers = g_slist_append(transfers, transfer_info);
	}

	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_search_phonebook_cb(DBusGProxy *proxy,
		DBusGProxyCall *call, void *user_data)
{
	BT_DBG("+");
	GError *g_error = NULL;
	int i;
	GPtrArray *vcardlist = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	char **vcard_list = NULL;
	char list_entry[PBAP_VCARDLIST_MAXLENGTH] = { 0, };
	int length = 0;
	int result = BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("Address = %s", address_string);
	if (!dbus_g_proxy_end_call(proxy, call, &g_error,
			dbus_g_type_get_collection("GPtrArray", dbus_g_type_get_struct("GValueArray",
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID)),
				&vcardlist, G_TYPE_INVALID)) {
		BT_ERR("Error Code[%d]: Message %s \n", g_error->code, g_error->message);
		g_error_free(g_error);
	} else {
		BT_DBG("vcardlist len %d", vcardlist->len);
		length = vcardlist->len;
		result = BLUETOOTH_ERROR_NONE;

		vcard_list = g_new0(char *, length + 1);

		GValue *v = g_new0(GValue, 1);//g_ptr_array_index(vcardlist, 0);
		gchar *elname, *elval;
		g_value_init(v, dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID));
		for (i = 0; i < length; i++) {
			g_value_set_boxed(v, g_ptr_array_index(vcardlist, i));
			if (dbus_g_type_struct_get (v, 0, &elname, 1, &elval, G_MAXUINT)) {
				memset(list_entry, 0, PBAP_VCARDLIST_MAXLENGTH);
				g_snprintf (list_entry, PBAP_VCARDLIST_MAXLENGTH - 1,
						"<card handle = \"%s\" name = \"%s\"/>", elname, elval);
				//If possible send as Array of <STRING, STRING>
				BT_DBG("%s", list_entry);
				vcard_list[i] = g_strdup(list_entry);
			}
		}
	}

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_PHONEBOOK_SEARCH,
				DBUS_TYPE_INT32, &result,
				DBUS_TYPE_STRING, &address_string,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
				&vcard_list, length,
				DBUS_TYPE_INVALID);
	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

int __bt_pbap_call_get_phonebook_size(DBusGProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	if (!dbus_g_proxy_begin_call(proxy, "GetSize",
			(DBusGProxyCallNotify)__bt_pbap_get_phonebook_size_cb,
			pbap_data, NULL,
			G_TYPE_INVALID)) {
		BT_ERR("GetSize Dbus Call Error");
		__bt_pbap_free_data(pbap_data);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	return BLUETOOTH_ERROR_NONE;
	BT_DBG("-");
}

int __bt_pbap_call_get_phonebook(DBusGProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	GHashTable *filters;
	GValue *max_count;
	GValue *format;
	GValue *order;
	GValue *offset;
	char *format_str = NULL;
	char *order_str = NULL;
	char *target_file = "/opt/usr/media/Downloads/pb.vcf";
	bt_pbap_pull_parameters_t *app_param = pbap_data->app_param;

	filters = g_hash_table_new_full(g_str_hash, g_str_equal,
				NULL, (GDestroyNotify)g_free);

	/* Add Format Filter only if other than vCard 2.1 (default)*/
	if (app_param->format > 0) {
		format_str = g_strdup(FORMAT[app_param->format]);
		format = g_new0(GValue, 1);
		g_value_init(format, G_TYPE_STRING);
		g_value_set_string(format, format_str);
		g_hash_table_insert(filters, "Format", format);
	}

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		order = g_new0(GValue, 1);
		g_value_init(order, G_TYPE_STRING);
		g_value_set_string(order, order_str);
		g_hash_table_insert(filters, "Order", order);
	}

	max_count = g_new0(GValue, 1);
	g_value_init(max_count, G_TYPE_UINT);
	g_value_set_uint(max_count, app_param->maxlist);
	g_hash_table_insert(filters, "MaxCount", max_count);

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		offset = g_new0(GValue, 1);
		g_value_init(offset, G_TYPE_UINT);
		g_value_set_uint(offset, app_param->offset);
		g_hash_table_insert(filters, "Offset", offset);
	}

//****************************
// Add code for Fields
//
//****************************

	if (!dbus_g_proxy_begin_call(proxy, "PullAll",
			(DBusGProxyCallNotify)__bt_pbap_get_phonebook_cb,
			pbap_data, NULL,
			G_TYPE_STRING, target_file,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			filters,
			G_TYPE_INVALID)) {
		BT_ERR("GetSize Dbus Call Error");
		g_free(format_str);
		g_free(order_str);
		g_hash_table_destroy(filters);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_free(format_str);
	g_free(order_str);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_get_vcards_list(DBusGProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	GHashTable *filters;
	GValue *max_count;
	GValue *order;
	GValue *offset;
	char *order_str = NULL;
	char *folder = NULL;
	bt_pbap_list_parameters_t *app_param = pbap_data->app_param;

	filters = g_hash_table_new_full(g_str_hash, g_str_equal,
				NULL, (GDestroyNotify)g_free);

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		order = g_new0(GValue, 1);
		g_value_init(order, G_TYPE_STRING);
		g_value_set_string(order, order_str);
		g_hash_table_insert(filters, "Order", order);
	}

	max_count = g_new0(GValue, 1);
	g_value_init(max_count, G_TYPE_UINT);
	g_value_set_uint(max_count, app_param->maxlist);
	g_hash_table_insert(filters, "MaxCount", max_count);

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		offset = g_new0(GValue, 1);
		g_value_init(offset, G_TYPE_UINT);
		g_value_set_uint(offset, app_param->offset);
		g_hash_table_insert(filters, "Offset", offset);
	}

	folder = g_strdup(TYPE[selected_path.type]);
	BT_DBG("Folder: %s", folder);
	if (!dbus_g_proxy_begin_call(proxy, "List",
			(DBusGProxyCallNotify)__bt_pbap_get_vcard_list_cb,
			pbap_data, NULL,
			G_TYPE_STRING, folder,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			filters,
			G_TYPE_INVALID)) {
		BT_ERR("List Dbus Call Error");
		g_free(folder);
		g_free(order_str);
		g_hash_table_unref(filters);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_free(folder);
	g_free(order_str);
	g_hash_table_unref(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_get_vcard(DBusGProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	GHashTable *filters;
	GValue *format;
	char *format_str = NULL;
	char *target_file = "/opt/usr/media/Downloads/pb.vcf";
	char *vcard_handle = NULL;
	char vcard[10] = { 0, };

	bt_pbap_pull_vcard_parameters_t *app_param = pbap_data->app_param;

	filters = g_hash_table_new_full(g_str_hash, g_str_equal,
				NULL, (GDestroyNotify)g_free);

	/* Add Format Filter only if other than vCard 2.1 (default)*/
	if (app_param->format > 0) {
		format_str = g_strdup(FORMAT[app_param->format]);
		format = g_new0(GValue, 1);
		g_value_init(format, G_TYPE_STRING);
		g_value_set_string(format, format_str);
		g_hash_table_insert(filters, "Format", format);
	}


//****************************
// Add code for Fields
//
//****************************

	sprintf(vcard, "%d.vcf", app_param->index);
	BT_DBG("Handle: %s", vcard);
	vcard_handle = g_strdup(vcard);

	if (!dbus_g_proxy_begin_call(proxy, "Pull",
			(DBusGProxyCallNotify)__bt_pbap_get_vcard_cb,
			pbap_data, NULL,
			G_TYPE_STRING, vcard_handle,
			G_TYPE_STRING, target_file,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			filters,
			G_TYPE_INVALID)) {
		BT_ERR("GetSize Dbus Call Error");
		g_free(format_str);
		g_free(vcard_handle);
		g_hash_table_destroy(filters);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_free(format_str);
	g_free(vcard_handle);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_search_phonebook(DBusGProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	GHashTable *filters;
	GValue *max_count;
	GValue *order;
	GValue *offset;
	char *order_str = NULL;
	char *field = NULL;
	char *value = NULL;
	bt_pbap_search_parameters_t *app_param = pbap_data->app_param;

	filters = g_hash_table_new_full(g_str_hash, g_str_equal,
				NULL, (GDestroyNotify)g_free);

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		order = g_new0(GValue, 1);
		g_value_init(order, G_TYPE_STRING);
		g_value_set_string(order, order_str);
		g_hash_table_insert(filters, "Order", order);
	}

	max_count = g_new0(GValue, 1);
	g_value_init(max_count, G_TYPE_UINT);
	g_value_set_uint(max_count, app_param->maxlist);
	g_hash_table_insert(filters, "MaxCount", max_count);

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		offset = g_new0(GValue, 1);
		g_value_init(offset, G_TYPE_UINT);
		g_value_set_uint(offset, app_param->offset);
		g_hash_table_insert(filters, "Offset", offset);
	}

	field = g_strdup(SEARCH_FIELD[app_param->search_attribute]);
	value = g_strdup(app_param->search_value);
	if (!dbus_g_proxy_begin_call(proxy, "Search",
			(DBusGProxyCallNotify)__bt_pbap_search_phonebook_cb,
			pbap_data, NULL,
			G_TYPE_STRING, field,
			G_TYPE_STRING, value,
			dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
			filters,
			G_TYPE_INVALID)) {
		BT_ERR("List Dbus Call Error");

		g_free(field);
		g_free(value);
		g_hash_table_destroy(filters);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	g_free(field);
	g_free(value);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_pbap_get_phonebook_size(const bluetooth_device_address_t *address,
		int source, int type)
{
	BT_DBG("+");
	DBusGProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	bt_pbap_data_t *pbap_data = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL) {
		BT_ERR("NOT CONNECTED");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
			BT_DBG("Address String: %s", address_string);
	source_string = g_strdup(SOURCE[source]);
	type_string = g_strdup(TYPE[type]);

	BT_DBG("Address[%s] Source[%s] Type[%s]",
			address_string, source_string, type_string);
	BT_DBG("Session Path = %s\n", g_pbap_session_path);
	g_pbap_session_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_SESSION_SERVICE,
			g_pbap_session_path,
			PBAP_SESSION_INTERFACE);
	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = GET_SIZE;
	pbap_data->data = g_strdup(address_string);

	if (source ==  selected_path.folder && type == selected_path.type) {
		g_free(source_string);
		g_free(type_string);
		return __bt_pbap_call_get_phonebook_size(g_pbap_session_proxy, pbap_data);
	}

	if (!dbus_g_proxy_begin_call(g_pbap_session_proxy, "Select",
			(DBusGProxyCallNotify)__bt_pbap_select_cb,
			pbap_data, NULL,
			G_TYPE_STRING, source_string,
			G_TYPE_STRING, type_string,
			G_TYPE_INVALID)) {
		BT_ERR("Select Dbus Call Error");
		g_object_unref(g_pbap_session_proxy);
		__bt_pbap_free_data(pbap_data);
		g_free(source_string);
		g_free(type_string);
		return BLUETOOTH_ERROR_INTERNAL;
	}
	selected_path.folder = source;
	selected_path.type = type;

	g_free(source_string);
	g_free(type_string);
	return 0;
}

int _bt_pbap_get_phonebook(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_pull_parameters_t *app_param)
{
	BT_DBG("+");
	DBusGProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;

	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_pull_parameters_t *param = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL) {
		BT_ERR("NOT CONNECTED");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
		BT_DBG("Address String: %s", address_string);

	source_string = g_strdup(SOURCE[source]);
	type_string = g_strdup(TYPE[type]);

	BT_DBG("Address[%s] Source[%s] Type[%s]",
			address_string, source_string, type_string);

	BT_DBG("Session Path = %s\n", g_pbap_session_path);
	g_pbap_session_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_SESSION_SERVICE,
			g_pbap_session_path,
			PBAP_SESSION_INTERFACE);
	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = PULL_ALL;
	pbap_data->data = g_strdup(address_string);
	param = g_new0(bt_pbap_pull_parameters_t, 1);
	memcpy(param, app_param, sizeof(bt_pbap_pull_parameters_t));
	pbap_data->app_param = param;

	if (source ==  selected_path.folder && type == selected_path.type) {
		return __bt_pbap_call_get_phonebook(g_pbap_session_proxy, pbap_data);
	}
	if (!dbus_g_proxy_begin_call(g_pbap_session_proxy, "Select",
			(DBusGProxyCallNotify)__bt_pbap_select_cb,
			pbap_data, NULL,
			G_TYPE_STRING, source_string,
			G_TYPE_STRING, type_string,
			G_TYPE_INVALID)) {
		BT_ERR("Select Dbus Call Error");
		g_object_unref(g_pbap_session_proxy);
		__bt_pbap_free_data(pbap_data);
		g_free(source_string);
		g_free(type_string);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	selected_path.folder = source;
	selected_path.type = type;
	g_free(source_string);
	g_free(type_string);

	return 0;
}

int _bt_pbap_get_list(const bluetooth_device_address_t *address, int source,
		int type,  bt_pbap_list_parameters_t *app_param)
{
	BT_DBG("+");
	DBusGProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;

	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_list_parameters_t *param = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL) {
		BT_ERR("NOT CONNECTED");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
		BT_DBG("Address String: %s", address_string);

	source_string = g_strdup(SOURCE[source]);
	type_string = g_strdup("nil");

	BT_DBG("Address[%s] Source[%s] Type[%s]",
			address_string, source_string, type_string);

	BT_DBG("Session Path = %s\n", g_pbap_session_path);
	g_pbap_session_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_SESSION_SERVICE,
			g_pbap_session_path,
			PBAP_SESSION_INTERFACE);
	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = GET_LIST;
	pbap_data->data = g_strdup(address_string);
	param = g_new0(bt_pbap_list_parameters_t, 1);
	memcpy(param, app_param, sizeof(bt_pbap_list_parameters_t));
	pbap_data->app_param = param;

	if (source ==  selected_path.folder && type == selected_path.type) {
		return __bt_pbap_call_get_vcards_list(g_pbap_session_proxy, pbap_data);
	}
	if (!dbus_g_proxy_begin_call(g_pbap_session_proxy, "Select",
			(DBusGProxyCallNotify)__bt_pbap_select_cb,
			pbap_data, NULL,
			G_TYPE_STRING, source_string,
			G_TYPE_STRING, type_string,
			G_TYPE_INVALID)) {
		BT_ERR("Select Dbus Call Error");
		g_object_unref(g_pbap_session_proxy);
		__bt_pbap_free_data(pbap_data);
		g_free(source_string);
		g_free(type_string);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	selected_path.folder = source;
	selected_path.type = type;
	g_free(source_string);
	g_free(type_string);

	return 0;
}


int _bt_pbap_pull_vcard(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_pull_vcard_parameters_t *app_param)
{
	BT_DBG("+");
	DBusGProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_pull_vcard_parameters_t *param = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL) {
		BT_ERR("NOT CONNECTED");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
		BT_DBG("Address String: %s", address_string);

	source_string = g_strdup(SOURCE[source]);
	type_string = g_strdup(TYPE[type]);

	BT_DBG("Address[%s] Source[%s] Type[%s]",
			address_string, source_string, type_string);

	BT_DBG("Session Path = %s\n", g_pbap_session_path);
	g_pbap_session_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_SESSION_SERVICE,
			g_pbap_session_path,
			PBAP_SESSION_INTERFACE);
	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = GET_VCARD;
	pbap_data->data = g_strdup(address_string);
	param = g_new0(bt_pbap_pull_vcard_parameters_t, 1);
	memcpy(param, app_param, sizeof(bt_pbap_pull_vcard_parameters_t));
	pbap_data->app_param = param;

	if (source ==  selected_path.folder && type == selected_path.type) {
		return __bt_pbap_call_get_vcard(g_pbap_session_proxy, pbap_data);
	}
	if (!dbus_g_proxy_begin_call(g_pbap_session_proxy, "Select",
			(DBusGProxyCallNotify)__bt_pbap_select_cb,
			pbap_data, NULL,
			G_TYPE_STRING, source_string,
			G_TYPE_STRING, type_string,
			G_TYPE_INVALID)) {
		BT_ERR("Select Dbus Call Error");
		g_object_unref(g_pbap_session_proxy);
		__bt_pbap_free_data(pbap_data);
		g_free(source_string);
		g_free(type_string);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	selected_path.folder = source;
	selected_path.type = type;
	g_free(source_string);
	g_free(type_string);

	return 0;
}

int _bt_pbap_phonebook_search(const bluetooth_device_address_t *address,
		int source, int type, bt_pbap_search_parameters_t *app_param)
{
	BT_DBG("+");
	DBusGProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_search_parameters_t *param = NULL;

	BT_CHECK_PARAMETER(address, return);

	/* check if connected */
	if (g_pbap_session_path == NULL) {
		BT_ERR("NOT CONNECTED");
		return BLUETOOTH_ERROR_NOT_CONNECTED;
	}

	BT_DBG("BD Address [%2.2X %2.2X %2.2X %2.2X %2.2X %2.2X]",
			address->addr[0], address->addr[1],
			address->addr[2], address->addr[3],
			address->addr[4], address->addr[5]);

	_bt_convert_addr_type_to_string(address_string, (unsigned char *)address->addr);
		BT_DBG("Address String: %s", address_string);

	source_string = g_strdup(SOURCE[source]);
	type_string = g_strdup(TYPE[type]);

	BT_DBG("Address[%s] Source[%s] Type[%s]",
			address_string, source_string, type_string);

	BT_DBG("Session Path = %s\n", g_pbap_session_path);
	g_pbap_session_proxy = dbus_g_proxy_new_for_name(dbus_connection,
			PBAP_SESSION_SERVICE,
			g_pbap_session_path,
			PBAP_SESSION_INTERFACE);
	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = PB_SEARCH;
	pbap_data->data = g_strdup(address_string);
	param = g_new0(bt_pbap_search_parameters_t, 1);
	memcpy(param, app_param, sizeof(bt_pbap_search_parameters_t));
	pbap_data->app_param = param;

	if (source ==  selected_path.folder && type == selected_path.type) {
		return __bt_pbap_call_search_phonebook(g_pbap_session_proxy, pbap_data);
	}
	if (!dbus_g_proxy_begin_call(g_pbap_session_proxy, "Select",
			(DBusGProxyCallNotify)__bt_pbap_select_cb,
			pbap_data, NULL,
			G_TYPE_STRING, source_string,
			G_TYPE_STRING, type_string,
			G_TYPE_INVALID)) {
		BT_ERR("Select Dbus Call Error");
		__bt_pbap_free_data(pbap_data);
		g_free(source_string);
		g_free(type_string);
		g_object_unref(g_pbap_session_proxy);
		return BLUETOOTH_ERROR_INTERNAL;
	}

	selected_path.folder = source;
	selected_path.type = type;

	g_free(source_string);
	g_free(type_string);

	return 0;
}
