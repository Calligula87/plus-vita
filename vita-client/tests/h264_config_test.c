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

static size_t make_ps3_p_slice(uint8_t *out, unsigned frame, unsigned denominator,
	bool adaptive_marking)
{
	Writer w = { 0 };
	put_ue(&w, 0); /* first_mb_in_slice */
	put_ue(&w, 5); /* P slice */
	put_ue(&w, 0); /* PPS id */
	put_bits(&w, frame, 4);
	put_bits(&w, 0, 1); /* no active-reference override */
	put_bits(&w, 0, 1); /* no list modification */
	put_ue(&w, denominator);
	put_ue(&w, 2); /* chroma denominator */
	put_bits(&w, 1, 1); /* explicit luma weight and offset */
	put_ue(&w, 7); /* se(v) = +4 */
	put_ue(&w, 2); /* se(v) = -1 */
	put_bits(&w, 1, 1); /* explicit weights and offsets for both chroma components */
	for(unsigned i = 0; i < 2; i++)
	{
		put_ue(&w, 7);
		put_ue(&w, 0);
	}
	put_bits(&w, adaptive_marking, 1);
	put_bits(&w, 1, 1);
	const uint8_t prefix[] = { 0, 0, 1, 0x41 };
	memcpy(out, prefix, sizeof(prefix));
	size_t bytes = (w.bit + 7) / 8;
	memcpy(out + sizeof(prefix), w.data, bytes);
	return sizeof(prefix) + bytes;
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

static uint32_t read_u32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8
		| (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

/* Optional local regression. The recorded video stays outside version control. */
static void test_ps3_capture(const char *path, size_t header_size,
	const uint8_t *expected_header, size_t expected_size)
{
	FILE *file = fopen(path, "rb");
	assert(file);
	uint8_t record[28];
	assert(fread(record, 1, sizeof(record), file) == sizeof(record));
	assert(memcmp(record, "PVREPL01", 8) == 0 && read_u32(record + 20) == 300);
	static uint8_t frame[1024 * 1024];
	PlusVitaH264Guard guard = { 0 };
	for(unsigned i = 0; i < 300; i++)
	{
		assert(fread(record, 1, 4, file) == 4);
		size_t size = read_u32(record), offset = 0;
		assert(size && size <= sizeof(frame));
		assert(fread(frame, 1, size, file) == size);
		if(!i)
		{
			assert(size > header_size);
			uint8_t patched[64];
			size_t patched_size = 0;
			assert(!plus_vita_h264_guard_header(frame, header_size,
				patched, sizeof(patched), &patched_size));
			assert(plus_vita_h264_guard_ps3_header(&guard, frame, header_size,
				patched, sizeof(patched), &patched_size));
			assert(patched_size == expected_size && !memcmp(patched, expected_header, patched_size));
			offset = header_size;
		}
		if(!plus_vita_h264_guard_frame(&guard, frame + offset, size - offset))
		{
			fprintf(stderr, "PS3 access unit %u rejected: %s\n", i, guard.reason);
			assert(false);
		}
	}
	assert(fgetc(file) == EOF && !ferror(file));
	assert(fclose(file) == 0);
}

int main(int argc, char **argv)
{
	/* The 720p PS3 profile from the startup diagnosis. */
	const uint8_t ps3_header[] = {
		0,0,0,1,0x67,0x64,0x00,0x29,0xac,0xb1,0x40,0x28,0x02,0xdd,0x21,0x06,
		0x06,0x06,0x40,0x00,0x00,0x03,0x00,0x40,0x00,0x00,0x1e,0x23,0xc6,0x0c,0x8a,0x80,
		0,0,0,1,0x68,0xef,0x3c,0xb0,
	};
	uint8_t patched[64];
	size_t patched_size = 0;
	PlusVitaH264Guard ps3 = { 0 };
	assert(!plus_vita_h264_guard_header(ps3_header, sizeof(ps3_header),
		patched, sizeof(patched), &patched_size));
	assert(plus_vita_h264_guard_ps3_header(&ps3, ps3_header, sizeof(ps3_header),
		patched, sizeof(patched), &patched_size));
	const uint8_t expected_ps3_header[] = {
		0,0,0,1,0x67,0x64,0x00,0x29,0xac,0xb3,0x00,0xa0,0x0b,0x74,0x84,0x18,
		0x18,0x19,0x00,0x00,0x03,0x00,0x01,0x00,0x00,0x03,0x00,0x78,0x8f,0x18,0x32,0x68,
		0,0,0,1,0x68,0xef,0x3c,0xb0,
	};
	assert(patched_size == sizeof(expected_ps3_header));
	assert(!memcmp(patched, expected_ps3_header, patched_size));
	assert(plus_vita_h264_guard_selftest()); /* The accepted seven-bit PS4 guard. */
	uint8_t unsupported_header[sizeof(ps3_header)];
	memcpy(unsupported_header, ps3_header, sizeof(ps3_header));
	unsupported_header[sizeof(ps3_header) - 1] ^= 1;
	assert(!plus_vita_h264_guard_ps3_header(&ps3, unsupported_header,
		sizeof(unsupported_header), patched, sizeof(patched), &patched_size));

	const uint8_t idr[] = { 0, 0, 1, 0x65, 0x88, 0x84, 0x40 };
	assert(plus_vita_h264_guard_frame(&ps3, idr, sizeof(idr)));
	uint8_t slice[32];
	size_t slice_size;
	for(unsigned frame = 1; frame <= 16; frame++)
	{
		slice_size = make_ps3_p_slice(slice, frame % 16, 2, false);
		assert(plus_vita_h264_guard_frame(&ps3, slice, slice_size));
	}
	assert(ps3.frame_num == 0);
	slice_size = make_ps3_p_slice(slice, 1, 2, true);
	assert(!plus_vita_h264_guard_frame(&ps3, slice, slice_size));
	assert(!strcmp(ps3.reason, "ADAPTIVE_REFERENCE_MARKING"));
	slice_size = make_ps3_p_slice(slice, 1, 8, false);
	assert(!plus_vita_h264_guard_frame(&ps3, slice, slice_size));
	assert(!strcmp(ps3.reason, "WEIGHT_TABLE") && ps3.frame_num == 0);

	uint8_t data[320];
	PlusVitaH264Config config;
	assert(plus_vita_h264_config(patched, patched_size, &config));
	assert(config.profile == 100 && config.level == 41);
	assert(config.coded_width == 1280 && config.coded_height == 720);
	assert(config.max_ref_frames == 5 && config.max_buffered_frames == 5
		&& config.max_reorder_frames == 0);
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

	if(argc > 1)
		test_ps3_capture(argv[1], sizeof(ps3_header), expected_ps3_header, sizeof(expected_ps3_header));
	puts("h264_config_test: PASS");
	return 0;
}
