// SPDX-License-Identifier: AGPL-3.0-only
#include "video_reorder.h"
#include <assert.h>
#include <stdio.h>

static int admit(PlusVitaVideoReorder *w, uint16_t index, uint64_t time)
{
	int slot = plus_vita_video_reorder_admit(w, index, time);
	assert(slot >= 0 && slot < PLUS_VITA_VIDEO_REORDER_SLOTS);
	return slot;
}

static void immediate_and_wrap(void)
{
	PlusVitaVideoReorder w = { 0 };
	for(unsigned i = 0; i < 200000; i++)
	{
		uint16_t index = (uint16_t)(65534u + i);
		int slot = admit(&w, index, (uint64_t)i * 16667);
		assert(admit(&w, index, (uint64_t)i * 16667) == slot);
		plus_vita_video_reorder_ready(&w, slot, (uint64_t)i * 16667);
		PlusVitaVideoReorderEvent e = plus_vita_video_reorder_poll(&w, (uint64_t)i * 16667, false);
		assert(e.action == PLUS_VITA_VIDEO_REORDER_READY && e.index == index && e.slot == slot);
		assert(!e.wait_us && !e.held_us);
		assert(plus_vita_video_reorder_admit(&w, index, (uint64_t)i * 16667) == PLUS_VITA_VIDEO_REORDER_STALE);
	}
	puts("PASS: 200000 immediate in-order frames, duplicates, stale packets and repeated sequence wrap; no added wait");
}

static void late_completion(void)
{
	PlusVitaVideoReorder w = { 0 };
	int a = admit(&w, 500, 0), b = admit(&w, 501, 1000);
	plus_vita_video_reorder_ready(&w, b, 2000);
	assert(plus_vita_video_reorder_poll(&w, 2000, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	assert(admit(&w, 500, 10000) == a);
	plus_vita_video_reorder_ready(&w, a, 10000);
	/* Duplicate ready must not reset the completed frame's timestamp. */
	plus_vita_video_reorder_ready(&w, b, 10000);
	PlusVitaVideoReorderEvent e = plus_vita_video_reorder_poll(&w, 10000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_READY && e.index == 500 && e.wait_us == 9000);
	e = plus_vita_video_reorder_poll(&w, 10000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_READY && e.index == 501 && e.held_us == 8000);
	puts("PASS: late predecessor releases before the ready successor; exact blocked/ready-hold timing");
}

static void expiry_and_missing(void)
{
	PlusVitaVideoReorder w = { 0 };
	int a = admit(&w, 65535, 0), b = admit(&w, 0, 1000);
	plus_vita_video_reorder_ready(&w, b, 1100);
	assert(plus_vita_video_reorder_poll(&w, 20999, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	PlusVitaVideoReorderEvent e = plus_vita_video_reorder_poll(&w, 21000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_DROP && e.slot == a && e.skipped == 1 && e.wait_us == 20000);
	assert(plus_vita_video_reorder_admit(&w, 65535, 21000) == PLUS_VITA_VIDEO_REORDER_STALE);
	e = plus_vita_video_reorder_poll(&w, 21000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_READY && e.index == 0);
	/* Missing 1..9: one bounded gap event, not nine sequential grace periods. */
	b = admit(&w, 10, 30000);
	plus_vita_video_reorder_ready(&w, b, 31000);
	assert(plus_vita_video_reorder_poll(&w, 49999, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	e = plus_vita_video_reorder_poll(&w, 50000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_DROP && e.slot == -1 && e.index == 1 && e.skipped == 9);
	e = plus_vita_video_reorder_poll(&w, 50000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_READY && e.index == 10);
	puts("PASS: timer expires at 20ms without further packets; missing ranges skip once across wrap");
}

static void fixed_deadline_and_pressure(void)
{
	PlusVitaVideoReorder w = { 0 };
	int a = admit(&w, 100, 0);
	int b = admit(&w, 102, 1000);
	assert(plus_vita_video_reorder_poll(&w, 1000, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	int c = admit(&w, 101, 15000);
	assert(plus_vita_video_reorder_admit(&w, 103, 16000) == PLUS_VITA_VIDEO_REORDER_FULL);
	assert(plus_vita_video_reorder_poll(&w, 20999, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	PlusVitaVideoReorderEvent e = plus_vita_video_reorder_poll(&w, 21000, false);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_DROP && e.slot == a && e.wait_us == 20000);
	/* Later insertion of frame 101 does not extend the original deadline. */
	assert(w.slots[b].used && w.slots[c].used);
	admit(&w, 103, 21000);
	e = plus_vita_video_reorder_poll(&w, 21000, true);
	assert(e.action == PLUS_VITA_VIDEO_REORDER_DROP && e.slot == c && e.pressure);
	assert(w.slots[b].used);
	assert(plus_vita_video_reorder_admit(&w, 104, 21000) >= 0);
	puts("PASS: fixed gap deadline, three-slot memory bound and pressure frees only the oldest frame");
}

static void empty_and_clock(void)
{
	PlusVitaVideoReorder w = { 0 };
	assert(plus_vita_video_reorder_poll(&w, 1000000, true).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	admit(&w, 10, 1000);
	/* A partially received current frame has no artificial arrival deadline. */
	assert(plus_vita_video_reorder_poll(&w, 1000000, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	admit(&w, 11, 1000100);
	assert(plus_vita_video_reorder_poll(&w, 1000100, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	assert(plus_vita_video_reorder_poll(&w, 1000000, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	assert(plus_vita_video_reorder_poll(&w, 1020100, false).action == PLUS_VITA_VIDEO_REORDER_DROP);
	assert(plus_vita_video_reorder_poll(&w, 9999999, false).action == PLUS_VITA_VIDEO_REORDER_WAIT);
	assert(plus_vita_video_reorder_admit(&w, (uint16_t)(w.next + 0x8000u), 9999999) == PLUS_VITA_VIDEO_REORDER_STALE);
	puts("PASS: idle/partial frame, backward clock sample and ambiguous half-range rejected safely");
}

int main(void)
{
	immediate_and_wrap();
	late_completion();
	expiry_and_missing();
	fixed_deadline_and_pressure();
	empty_and_clock();
	return 0;
}
