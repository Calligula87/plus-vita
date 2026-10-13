// SPDX-License-Identifier: AGPL-3.0-only
#include "video_fec.h"
#include "video_reorder.h"
#include <chiaki/base64.h>
#include <chiaki/fec.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	unsigned k, m;
	const int erasures[16];
	const char *frame_buffer_b64;
	const size_t unit_size;
} FECTestCase;
#include "fec_test_cases.inl"

static unsigned padding(const uint8_t *data)
{
	return ((unsigned)data[0] << 8) | data[1];
}

static void upstream_vectors(void)
{
	unsigned wrapper_cases = 0;
	for(unsigned t = 0; t < sizeof(fec_test_cases) / sizeof(fec_test_cases[0]); t++)
	{
		const FECTestCase *tc = &fec_test_cases[t];
		size_t decoded_size = strlen(tc->frame_buffer_b64);
		uint8_t *reference = malloc(decoded_size);
		assert(reference);
		assert(chiaki_base64_decode(tc->frame_buffer_b64, decoded_size,
			reference, &decoded_size) == CHIAKI_ERR_SUCCESS);
		assert(decoded_size == tc->unit_size * (tc->k + tc->m));
		size_t stride = (tc->unit_size + 15u) & ~(size_t)15u;
		uint8_t *work = calloc(tc->k + tc->m, stride);
		assert(work);
		for(unsigned i = 0; i < tc->k + tc->m; i++)
			memcpy(work + i * stride, reference + i * tc->unit_size, tc->unit_size);
		unsigned erased[16], erasures = 0;
		for(unsigned i = 0; i < 16 && tc->erasures[i] >= 0; i++)
		{
			erased[erasures++] = (unsigned)tc->erasures[i];
			assert(erased[i] < tc->k + tc->m);
			memset(work + erased[i] * stride, 0x42, tc->unit_size);
		}
		assert(chiaki_fec_decode(work, tc->unit_size, stride, tc->k, tc->m,
			erased, erasures) == CHIAKI_ERR_SUCCESS);
		for(unsigned i = 0; i < tc->k; i++)
			assert(!memcmp(work + i * stride, reference + i * tc->unit_size, tc->unit_size));

		/* Apply the actual video wrapper to independently supplied parity. */
		bool valid_headers = true;
		for(unsigned i = 0; i < tc->k; i++)
			if(padding(reference + i * tc->unit_size) > tc->unit_size - 2)
				valid_headers = false;
		if(valid_headers)
		{
			uint8_t *units[256] = { 0 };
			size_t sizes[256] = { 0 };
			assert(tc->k + tc->m <= 256);
			unsigned missing_source = 0;
			for(unsigned i = 0; i < tc->k + tc->m; i++)
			{
				bool missing = false;
				for(unsigned j = 0; j < erasures; j++) if(erased[j] == i) missing = true;
				if(missing) { if(i < tc->k) missing_source++; continue; }
				const uint8_t *src = reference + i * tc->unit_size;
				sizes[i] = tc->unit_size - (i < tc->k ? padding(src) : 0);
				units[i] = malloc(sizes[i]); assert(units[i]);
				memcpy(units[i], src, sizes[i]);
			}
			unsigned repaired;
			assert(plus_vita_video_fec_repair(units, sizes, tc->k, tc->m,
				&repaired) == CHIAKI_ERR_SUCCESS);
			assert(repaired == missing_source);
			for(unsigned i = 0; i < tc->k; i++)
			{
				assert(sizes[i] == tc->unit_size - padding(reference + i * tc->unit_size));
				assert(!memcmp(units[i], reference + i * tc->unit_size, sizes[i]));
			}
			for(unsigned i = 0; i < tc->k + tc->m; i++) free(units[i]);
			wrapper_cases++;
		}
		free(work); free(reference);
	}
	assert(wrapper_cases > 0);
	printf("PASS: %zu upstream FEC vectors (%u with video length headers); ids=%s..%s\n",
		sizeof(fec_test_cases) / sizeof(fec_test_cases[0]), wrapper_cases,
		fec_test_case_ids[0], fec_test_case_ids[63]);
}

/* Independent scalar GF(256) encoder, never chiaki_fec_encode. */
static uint8_t gf_mul(unsigned a, unsigned b)
{
	unsigned r = 0;
	while(b) { if(b & 1) r ^= a; b >>= 1; a <<= 1; if(a & 256) a ^= 0x11d; }
	return (uint8_t)r;
}

static uint8_t gf_inverse(unsigned v)
{
	unsigned r = 1;
	for(unsigned i = 0; i < 254; i++) r = gf_mul(r, v);
	return (uint8_t)r;
}

static void reference_encode(uint8_t *data, unsigned k, unsigned m, size_t width)
{
	for(unsigned row = 0; row < m; row++) for(unsigned col = 0; col < k; col++)
	{
		unsigned coefficient = gf_inverse(row ^ (m + col));
		for(size_t j = 0; j < width; j++)
			data[(k + row) * width + j] ^= gf_mul(coefficient, data[col * width + j]);
	}
}

static void controlled_loss(unsigned k, unsigned m, size_t width)
{
	PlusVitaVideoReorder window = { 0 };
	int old_slot = plus_vita_video_reorder_admit(&window, 65535, 0);
	int next_slot = plus_vita_video_reorder_admit(&window, 0, 1000);
	assert(old_slot >= 0 && next_slot >= 0 && old_slot != next_slot);
	plus_vita_video_reorder_ready(&window, next_slot, 2000);
	assert(plus_vita_video_reorder_poll(&window, 2000, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	uint8_t *data = calloc(k + m, width); assert(data);
	uint8_t *units[256] = { 0 };
	size_t sizes[256] = { 0 };
	for(unsigned i = 0; i < k; i++)
	{
		unsigned pad = i % (width < 17 ? width - 1 : 16);
		data[i * width] = (uint8_t)(pad >> 8); data[i * width + 1] = (uint8_t)pad;
		for(size_t j = 2; j < width - pad; j++) data[i * width + j] = (uint8_t)(i * 53 + j * 19);
	}
	reference_encode(data, k, m, width);
	/* Omit first and last source units, and one parity unit when possible. */
	unsigned missing = 0;
	for(unsigned i = 0; i < k + m; i++)
	{
		if(i == 0 || i == k - 1 || (m >= 3 && i == k))
		{ if(i < k) missing++; continue; }
		sizes[i] = width - (i < k ? padding(data + i * width) : 0);
		units[i] = malloc(sizes[i]); assert(units[i]);
		memcpy(units[i], data + i * width, sizes[i]);
	}
	unsigned repaired;
	assert(plus_vita_video_fec_repair(units, sizes, k, m, &repaired) == CHIAKI_ERR_SUCCESS);
	assert(repaired == missing);
	for(unsigned i = 0; i < k; i++)
	{
		assert(sizes[i] == width - padding(data + i * width));
		assert(!memcmp(units[i], data + i * width, sizes[i]));
	}
	/* A later parity packet completes the predecessor before expiry. Both
	 * source bytes/lengths and release order are validated, including wrap. */
	assert(plus_vita_video_reorder_admit(&window, 65535, 10000) == old_slot);
	plus_vita_video_reorder_ready(&window, old_slot, 10000);
	PlusVitaVideoReorderEvent event = plus_vita_video_reorder_poll(&window, 10000, false);
	assert(event.action == PLUS_VITA_VIDEO_REORDER_READY && event.index == 65535 && event.wait_us == 9000);
	event = plus_vita_video_reorder_poll(&window, 10000, false);
	assert(event.action == PLUS_VITA_VIDEO_REORDER_READY && event.index == 0 && event.held_us == 8000);
	for(unsigned i = 0; i < k + m; i++) free(units[i]);
	free(data);
}

static void rejected_inputs(void)
{
	uint8_t zero[8] = { 0 };
	uint8_t *units[256] = { 0 };
	size_t sizes[256] = { 0 };
	unsigned repaired;
	units[3] = zero; units[4] = zero; sizes[3] = sizes[4] = 8;
	assert(plus_vita_video_fec_repair(units, sizes, 3, 2, &repaired) == CHIAKI_ERR_BUF_TOO_SMALL);
	assert(!repaired && !units[0] && !units[1] && !units[2]);
	assert(plus_vita_video_fec_repair(units, sizes, 256, 1, &repaired) == CHIAKI_ERR_INVALID_DATA);
	assert(plus_vita_video_fec_repair(units, sizes, 3, 0, &repaired) == CHIAKI_ERR_INVALID_DATA);
	units[0] = zero; sizes[0] = 8; sizes[4] = 7;
	assert(plus_vita_video_fec_repair(units, sizes, 3, 2, &repaired) == CHIAKI_ERR_INVALID_DATA);
	assert(!units[1] && !units[2] && units[0] == zero);

	/* A reconstructed header cannot describe less than its own two bytes. */
	uint8_t bad_header[8] = { 255, 255 };
	uint8_t *one[2] = { NULL, bad_header };
	size_t one_sizes[2] = { 0, 8 };
	assert(plus_vita_video_fec_repair(one, one_sizes, 1, 1, &repaired) == CHIAKI_ERR_INVALID_DATA);
	assert(!one[0] && !one_sizes[0]);
	uint8_t bad_padding[8] = { 0, 4, 1, 2, 3, 4, 5, 6 };
	one[1] = bad_padding;
	assert(plus_vita_video_fec_repair(one, one_sizes, 1, 1, &repaired) == CHIAKI_ERR_INVALID_DATA);
	assert(!one[0] && !one_sizes[0]);

	memset(units, 0, sizeof(units)); memset(sizes, 0, sizeof(sizes));
	uint8_t *wide = calloc(20000, 1); assert(wide);
	for(unsigned i = 1; i < 129; i++) { units[i] = wide; sizes[i] = 20000; }
	assert(plus_vita_video_fec_repair(units, sizes, 128, 1, &repaired) == CHIAKI_ERR_OVERFLOW);
	assert(!units[0] && !sizes[0]); free(wide);
	puts("PASS: excessive loss, inconsistent widths, invalid restored headers/padding, unsupported geometry and memory bounds are rejected without partial output");
}

int main(void)
{
	assert(plus_vita_video_fec_selftest());
	upstream_vectors();
	const size_t widths[] = { 8, 9, 31, 1400, 1407 };
	for(unsigned i = 0; i < sizeof(widths) / sizeof(widths[0]); i++)
	{
		controlled_loss(3, 2, widths[i]);
		controlled_loss(16, 4, widths[i]);
		controlled_loss(64, 4, widths[i]);
	}
	rejected_inputs();
	puts("PASS: independent parity restores exact source bytes/lengths and ordered late-frame release in 15 controlled-loss cases");
	return 0;
}
