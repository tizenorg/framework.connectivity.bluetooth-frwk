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

#ifndef __BLUETOOTH_HID_API_H
#define __BLUETOOTH_HID_API_H

#ifdef __cplusplus
extern "C" {
#endif


#define HID_ADDRESS_LENGTH 6

#define HID_ERROR_NONE			((int)0)

#define HID_ERROR_BASE			((int)0)
#define HID_ERROR_ALREADY_INITIALIZED	((int)HID_ERROR_BASE - 0x01)
#define HID_ERROR_NOT_INITIALIZED	((int)HID_ERROR_BASE - 0x01)
#define HID_ERROR_NOT_PAIRED		((int)HID_ERROR_BASE - 0x02)
#define HID_ERROR_INTERNAL		((int)HID_ERROR_BASE - 0x03)
#define HID_ERROR_INVALID_PARAM		((int)HID_ERROR_BASE - 0x04)
#define HID_ERROR_NOT_ENABLED		((int)HID_ERROR_BASE - 0x05)
#define HID_ERROR_CONNECTION_FAILED	((int)HID_ERROR_BASE - 0x06)

typedef struct {
	int event;
	int result;
	void *param_data;
	void *user_data;
} hid_event_param_t;

typedef struct {
	unsigned char btcode;
	unsigned char rep_id;
	unsigned char button;
	signed char axis_x;
	signed char axis_y;
	signed char axis_z;
} hid_send_mouse_event_t;

typedef struct {
	unsigned char	btcode;
	unsigned char	rep_id;
	unsigned char	modify;
	unsigned char	key[8];
} hid_send_key_event_t;

typedef void (*hid_cb_func_ptr)(int, hid_event_param_t *, void *);

typedef struct {
	unsigned char addr[HID_ADDRESS_LENGTH];
} hid_device_address_t;


/**
 * @fn int bluetooth_hid_init(hid_cb_func_ptr callback_ptr, void *user_data)
 * @brief Initialize HID service and register the callback
 *
 * This function is a synchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_ALREADY_INITIALIZED - Aready Initialized \n
 *              HID_ERROR_INTERNAL - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *
 * @remark      None
 *
 */
int bluetooth_hid_init(hid_cb_func_ptr callback_ptr, void *user_data);

/**
 * @fn int bluetooth_hid_deinit(void)
 * @brief Initialize HID service and register the callback
 *
 * This function is a synchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_NOT_INITIALIZED - Not Initialiezed \n
 *
 * @remark      None
 *
 */
int bluetooth_hid_deinit(void);


/**
 * @fn int bluetooth_hid_connect(hid_device_address_t *device_address)
 *
 * @brief Connect the HID device in the peer
 *
 * This function is a asynchronous call.
 * The HID connect request is responded by BLUETOOTH_HID_CONNECTED event.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_INVALID_PARAM - Invalid parameter \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *              HID_ERROR_NOT_PAIRED - Not enabled \n
 *              HID_ERROR_CONNECTION_FAILED - Connection Fail \n
 *
 * @exception   None
 * @param[in]  device_address   This indicates an address of the device with which the pairing
 *				should be initiated
 * @remark      None
 */
int bluetooth_hid_connect(hid_device_address_t *device_address);

/**
 * @fn int bluetooth_hid_disconnect(hid_device_address_t *device_address)
 *
 * @brief Disconnect the HID device in the peer
 *
 * This function is a asynchronous call.
 * The HID connect request is responded by BLUETOOTH_HID_DISCONNECTED event.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_INVALID_PARAM - Invalid parameter \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *              HID_ERROR_NOT_PAIRED - Not enabled \n
 *              HID_ERROR_CONNECTION_FAILED - Connection Fail \n
 *
 * @exception   None
 * @param[in]  device_address   This indicates an address of the device with which the pairing
 *				should be initiated
 * @remark      None
 */
int bluetooth_hid_disconnect(hid_device_address_t *device_address);

/**
 * @fn int bluetooth_hid_device_init(hid_cb_func_ptr callback_ptr, void *user_data)
 * @brief Initialize HID Device service and register the callback
 *
 * This function is a synchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_ALREADY_INITIALIZED - Aready Initialized \n
 *              HID_ERROR_INTERNAL - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *
 * @remark      None
 *
 */
int bluetooth_hid_device_init(hid_cb_func_ptr callback, void *user_data);

/**
 * @fn int bluetooth_hid_device_deinit(void)
 * @brief Initialize HID service and register the callback
 *
 * This function is a synchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_NOT_INITIALIZED - Not Initialiezed \n
 *
 * @remark      None
 *
 */
int bluetooth_hid_device_deinit();

/**
 * @fn int bluetooth_hid_device_activate()
 *
 * @brief Register the HID Device UUID with bluez with RegisterProfile.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @remark      None
 */
int bluetooth_hid_device_activate(void);

/**
 * @fn int bluetooth_hid_device_deactivate()
 *
 * @brief Unregister the HID Device UUID with bluez with UnRegisterProfile.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @remark      None
 */
int bluetooth_hid_device_deactivate(void);

/**
 * @fn int bluetooth_hid_device_connect(const char *remote_addr)
 *
 * @brief Initiate the connection for HID device role with remote device
 * address specified as parameter.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *
 * @exception   None
 * @param[in]  remote_addr   device address of remote device.
 * @remark      None
 */
int bluetooth_hid_device_connect(const char *remote_addr);
/**
 * @fn int bluetooth_hid_device_disconnect()
 *
 * @brief Terminate the connection with HID host device with DisconnectProfile.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @param[in]  remote_addr   device address of remote device.
 * @remark      None
 */

int bluetooth_hid_device_disconnect(const char *remote_addr);

/**
 * @fn int bluetooth_hid_send_mouse_event(hid_send_mouse_event_t send_event)
 *
 * @brief write the event data on the socket.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_INVALID_PARAM - Invalid parameter \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @param[in]  remote_addr   device address of remote device.
 * @param[in]  send_event   This indicates the event data to send to remote device.
 * @remark      None
 */
int bluetooth_hid_device_send_mouse_event(const char *remote_addr,
		hid_send_mouse_event_t send_event);

/**
 * @fn int bluetooth_hid_device_send_key_event(hid_send_key_event_t send_event)
 *
 * @brief write the event data on the socket.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_INVALID_PARAM - Invalid parameter \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @param[in]  remote_addr   device address of remote device.
 * @param[in]  send_event   This indicates the event data to send to remote device.
 * @remark      None
 */
int bluetooth_hid_device_send_key_event(const char *remote_addr,
		hid_send_key_event_t send_event);

/**
 * @fn int bluetooth_hid_device_reply_to_report(const char *remote_addr,
 *			bt_hid_header_type_t htype,
 *			bt_hid_param_type_t ptype,
 *			char *buffer)
 *
 * @brief write the event data on the socket.
 *
 * This function is a asynchronous call.
 *
 * @return  HID_ERROR_NONE  - Success \n
 *              HID_ERROR_INVALID_PARAM - Invalid parameter \n
 *              HID_ERROR_NOT_INITIALIZED - Internal Error \n
 *              HID_ERROR_NOT_ENABLED - Not enabled \n
 *              HID_ERROR_INTERNAL - Not enabled \n
 *
 * @exception   None
 * @param[in]  remote_addr   device address of remote device.
 * @param[in]  htype   header type to be present in response
 * @param[in]  ptype   Parameter type to be present in response.
 * @param[in]  buffer   Data need to be send in reponse payload.
 * @remark      None
 */
int bluetooth_hid_device_reply_to_report(const char *remote_addr,
		bt_hid_header_type_t htype,
		bt_hid_param_type_t ptype,
		const char *data,
		unsigned int data_len);

#ifdef __cplusplus
}
#endif
#endif /* __BLUETOOTH_HID_API_H */
