// SPDX-License-Identifier: AGPL-3.0-only
#include "h264_config.h"
#include <string.h>

typedef struct {
	uint8_t data[2048];
	size_t size, bit;
	bool ok;
} Bits;

static uint32_t bits(Bits *b, unsigned count)
{
	if(!b->ok || count > 32 || b->bit > b->size * 8 || count > b->size * 8 - b->bit)
	{
		b->ok = false;
		return 0;
	}
	uint32_t value = 0;
	while(count--)
	{
		value = (value << 1) | ((b->data[b->bit / 8] >> (7 - b->bit % 8)) & 1);
		b->bit++;
	}
	return value;
}

static uint32_t ue(Bits *b)
{
	unsigned zeros = 0;
	while(b->ok && bits(b, 1) == 0)
		if(++zeros > 31) { b->ok = false; return 0; }
	uint32_t suffix = bits(b, zeros);
	return b->ok ? ((1u << zeros) - 1u) + suffix : 0;
}

static int64_t se(Bits *b)
{
	uint32_t value = ue(b);
	return value & 1u ? ((int64_t)value + 1) / 2 : -(int64_t)(value / 2);
}

static void scaling_list(Bits *b, unsigned count)
{
	int last = 8, next = 8;
	for(unsigned i = 0; b->ok && i < count; i++)
	{
		if(next)
			next = (int)(((int64_t)last + se(b)) % 256 + 256) % 256;
		if(next) last = next;
	}
}

static void hrd(Bits *b)
{
	uint32_t count = ue(b);
	if(count > 31) { b->ok = false; return; }
	bits(b, 8);
	for(uint32_t i = 0; b->ok && i <= count; i++)
	{
		ue(b); ue(b); bits(b, 1);
	}
	bits(b, 20);
}

static void vui(Bits *b, PlusVitaH264Config *c)
{
	if(bits(b, 1) && bits(b, 8) == 255) { bits(b, 16); bits(b, 16); }
	if(bits(b, 1)) bits(b, 1);
	if(bits(b, 1))
	{
		bits(b, 4);
		if(bits(b, 1)) bits(b, 24);
	}
	if(bits(b, 1)) { ue(b); ue(b); }
	if(bits(b, 1)) { bits(b, 32); bits(b, 32); bits(b, 1); }
	bool nal_hrd = bits(b, 1) != 0;
	if(nal_hrd) hrd(b);
	bool vcl_hrd = bits(b, 1) != 0;
	if(vcl_hrd) hrd(b);
	if(nal_hrd || vcl_hrd) bits(b, 1);
	bits(b, 1);
	c->has_buffer_restriction = bits(b, 1) != 0;
	if(c->has_buffer_restriction)
	{
		bits(b, 1);
		ue(b); ue(b); ue(b); ue(b);
		c->max_reorder_frames = ue(b);
		c->max_buffered_frames = ue(b);
		if(c->max_buffered_frames > 16 || c->max_buffered_frames < c->max_ref_frames
			|| c->max_reorder_frames > c->max_buffered_frames)
			b->ok = false;
	}
}

static size_t startcode(const uint8_t *data, size_t size, size_t pos)
{
	if(pos > size || size - pos < 3 || data[pos] || data[pos + 1]) return 0;
	if(data[pos + 2] == 1) return 3;
	if(size - pos >= 4 && !data[pos + 2] && data[pos + 3] == 1) return 4;
	return 0;
}

bool plus_vita_h264_config(const uint8_t *data, size_t size, PlusVitaH264Config *c)
{
	if(!data || !c || !size || size > 2048) return false;
	memset(c, 0, sizeof(*c));
	Bits b = { .ok = true };
	bool found = false;
	for(size_t pos = 0; pos < size; pos++)
	{
		size_t prefix = startcode(data, size, pos);
		if(!prefix || pos + prefix >= size) continue;
		pos += prefix;
		uint8_t nal = data[pos];
		size_t end = pos + 1;
		while(end < size && !startcode(data, size, end)) end++;
		if((nal & 31) != 7) { pos = end - 1; continue; }
		if(nal & 0x80) return false;
		unsigned zeros = 0;
		for(size_t i = pos + 1; i < end; i++)
		{
			uint8_t byte = data[i];
			if(zeros >= 2 && byte == 3)
			{
				if(i + 1 >= end || data[i + 1] > 3) return false;
				zeros = 0;
				continue;
			}
			b.data[b.size++] = byte;
			zeros = byte ? 0 : zeros + 1;
		}
		found = true;
		break;
	}
	if(!found) return false;
	c->profile = bits(&b, 8);
	bits(&b, 8);
	c->level = bits(&b, 8);
	if(ue(&b) > 31) return false;
	uint32_t chroma = 1;
	switch(c->profile)
	{
		case 100: case 110: case 122: case 244: case 44: case 83: case 86:
		case 118: case 128: case 138: case 139: case 134: case 135:
			chroma = ue(&b);
			if(chroma > 3) return false;
			if(chroma == 3) bits(&b, 1);
			/* Vita's preview supports 8-bit 4:2:0 AVC. */
			if(ue(&b) != 0 || ue(&b) != 0 || chroma != 1) return false;
			bits(&b, 1);
			if(bits(&b, 1))
				for(unsigned i = 0; b.ok && i < 8; i++)
					if(bits(&b, 1)) scaling_list(&b, i < 6 ? 16 : 64);
			break;
		case 66: case 77: case 88: break;
		default: return false;
	}
	if(ue(&b) > 12) return false;
	uint32_t poc = ue(&b);
	if(poc == 0) { if(ue(&b) > 12) return false; }
	else if(poc == 1)
	{
		bits(&b, 1); se(&b); se(&b);
		uint32_t cycle = ue(&b);
		if(cycle > 255) return false;
		for(uint32_t i = 0; b.ok && i < cycle; i++) se(&b);
	}
	else if(poc != 2) return false;
	c->max_ref_frames = ue(&b);
	if(c->max_ref_frames > 16) return false;
	bits(&b, 1);
	uint32_t width_mbs = ue(&b), height_maps = ue(&b);
	if(width_mbs >= 120 || height_maps >= 68) return false;
	bool progressive = bits(&b, 1) != 0;
	if(!progressive) return false;
	c->coded_width = (width_mbs + 1) * 16;
	c->coded_height = (height_maps + 1) * 16;
	bits(&b, 1);
	if(bits(&b, 1)) { ue(&b); ue(&b); ue(&b); ue(&b); }
	if(bits(&b, 1)) vui(&b, c);
	/* rbsp_stop_one_bit, followed by zero padding/trailing_zero_8bits. */
	if(!bits(&b, 1)) return false;
	while(b.ok && b.bit < b.size * 8)
		if(bits(&b, 1)) return false;
	return b.ok;
}

uint32_t plus_vita_h264_decoder_slots(const PlusVitaH264Config *c)
{
	if(!c || c->max_ref_frames > 16 || c->max_buffered_frames > 16
		|| (c->has_buffer_restriction && (c->max_buffered_frames < c->max_ref_frames
			|| c->max_reorder_frames > c->max_buffered_frames))) return 0;
	uint32_t required = c->max_ref_frames;
	if(c->has_buffer_restriction && c->max_buffered_frames > required)
		required = c->max_buffered_frames;
	return required ? required : 1;
}

static const uint8_t audited_sps[] = {
	0x67,0x4d,0x40,0x28,0x91,0x8a,0x01,0x40,0x16,0xec,0x05,0xa8,0x08,
	0x08,0x0a,0x00,0x00,0x0f,0xa4,0x00,0x07,0x53,0x01,0x13,0x43,0xc7,0xc5,0x40 };
static const uint8_t audited_pps[] = { 0x68,0xee,0x3c,0x80 };
static const uint8_t patched_sps[] = {
	0x67,0x4d,0x40,0x28,0x91,0x98,0x05,0x00,0x5b,0xb0,0x16,0xa0,0x20,
	0x20,0x28,0x00,0x00,0x3e,0x90,0x00,0x1d,0x4c,0x04,0x4d,0x0f,0x1f,0x34 };

bool plus_vita_h264_guard_header(const uint8_t *data, size_t size,
	uint8_t *patched, size_t capacity, size_t *patched_size)
{
	if(!data || !patched || !patched_size || size > 2048) return false;
	bool sps = false, pps = false;
	size_t pos = 0;
	while(pos < size)
	{
		size_t prefix = startcode(data, size, pos);
		if(!prefix || pos + prefix >= size) return false;
		size_t begin = pos + prefix, end = begin + 1;
		while(end < size && !startcode(data, size, end)) end++;
		if(!sps && !pps && end - begin == sizeof(audited_sps)
			&& !memcmp(data + begin, audited_sps, sizeof(audited_sps))) sps = true;
		else if(sps && !pps && end - begin == sizeof(audited_pps)
			&& !memcmp(data + begin, audited_pps, sizeof(audited_pps))) pps = true;
		else return false;
		pos = end;
	}
	size_t needed = 8 + sizeof(patched_sps) + sizeof(audited_pps);
	if(!sps || !pps || capacity < needed) return false;
	const uint8_t prefix[] = { 0,0,0,1 };
	memcpy(patched, prefix, 4);
	memcpy(patched + 4, patched_sps, sizeof(patched_sps));
	memcpy(patched + 4 + sizeof(patched_sps), prefix, 4);
	memcpy(patched + 8 + sizeof(patched_sps), audited_pps, sizeof(audited_pps));
	*patched_size = needed;
	return true;
}

static bool guard_reject(PlusVitaH264Guard *guard, const char *reason)
{
	if(guard) guard->reason = reason;
	return false;
}

bool plus_vita_h264_guard_frame(PlusVitaH264Guard *guard, const uint8_t *data, size_t size)
{
	if(!guard || !data || !size || size > 1024u * 1024u) return guard_reject(guard, "INVALID_BOUNDS");
	guard->reason = "NONE";
	guard->observed_frame = guard->observed_mb = guard->observed_slice = guard->observed_nal = UINT32_MAX;
	bool picture = false, idr = false;
	uint32_t frame = 0, previous_mb = 0, idr_id = UINT32_MAX;
	size_t pos = 0;
	while(pos < size)
	{
		size_t prefix = startcode(data, size, pos);
		if(!prefix || pos + prefix >= size) return guard_reject(guard, "ANNEXB_PREFIX");
		size_t begin = pos + prefix, end = begin + 1;
		while(end < size && !startcode(data, size, end)) end++;
		uint8_t nal = data[begin], type = nal & 31;
		guard->observed_nal = type;
		if(nal & 0x80) return guard_reject(guard, "FORBIDDEN_NAL_BIT");
		/* No in-band parameter replacement or unsupported VCL syntax. */
		if(type != 1 && type != 5)
		{
			if(type != 6 && type != 9 && type != 12) return guard_reject(guard, "UNSUPPORTED_OR_INBAND_PARAMETER_NAL");
			pos = end; continue;
		}
		if((type == 1 && (nal >> 5) != 2) || (type == 5 && (nal >> 5) != 3)) return guard_reject(guard, "NAL_REF_IDC");
		Bits b = { .ok = true };
		unsigned zeros = 0;
		/* Only the header prefix is needed; never copy an entire slice to stack. */
		for(size_t i = begin + 1; i < end && b.size < 128; i++)
		{
			uint8_t byte = data[i];
			if(zeros >= 2 && byte == 3)
			{
				if(i + 1 >= end || data[i + 1] > 3) return guard_reject(guard, "EMULATION_PREVENTION");
				zeros = 0; continue;
			}
			b.data[b.size++] = byte;
			zeros = byte ? 0 : zeros + 1;
		}
		uint32_t mb = ue(&b), slice = ue(&b), pps = ue(&b), number = bits(&b, 7);
		guard->observed_frame = number; guard->observed_mb = mb; guard->observed_slice = slice;
		if(!b.ok) return guard_reject(guard, "TRUNCATED_SLICE_HEADER");
		bool is_idr = type == 5;
		if(pps) return guard_reject(guard, "PPS_ID");
		if(mb >= 3600) return guard_reject(guard, "MACROBLOCK_BOUNDS");
		if(slice != (is_idr ? 7u : 5u)) return guard_reject(guard, "SLICE_TYPE");
		if(!picture)
		{
			if(mb) return guard_reject(guard, "MISSING_FIRST_SLICE");
			if(is_idr && number) return guard_reject(guard, "IDR_FRAME_NUM");
			if(!is_idr && !guard->started) return guard_reject(guard, "IDR_REQUIRED");
			if(!is_idr && number != (guard->frame_num + 1) % 128) return guard_reject(guard, "FRAME_NUM_GAP");
			picture = true; frame = number; idr = is_idr;
		}
		else if(number != frame || is_idr != idr || mb <= previous_mb) return guard_reject(guard, "SLICE_ORDER_OR_PICTURE_MISMATCH");
		previous_mb = mb;
		if(is_idr)
		{
			uint32_t observed_id = ue(&b);
			if(observed_id > 65535 || (idr_id != UINT32_MAX && observed_id != idr_id))
				return guard_reject(guard, "IDR_PICTURE_ID_MISMATCH");
			idr_id = observed_id;
			bits(&b, 1); /* no_output_of_prior_pics_flag */
			if(bits(&b, 1)) return guard_reject(guard, "LONG_TERM_IDR");
		}
		else
		{
			if(bits(&b, 1)) return guard_reject(guard, "ACTIVE_REF_OVERRIDE");
			if(bits(&b, 1)) return guard_reject(guard, "REF_LIST_MODIFICATION");
			if(bits(&b, 1)) return guard_reject(guard, "ADAPTIVE_REFERENCE_MARKING");
		}
		/* P: no active-count override, list modification, or adaptive marking.
		 * Audited PPS disables weighted prediction and redundant_pic_cnt. */
		if(!b.ok) return guard_reject(guard, "TRUNCATED_REFERENCE_HEADER");
		pos = end;
	}
	if(!picture) return guard_reject(guard, "NO_VCL_PICTURE");
	if(idr) guard->idr_count++;
	guard->started = true; guard->frame_num = frame;
	return true;
}

bool plus_vita_h264_guard_selftest(void)
{
	/* IDR header with long_term_reference_flag=1 must remain forbidden. */
	const uint8_t long_term_idr[] = { 0,0,1,0x65,0x88,0x80,0xa0 };
	PlusVitaH264Guard rejected = { .started = true, .frame_num = 76 };
	if(plus_vita_h264_guard_frame(&rejected, long_term_idr, sizeof(long_term_idr))
		|| !rejected.reason || strcmp(rejected.reason, "LONG_TERM_IDR")
		|| rejected.frame_num != 76 || rejected.idr_count) return false;
	const uint8_t short_term_idr[] = { 0,0,1,0x65,0x88,0x80,0x80 };
	if(!plus_vita_h264_guard_frame(&rejected, short_term_idr, sizeof(short_term_idr))
		|| rejected.frame_num != 0 || rejected.idr_count != 1) return false;
	uint8_t header[8 + sizeof(audited_sps) + sizeof(audited_pps)] = { 0,0,0,1 };
	memcpy(header + 4, audited_sps, sizeof(audited_sps));
	header[7 + sizeof(audited_sps)] = 1;
	memcpy(header + 8 + sizeof(audited_sps), audited_pps, sizeof(audited_pps));
	uint8_t patched[64]; size_t size = 0;
	PlusVitaH264Config config;
	if(!plus_vita_h264_guard_header(header, sizeof(header), patched, sizeof(patched), &size)
		|| !plus_vita_h264_config(patched, size, &config)
		|| config.max_ref_frames != 5 || config.max_buffered_frames != 5
		|| config.coded_width != 1280 || config.coded_height != 720) return false;
	header[5] ^= 1;
	if(plus_vita_h264_guard_header(header, sizeof(header), patched, sizeof(patched), &size)) return false;
	header[5] ^= 1;
	return !plus_vita_h264_guard_header(header, sizeof(header) - 1, patched, sizeof(patched), &size);
}
