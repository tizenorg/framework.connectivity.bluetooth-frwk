/*
 * Bluetooth-hfp-agent
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  	Hocheol Seo <hocheol.seo@samsung.com>
 *		Girishashok Joshi <girish.joshi@samsung.com>
 *		Chanyeol Park <chanyeol.park@samsung.com>
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

#ifndef __DEF_BT_HFP_AGENT_H_
#define __DEF_BT_HFP_AGENT_H_

#include <unistd.h>
#include <dlog.h>
#include <stdio.h>

#define BT_HFP_AGENT_ERROR (__bt_hfp_agent_error_quark())

typedef enum {
	BT_HFP_AGENT_NETWORK_REG_STATUS_HOME,
	BT_HFP_AGENT_NETWORK_REG_STATUS_ROAMING,
	BT_HFP_AGENT_NETWORK_REG_STATUS_OFFLINE,
	BT_HFP_AGENT_NETWORK_REG_STATUS_SEARCHING,
	BT_HFP_AGENT_NETWORK_REG_STATUS_NO_SIM,
	BT_HFP_AGENT_NETWORK_REG_STATUS_POWEROFF,
	BT_HFP_AGENT_NETWORK_REG_STATUS_POWERSAFE,
	BT_HFP_AGENT_NETWORK_REG_STATUS_NO_COVERAGE,
	BT_HFP_AGENT_NETWORK_REG_STATUS_REJECTED,
	BT_HFP_AGENT_NETWORK_REG_STATUS_UNKOWN,
} bt_hfp_agent_network_registration_status_t;

typedef enum {
	BT_HFP_AGENT_ERROR_INTERNAL,
	BT_HFP_AGENT_ERROR_NOT_AVAILABLE,
	BT_HFP_AGENT_ERROR_NOT_CONNECTED,
	BT_HFP_AGENT_ERROR_BUSY,
	BT_HFP_AGENT_ERROR_INVALID_PARAM,
	BT_HFP_AGENT_ERROR_ALREADY_EXSIST,
	BT_HFP_AGENT_ERROR_ALREADY_CONNECTED,
	BT_HFP_AGENT_ERROR_NO_MEMORY,
	BT_HFP_AGENT_ERROR_I_O_ERROR,
	BT_HFP_AGENT_ERROR_OPERATION_NOT_AVAILABLE,
	BT_HFP_AGENT_ERROR_NO_CALL_LOGS,
	BT_HFP_AGENT_ERROR_INVALID_MEMORY_INDEX,
	BT_HFP_AGENT_ERROR_INVALID_CHLD_INDEX,
	BT_HFP_AGENT_ERROR_BATTERY_STATUS,
	BT_HFP_AGENT_ERROR_SIGNAL_STATUS,
	BT_HFP_AGENT_ERROR_NOT_SUPPORTED,
	BT_HFP_AGENT_ERROR_INVALID_NUMBER,
	BT_HFP_AGENT_ERROR_APPLICATION,
	BT_HFP_AGENT_ERROR_INVALID_DTMF,
	BT_HFP_AGENT_ERROR_NONE,
} bt_hfp_agent_error_t;

#define BT_HFP_SERVICE_OBJECT_PATH "/org/bluez/hfp_agent"
#define BT_HFP_SERVICE "org.bluez.hfp_agent"

#define BT_HFP "BT_HFP"
#define DBG(fmt, args...) SLOG(LOG_DEBUG, BT_HFP, "%s():%d "fmt,\
						__func__, __LINE__, ##args)
#define ERR(fmt, args...) SLOG(LOG_ERROR, BT_HFP, "%s():%d "fmt,\
						__func__, __LINE__, ##args)
#endif /* __DEF_BT_HFP_AGENT_H_ */
