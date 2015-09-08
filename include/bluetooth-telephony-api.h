/*
 * Bluetooth-telephony
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:	Hocheol Seo <hocheol.seo@samsung.com>
 * 		GirishAshok Joshi <girish.joshi@samsung.com>
 *		Chanyeol Park <chanyeol.park@samsung.com>
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

#ifndef _BLUETOOTH_TELEPHONY_API_H_
#define _BLUETOOTH_TELEPHONY_API_H_

#include <stdint.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dlog.h>

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

typedef void (*bt_telephony_func_ptr)(int, void *, void *);

#define BLUETOOTH_TELEPHONY_ERROR_NONE ((int)0)
#define BLUETOOTH_TELEPHONY_ERROR_INTERNAL \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x01)
#define BLUETOOTH_TELEPHONY_ERROR_INVALID_PARAM \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x02)
#define BLUETOOTH_TELEPHONY_ERROR_ALREADY_INITIALIZED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x03)
#define BLUETOOTH_TELEPHONY_ERROR_NOT_INITIALIZED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x04)
#define BLUETOOTH_TELEPHONY_ERROR_AUDIO_NOT_CONNECTED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x05)
#define BLUETOOTH_TELEPHONY_ERROR_NOT_ENABLED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x06)
#define BLUETOOTH_TELEPHONY_ERROR_NOT_AVAILABLE \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x07)
#define BLUETOOTH_TELEPHONY_ERROR_NOT_CONNECTED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x08)
#define BLUETOOTH_TELEPHONY_ERROR_BUSY \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x09)
#define BLUETOOTH_TELEPHONY_ERROR_ALREADY_EXSIST \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0A)
#define BLUETOOTH_TELEPHONY_ERROR_NO_MEMORY \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0B)
#define BLUETOOTH_TELEPHONY_ERROR_I_O_ERROR \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0C)
#define BLUETOOTH_TELEPHONY_ERROR_ALREADY_CONNECTED \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0D)
#define BLUETOOTH_TELEPHONY_ERROR_OPERATION_NOT_AVAILABLE \
				((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0E)
#define BLUETOOTH_TELEPHONY_ERROR_PERMISSION_DENIED \
					((int)BLUETOOTH_TELEPHONY_ERROR_NONE - 0x0F)

#define BT_ADDRESS_STR_LEN 18
#define BT_ADAPTER_PATH_LEN 50
#define BT_AUDIO_CALL_PATH_LEN 50

typedef struct {
	int event;
	int result;
	void *param_data;
} telephony_event_param_t;

typedef struct {
	 int callid;
} telephony_event_callid_t;

typedef struct {
	gchar *dtmf;
} telephony_event_dtmf_t;

typedef enum {
	BLUETOOTH_STATE_CONNECTED,
	BLUETOOTH_STATE_PLAYING,
	BLUETOOTH_STATE_DISCONNETED,
} bluetooth_headset_state_t;

typedef enum {
	BLUETOOTH_TELEPHONY_ERROR_INVALID_CHLD_INDEX,
	BLUETOOTH_TELEPHONY_ERROR_BATTERY_STATUS,
	BLUETOOTH_TELEPHONY_ERROR_SIGNAL_STATUS,
	BLUETOOTH_TELEPHONY_ERROR_NOT_SUPPORTED,
	BLUETOOTH_TELEPHONY_ERROR_APPLICATION,
	BLUETOOTH_TELEPHONY_ERROR_INVALID_DTMF,
} bluetooth_telephony_error_t;

typedef enum {
	CSD_CALL_STATUS_IDLE,
	CSD_CALL_STATUS_CREATE,
	CSD_CALL_STATUS_COMING,
	CSD_CALL_STATUS_PROCEEDING,
	CSD_CALL_STATUS_MO_ALERTING,
	CSD_CALL_STATUS_MT_ALERTING,
	CSD_CALL_STATUS_WAITING,
	CSD_CALL_STATUS_ANSWERED,
	CSD_CALL_STATUS_ACTIVE,
	CSD_CALL_STATUS_MO_RELEASE,
	CSD_CALL_STATUS_MT_RELEASE,
	CSD_CALL_STATUS_HOLD_INITIATED,
	CSD_CALL_STATUS_HOLD,
	CSD_CALL_STATUS_RETRIEVE_INITIATED,
	CSD_CALL_STATUS_RECONNECT_PENDING,
	CSD_CALL_STATUS_TERMINATED,
	CSD_CALL_STATUS_SWAP_INITIATED,
} bt_telephony_call_status_t;

#define BLUETOOTH_EVENT_TYPE_TELEPHONY_BASE	(unsigned int)(0x00500)

typedef enum {
	BLUETOOTH_EVENT_TELEPHONY_ANSWER_CALL = BLUETOOTH_EVENT_TYPE_TELEPHONY_BASE,
	BLUETOOTH_EVENT_TELEPHONY_RELEASE_CALL,
	BLUETOOTH_EVENT_TELEPHONY_REJECT_CALL,
	BLUETOOTH_EVENT_TELEPHONY_CHLD_0_RELEASE_ALL_HELD_CALL,
	BLUETOOTH_EVENT_TELEPHONY_CHLD_1_RELEASE_ALL_ACTIVE_CALL,
	BLUETOOTH_EVENT_TELEPHONY_CHLD_2_ACTIVE_HELD_CALL,
	BLUETOOTH_EVENT_TELEPHONY_CHLD_3_MERGE_CALL,
	BLUETOOTH_EVENT_TELEPHONY_CHLD_4_EXPLICIT_CALL_TRANSFER,
	BLUETOOTH_EVENT_TELEPHONY_SEND_DTMF,
	BLUETOOTH_EVENT_TELEPHONY_HFP_CONNECTED,
	BLUETOOTH_EVENT_TELEPHONY_HFP_DISCONNECTED,
	BLUETOOTH_EVENT_TELEPHONY_AUDIO_CONNECTED,
	BLUETOOTH_EVENT_TELEPHONY_AUDIO_DISCONNECTED,
	BLUETOOTH_EVENT_TELEPHONY_SET_SPEAKER_GAIN,
	BLUETOOTH_EVENT_TELEPHONY_SET_MIC_GAIN,
	BLUETOOTH_EVENT_TELEPHONY_NREC_CHANGED,
	BLUETOOTH_EVENT_TELEPHONY_VENDOR_AT_CMD,
} bluetooth_telephony_event_type;

typedef enum {
	BLUETOOTH_CALL_STATE_NONE,
	BLUETOOTH_CALL_STATE_CONNECTED,
	BLUETOOTH_CALL_STATE_HELD,
	BLUETOOTH_CALL_STATE_DIALLING,
	BLUETOOTH_CALL_STATE_ALERTING,
	BLUETOOTH_CALL_STATE_INCOMING,
	BLUETOOTH_CALL_STATE_WAITING,
	BLUETOOTH_CALL_STATE_ERROR,
} bt_telephony_call_state_t;

typedef struct {
	unsigned int call_id;
	bt_telephony_call_state_t call_status;
	char *phone_number;
} bt_telephony_call_status_info_t;

/**
 * @brief	The function bluetooth_telephony_init is initialize telephony calls.
 *
 * @param[in]	cb	Callback function
 * @param[in]	user_data	Data sent by application, which will be
 *				returned in event handler.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_init(bt_telephony_func_ptr cb, void  *user_data);

/**
 * @brief	The function bluetooth_telephony_deinit is deinitialize telephony calls.
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_deinit(void);

/**
 * @brief	The function bluetooth_telephony_audio_open is to open SCO channel
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_audio_open(void);

/**
 * @brief	The function bluetooth_telephony_audio_close is to close SCO channel.
 *	that the Volume on AG is changed.
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_audio_close(void);

/**
  * @brief	The function bluetooth_telephony_call_remote_ringing is send
  *	call status.
 *
 * @param[in]	call_id	Call Id.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_remote_ringing(unsigned int call_id);

/**
 * @brief	The function bluetooth_telephony_call_answered is called to
 *	answer calls.
 *
 * @param[in]	call_id	Call Id.
  * @param[in]	bt_audio	flag
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_answered(unsigned int call_id,
						unsigned int bt_audio);

/**
 * @brief	The function bluetooth_telephony_call_end to end call
 *
 * @param[in]	call_id	Call Id.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_end(unsigned int call_id);

/**
 * @brief	The function bluetooth_telephony_call_held to hold call
 *
 * @param[in]	call_id	Call Id.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_held(unsigned int call_id);

/**
 * @brief	The function bluetooth_telephony_call_retrieved to retrieve call
 *
 * @param[in]	call_id	Call Id.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_retrieved(unsigned int call_id);

/**
 * @brief	The function bluetooth_telephony_call_swapped to swap call
 *
 * @param[in]	call_list	Call info such as id and status.
 * @param[in]	call_count	Call count.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_call_swapped(void *call_list,
				unsigned int call_count);

/**
 * @brief	The function bluetooth_telephony_set_call_status to set call status
 *
 * @param[in]	call_list	Call info such as id and status.
 * @param[in]	call_count	Call count.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_set_call_status(void *call_list,
				unsigned int call_count);

/**
 * @brief	The function bluetooth_telephony_indicate_outgoing_call toindicate
 *	outgoing call.
 *
 * @param[in]	ph_number	Phone number of the outgoing call.
 * @param[in]	call_id		Call ID.
 * @param[in]	bt_audio		Flag.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_indicate_outgoing_call(
		const char *ph_number, unsigned int call_id,
		unsigned int bt_audio);

/**
 * @brief	The function bluetooth_telephony_indicate_incoming_call  to indicate
 *	incoming call.
 *
 * @param[in]	call_info	Call info such as id and status.
 * @param[in]	call_count	Call count.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_indicate_incoming_call(
		const char *ph_number, unsigned int call_id);

/**
 * @brief	The function bluetooth_telephony_is_sco_connected  to check
 *		if SCO channel is connected.
 *
 * @return	gboolean	TRUE if headset playing else FALSE.
 *
 */
gboolean bluetooth_telephony_is_sco_connected(void);

/**
 * @brief	The function bluetooth_telephony_is_nrec_enabled  to check
 *		for noise reduction and echo cancelation(nrec) status
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
 int bluetooth_telephony_is_nrec_enabled(gboolean *status);

/**
 * @brief	The function bluetooth_telephony_is_nrec_enabled  to check
 *		for wide band speech status
 *
 * @return	int	Zero on Success or reason for error if any.
 *
 */
 int bluetooth_telephony_is_wbs_mode(gboolean *status);

/**
 * @brief This function send XSAT vendor specific AT command
 *
 * @return	int	Zero on Success or reason for error if any.
 */
 int bluetooth_telephony_send_vendor_cmd(const char *cmd);


/**
 * @brief This function sends request to enable voice recognition feature
 *
 * @return	int	Zero on Success or reason for error if any.
 */
 int bluetooth_telephony_start_voice_recognition(void);

/**
 * @brief This function sends request to disable voice recognition feature
 *
 * @return	int	Zero on Success or reason for error if any.
 */
 int bluetooth_telephony_stop_voice_recognition(void);


/**
 * @brief	The function bluetooth_telephony_get_headset_volume is called to get
 *	the changed Volume on AG.
 *
 * @param[out]	speaker_gain		Speaker gain.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_get_headset_volume(unsigned int *speaker_gain);

/**
 * @brief	The function bluetooth_telephony_set_speaker_gain is called to indicate
 *	that the Volume on AG is changed.
 *
 * @param[in]	speaker_gain		Speaker gain.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_set_speaker_gain(unsigned short speaker_gain);

/**
 * @brief	The function bluetooth_telephony_is_connected is called to get
 *	the connection state on AG.
 *
 * @param[in]	ag_connnected	Connection state.
 * @return	int	Zero on Success or reason for error if any.
 *
 */
int bluetooth_telephony_is_connected(gboolean *ag_connected);

#ifdef __cplusplus
}
#endif /*__cplusplus*/
#endif/*_BLUETOOTH_TELEPHONY_API_H_*/
