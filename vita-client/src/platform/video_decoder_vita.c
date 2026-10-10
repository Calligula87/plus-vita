// SPDX-License-Identifier: AGPL-3.0-only

#include "video_decoder_vita.h"

#include <psp2/sysmodule.h>
#include <psp2/kernel/processmgr.h>

#include <stdlib.h>
#include <string.h>

#define AVC_REF_FRAMES_MIN 1u
#define AVC_REF_FRAMES_MAX 16u
#define AVC_MEM_ALIGNMENT 0x100000u
#define AVC_INPUT_PADDING 64u

/* Exported by libSceVideodec; absent from this VitaSDK header revision. */
extern int sceAvcdecDecodeStop(SceAvcdecCtrl *, SceAvcdecArrayPicture *);

int plus_vita_video_decoder_drain(PlusVitaVideoDecoder *decoder,
	uint32_t *output, uint32_t pitch, uint32_t width, uint32_t height,
	bool *produced_frame)
{
	if(produced_frame) *produced_frame = false;
	if(!decoder || !decoder->decoder_created || !output || !width || !height
		|| pitch < width || width > 1920 || height > 1088 || pitch > 2048
		|| width % 16 || height % 16 || pitch % 16) return -1;
	SceAvcdecPicture picture = { .size = sizeof(picture) };
	picture.frame.pixelType = SCE_AVCDEC_PIXELFORMAT_RGBA8888;
	picture.frame.framePitch = pitch;
	picture.frame.frameWidth = width;
	picture.frame.frameHeight = height;
	picture.frame.opt.rgba.alpha = 0xff;
	picture.frame.pPicture[0] = output;
	SceAvcdecPicture *ptr = &picture;
	SceAvcdecArrayPicture pictures = { .numOfElm = 1, .pPicture = &ptr };
	int result = sceAvcdecDecodeStop(&decoder->decoder, &pictures);
	CHIAKI_LOGI(decoder->log, "Vita AVC drain result=0x%08x output=%u dimensions=%ux%u",
		result, pictures.numOfOutput, picture.frame.horizontalSize, picture.frame.verticalSize);
	if(result >= 0 && pictures.numOfOutput)
	{
		if(pictures.numOfOutput != 1 || !picture.frame.horizontalSize
			|| !picture.frame.verticalSize || picture.frame.horizontalSize > decoder->width
			|| picture.frame.verticalSize > decoder->height) return -1;
		decoder->outputs++;
		if(produced_frame) *produced_frame = true;
	}
	return result;
}

static uint32_t decoder_dimension(uint32_t value)
{
	return (value + 15u) & ~15u;
}

static uint32_t decoder_mem_size(uint32_t value)
{
	return (value + AVC_MEM_ALIGNMENT - 1u) & ~(AVC_MEM_ALIGNMENT - 1u);
}

void plus_vita_video_decoder_fini(PlusVitaVideoDecoder *decoder)
{
	if(!decoder)
		return;
	if(decoder->decoder_created)
		sceAvcdecDeleteDecoder(&decoder->decoder);
	if(decoder->decoder_block >= 0)
		sceKernelFreeMemBlock(decoder->decoder_block);
	if(decoder->library_initialized)
		sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
	if(decoder->module_loaded)
		sceSysmoduleUnloadModule(SCE_SYSMODULE_AVCDEC);
	memset(decoder, 0, sizeof(*decoder));
	decoder->decoder_block = -1;
}

int plus_vita_video_decoder_init(PlusVitaVideoDecoder *decoder,
	ChiakiLog *log, uint32_t width, uint32_t height,
	uint32_t reference_frames)
{
	if(!decoder || !width || !height || width > 1920 || height > 1088
		|| reference_frames < AVC_REF_FRAMES_MIN
		|| reference_frames > AVC_REF_FRAMES_MAX)
		return -1;
	memset(decoder, 0, sizeof(*decoder));
	decoder->log = log;
	decoder->decoder_block = -1;
	decoder->width = decoder_dimension(width);
	decoder->height = decoder_dimension(height);
	decoder->reference_frames = reference_frames;

	int result = sceSysmoduleLoadModule(SCE_SYSMODULE_AVCDEC);
	CHIAKI_LOGI(log, "Vita AVC module load result=0x%08x", result);
	if(result < 0)
		goto error;
	decoder->module_loaded = true;

	SceVideodecQueryInitInfoHwAvcdec init = {
		.size = sizeof(init),
		.horizontal = decoder->width,
		.vertical = decoder->height,
		.numOfRefFrames = reference_frames,
		.numOfStreams = 1,
	};
	result = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, &init);
	CHIAKI_LOGI(log, "Vita AVC library init %ux%u refs=%u result=0x%08x",
		decoder->width, decoder->height, reference_frames, result);
	if(result < 0)
		goto error;
	decoder->library_initialized = true;

	SceAvcdecQueryDecoderInfo query = {
		.horizontal = decoder->width,
		.vertical = decoder->height,
		.numOfRefFrames = reference_frames,
	};
	SceAvcdecDecoderInfo info = { 0 };
	result = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC,
		&query, &info);
	CHIAKI_LOGI(log, "Vita AVC memory query refs=%u result=0x%08x size=%u",
		reference_frames, result, info.frameMemSize);
	if(result < 0)
		goto error;
	if(!info.frameMemSize)
	{
		result = -1;
		CHIAKI_LOGE(log, "Vita AVC memory query returned zero bytes");
		goto error;
	}

	decoder->decoder.frameBuf.size = decoder_mem_size(info.frameMemSize);
	decoder->decoder_block = sceKernelAllocMemBlock("PlusVitaAvcDecoder",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW,
		decoder->decoder.frameBuf.size, NULL);
	if(decoder->decoder_block < 0)
	{
		result = decoder->decoder_block;
		CHIAKI_LOGE(log, "Vita AVC decoder memory allocation failed=0x%08x",
			result);
		goto error;
	}
	result = sceKernelGetMemBlockBase(decoder->decoder_block,
		&decoder->decoder.frameBuf.pBuf);
	CHIAKI_LOGI(log, "Vita AVC decoder memory base result=0x%08x", result);
	if(result < 0)
		goto error;

	result = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC,
		&decoder->decoder, &query);
	CHIAKI_LOGI(log, "Vita AVC decoder create result=0x%08x", result);
	if(result < 0)
		goto error;
	decoder->decoder_created = true;
	return 0;

error:
	CHIAKI_LOGE(log, "Vita AVC decoder initialization failed=0x%08x", result);
	plus_vita_video_decoder_fini(decoder);
	return result;
}

int plus_vita_video_decoder_decode(PlusVitaVideoDecoder *decoder,
	const uint8_t *header, size_t header_size, const uint8_t *frame,
	size_t frame_size, uint32_t *output, uint32_t output_pitch,
	uint32_t output_width, uint32_t output_height, bool *produced_frame)
{
	uint64_t start = sceKernelGetProcessTimeWide();
	if(decoder)
		decoder->last_prepare_us = decoder->last_decode_us = 0;
	if(produced_frame)
		*produced_frame = false;
	if(!decoder || !decoder->decoder_created || !output
		|| !output_width || !output_height || output_pitch < output_width
		|| output_width > 1920 || output_height > 1088 || output_pitch > 2048
		|| output_width % 16 || output_height % 16 || output_pitch % 16
		|| (header_size && !header)
		|| !frame || !frame_size || header_size > SIZE_MAX - frame_size
		|| header_size + frame_size > UINT32_MAX - AVC_INPUT_PADDING)
	{
		if(decoder)
			CHIAKI_LOGE(decoder->log, "Vita AVC decode rejected invalid input or decoder state");
		return -1;
	}

	size_t input_size = header_size + frame_size;
	uint8_t *input = malloc(input_size + AVC_INPUT_PADDING);
	if(!input)
	{
		CHIAKI_LOGE(decoder->log, "Vita AVC decode input allocation failed: size=%lu",
			(unsigned long)input_size);
		return -1;
	}
	if(header_size)
		memcpy(input, header, header_size);
	memcpy(input + header_size, frame, frame_size);
	memset(input + input_size, 0, AVC_INPUT_PADDING);

	SceAvcdecAu au = { 0 };
	au.es.pBuf = input;
	au.es.size = (uint32_t)input_size;
	au.dts.upper = au.dts.lower = UINT32_MAX;
	au.pts.upper = au.pts.lower = UINT32_MAX;

	SceAvcdecPicture picture = { .size = sizeof(picture) };
	picture.frame.pixelType = SCE_AVCDEC_PIXELFORMAT_RGBA8888;
	/* AVC's output sizing performs the resize during hardware conversion.
	 * The caller supplies a physically contiguous, non-displayed buffer. */
	picture.frame.framePitch = output_pitch;
	picture.frame.frameWidth = output_width;
	picture.frame.frameHeight = output_height;
	picture.frame.opt.rgba.alpha = 0xff;
	picture.frame.pPicture[0] = output;
	SceAvcdecPicture *picture_ptr = &picture;
	SceAvcdecArrayPicture pictures = {
		.numOfElm = 1,
		.pPicture = &picture_ptr,
	};
	decoder->last_prepare_us = sceKernelGetProcessTimeWide() - start;
	start = sceKernelGetProcessTimeWide();
	int result = sceAvcdecDecode(&decoder->decoder, &au, &pictures);
	decoder->last_decode_us = sceKernelGetProcessTimeWide() - start;
	free(input);
	decoder->submissions++;
	if(result < 0 || decoder->submissions <= 4 || decoder->submissions % 60 == 0)
		CHIAKI_LOGI(decoder->log,
		"Vita AVC decode result=0x%08x output=%u size=%lu dimensions=%ux%u",
		result, pictures.numOfOutput, (unsigned long)input_size,
		picture.frame.horizontalSize, picture.frame.verticalSize);
	if(result >= 0 && pictures.numOfOutput > 0)
	{
		if(!picture.frame.horizontalSize || !picture.frame.verticalSize
			|| picture.frame.horizontalSize > decoder->width
			|| picture.frame.verticalSize > decoder->height)
			return -1;
		decoder->picture_width = picture.frame.horizontalSize;
		decoder->picture_height = picture.frame.verticalSize;
		decoder->outputs += pictures.numOfOutput;
		if(produced_frame)
			*produced_frame = true;
	}
	if(result == (int)SCE_AVCDEC_ERROR_OUT_OF_MEMORY)
		CHIAKI_LOGE(decoder->log,
			"Vita AVC decoder reference pool exhausted: refs=%u submissions=%u outputs=%u",
			decoder->reference_frames, decoder->submissions, decoder->outputs);
	return result;
}
