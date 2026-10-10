// SPDX-License-Identifier: AGPL-3.0-only
#include "video_present_vita.h"
#include <string.h>
#include <psp2/kernel/processmgr.h>

#define SCREEN_WIDTH PLUS_VITA_SCREEN_WIDTH
#define SCREEN_HEIGHT PLUS_VITA_SCREEN_HEIGHT
#define SCREEN_PITCH PLUS_VITA_SCREEN_PITCH
#define SCREEN_BYTES ((SCREEN_PITCH * SCREEN_HEIGHT * 4u + 0x3ffffu) & ~0x3ffffu)

int plus_vita_video_present_fini(PlusVitaVideoPresent *present)
{
	if(present->displayed)
	{
		int result = sceDisplaySetFrameBuf(&present->saved, SCE_DISPLAY_SETBUF_NEXTFRAME);
		if(result < 0)
			return result; /* Never release memory still being scanned out. */
		result = sceDisplayWaitSetFrameBuf();
		if(result < 0)
			return result;
		present->displayed = false;
	}
	for(unsigned int i = 0; i < 2; i++)
	{
		if(present->blocks[i] >= 0)
			sceKernelFreeMemBlock(present->blocks[i]);
		present->blocks[i] = -1;
		present->pixels[i] = NULL;
	}
	return 0;
}

int plus_vita_video_present_init(PlusVitaVideoPresent *present)
{
	memset(present, 0, sizeof(*present));
	present->blocks[0] = present->blocks[1] = -1;
	present->saved.size = sizeof(present->saved);
	int result = sceDisplayGetFrameBuf(&present->saved, SCE_DISPLAY_SETBUF_NEXTFRAME);
	if(result < 0 || !present->saved.base)
		return result < 0 ? result : -1;
	for(unsigned int i = 0; i < 2; i++)
	{
		present->blocks[i] = sceKernelAllocMemBlock("PlusVitaPreview",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_BYTES, NULL);
		result = present->blocks[i];
		if(result < 0)
			goto error;
		result = sceKernelGetMemBlockBase(present->blocks[i], (void **)&present->pixels[i]);
		if(result < 0)
			goto error;
		memset(present->pixels[i], 0, SCREEN_BYTES);
	}
	return 0;
error:
	plus_vita_video_present_fini(present);
	return result;
}

uint32_t *plus_vita_video_present_back_buffer(PlusVitaVideoPresent *present)
{
	return present->pixels[present->next];
}

int plus_vita_video_present_frame(PlusVitaVideoPresent *present)
{
	present->last_display_us = present->last_wait_us = 0;
	uint32_t *base = plus_vita_video_present_back_buffer(present);
	if(!base)
		return -1;
	SceDisplayFrameBuf frame = {
		.size = sizeof(frame), .base = base, .pitch = SCREEN_PITCH,
		.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
		.width = SCREEN_WIDTH, .height = SCREEN_HEIGHT,
	};
	uint64_t start = sceKernelGetProcessTimeWide();
	int result = sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
	present->last_display_us = sceKernelGetProcessTimeWide() - start;
	if(result < 0)
		return result;
	present->displayed = true;
	start = sceKernelGetProcessTimeWide();
	result = sceDisplayWaitSetFrameBuf();
	present->last_wait_us = sceKernelGetProcessTimeWide() - start;
	if(result < 0)
		return result;
	present->next ^= 1u;
	return 0;
}
