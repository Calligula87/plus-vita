// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_VIDEO_PRESENT_H
#define PLUS_VITA_VIDEO_PRESENT_H
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <stdint.h>
#include <stdbool.h>

#define PLUS_VITA_SCREEN_WIDTH 960u
#define PLUS_VITA_SCREEN_HEIGHT 544u
#define PLUS_VITA_SCREEN_PITCH 1024u

typedef struct {
	SceDisplayFrameBuf saved;
	SceUID blocks[2];
	uint32_t *pixels[2];
	unsigned int next;
	bool displayed;
	uint64_t last_display_us;
	uint64_t last_wait_us;
} PlusVitaVideoPresent;

int plus_vita_video_present_init(PlusVitaVideoPresent *present);
/* Decoder writes only to this back buffer, never the active scanout. */
uint32_t *plus_vita_video_present_back_buffer(PlusVitaVideoPresent *present);
int plus_vita_video_present_frame(PlusVitaVideoPresent *present);
int plus_vita_video_present_fini(PlusVitaVideoPresent *present);
#endif
