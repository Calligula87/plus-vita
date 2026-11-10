// SPDX-License-Identifier: AGPL-3.0-only
#include "result.h"
#include <string.h>

static const uint32_t avc_parameter_sets = UINT32_C(3) << 7;

static size_t start_code(const uint8_t *data, size_t size)
{
	if(size < 3 || data[0] || data[1]) return 0;
	if(data[2] == 1) return 3;
	return size >= 4 && data[2] == 0 && data[3] == 1 ? 4 : 0;
}

struct nal_types {
	uint32_t avc;
	uint64_t hevc;
	unsigned avc_sps_count, avc_pps_count;
};

static struct nal_types annex_b_types(const uint8_t *data, size_t size)
{
	struct nal_types types = { 0 };
	while(size)
	{
		size_t prefix = start_code(data, size);
		if(!prefix || size == prefix) return (struct nal_types){ 0 };
		data += prefix;
		size -= prefix;
		size_t length = 1;
		while(length < size && !start_code(data + length, size - length)) length++;
		if(data[0] & 0x80) return (struct nal_types){ 0 };
		if(length >= 2)
		{
			unsigned type = data[0] & 31u;
			types.avc |= UINT32_C(1) << type;
			if(type == 7) types.avc_sps_count++;
			if(type == 8) types.avc_pps_count++;
		}
		/* HEVC base layer, with a nonzero temporal_id_plus1. */
		if(length >= 3 && !(data[0] & 1) && data[1] >= 1 && data[1] <= 7)
			types.hevc |= UINT64_C(1) << (data[0] >> 1);
		data += length;
		size -= length;
	}
	return types;
}

void probe_video_header(struct probe_video *video, unsigned width, unsigned height,
	const uint8_t *data, size_t size)
{
	*video = (struct probe_video){ .width = width, .height = height };
	struct nal_types types = annex_b_types(data, size);
	const uint64_t hevc_parameter_sets = UINT64_C(7) << 32;
	bool avc = (types.avc & avc_parameter_sets) == avc_parameter_sets;
	bool hevc = (types.hevc & hevc_parameter_sets) == hevc_parameter_sets;
	if(avc == hevc) return;
	video->codec = avc ? PROBE_CODEC_H264 : PROBE_CODEC_HEVC;
	if(avc && types.avc_sps_count == 1 && types.avc_pps_count == 1)
		video->parsed_avc = plus_vita_h264_config(data, size, &video->avc);
}

bool probe_video_sample(struct probe_video *video, const uint8_t *data, size_t size,
	bool damaged)
{
	if(damaged) video->samples = 0;
	if(damaged || video->codec == PROBE_CODEC_UNKNOWN) return false;
	struct nal_types types = annex_b_types(data, size);
	/* Require a random-access picture before counting subsequent pictures. */
	bool picture;
	if(video->codec == PROBE_CODEC_H264)
	{
		/* A new in-band SPS/PPS would invalidate the negotiated limits. */
		if(types.avc & avc_parameter_sets)
		{
			video->samples = 0;
			video->parsed_avc = false;
		}
		const uint32_t idr = UINT32_C(1) << 5;
		const uint32_t non_idr = UINT32_C(1) << 1;
		picture = (types.avc & (video->samples ? idr | non_idr : idr)) != 0;
	}
	else
	{
		const uint64_t irap = UINT64_C(0x3f) << 16;
		const uint64_t vcl = irap | UINT64_C(0x3ff);
		picture = (types.hevc & (video->samples ? vcl : irap)) != 0;
	}
	if(picture && video->samples < 2) video->samples++;
	return video->samples >= 2;
}

static bool within_vita_limits(const struct probe_video *video)
{
	const PlusVitaH264Config *avc = &video->avc;
	return video->parsed_avc && (avc->profile == 66 || avc->profile == 77 || avc->profile == 100)
		&& avc->level > 0 && avc->level <= 40 && avc->coded_width <= 1280
		&& avc->coded_height <= 720 && video->width <= avc->coded_width
		&& video->height <= avc->coded_height && avc->has_buffer_restriction
		&& plus_vita_h264_decoder_slots(avc) <= 5;
}

enum probe_status probe_provisioning_failure(const char *message)
{
	if(!message) return PROBE_PROVISION;
	if(strcmp(message, "AUTHORIZATION_FAILED") == 0) return PROBE_AUTH;
	if(strcmp(message, "PS_PLUS_SUBSCRIPTION_REQUIRED") == 0) return PROBE_PREMIUM;
	if(strcmp(message, "PING_TIMEOUT") == 0) return PROBE_DATACENTER;
	return PROBE_PROVISION;
}

int probe_report(FILE *output, const struct probe_video *video, enum probe_status status,
	int code)
{
	static const struct { const char *stage, *reason; } descriptions[] = {
		[PROBE_COMPLETE] = { "video", "complete" },
		[PROBE_INPUT] = { "setup", "missing_or_invalid_inputs" },
		[PROBE_ISOLATION] = { "setup", "diagnostic_isolation_failed" },
		[PROBE_LIBRARY_INIT] = { "setup", "pylux_init_failed" },
		[PROBE_AUTH] = { "authorization", "preflight_failed" },
		[PROBE_PREMIUM] = { "authorization", "premium_required" },
		[PROBE_DATACENTER] = { "provisioning", "datacenter_ping_timeout" },
		[PROBE_PROVISION] = { "provisioning", "pylux_provision_failed" },
		[PROBE_ALLOCATION] = { "provisioning", "unusable_allocation" },
		[PROBE_SESSION_INIT] = { "session", "init_failed" },
		[PROBE_SESSION_START] = { "session", "start_failed" },
		[PROBE_DISCONNECTED] = { "session", "disconnected" },
		[PROBE_VIDEO_TIMEOUT] = { "video", "timeout" },
		[PROBE_INTERRUPTED] = { "probe", "interrupted" },
		[PROBE_CLEANUP] = { "cleanup", "stop_or_join_failed" },
	};
	const char *outcome = "inconclusive";
	const char *reason = descriptions[status].reason;
	const char *codec = "unknown";
	if(video->codec == PROBE_CODEC_H264) codec = "h264";
	if(video->codec == PROBE_CODEC_HEVC) codec = "hevc";
	if(status == PROBE_VIDEO_TIMEOUT)
	{
		if(!video->width || !video->height) reason = "no_stream_header_before_timeout";
		else if(video->codec == PROBE_CODEC_UNKNOWN) reason = "unrecognized_parameter_sets";
		else reason = "no_clean_sample_pair_before_timeout";
	}
	if(status == PROBE_COMPLETE)
	{
		if(video->samples < 2 || !video->width || !video->height)
			reason = "missing_video_evidence";
		else if(video->codec == PROBE_CODEC_HEVC)
			outcome = "hevc_delivered";
		else if(video->codec == PROBE_CODEC_H264 && within_vita_limits(video))
			outcome = "usable_h264";
		else
			reason = "unverified_h264_limits";
	}
	fprintf(output, "profile=pylux-default-hevc-sdr-%dp%d-%dkbps outcome=%s "
		"stage=%s reason=%s code=%d codec=%s stream=%ux%u samples=%u",
		PROBE_DEFAULT_HEIGHT, PROBE_DEFAULT_FPS, PROBE_DEFAULT_BITRATE_KBPS,
		outcome, descriptions[status].stage, reason, code, codec,
		video->width, video->height, video->samples);
	if(video->parsed_avc)
	{
		const PlusVitaH264Config *avc = &video->avc;
		fprintf(output, " avc_profile=%u avc_level=%u coded=%ux%u refs=%u "
			"buffer_restriction=%u reorder=%u buffered=%u",
			avc->profile, avc->level, avc->coded_width, avc->coded_height,
			avc->max_ref_frames, (unsigned)avc->has_buffer_restriction,
			avc->max_reorder_frames, avc->max_buffered_frames);
	}
	fputc('\n', output);
	return fflush(output) == 0 && strcmp(outcome, "inconclusive") != 0 ? 0 : 1;
}
