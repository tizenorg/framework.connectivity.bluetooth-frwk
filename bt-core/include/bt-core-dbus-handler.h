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


#ifndef _BT_CORE_DBUS_HANDLER_H_
#define _BT_CORE_DBUS_HANDLER_H_

#include <sys/types.h>
#include <sys/wait.h>
#include <dlog.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_FRWK_CORE"

#define BT_DBG(fmt, args...) \
        SLOGD(fmt, ##args)
#define BT_INFO(fmt, args...) \
	SLOGI(fmt, ##args)
#define BT_ERR(fmt, args...) \
        SLOGE(fmt, ##args)

#define BT_CORE_NAME "org.projectx.bt_core"
#define BT_CORE_PATH "/org/projectx/bt_core"

typedef struct _BtCore
{
    GObject object;
} BtCore;

typedef struct _BtCoreClass
{
    GObjectClass object_class;
} BtCoreClass;


DBusGProxy *_bt_core_register_event_filter(DBusGConnection *g_conn, BtCore *bt_core);
void _bt_unregister_event_filter(DBusGConnection *g_conn, BtCore *bt_core, DBusGProxy *dbus_proxy);

int _bt_core_service_request(int service_type, int service_function,
			GArray *in_param1, GArray *in_param2,
			GArray *in_param3, GArray *in_param4,
			GArray **out_param1);
void _bt_core_fill_garray_from_variant(GVariant *var, GArray *param);
GDBusProxy *_bt_core_gdbus_get_service_proxy(void);
void _bt_core_gdbus_deinit_proxys(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_CORE_DBUS_HANDLER_H_*/
