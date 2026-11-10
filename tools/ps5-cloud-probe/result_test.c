// SPDX-License-Identifier: AGPL-3.0-only
#include "result.h"
#include <assert.h>
#include <string.h>

static void check_report(const struct probe_video *video, enum probe_status status,
	int code, int expected_exit, const char *expected)
{
	char actual[512];
	FILE *output = tmpfile();
	assert(output);
	assert(probe_report(output, video, status, code) == expected_exit);
	rewind(output);
	assert(fgets(actual, sizeof(actual), output));
	assert(strcmp(actual, expected) == 0);
	assert(fgetc(output) == EOF);
	fclose(output);
}

static void hevc_delivery(void)
{
	/* Synthetic Annex B VPS/SPS/PPS and an IDR followed by a trail sample. */
	const uint8_t hevc_header[] = {
		0,0,0,1,0x40,1,0x80, 0,0,1,0x42,1,0x80, 0,0,1,0x44,1,0x80
	};
	const uint8_t hevc_idr[] = { 0,0,1,0x26,1,0x80 };
	const uint8_t hevc_trail[] = { 0,0,1,0x02,1,0x80 };
	struct probe_video video = { 0 };
	probe_video_header(&video, 1920, 1080, hevc_header, sizeof(hevc_header));
	assert(!probe_video_sample(&video, hevc_trail, sizeof(hevc_trail), false));
	assert(!probe_video_sample(&video, hevc_idr, sizeof(hevc_idr), false));
	assert(probe_video_sample(&video, hevc_trail, sizeof(hevc_trail), false));
	check_report(&video, PROBE_COMPLETE, 0, 0,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=hevc_delivered "
		"stage=video reason=complete code=0 codec=hevc stream=1920x1080 samples=2\n");
}

static void h264_delivery(void)
{
	/* Baseline L4.0, 1280x720, three reference and five buffered pictures. */
	const uint8_t avc_header[] = {
		0,0,0,1,0x67,0x42,0,0x28,0xf2,0,0xa0,0x0b,0x74,0x03,0xf4,0x68,
		0,0,1,0x68,0xce,0x38,0x80
	};
	const uint8_t avc_idr[] = { 0,0,1,0x65,0x88 };
	const uint8_t avc_partition[] = { 0,0,1,0x62,0x88 };
	const uint8_t avc_trail[] = { 0,0,1,0x41,0x88 };
	struct probe_video video;
	probe_video_header(&video, 1280, 720, avc_header, sizeof(avc_header));
	assert(!probe_video_sample(&video, avc_idr, sizeof(avc_idr), false));
	assert(!probe_video_sample(&video, avc_partition, sizeof(avc_partition), false));
	assert(!probe_video_sample(&video, avc_trail, sizeof(avc_trail), true));
	assert(!probe_video_sample(&video, avc_trail, sizeof(avc_trail), false));
	assert(!probe_video_sample(&video, avc_idr, sizeof(avc_idr), false));
	assert(probe_video_sample(&video, avc_trail, sizeof(avc_trail), false));
	check_report(&video, PROBE_COMPLETE, 0, 0,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=usable_h264 "
		"stage=video reason=complete code=0 codec=h264 stream=1280x720 samples=2 "
		"avc_profile=66 avc_level=40 coded=1280x720 refs=3 buffer_restriction=1 reorder=1 buffered=5\n");
}

static void h264_outside_vita_limits(void)
{
	/* Six required decoder slots at 720p. */
	const uint8_t avc_header[] = {
		0,0,0,1,0x67,0x42,0,0x28,0xf3,0x80,0xa0,0x0b,0x74,0x03,0xf4,0x78,
		0,0,1,0x68,0xce,0x38,0x80
	};
	const uint8_t avc_idr[] = { 0,0,1,0x65,0x88 };
	const uint8_t avc_trail[] = { 0,0,1,0x41,0x88 };
	struct probe_video video;
	probe_video_header(&video, 1280, 720, avc_header, sizeof(avc_header));
	assert(!probe_video_sample(&video, avc_idr, sizeof(avc_idr), false));
	assert(probe_video_sample(&video, avc_trail, sizeof(avc_trail), false));
	check_report(&video, PROBE_COMPLETE, 0, 1,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=inconclusive "
		"stage=video reason=unverified_h264_limits code=0 codec=h264 stream=1280x720 samples=2 "
		"avc_profile=66 avc_level=40 coded=1280x720 refs=6 buffer_restriction=1 reorder=1 buffered=6\n");
}

static void failures_are_inconclusive(void)
{
	struct probe_video video = { 0 };
	check_report(&video, probe_provisioning_failure("AUTHORIZATION_FAILED"), 1, 1,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=inconclusive "
		"stage=authorization reason=preflight_failed code=1 codec=unknown stream=0x0 samples=0\n");
	check_report(&video, probe_provisioning_failure("{\"sessionId\":\"private-marker\"}"), 1, 1,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=inconclusive "
		"stage=provisioning reason=pylux_provision_failed code=1 codec=unknown stream=0x0 samples=0\n");
	const uint8_t unknown_header[] = "private marker that must never enter the report";
	probe_video_header(&video, 1920, 1080, unknown_header, sizeof(unknown_header));
	check_report(&video, PROBE_VIDEO_TIMEOUT, 0, 1,
		"profile=pylux-default-hevc-sdr-1080p60-20000kbps outcome=inconclusive "
		"stage=video reason=unrecognized_parameter_sets code=0 codec=unknown stream=1920x1080 samples=0\n");
}

int main(void)
{
	hevc_delivery();
	h264_delivery();
	h264_outside_vita_limits();
	failures_are_inconclusive();
	return 0;
}
