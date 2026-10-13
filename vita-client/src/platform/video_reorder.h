// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_VIDEO_REORDER_H
#define PLUS_VITA_VIDEO_REORDER_H
#include <stdbool.h>
#include <stdint.h>

#define PLUS_VITA_VIDEO_REORDER_SLOTS 3
#define PLUS_VITA_VIDEO_REORDER_GRACE_US 20000u
#define PLUS_VITA_VIDEO_REORDER_STALE (-1)
#define PLUS_VITA_VIDEO_REORDER_FULL (-2)

/* Ownership remains with the caller. Each slot maps to one packet assembly.
 * Call poll before admitting packets, including on a timer without packets.
 * FULL requires a pressure poll, then retry; never evict ready frames. */
typedef struct {
	bool used, ready;
	uint16_t index;
	uint64_t arrived_us, ready_us;
} PlusVitaVideoReorderSlot;
typedef struct {
	bool started, waiting;
	uint16_t next;
	uint64_t waiting_since_us;
	PlusVitaVideoReorderSlot slots[PLUS_VITA_VIDEO_REORDER_SLOTS];
} PlusVitaVideoReorder;
typedef enum {
	PLUS_VITA_VIDEO_REORDER_WAIT,
	PLUS_VITA_VIDEO_REORDER_READY,
	PLUS_VITA_VIDEO_REORDER_DROP
} PlusVitaVideoReorderAction;
typedef struct {
	PlusVitaVideoReorderAction action;
	int slot; /* -1 for a wholly absent frame/gap */
	uint16_t index, skipped;
	uint64_t wait_us, held_us;
	bool pressure;
} PlusVitaVideoReorderEvent;

int plus_vita_video_reorder_admit(PlusVitaVideoReorder *window,
	uint16_t index, uint64_t now_us);
void plus_vita_video_reorder_ready(PlusVitaVideoReorder *window,
	int slot, uint64_t now_us);
PlusVitaVideoReorderEvent plus_vita_video_reorder_poll(
	PlusVitaVideoReorder *window, uint64_t now_us, bool pressure);
#endif
