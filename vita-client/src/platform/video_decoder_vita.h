// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_VIDEO_DECODER_VITA_H
#define PLUS_VITA_VIDEO_DECODER_VITA_H

#include <chiaki/log.h>

#include <psp2/kernel/sysmem.h>
#include <psp2/types.h>
#include <psp2/videodec.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct plus_vita_video_decoder_t
{
	ChiakiLog *log;
	SceAvcdecCtrl decoder;
	SceUID decoder_block;
	uint32_t width;
	uint32_t height;
	uint32_t picture_width;
	uint32_t picture_height;
	uint32_t reference_frames;
	unsigned int submissions;
	unsigned int outputs;
	uint64_t last_prepare_us;
	uint64_t last_decode_us;
	bool module_loaded;
	bool library_initialized;
	bool decoder_created;
} PlusVitaVideoDecoder;

int plus_vita_video_decoder_init(PlusVitaVideoDecoder *decoder,
	ChiakiLog *log, uint32_t width, uint32_t height,
	uint32_t reference_frames);
/* output is borrowed, physically contiguous RGBA memory of at least
 * output_pitch * output_height * 4 bytes; it must not be active scanout.
 * The caller keeps it alive until the synchronous decode returns. */
int plus_vita_video_decoder_decode(PlusVitaVideoDecoder *decoder,
	const uint8_t *header, size_t header_size, const uint8_t *frame,
	size_t frame_size, uint32_t *output, uint32_t output_pitch,
	uint32_t output_width, uint32_t output_height, bool *produced_frame);
void plus_vita_video_decoder_fini(PlusVitaVideoDecoder *decoder);
/* End-of-stream only: collect a buffered picture without submitting an AU. */
int plus_vita_video_decoder_drain(PlusVitaVideoDecoder *decoder,
	uint32_t *output, uint32_t pitch, uint32_t width, uint32_t height,
	bool *produced_frame);

#endif
