// SPDX-License-Identifier: AGPL-3.0-only
#include "h264_config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint8_t data[256];
	size_t bit;
} Writer;

static void put_bits(Writer *w, uint32_t value, unsigned count)
{
	assert(count <= 32 && w->bit + count <= sizeof(w->data) * 8);
	for(unsigned i = count; i; i--, w->bit++)
		w->data[w->bit / 8] |= ((value >> (i - 1)) & 1u)
			<< (7 - w->bit % 8);
}

static void put_ue(Writer *w, uint32_t value)
{
	uint32_t code = value + 1;
	unsigned width = 0;
	for(uint32_t n = code; n; n >>= 1) width++;
	put_bits(w, 0, width - 1);
	put_bits(w, code, width);
}

static size_t make_sps(uint8_t *out, uint32_t refs, bool restriction,
	uint32_t reorder, uint32_t buffered)
{
	Writer w = { 0 };
	put_bits(&w, 66, 8); /* Baseline. */
	put_bits(&w, 0, 8);
	put_bits(&w, 40, 8);
	put_ue(&w, 0); /* SPS id. */
	put_ue(&w, 0); /* log2_max_frame_num_minus4. */
	put_ue(&w, 0); /* pic_order_cnt_type. */
	put_ue(&w, 0); /* log2_max_pic_order_cnt_lsb_minus4. */
	put_ue(&w, refs);
	put_bits(&w, 0, 1); /* gaps flag. */
	put_ue(&w, 79); /* 1280 / 16 - 1. */
	put_ue(&w, 44); /* 720 / 16 - 1. */
	put_bits(&w, 1, 1); /* progressive. */
	put_bits(&w, 1, 1); /* direct inference. */
	put_bits(&w, 0, 1); /* no crop. */
	put_bits(&w, restriction ? 1 : 0, 1); /* VUI present. */
	if(restriction)
	{
		put_bits(&w, 0, 1); /* aspect ratio. */
		put_bits(&w, 0, 1); /* overscan. */
		put_bits(&w, 0, 1); /* video signal. */
		put_bits(&w, 0, 1); /* chroma location. */
		put_bits(&w, 0, 1); /* timing. */
		put_bits(&w, 0, 1); /* nal hrd. */
		put_bits(&w, 0, 1); /* vcl hrd. */
		put_bits(&w, 0, 1); /* pic struct. */
		put_bits(&w, 1, 1); /* bitstream restriction. */
		put_bits(&w, 1, 1); /* motion vectors over boundaries. */
		put_ue(&w, 0); put_ue(&w, 0); put_ue(&w, 0); put_ue(&w, 0);
		put_ue(&w, reorder);
		put_ue(&w, buffered);
	}
	put_bits(&w, 1, 1); /* rbsp_stop_one_bit. */
	while(w.bit % 8) put_bits(&w, 0, 1);

	size_t n = 0;
	out[n++] = 0; out[n++] = 0; out[n++] = 0; out[n++] = 1;
	out[n++] = 0x67;
	unsigned zeros = 0;
	for(size_t i = 0; i < w.bit / 8; i++)
	{
		uint8_t byte = w.data[i];
		if(zeros >= 2 && byte <= 3)
		{
			out[n++] = 3;
			zeros = 0;
		}
		out[n++] = byte;
		zeros = byte ? 0 : zeros + 1;
	}
	out[n++] = 0; out[n++] = 0; out[n++] = 1; out[n++] = 0x68;
	return n;
}

int main(void)
{
	uint8_t data[320];
	PlusVitaH264Config config;
	size_t size = make_sps(data, 4, false, 0, 0);
	assert(plus_vita_h264_config(data, size, &config));
	assert(config.profile == 66 && config.level == 40);
	assert(config.coded_width == 1280 && config.coded_height == 720);
	assert(config.max_ref_frames == 4 && !config.has_buffer_restriction);
	assert(plus_vita_h264_decoder_slots(&config) == 4);

	size = make_sps(data, 9, true, 2, 12);
	assert(plus_vita_h264_config(data, size, &config));
	assert(config.max_ref_frames == 9 && config.has_buffer_restriction);
	assert(config.max_reorder_frames == 2 && config.max_buffered_frames == 12);
	assert(plus_vita_h264_decoder_slots(&config) == 12);

	size = make_sps(data, 16, true, 0, 16);
	assert(plus_vita_h264_config(data, size, &config));
	assert(plus_vita_h264_decoder_slots(&config) == 16);
	assert(!plus_vita_h264_config(NULL, size, &config));
	assert(!plus_vita_h264_config(data, 4, &config));
	assert(!plus_vita_h264_config(data, size, NULL));

	size = make_sps(data, 17, false, 0, 0);
	assert(!plus_vita_h264_config(data, size, &config));
	size = make_sps(data, 8, true, 9, 8);
	assert(!plus_vita_h264_config(data, size, &config));

	/* Every truncation of the valid SPS must fail cleanly under sanitizers. */
	size = make_sps(data, 8, false, 0, 0);
	for(size_t i = 0; i < size - 4; i++)
		(void)plus_vita_h264_config(data, i, &config);

	puts("h264_config_test: PASS");
	return 0;
}
