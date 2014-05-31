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


#ifndef _BT_CORE_H_
#define _BT_CORE_H_

#include <sys/types.h>
#include <dlog.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
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

#define BT_CORE_TYPE (bt_core_get_type())

typedef struct _BtCore
{
    GObject object;
} BtCore;

typedef struct _BtCoreClass
{
    GObjectClass object_class;
} BtCoreClass;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_CORE_H_*/
