// SPDX-License-Identifier: AGPL-3.0-only
#include "stream_info.h"
#include "diagnostic_log.h"
#include "h264_config.h"

#include <stdlib.h>
#include <string.h>
#include "pb_utils.h"
#include "takion.pb.h"

typedef struct {
	PlusVitaStreamInfo *info;
	ChiakiLog *log;
	bool invalid_profile;
	bool invalid_720p_header;
} StreamProfiles;

static bool decode_resolution(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	(void)field;
	StreamProfiles *profiles = *arg;
	PlusVitaStreamInfo *info = profiles->info;
	tkproto_ResolutionPayload resolution = {0};
	uint8_t bytes[PLUS_VITA_STREAM_VIDEO_HEADER_MAX];
	ChiakiPBDecodeBuf header = { .max_size = sizeof(bytes), .buf = bytes };
	resolution.video_header.arg = &header;
	resolution.video_header.funcs.decode = chiaki_pb_decode_buf;
	if(!pb_decode(stream, tkproto_ResolutionPayload_fields, &resolution))
		return false;

	PlusVitaH264Config h264 = {0};
	bool h264_valid = plus_vita_h264_config(bytes, header.size, &h264);
	if(h264_valid)
		CHIAKI_LOGI(profiles->log,
			"Stream probe profile[%u]: %ux%u header=%lu H264 profile=%u level=%u coded=%ux%u refs=%u buffered=%u restriction=%d",
			info->profile_count, resolution.width, resolution.height,
			(unsigned long)header.size, h264.profile, h264.level, h264.coded_width,
			h264.coded_height, h264.max_ref_frames, h264.max_buffered_frames,
			h264.has_buffer_restriction ? 1 : 0);
	else
		CHIAKI_LOGI(profiles->log, "Stream probe profile[%u]: %ux%u header=%lu H264 SPS unavailable",
			info->profile_count, resolution.width, resolution.height, (unsigned long)header.size);

	if(resolution.width == 1280 && resolution.height == 720)
	{
		if(!h264_valid)
			profiles->invalid_720p_header = true;
		else if(!info->video_header_size)
		{
			info->selected_profile_index = info->profile_count;
			info->video_header_size = header.size;
			info->video_width = resolution.width;
			info->video_height = resolution.height;
			memcpy(info->video_header, bytes, header.size);
		}
	}
	info->profile_count++;
	if(!resolution.width || !resolution.height || !header.size)
		profiles->invalid_profile = true;
	return true;
}

bool plus_vita_stream_info_decode(const uint8_t *buf, size_t size, ChiakiLog *log,
	PlusVitaStreamInfo *info)
{
	*info = (PlusVitaStreamInfo){0};
	ChiakiPBDecodeBuf audio = { .max_size = sizeof(info->audio_header), .buf = info->audio_header };
	StreamProfiles profiles = { .info = info, .log = log };
	tkproto_TakionMessage msg = {0};
	msg.stream_info_payload.audio_header.arg = &audio;
	msg.stream_info_payload.audio_header.funcs.decode = chiaki_pb_decode_buf;
	msg.stream_info_payload.resolution.arg = &profiles;
	msg.stream_info_payload.resolution.funcs.decode = decode_resolution;
	pb_istream_t stream = pb_istream_from_buffer(buf, size);
	if(!pb_decode(&stream, tkproto_TakionMessage_fields, &msg))
	{
		CHIAKI_LOGE(log, "Stream probe could not decode STREAMINFO (%lu bytes)", (unsigned long)size);
		plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_STREAMINFO_INVALID);
		return false;
	}
	if(msg.type == tkproto_TakionMessage_PayloadType_DISCONNECT)
	{
		info->disconnected = true;
		plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_STREAMINFO_INTERRUPTED);
		return false;
	}
	if(msg.type != tkproto_TakionMessage_PayloadType_STREAMINFO || !msg.has_stream_info_payload
		|| audio.size != sizeof(info->audio_header) || profiles.invalid_profile)
	{
		CHIAKI_LOGE(log, "Stream probe invalid STREAMINFO: type=%d audio=%lu profiles=%u",
			(int)msg.type, (unsigned long)audio.size, info->profile_count);
		plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_STREAMINFO_INVALID);
		return false;
	}
	if(!info->video_header_size)
	{
		CHIAKI_LOGE(log, "Stream probe found no compatible 720p H.264 profile");
		plus_vita_diagnostic_stream_startup(profiles.invalid_720p_header
			? PLUS_VITA_STREAM_STARTUP_VIDEO_HEADER_INVALID : PLUS_VITA_STREAM_STARTUP_PROFILE_UNAVAILABLE);
		return false;
	}
	return true;
}

bool plus_vita_stream_info_guard_header(const PlusVitaStreamInfo *info, ChiakiLog *log,
	bool ps3, PlusVitaH264Guard *guard, uint8_t *patched, size_t capacity, size_t *patched_size)
{
	bool accepted = ps3
		? plus_vita_h264_guard_ps3_header(guard, info->video_header, info->video_header_size,
			patched, capacity, patched_size)
		: guard && plus_vita_h264_guard_header(info->video_header, info->video_header_size,
			patched, capacity, patched_size);
	if(accepted)
	{
		if(!ps3) *guard = (PlusVitaH264Guard){ 0 };
		return true;
	}
	CHIAKI_LOGE(log, "Stream probe LIVE_GUARD_REJECT: parameter sets differ from audited sample");
	plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_HEADER_GUARD_REJECTED);
	return false;
}
