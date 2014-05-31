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

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "bt-common.h"
#include "bluetooth-scmst-api.h"


#define BT_CONTENT_PROTECTION_PATH "/org/tizen/bluetooth/a2dpcontentprotection"
#define BT_CONTENT_PROTECTION_INTERFACE "org.tizen.bluetooth.A2dpContentProtection"

BT_EXPORT_API int bluetooth_a2dp_set_content_protection(gboolean status)
{
	DBusConnection *conn;
	DBusMessage *signal;

	BT_DBG("+\n");

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (conn == NULL)
		return BLUETOOTH_ERROR_INTERNAL;

	BT_DBG("Content Protection status = [%d] \n", status);

	/*Emit Content protection Status change signal with value*/
	signal = dbus_message_new_signal(BT_CONTENT_PROTECTION_PATH,
					BT_CONTENT_PROTECTION_INTERFACE,
					"ProtectionRequired");
	if (!signal)
		goto err;

	if (!dbus_message_append_args(signal,
				DBUS_TYPE_BOOLEAN, &status,
				DBUS_TYPE_INVALID)) {
		BT_DBG("Signal appending failed\n");
		dbus_message_unref(signal);
		goto err;
	}

	dbus_connection_send(conn, signal, NULL);
	dbus_message_unref(signal);
	dbus_connection_unref(conn);

	BT_DBG("-\n");
	return BLUETOOTH_ERROR_NONE;

err:
	dbus_connection_unref(conn);
	return BLUETOOTH_ERROR_INTERNAL;
}

