// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_PS5_CLOUD_PROBE_RESULT_H
#define PLUS_VITA_PS5_CLOUD_PROBE_RESULT_H

#include "h264_config.h"
#include <stdio.h>

enum {
	PROBE_DEFAULT_WIDTH = 1920,
	PROBE_DEFAULT_HEIGHT = 1080,
	PROBE_DEFAULT_FPS = 60,
	PROBE_DEFAULT_BITRATE_KBPS = 20000,
};

enum probe_codec { PROBE_CODEC_UNKNOWN, PROBE_CODEC_H264, PROBE_CODEC_HEVC };

enum probe_status {
	PROBE_COMPLETE, PROBE_INPUT, PROBE_ISOLATION, PROBE_LIBRARY_INIT,
	PROBE_AUTH, PROBE_PREMIUM, PROBE_DATACENTER, PROBE_PROVISION, PROBE_ALLOCATION, PROBE_SESSION_INIT,
	PROBE_SESSION_START, PROBE_DISCONNECTED, PROBE_VIDEO_TIMEOUT,
	PROBE_INTERRUPTED, PROBE_CLEANUP
};

struct probe_video {
	enum probe_codec codec;
	unsigned width, height, samples;
	bool parsed_avc;
	PlusVitaH264Config avc;
};

void probe_video_header(struct probe_video *video, unsigned width, unsigned height,
	const uint8_t *data, size_t size);
bool probe_video_sample(struct probe_video *video, const uint8_t *data, size_t size,
	bool damaged);
enum probe_status probe_provisioning_failure(const char *message);
int probe_report(FILE *output, const struct probe_video *video, enum probe_status status,
	int code);

#endif
