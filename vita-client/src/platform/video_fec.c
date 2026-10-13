// SPDX-License-Identifier: AGPL-3.0-only
#include "video_fec.h"
#include <chiaki/fec.h>
#include <stdlib.h>
#include <string.h>

/* GF(2^8) Cauchy coding supports at most 256 distinct slots. Bound scratch
 * memory independently of the existing compressed-frame size limit. */
#define FEC_SLOTS_MAX 256u
#define FEC_UNIT_BYTES_MAX 65536u
#define FEC_WORK_BYTES_MAX (2u * 1024u * 1024u)

static unsigned unit_padding(const uint8_t *unit)
{
	return ((unsigned)unit[0] << 8) | unit[1];
}

ChiakiErrorCode plus_vita_video_fec_repair(uint8_t **units, size_t *sizes,
	unsigned source_count, unsigned parity_count, unsigned *repaired)
{
	if(!repaired) return CHIAKI_ERR_INVALID_DATA;
	*repaired = 0;
	if(!units || !sizes || !source_count || !parity_count
		|| source_count >= FEC_SLOTS_MAX
		|| parity_count > FEC_SLOTS_MAX - source_count)
		return CHIAKI_ERR_INVALID_DATA;
	unsigned total = source_count + parity_count;
	unsigned received = 0, missing_source = 0;
	size_t unit_size = 0;
	for(unsigned i = 0; i < total; i++)
	{
		if(!units[i])
		{
			if(sizes[i]) return CHIAKI_ERR_INVALID_DATA;
			if(i < source_count) missing_source++;
			continue;
		}
		if(sizes[i] < 2 || sizes[i] > FEC_UNIT_BYTES_MAX)
			return CHIAKI_ERR_INVALID_DATA;
		size_t width = sizes[i];
		if(i < source_count) width += unit_padding(units[i]);
		if(width > FEC_UNIT_BYTES_MAX || (unit_size && width != unit_size))
			return CHIAKI_ERR_INVALID_DATA;
		unit_size = width;
		received++;
	}
	if(!missing_source) return CHIAKI_ERR_SUCCESS;
	if(received < source_count) return CHIAKI_ERR_BUF_TOO_SMALL;
	size_t stride = (unit_size + 15u) & ~(size_t)15u;
	if(!stride || total > FEC_WORK_BYTES_MAX / stride)
		return CHIAKI_ERR_OVERFLOW;
	uint8_t *work = calloc(total, stride);
	if(!work) return CHIAKI_ERR_MEMORY;
	unsigned erasures[FEC_SLOTS_MAX];
	unsigned erased = 0;
	for(unsigned i = 0; i < total; i++)
	{
		if(units[i]) memcpy(work + i * stride, units[i], sizes[i]);
		else erasures[erased++] = i;
	}
	ChiakiErrorCode err = chiaki_fec_decode(work, unit_size, stride,
		source_count, parity_count, erasures, erased);
	if(err != CHIAKI_ERR_SUCCESS) { free(work); return err; }

	uint8_t *replacements[FEC_SLOTS_MAX] = { 0 };
	size_t restored_sizes[FEC_SLOTS_MAX] = { 0 };
	for(unsigned i = 0; i < source_count; i++)
	{
		const uint8_t *data = work + i * stride;
		unsigned padding = unit_padding(data);
		if(padding > unit_size - 2)
		{ err = CHIAKI_ERR_INVALID_DATA; goto failed; }
		size_t actual = unit_size - padding;
		if(units[i])
		{
			if(actual != sizes[i] || memcmp(data, units[i], actual))
			{ err = CHIAKI_ERR_INVALID_DATA; goto failed; }
			continue;
		}
		/* Padding is implicit zero on the wire, including after recovery. */
		for(size_t j = actual; j < unit_size; j++)
			if(data[j]) { err = CHIAKI_ERR_INVALID_DATA; goto failed; }
		replacements[i] = malloc(actual);
		if(!replacements[i]) { err = CHIAKI_ERR_MEMORY; goto failed; }
		memcpy(replacements[i], data, actual);
		restored_sizes[i] = actual;
	}
	for(unsigned i = 0; i < source_count; i++)
		if(replacements[i])
		{
			units[i] = replacements[i];
			sizes[i] = restored_sizes[i];
		}
	*repaired = missing_source;
	free(work);
	return CHIAKI_ERR_SUCCESS;
failed:
	for(unsigned i = 0; i < source_count; i++) free(replacements[i]);
	free(work);
	return err;
}

bool plus_vita_video_fec_selftest(void)
{
	/* GF(256), polynomial 0x11d, Cauchy coefficient 1/(row XOR (2+col)).
	 * These parity bytes were calculated independently of chiaki_fec_encode. */
	static const uint8_t fixture[5][8] = {
		{ 0, 0, 1, 2, 3, 4, 5, 6 },
		{ 0, 2, 7, 8, 9, 10, 0, 0 },
		{ 0, 4, 11, 12, 0, 0, 0, 0 },
		{ 0, 244, 179, 241, 136, 4, 140, 3 },
		{ 0, 167, 220, 6, 139, 242, 3, 2 }
	};
	uint8_t *units[5] = { (uint8_t *)fixture[0], NULL, NULL,
		(uint8_t *)fixture[3], (uint8_t *)fixture[4] };
	size_t sizes[5] = { 8, 0, 0, 8, 8 };
	unsigned repaired;
	ChiakiErrorCode err = plus_vita_video_fec_repair(units, sizes, 3, 2, &repaired);
	bool valid = err == CHIAKI_ERR_SUCCESS && repaired == 2
		&& sizes[1] == 6 && sizes[2] == 4
		&& !memcmp(units[1], fixture[1], 6)
		&& !memcmp(units[2], fixture[2], 4);
	free(units[1]); free(units[2]);
	return valid;
}
