// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_H264_CONFIG_H
#define PLUS_VITA_H264_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint32_t profile, level;
	uint32_t coded_width, coded_height;
	uint32_t max_ref_frames, max_reorder_frames, max_buffered_frames;
	bool has_buffer_restriction;
} PlusVitaH264Config;

/* Read SPS metadata only. Never rewrite the received video bitstream. */
bool plus_vita_h264_config(const uint8_t *annexb, size_t size, PlusVitaH264Config *config);
uint32_t plus_vita_h264_decoder_slots(const PlusVitaH264Config *config);
typedef struct {
	bool started;
	uint32_t frame_num;
	uint32_t idr_count;
	const char *reason;
	uint32_t observed_frame, observed_mb, observed_slice, observed_nal;
} PlusVitaH264Guard;
/* Narrow experiment: exact audited parameter sets, then every slice checked. */
bool plus_vita_h264_guard_header(const uint8_t *data, size_t size,
	uint8_t *patched, size_t capacity, size_t *patched_size);
bool plus_vita_h264_guard_frame(PlusVitaH264Guard *guard,
	const uint8_t *data, size_t size);
bool plus_vita_h264_guard_selftest(void);
#endif
