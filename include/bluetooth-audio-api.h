/*
 * bluetooth-audio-api
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:	Hocheol Seo <hocheol.seo@samsung.com>
 * 		GirishAshok Joshi <girish.joshi@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _BLUETOOTH_AUDIO_API_H_
#define _BLUETOOTH_AUDIO_API_H_

#include <stdint.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dlog.h>

#include "bluetooth-api.h"

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#define BLUETOOTH_AUDIO_ERROR_BASE ((int)0)
#define BLUETOOTH_AUDIO_ERROR_NONE ((int)0)
#define BLUETOOTH_AUDIO_ERROR_INTERNAL \
				((int)BLUETOOTH_AUDIO_ERROR_BASE - 0x01)
#define BLUETOOTH_AUDIO_ERROR_INVALID_PARAM \
				((int)BLUETOOTH_AUDIO_ERROR_BASE - 0x02)
#define BLUETOOTH_AG_ERROR_CONNECTION_ERROR \
				((int)BLUETOOTH_AUDIO_ERROR_BASE - 0x03)
#define BLUETOOTH_AV_ERROR_CONNECTION_ERROR \
				((int)BLUETOOTH_AUDIO_ERROR_BASE - 0x04)

typedef struct {
	int event;
	int result;
	void *param_data;
	void *user_data;
} bt_audio_event_param_t;

typedef enum {
	BLUETOOTH_AG_STATE_NONE,
	BLUETOOTH_AG_STATE_CONNECTING,
	BLUETOOTH_AG_STATE_CONNECTED,
	BLUETOOTH_AG_STATE_DISCONNECTED,
	BLUETOOTH_AG_STATE_PLAYING,
} bt_ag_conn_status_t;

typedef enum {
	BLUETOOTH_AV_STATE_NONE,
	BLUETOOTH_AV_STATE_CONNECTING,
	BLUETOOTH_AV_STATE_CONNECTED,
	BLUETOOTH_AV_STATE_DISCONNECTED,
} bt_av_conn_status_t;

typedef void (*bt_audio_func_ptr) (int, bt_audio_event_param_t *, void *);

typedef struct {
	bt_ag_conn_status_t ag_state;
	bt_av_conn_status_t av_state;
	unsigned int ag_audio_flag;
	unsigned int ag_spkr_gain;
	bluetooth_device_address_t local_address;
	bluetooth_device_address_t remote_address;
	bt_audio_func_ptr audio_cb;
	void *user_data;
} bt_audio_info_t;

typedef enum {
                BLUETOOTH_STATE_NONE = 0x0000,
                BLUETOOTH_STATE_HEADSET_CONNECTED = 0x0004,
                BLUETOOTH_STATE_A2DP_HEADSET_CONNECTED = 0x0010,
} bluetooth_device_state_t;

/**
 * @brief	The function bluetooth_audio_init called to initializes the Audio
 * 	service to bluetoothD and Global data related to audio.
 * @param[in]	cb	Callback function
 * @param[in]	user_data	Data sent by application, which will be
 *				returned in event handler.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_audio_init(bt_audio_func_ptr cb, void *user_data);

/**
 * @brief	The function bluetooth_audio_deinit is called to free the Audio
 *	related Global data.
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_audio_deinit(void);

/**
 * @brief	The function bluetooth_audio_connect is called to establish an
 *	AG connection with  the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_audio_connect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_audio_disconnect is called to disconnect
 *	an existing AG connection with the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_audio_disconnect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_ag_connect is called to establish an AG
 *	connection with  the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_ag_connect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_ag_disconnect is called to disconnect an
 *	existing AG connection with the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_ag_disconnect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_av_connect is called to establish an AV
 *	connection with  the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_av_connect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_av_source_connect is called to establish an AV Source
 *	connection with  the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_av_source_connect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_av_disconnect is called to disconnect an
 *	existing AV connection with the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_av_disconnect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_av_source_disconnect is called to disconnect an
 *	existing AV connection with the specified device.
 *
 * @param[in]	remote_address	Bluetooth device address.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_av_source_disconnect(bluetooth_device_address_t *remote_address);

/**
 * @brief	The function bluetooth_ag_get_headset_volume is called to get
 *	the changed Volume on AG.
 *
 * @param[in]	speaker_gain	Speaker gain/loss.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_ag_get_headset_volume(unsigned int *speaker_gain);

/**
 * @brief	The function bluetooth_ag_set_speaker_gain is called to indicate
 *	that the Volume on AG is changed.
 *
 * @param[in]	speaker_gain	Speaker gain/loss.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_ag_set_speaker_gain(unsigned int speaker_gain);

typedef struct {
	int event;
	int result;
	void *param_data;
	void *user_data;
} bt_hf_event_param_t;

typedef void (*bt_hf_func_ptr) (int, bt_hf_event_param_t *, void *);


int bluetooth_hf_init(bt_hf_func_ptr cb, void *user_data);

int bluetooth_hf_deinit(void);

int bluetooth_hf_connect(bluetooth_device_address_t *remote_address);

int bluetooth_hf_disconnect(bluetooth_device_address_t *remote_address);

int bluetooth_hf_answer_call();

int bluetooth_hf_terminate_call();

int bluetooth_hf_initiate_call(char *number);

int bluetooth_hf_voice_recognition(unsigned int status);

int bluetooth_hf_audio_disconnect(void);

int bluetooth_hf_set_speaker_gain(unsigned int speaker_gain);

int bluetooth_hf_send_dtmf(char *dtmf);

int bluetooth_hf_send_xsat_cmd(int app_id, char *xsat_cmd);

int bluetooth_hf_swap_call(void);

int bluetooth_hf_release_all_call(void);

int bluetooth_hf_release_and_accept(void);

int bluetooth_hf_join_call(void);

int bluetooth_hf_get_call_list(void *call_list, bt_hf_call_status_info_t **call_status);

int bluetooth_hf_free_call_list(bt_hf_call_list_s *call_list);

int bluetooth_hf_request_call_list(bt_hf_call_list_s **call_list);

int bluetooth_hf_get_codec(unsigned int *codec_id);

int bluetooth_hf_get_audio_connected(unsigned int *audio_connected);

int bluetooth_hf_is_connected(gboolean *hf_connected);

#ifdef __cplusplus
}
#endif /*__cplusplus*/
#endif/*_BLUETOOTH_AUDIO_API_H_*/
