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
#include "bt-service-event.h"
#include "bt-service-pbap.h"
#include <glib.h>
#include <gio/gio.h>

#define	 PBAP_UUID "0000112f-0000-1000-8000-00805f9b34fb"
#define	 PBAP_OBEX_CLIENT_SERVICE "org.bluez.obex"
#define	 PBAP_OBEX_CLIENT_PATH "/org/bluez/obex"
#define	 PBAP_OBEX_CLIENT_INTERFACE "org.bluez.obex.Client1"

#define	 PBAP_SESSION_SERVICE	"org.bluez.obex"
#define	 PBAP_SESSION_INTERFACE	"org.bluez.obex.PhonebookAccess1"
#define	PBAP_VCARDLIST_MAXLENGTH 256

#define	PBAP_NUM_OF_FIELDS_ENTRY 29
#define	PBAP_FIELD_ALL (0xFFFFFFFFFFFFFFFFULL)

char *FIELDS[] = {
		"VERSION",
		"FN",
		"N",
		"PHOTO",
		"BDAY",
		"ADR",
		"LABEL",
		"TEL",
		"EMAIL",
		"MAILER",
		"TZ",
		"GEO",
		"TITLE",
		"ROLE",
		"LOGO",
		"AGENT",
		"ORG",
		"NOTE",
		"REV",
		"SOUND",
		"URL",
		"UID",
		"KEY",
		"NICKNAME",
		"CATEGORIES",
		"PROID",
		"CLASS",
		"SORT-STRING",
		"X-IRMC-CALL-DATETIME",	/* 29 */
};

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
static GDBusConnection *dbus_connection = NULL;
static GDBusProxy *g_pbap_proxy = NULL;

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

int __bt_pbap_call_get_phonebook_size(GDBusProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_phonebook(GDBusProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_vcards_list(GDBusProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_get_vcard(GDBusProxy *proxy, bt_pbap_data_t *pbap_data);
int __bt_pbap_call_search_phonebook(GDBusProxy *proxy, bt_pbap_data_t *pbap_data);

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
	GVariant *signal = NULL;
	BT_DBG("Transfer [%s] Success [%d] \n", transfer_path, success);

	result = (success == TRUE) ? BLUETOOTH_ERROR_NONE
				: BLUETOOTH_ERROR_INTERNAL;

	transfer_info = __bt_find_transfer_by_path(transfer_path);
	ret_if(transfer_info == NULL);

	BT_DBG("Remote Device [%s] FileName: [%s] Operation[%d]",
			transfer_info->remote_device, transfer_info->filename,
			transfer_info->operation);

	signal = g_variant_new("(issi)", result,
			transfer_info->remote_device,
			transfer_info->filename, success);
	switch(transfer_info->operation) {
	case PULL_ALL: {
		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_PHONEBOOK_PULL,
					signal);
		break;
		}
	case GET_VCARD: {
		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_VCARD_PULL,
					signal);
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
		int result = BLUETOOTH_ERROR_NONE;
		GVariant *signal = g_variant_new("(is)", result,
				g_pbap_server_address);

		_bt_send_event(BT_PBAP_CLIENT_EVENT,
					BLUETOOTH_PBAP_DISCONNECTED,
					signal);

		g_free(g_pbap_session_path);
		g_pbap_session_path = NULL;

		g_free(g_pbap_server_address);
		g_pbap_server_address = NULL;

		g_object_unref(g_pbap_proxy);
		g_pbap_proxy = NULL;

		selected_path.folder = -1;
		selected_path.type = -1;
	}
	BT_DBG("-");
}

void __bt_pbap_connect_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	char *session_path = NULL;
	char *address_string = user_data;
	GError *error = NULL;
	GVariant *value;
	GVariant *signal = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	BT_DBG("Address = %s", address_string);

	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
		g_object_unref(g_pbap_proxy);
		g_pbap_proxy = NULL;
	} else {
		g_variant_get(value, "(&o)", &session_path);

		g_pbap_session_path = g_strdup(session_path);
		BT_DBG("Session Path = %s\n", g_pbap_session_path);
		result = BLUETOOTH_ERROR_NONE;
		g_pbap_server_address = g_strdup(address_string);
	}

	signal = g_variant_new("(is)", result, address_string);

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_CONNECTED,
				signal);

	g_free(address_string);
	BT_DBG("-");
}

int _bt_pbap_connect(const bluetooth_device_address_t *address)
{
	BT_DBG("+");
	GError *error = NULL;
	char address_string[18] = { 0, };
	char *ptr = NULL;
	GVariantBuilder builder;
	GVariant *args;

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
	dbus_connection = _bt_get_session_gconn();
	if (dbus_connection == NULL) {
			BT_ERR("Couldn't connect to system bus");
			return EXIT_FAILURE;
	}

	g_pbap_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_OBEX_CLIENT_SERVICE, PBAP_OBEX_CLIENT_PATH,
			PBAP_OBEX_CLIENT_INTERFACE, NULL, &error);
	if (!g_pbap_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (error) {
			ERR("Unable to create proxy: %s", error->message);
			g_clear_error(&error);
		}
		return -1;
	}

	/* Create Hash*/
	g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add(&builder, "{sv}", "Target",
					g_variant_new("s", "pbap"));
	args = g_variant_builder_end(&builder);

	ptr = g_strdup(address_string);

	GVariant *temp = g_variant_new("(s@a{sv})", ptr, args);

	g_dbus_proxy_call(g_pbap_proxy, "CreateSession",
			temp,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_connect_cb, ptr);
	g_free(ptr);

	BT_DBG("-");
	return 0;
}

void __bt_pbap_disconnect_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	char *address_string = user_data;
	GError *error = NULL;
	GVariant *value;
	GVariant *signal = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL ;

	BT_DBG("Address = %s", address_string);

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	BT_DBG("Address = %s", address_string);

	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		g_object_unref(g_pbap_proxy);
		g_pbap_proxy = NULL;

		g_free(g_pbap_session_path);
		g_pbap_session_path = NULL;

		g_free(g_pbap_server_address);
		g_pbap_server_address = NULL;

		result = BLUETOOTH_ERROR_NONE;
		selected_path.folder = -1;
		selected_path.type = -1;
	}

	signal = g_variant_new("(is)", result, address_string);
	_bt_send_event(BT_PBAP_CLIENT_EVENT,
			BLUETOOTH_PBAP_DISCONNECTED,
			signal);

	g_free(address_string);
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

	g_dbus_proxy_call(g_pbap_proxy, "RemoveSession",
			g_variant_new("(o)", g_pbap_session_path),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_disconnect_cb, ptr);

	return 0;
}

void __bt_pbap_select_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	GVariant *value;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;

	BT_DBG("Address = %s", address_string);

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}

		selected_path.folder = -1;
		selected_path.type = -1;

		g_object_unref(proxy);
		__bt_pbap_free_data(pbap_data);
		return;
	}

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
		selected_path.folder = -1;
		selected_path.type = -1;
		g_object_unref(proxy);
		__bt_pbap_free_data(pbap_data);
	}
	} // End of Case

	g_variant_unref(value);
	BT_DBG("-");
}


void __bt_pbap_get_phonebook_size_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	int result = BLUETOOTH_ERROR_INTERNAL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	unsigned short int size = 0;
	GVariant *value;
	GVariant *signal = NULL;

	BT_DBG("Address = %s", address_string);
	value = g_dbus_proxy_call_finish(proxy, res, &error);

	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		g_variant_get(value, "(q)", &size);
		result = BLUETOOTH_ERROR_NONE;
	}

	BT_DBG("Size of Phonebook: %d", size);

	signal = g_variant_new("(isi)", result, address_string, size);
	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_PHONEBOOK_SIZE,
				signal);

	g_variant_unref(value);
	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_get_phonebook_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	bt_pbap_transfer_info_t *transfer_info;
	char *transfer = NULL;
	const gchar *filename =  NULL;
	GVariant *value;
	GVariant *properties;

	BT_DBG("Address = %s", address_string);
	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		g_variant_get(value, "(o@a{sv})", &transfer, &properties);

		if (g_variant_lookup(properties, "Filename", "s", &filename) == FALSE)
			filename = NULL;

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

void __bt_pbap_get_vcard_list_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	int i;
	int result = BLUETOOTH_ERROR_INTERNAL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	char **vcard_list = NULL;
	char list_entry[PBAP_VCARDLIST_MAXLENGTH] = { 0, };
	int length = 0;
	GVariant *value;
	GVariant *signal = NULL;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		result = BLUETOOTH_ERROR_NONE;
		gchar *elname, *elval;

		GVariantIter iter;
		GVariant *child = NULL;
		GVariant *value1 = NULL;

		g_variant_get(value ,"(@a(ss))", &value1); /* Format for value1 a(ss)*/
		gsize items = g_variant_iter_init (&iter, value1);
		vcard_list = g_new0(char *, items + 1);

		for (i = 0; (child = g_variant_iter_next_value (&iter)) != NULL; i++) {
			g_variant_get(child ,"(&s&s)", &elname, &elval);

			memset(list_entry, 0, PBAP_VCARDLIST_MAXLENGTH);
			g_snprintf (list_entry, PBAP_VCARDLIST_MAXLENGTH - 1,
					"<card handle = \"%s\" name = \"%s\"/>", elname, elval);
			//If possible send as Array of <STRING, STRING>
			BT_DBG("%s", list_entry);
			vcard_list[i] = g_strdup(list_entry);
			g_variant_unref(child);
		}

		length = i;
		g_variant_unref(value1);
		g_variant_unref(value);
	}

	BT_DBG("Address = %s", address_string);
	GVariant *temp = g_variant_new_strv((const gchar * const*)vcard_list, length);
	signal = g_variant_new("(isv)", result, address_string, temp);

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
			BLUETOOTH_PBAP_VCARD_LIST,
			signal);

	for (i = 0; i < length; i++)
		g_free(vcard_list[i]);

	g_free(vcard_list);
	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_get_vcard_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	bt_pbap_transfer_info_t *transfer_info;
	char *transfer = NULL;
	const gchar *filename =  NULL;
	GVariant *value;
	GVariant *properties;

	BT_DBG("Address = %s", address_string);
	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		g_variant_get(value, "(o@a{sv})", &transfer, &properties);

		if (g_variant_lookup (properties, "Filename", "s", &filename) == FALSE)
			filename = NULL;

		BT_DBG("Transfer Path: %s", transfer);
		BT_DBG("File Name: %s", filename);
		transfer_info = g_new0(bt_pbap_transfer_info_t, 1);
		transfer_info->path = transfer;
		transfer_info->remote_device = g_strdup(address_string);
		transfer_info->filename = (char *)filename;
		transfer_info->operation = GET_VCARD;
		transfers = g_slist_append(transfers, transfer_info);

		g_variant_unref(properties);
		g_variant_unref(value);
	}

	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

void __bt_pbap_search_phonebook_cb(GDBusProxy *proxy,
		GAsyncResult *res, gpointer user_data)
{
	BT_DBG("+");
	GError *error = NULL;
	int i;
	bt_pbap_data_t *pbap_data = user_data;
	char *address_string = pbap_data->data;
	char **vcard_list = NULL;
	char list_entry[PBAP_VCARDLIST_MAXLENGTH] = { 0, };
	int length = 0;
	int result = BLUETOOTH_ERROR_INTERNAL;
	GVariant *value;
	GVariant *signal = NULL;

	value = g_dbus_proxy_call_finish(proxy, res, &error);
	if (value == NULL) {
		BT_ERR("g_dbus_proxy_call_finish failed");
		if (error) {
			BT_ERR("errCode[%x], message[%s]\n",
					error->code, error->message);
			g_clear_error(&error);
		}
	} else {
		result = BLUETOOTH_ERROR_NONE;
		gchar *elname, *elval;

		GVariantIter iter;
		GVariant *child = NULL;
		GVariant *value1 = NULL;

		g_variant_get(value ,"(@a(ss))", &value1);
		gsize items = g_variant_iter_init (&iter, value1);
		vcard_list = g_new0(char *, items + 1);

		for (i = 0; (child = g_variant_iter_next_value (&iter)) != NULL; i++) {
			g_variant_get(child, "(&s&s)", &elname, &elval);

			memset(list_entry, 0, PBAP_VCARDLIST_MAXLENGTH);
			g_snprintf (list_entry, PBAP_VCARDLIST_MAXLENGTH - 1,
					"<card handle = \"%s\" name = \"%s\"/>", elname, elval);
			//If possible send as Array of <STRING, STRING>
			BT_DBG("%s", list_entry);
			vcard_list[i] = g_strdup(list_entry);

			g_variant_unref(child);
		}
		length = i;
		g_variant_unref(value1);
		g_variant_unref(value);
	}

	BT_DBG("Address = %s", address_string);

	signal = g_variant_new("(is@as)", result, address_string,
			g_variant_new_strv((const gchar * const*)vcard_list, length));

	_bt_send_event(BT_PBAP_CLIENT_EVENT,
				BLUETOOTH_PBAP_PHONEBOOK_SEARCH,
				signal);

	for (i = 0; i < length; i++)
		g_free(vcard_list[i]);

	g_free(vcard_list);
	g_object_unref(proxy);
	__bt_pbap_free_data(pbap_data);
	BT_DBG("-");
}

int __bt_pbap_call_get_phonebook_size(GDBusProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");

	g_dbus_proxy_call(proxy, "GetSize",
			NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_get_phonebook_size_cb,
			pbap_data);

	return BLUETOOTH_ERROR_NONE;
	BT_DBG("-");
}

int __bt_pbap_call_get_phonebook(GDBusProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");

	int i;
	char *format_str = NULL;
	char *fields_str = NULL;
	char *order_str = NULL;
	char *target_file = "/opt/usr/media/Downloads/pb.vcf";
	bt_pbap_pull_parameters_t *app_param = pbap_data->app_param;
	GVariantBuilder builder;
	GVariantBuilder inner_builder;
	GVariant *filters;

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_init (&inner_builder, G_VARIANT_TYPE_ARRAY);

	/* Add MaxlistCount*/
	g_variant_builder_add(&builder, "{sv}", "MaxCount",
					g_variant_new("u",app_param->maxlist));

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		g_variant_builder_add(&builder, "{sv}", "Order",
				g_variant_new("s",order_str));
	}

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		g_variant_builder_add(&builder, "{sv}", "Offset",
						g_variant_new("u",app_param->offset));
	}

	/* Add Format Filter only if other than vCard 2.1 (default)*/
	if (app_param->format > 0) {
		format_str = g_strdup(FORMAT[app_param->format]);
		g_variant_builder_add(&builder, "{sv}", "Format",
							g_variant_new("s", format_str));
	}

	/* Add Filter AttributeMask (64bit) */
	if (app_param->fields > 0) {
		if (app_param->fields == PBAP_FIELD_ALL) {
			BT_DBG("** CHECKED ALL **");
			fields_str = g_strdup("ALL");
			g_variant_builder_add(&inner_builder, "s", fields_str);
			g_free(fields_str);
		} else {
			for (i = 0; i < PBAP_NUM_OF_FIELDS_ENTRY; i++) {
				if (app_param->fields & (1ULL << i)) {
					BT_DBG("** CHECKED[%d]", i);
					fields_str = g_strdup(FIELDS[i]);
					g_variant_builder_add(&inner_builder, "s", fields_str);
					g_free(fields_str);
				}
			}
		}

		g_variant_builder_add(&builder, "{sv}", "Fields",
			g_variant_new("as", &inner_builder));
	}

	filters = g_variant_builder_end(&builder);

//****************************
// Add code for Fields
//
//****************************

	g_dbus_proxy_call(proxy, "PullAll",
			g_variant_new("(s@a{sv})", target_file, filters),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_get_phonebook_cb,
			pbap_data);

	g_free(format_str);
	g_free(order_str);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_get_vcards_list(GDBusProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");
	char *order_str = NULL;
	char *folder = NULL;
	GVariantBuilder builder;
	GVariant *filters;

	bt_pbap_list_parameters_t *app_param = pbap_data->app_param;

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

	/* Add MaxlistCount*/
	g_variant_builder_add(&builder, "{sv}", "MaxCount",
					g_variant_new("u",app_param->maxlist));

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		g_variant_builder_add(&builder, "{sv}", "Order",
				g_variant_new("s",order_str));
	}

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		g_variant_builder_add(&builder, "{sv}", "Offset",
						g_variant_new("u",app_param->offset));
	}

	filters = g_variant_builder_end(&builder);

	folder = g_strdup(TYPE[selected_path.type]);
	BT_DBG("Folder: %s", folder);


	g_dbus_proxy_call(proxy, "List",
			g_variant_new("(s@a{sv})", folder, filters),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_get_vcard_list_cb,
			pbap_data);

	g_free(folder);
	g_free(order_str);
	g_hash_table_unref(filters);
	/* In _bt_pbap_get_list(), path(type) is set to "nil", but current type is not null.
	     The path should be reset here */
	selected_path.type = -1;

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_get_vcard(GDBusProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");

	int i;
	char *format_str = NULL;
	char *fields_str = NULL;
	char *target_file = "/opt/usr/media/Downloads/pb.vcf";
	char *vcard_handle = NULL;
	char vcard[10] = { 0, };
	GVariantBuilder builder;
	GVariantBuilder inner_builder;
	GVariant *filters;
	bt_pbap_pull_vcard_parameters_t *app_param = pbap_data->app_param;

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_init (&inner_builder, G_VARIANT_TYPE_ARRAY);

	/* Add Format Filter only if other than vCard 2.1 (default)*/
//	if (app_param->format > 0) {
		format_str = g_strdup(FORMAT[app_param->format]);
		g_variant_builder_add(&builder, "{sv}", "Format",
							g_variant_new("s", format_str));
//	}

	/* Add Filter AttributeMask (64bit) */
	if (app_param->fields > 0) {
		if (app_param->fields == PBAP_FIELD_ALL) {
			BT_DBG("** CHECKED ALL **");
			fields_str = g_strdup("ALL");
			g_variant_builder_add(&inner_builder, "s", fields_str);
			g_free(fields_str);
		} else {
			for (i = 0; i < PBAP_NUM_OF_FIELDS_ENTRY; i++) {
				if (app_param->fields & (1ULL << i)) {
					BT_DBG("** CHECKED[%d]", i);
					fields_str = g_strdup(FIELDS[i]);
					g_variant_builder_add(&inner_builder, "s", fields_str);
					g_free(fields_str);
				}
			}
		}

		g_variant_builder_add(&builder, "{sv}", "Fields",
			g_variant_new("as", &inner_builder));
	}

	filters = g_variant_builder_end(&builder);

//****************************
// Add code for Fields
//
//****************************

	sprintf(vcard, "%d.vcf", app_param->index);
	BT_DBG("Handle: %s", vcard);
	vcard_handle = g_strdup(vcard);
	BT_DBG("vcard_handle: %s", vcard_handle);
	GVariant *temp = g_variant_new("(ss@a{sv})", vcard_handle, target_file, filters);

	g_dbus_proxy_call(proxy, "Pull",
			temp,
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_get_vcard_cb,
			pbap_data);

	g_free(format_str);
	g_free(vcard_handle);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int __bt_pbap_call_search_phonebook(GDBusProxy *proxy, bt_pbap_data_t *pbap_data)
{
	BT_DBG("+");

	char *order_str = NULL;
	char *field = NULL;
	char *value = NULL;
	bt_pbap_search_parameters_t *app_param = pbap_data->app_param;
	GVariantBuilder builder;
	GVariant *filters;

	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);

	/* Add MaxlistCount*/
	g_variant_builder_add(&builder, "{sv}", "MaxCount",
					g_variant_new("u",app_param->maxlist));

	/* Add Order Filter only if other than Indexed (default)*/
	if (app_param->order > 0) {
		order_str = g_strdup(ORDER[app_param->order]);
		g_variant_builder_add(&builder, "{sv}", "Order",
				g_variant_new("s",order_str));
	}

	/* Add Offset Filter only if other than 0 (default)*/
	if (app_param->offset > 0) {
		g_variant_builder_add(&builder, "{sv}", "Offset",
						g_variant_new("u",app_param->offset));
	}

	filters = g_variant_builder_end(&builder);

	field = g_strdup(SEARCH_FIELD[app_param->search_attribute]);
	value = g_strdup(app_param->search_value);

	g_dbus_proxy_call(proxy, "Search",
			g_variant_new("(ss@a{sv})", field, value, filters),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_search_phonebook_cb,
			pbap_data);

	g_free(value);
	g_free(order_str);
	g_free(field);
	g_hash_table_destroy(filters);

	BT_DBG("-");
	return BLUETOOTH_ERROR_NONE;
}

int _bt_pbap_get_phonebook_size(const bluetooth_device_address_t *address,
		int source, int type)
{
	BT_DBG("+");
	GDBusProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	GError *err = NULL;
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
	g_pbap_session_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_SESSION_SERVICE, g_pbap_session_path,
			PBAP_SESSION_INTERFACE, NULL, &err);

	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (err) {
			ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	BT_DBG("Prepare PBAP data");
	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = GET_SIZE;
	pbap_data->data = g_strdup(address_string);

	if (source ==  selected_path.folder && type == selected_path.type) {
		BT_DBG("Call get_phonebook_size directly");
		g_free(source_string);
		g_free(type_string);
		return __bt_pbap_call_get_phonebook_size(g_pbap_session_proxy, pbap_data);
	}

	BT_DBG("Call SELECT");
	g_dbus_proxy_call(g_pbap_session_proxy, "Select",
			g_variant_new("(ss)", source_string, type_string),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_select_cb,
			pbap_data);

	BT_DBG("Set Folders");
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
	GDBusProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	GError *err = NULL;

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
	g_pbap_session_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_SESSION_SERVICE, g_pbap_session_path,
			PBAP_SESSION_INTERFACE, NULL, &err);

	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (err) {
			ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
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

	g_dbus_proxy_call(g_pbap_session_proxy, "Select",
			g_variant_new("(ss)", source_string, type_string),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_select_cb,
			pbap_data);

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
	GDBusProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	GError *err = NULL;

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
	g_pbap_session_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_SESSION_SERVICE, g_pbap_session_path,
			PBAP_SESSION_INTERFACE, NULL, &err);

	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (err) {
			ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
		g_free(source_string);
		g_free(type_string);
		return -1;
	}

	BT_DBG("Set PBAP Data");
	pbap_data = g_new0(bt_pbap_data_t, 1);
	pbap_data->operation = GET_LIST;
	pbap_data->data = g_strdup(address_string);
	param = g_new0(bt_pbap_list_parameters_t, 1);
	memcpy(param, app_param, sizeof(bt_pbap_list_parameters_t));
	pbap_data->app_param = param;

	/* Always Call Select for vCardListing
	if (source ==  selected_path.folder && type == selected_path.type) {
		BT_DBG("Call Directly");
		return __bt_pbap_call_get_vcards_list(g_pbap_session_proxy, pbap_data);
	} */
	BT_DBG("Call SELECT");
	g_dbus_proxy_call(g_pbap_session_proxy, "Select",
			g_variant_new("(ss)", source_string, type_string),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_select_cb,
			pbap_data);
	BT_DBG("Set Folders");
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
	GDBusProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_pull_vcard_parameters_t *param = NULL;
	GError *err = NULL;

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
	g_pbap_session_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_SESSION_SERVICE, g_pbap_session_path,
			PBAP_SESSION_INTERFACE, NULL, &err);

	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (err) {
			ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
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

	g_dbus_proxy_call(g_pbap_session_proxy, "Select",
			g_variant_new("(ss)", source_string, type_string),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_select_cb,
			pbap_data);

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
	GDBusProxy *g_pbap_session_proxy = NULL;
	char address_string[18] = { 0, };
	char *source_string = NULL;
	char *type_string = NULL;
	bt_pbap_data_t *pbap_data = NULL;
	bt_pbap_search_parameters_t *param = NULL;
	GError *err = NULL;

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

	g_pbap_session_proxy =  g_dbus_proxy_new_sync(dbus_connection,
			G_DBUS_PROXY_FLAGS_NONE, NULL,
			PBAP_SESSION_SERVICE, g_pbap_session_path,
			PBAP_SESSION_INTERFACE, NULL, &err);

	if (!g_pbap_session_proxy) {
		BT_ERR("Failed to get a proxy for D-Bus\n");
		if (err) {
			ERR("Unable to create proxy: %s", err->message);
			g_clear_error(&err);
		}
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

	/* Call Select for vCardListing
	if (source ==  selected_path.folder && type == selected_path.type) {
		return __bt_pbap_call_search_phonebook(g_pbap_session_proxy, pbap_data);
	}*/

	g_dbus_proxy_call(g_pbap_session_proxy, "Select",
			g_variant_new("(ss)", source_string, type_string),
			G_DBUS_CALL_FLAGS_NONE, -1, NULL,
			(GAsyncReadyCallback)__bt_pbap_select_cb,
			pbap_data);

	selected_path.folder = source;
	selected_path.type = type;

	g_free(source_string);
	g_free(type_string);

	return 0;
}

