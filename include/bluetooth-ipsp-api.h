/*
 * Bluetooth-frwk
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Paras Kumar <paras.kumar@samsung.com>
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

#ifndef __BLUETOOTH_LE_IPSP_API_H
#define __BLUETOOTH_LE_IPSP_API_H

#ifdef __cplusplus
extern "C" {
#endif


#define IPSP_ADDRESS_LENGTH 6

#define IPSP_ERROR_NONE			((int)0)

#define IPSP_ERROR_BASE			((int)0)
#define IPSP_ERROR_ALREADY_INITIALIZED	((int)IPSP_ERROR_BASE - 0x01)
#define IPSP_ERROR_NOT_INITIALIZED	((int)IPSP_ERROR_BASE - 0x01)
#define IPSP_ERROR_NOT_PAIRED		((int)IPSP_ERROR_BASE - 0x02)
#define IPSP_ERROR_INTERNAL		((int)IPSP_ERROR_BASE - 0x03)
#define IPSP_ERROR_INVALID_PARAM		((int)IPSP_ERROR_BASE - 0x04)
#define IPSP_ERROR_NOT_ENABLED		((int)IPSP_ERROR_BASE - 0x05)
#define IPSP_ERROR_CONNECTION_FAILED	((int)IPSP_ERROR_BASE - 0x06)
#define IPSP_ERROR_PERMISSION_DEINED    ((int)BLUETOOTH_ERROR_BASE - 0x07)

typedef struct {
	int event;
	int result;
	void *param_data;
	void *user_data;
} ipsp_event_param_t;

typedef void (*ipsp_cb_func_ptr)(int, ipsp_event_param_t *, void *);

typedef struct {
	unsigned char addr[IPSP_ADDRESS_LENGTH];
} ipsp_device_address_t;

/**
 * @fn int bluetooth_le_ipsp_init(void)
 *
 * @brief Initializes IPSP service and register the service to Bluez
 *
 * This function is a synchronous call.
 * The IPSP Initialize request is responded by BLUETOOTH_EVENT_IPSP_INIT_STATE_CHANGED event.
 *
 * @return  IPSP_ERROR_NONE  - Success \n
 *              IPSP_ERROR_ALREADY_INITIALIZED - Aready Initialized \n
 *              IPSP_ERROR_INTERNAL - Internal Error \n
 *              IPSP_ERROR_NOT_ENABLED - Not enabled \n
 *
 * @exception   None
 * @param[in]   None
 * @remark      None
 *
 */
int bluetooth_le_ipsp_init(void);

/**
 * @fn int bluetooth_le_ipsp_deinit(void)
 *
 * @brief De-Initialize IPSP service and un-register the callback
 * The IPSP De-Initialize request is responded by BLUETOOTH_EVENT_IPSP_INIT_STATE_CHANGED event.
 *
 * This function is a synchronous call.
 *
 * @return  IPSP_ERROR_NONE  - Success \n
 *              IPSP_ERROR_NOTY_INITIALIZED - Aready Initialized \n
 *              IPSP_ERROR_INTERNAL - Internal Error \n
 *              IPSP_ERROR_NOT_ENABLED - Not enabled \n
 *
 * @remark      None
 *
 */
int bluetooth_le_ipsp_deinit(void);

/**
 * @fn int bluetooth_le_ipsp_connect(const ipsp_device_address_t *device_address);
 *
 * @brief Connects to IPSP Router device. It establishes connection for 6Lowpan over LE.
 *
 * This function is a asynchronous call.
 *
 * @return	 IPSP_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth Not enabled \n
 *
 * @exception	None
 * @param[in]	device_address - remote device address.
 *
 * @remark	None
 * @see 	bluetooth_le_ipsp_disconnect()
 */
int bluetooth_le_ipsp_connect(const ipsp_device_address_t *device_address);

/**
 * @fn int bluetooth_le_ipsp_disconnect(const ipsp_device_address_t *device_address);
 *
 * @brief Disconnects to IPSP Router device. It disconnects connection for 6Lowpan over LE.
 *
 * This function is a asynchronous call.
 *
 * @return	 IPSP_ERROR_NONE  - Success \n
 *		BLUETOOTH_ERROR_INVALID_PARAM -Invalid Parameters \n
 *		BLUETOOTH_ERROR_DEVICE_NOT_ENABLED - Bluetooth Not enabled \n
 *
 * @exception	None
 * @param[in]	device_address - remote device address.
 *
 * @remark	None
 * @see 	bluetooth_le_ipsp_disconnect()
 */
int bluetooth_le_ipsp_disconnect(const ipsp_device_address_t *device_address);


#ifdef __cplusplus
}
#endif
#endif /* __BLUETOOTH_LE_IPSP_API_H */

