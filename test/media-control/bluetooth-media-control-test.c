/*
 *   bluetooth-media-control
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Hocheol Seo <hocheol.seo@samsung.com>
 *		 Girishashok Joshi <girish.joshi@samsung.com>
 *		 Chanyeol Park <chanyeol.park@samsung.com>
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "bluetooth-media-control.h"

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define MEDIA_ATRRIBUTE_LENGTH	256

media_player_settings_t player_settings = {0x00, 0x00, 0x00, 0x00, 0x01, 1111};
media_metadata_attributes_t metadata = {"Test Track", "Chethan", "TNC", "Tumkur", 1, 1, 14400};

void static __choose_metadata_settings(void)
{
	int cmd;
	media_metadata_attributes_t metadata = {0,};

	while (1) {
		printf("\nPlease enter\n");
		printf("\t0: return to main menu\n");
		printf("\t1: Meta data settings\n");
		printf("\tEnter your choice [ ]\b\b");

		scanf("%d", &cmd);

		switch (cmd) {
		case 0:
			return;
		case 1:	/* Title */

			metadata.title = calloc(1, MEDIA_ATRRIBUTE_LENGTH);
			metadata.artist = calloc(1, MEDIA_ATRRIBUTE_LENGTH);
			metadata.album = calloc(1, MEDIA_ATRRIBUTE_LENGTH);
			metadata.genre = calloc(1, MEDIA_ATRRIBUTE_LENGTH);

			printf("Enter the \"Track\" name\n");
			scanf("%s", (char *)metadata.title);

			printf(" Enter the \"Artist\" name\n");
			scanf("%s", (char *)metadata.artist);

			printf(" Enter the \"Album\" name\n");
			scanf("%s", (char *)metadata.album);

			printf(" Enter the \"Genre\" \n");
			scanf("%s", (char *)metadata.genre);

			printf(" Enter the \" Totol NumberOfTracks\" \n");
			scanf("%d", &metadata.total_tracks);

			printf(" Enter the \" Track Number\" \n");
			scanf("%d", &metadata.number);

			printf(" Enter the \"Duration\" \n");
			scanf("%d", &metadata.duration);
			break;
		default:
			break;
		}
		bluetooth_media_player_change_track(&metadata);

		if (NULL !=  metadata.title) {
			free((void *)metadata.title);
			metadata.title = NULL;
		}
		if (NULL !=  metadata.artist) {
			free((void *)metadata.artist);
			metadata.artist = NULL;
		}
		if (NULL !=  metadata.album) {
			free((void *)metadata.album);
			metadata.album = NULL;
		}
		if (NULL !=  metadata.genre) {
			free((void *)metadata.genre);
			metadata.genre = NULL;
		}
	}
}

void static __choose_player_settings(void)
{
	int cmd;
	media_player_property_type type;

	while (1) {
		printf("\nPlease choose player settings\n");
		printf("\t0: return to main menu\n");
		printf("\t1. Equalizer\n");
		printf("\t2. Repeat\n");
		printf("\t3. Shuffle\n");
		printf("\t4. Scan \n");
		printf("\t5. Status \n");
		printf("\t6. Position \n");
		printf("\tEnter your choice [ ]\b\b");

		scanf("%d", &cmd);

		switch (cmd) {
		case 0:
			return;
		case 1:	/* Equalizer */
		{
			printf("Possible Values - EQUALIZER_OFF = 0x00 and EQUALIZER_ON = 0x01,\n");
			scanf("%d", &player_settings.equalizer);
			type = EQUALIZER;

			bluetooth_media_player_change_property(type,
				(unsigned int)player_settings.equalizer);

			break;
		}
		case 2:	/*Repeat */
		{
			printf(" Possible Values - REPEAT_MODE_OFF = 0x00, REPEAT_SINGLE_TRACK = 0x01 , \
					REPEAT_ALL_TRACK = 0x02,	REPEAT_GROUP = 0x03\n");
			scanf("%d", &player_settings.repeat);
			type = REPEAT;

			bluetooth_media_player_change_property(type,
				(unsigned int)player_settings.repeat);
			break;
		}
		case 3:	/* Shuffle */
		{
			printf(" Possible Values - SHUFFLE_MODE_OFF = 0x00, SHUFFLE_ALL_TRACK = 0x01 , \
					SHUFFLE_GROUP = 0x02\n");
			scanf("%d", &player_settings.shuffle);
			type = SHUFFLE;

			bluetooth_media_player_change_property(type,
				(unsigned int)player_settings.shuffle);
			break;
		}
		case 4: /* Scan */
		{
			printf(" Possible Values - SCAN_MODE_OFF = 0x00, SCAN_ALL_TRACK = 0x01 , \
					SCAN_GROUP = 0x02\n");
			scanf("%d", &player_settings.scan);
			type = SCAN;

			bluetooth_media_player_change_property(type,
				(unsigned int)player_settings.scan);
			break;
		}
		case 5: /* Status */
		{
			printf(" Possible Values - STATUS_PLAYING = 0x00, STATUS_STOPPED = 0x01 , \
					STATUS_PAUSED = 0x02,STATUS_FORWARD_SEEK = 0x03 \
					STATUS_REVERSE_SEEK = 0x04 STATUS_ERROR = 0x05\n");
			scanf("%d", &player_settings.status);
			type = STATUS;

			bluetooth_media_player_change_property(type,
				(unsigned int)player_settings.status);
			break;
		}
		case 6: /* Position */
		{
			printf("Enter the possible value: ");
			scanf("%d", &player_settings.position);
			type = POSITION;

			bluetooth_media_player_change_property(type,
				player_settings.position);
			break;
		}
		default:
			break;
		}
	}
}

int main()
{
	GMainLoop *agent_loop;
	int cmd;

	g_type_init();
	agent_loop = g_main_loop_new(NULL, FALSE);

	printf("MP-AV test application started\n");
	while (1) {
		printf("\n\n\t0. Exit\n");
		printf("\t1. bluetooth_media_player_property_changed\n");
		printf("\t2. bluetooth_media_player_track_changed\n");
		printf("\tEnter your choice [  ]\b\b");

		scanf("%d", &cmd);

		switch (cmd) {
		case 0:	/* exit the application */
			{
				exit(0);
				break;
			}
		case 1:
			{
				__choose_player_settings();
				break;
			}
		case 2:
			{
				__choose_metadata_settings();
				break;
			}
		}
	}
	printf("gmain loop enter\n");
	g_main_loop_run(agent_loop);
	printf("gmain loop leave\n");
	return 0;
}
