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


#ifndef _BT_CORE_COMMON_H_
#define _BT_CORE_COMMON_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef LOG_TAG
#define LOG_TAG "BLUETOOTH_FRWK_CORE"

#define LOG_COLOR_RESET    "\033[0m"
#define LOG_COLOR_RED      "\033[31m"
#define LOG_COLOR_YELLOW   "\033[33m"
#define LOG_COLOR_GREEN         "\033[32m"
#define LOG_COLOR_BLUE          "\033[36m"
#define LOG_COLOR_PURPLE   "\033[35m"

#define BT_DBG(fmt, args...) \
        SLOGD(fmt, ##args)
#define BT_INFO(fmt, args...) \
	SLOGI(fmt, ##args)
#define BT_ERR(fmt, args...) \
        SLOGE(fmt, ##args)

#define BT_INFO_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_GREEN""fmt""LOG_COLOR_RESET, ##arg)
#define BT_ERR_C(fmt, arg...) \
	SLOGI_IF(TRUE,  LOG_COLOR_RED""fmt""LOG_COLOR_RESET, ##arg)

#define DBG_SECURE(fmt, args...) SECURE_SLOGD(fmt, ##args)
#define ERR_SECURE(fmt, args...) SECURE_SLOGE(fmt, ##args)

#define ret_if(expr) \
	do { \
		if (expr) { \
			BT_ERR("(%s) return", #expr); \
			return; \
		} \
	} while (0)

#define retv_if(expr, val) \
	do { \
		if (expr) { \
			BT_ERR("(%s) return", #expr); \
			return (val); \
		} \
	} while (0)

#define BT_FREE_PARAMS(IP1,IP2,IP3,IP4,OP) \
	do { \
		if (IP1) \
			g_array_free(IP1, TRUE); \
		if (IP2) \
			g_array_free(IP2, TRUE); \
		if (IP3) \
			g_array_free(IP3, TRUE); \
		if (IP4) \
			g_array_free(IP4, TRUE); \
		if (OP) \
			g_array_free(OP, TRUE); \
	} while (0)

#define BT_ALLOC_PARAMS(IP1,IP2,IP3,IP4,OP ) \
	do { \
	        IP1 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP2 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP3 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	        IP4 = g_array_new(TRUE, TRUE, sizeof(gchar));	\
	} while (0)

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /*_BT_CORE_COMMON_H_*/

