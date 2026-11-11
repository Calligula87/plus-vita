// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_STREAM_INFO_H
#define PLUS_VITA_STREAM_INFO_H

#include <chiaki/audio.h>
#include <chiaki/log.h>

#define PLUS_VITA_STREAM_VIDEO_HEADER_MAX 2048

typedef struct {
	uint8_t audio_header[CHIAKI_AUDIO_HEADER_SIZE];
	uint8_t video_header[PLUS_VITA_STREAM_VIDEO_HEADER_MAX];
	size_t video_header_size;
	uint32_t video_width, video_height;
	unsigned int selected_profile_index, profile_count;
	bool disconnected;
} PlusVitaStreamInfo;

/* Validate STREAMINFO and select the first supported 720p H.264 profile.
 * Failure writes one fixed startup diagnostic. */
bool plus_vita_stream_info_decode(const uint8_t *buf, size_t size, ChiakiLog *log,
	PlusVitaStreamInfo *info);
bool plus_vita_stream_info_guard_header(const PlusVitaStreamInfo *info, ChiakiLog *log,
	uint8_t *patched, size_t capacity, size_t *patched_size);

#endif
