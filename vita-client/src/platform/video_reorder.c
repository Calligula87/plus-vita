// SPDX-License-Identifier: AGPL-3.0-only
#include "video_reorder.h"
#include <stddef.h>

static uint64_t elapsed(uint64_t now, uint64_t then)
{
	return now >= then ? now - then : 0;
}

int plus_vita_video_reorder_admit(PlusVitaVideoReorder *window,
	uint16_t index, uint64_t now_us)
{
	if(!window->started)
	{
		window->started = true;
		window->next = index;
	}
	/* Half-range ordering, including the 65535 -> 0 wrap. */
	if((uint16_t)(index - window->next) >= 0x8000u)
		return PLUS_VITA_VIDEO_REORDER_STALE;
	int empty = PLUS_VITA_VIDEO_REORDER_FULL;
	for(int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
	{
		if(window->slots[i].used && window->slots[i].index == index) return i;
		if(!window->slots[i].used) empty = i;
	}
	if(empty < 0) return empty;
	window->slots[empty] = (PlusVitaVideoReorderSlot){
		.used = true, .index = index, .arrived_us = now_us };
	return empty;
}

void plus_vita_video_reorder_ready(PlusVitaVideoReorder *window,
	int slot, uint64_t now_us)
{
	if(slot < 0 || slot >= PLUS_VITA_VIDEO_REORDER_SLOTS
		|| !window->slots[slot].used || window->slots[slot].ready) return;
	window->slots[slot].ready = true;
	window->slots[slot].ready_us = now_us;
}

PlusVitaVideoReorderEvent plus_vita_video_reorder_poll(
	PlusVitaVideoReorder *window, uint64_t now_us, bool pressure)
{
	PlusVitaVideoReorderEvent event = { .slot = -1 };
	int head = -1, future = -1;
	uint16_t distance = UINT16_MAX;
	uint64_t first_future_us = UINT64_MAX;
	for(int i = 0; i < PLUS_VITA_VIDEO_REORDER_SLOTS; i++)
	{
		PlusVitaVideoReorderSlot *slot = &window->slots[i];
		if(!slot->used) continue;
		uint16_t delta = (uint16_t)(slot->index - window->next);
		if(!delta) head = i;
		else
		{
			if(delta < distance) { distance = delta; future = i; }
			if(slot->arrived_us < first_future_us) first_future_us = slot->arrived_us;
		}
	}
	if(head >= 0 && window->slots[head].ready)
	{
		event.action = PLUS_VITA_VIDEO_REORDER_READY;
		event.slot = head;
		event.index = window->next++;
		event.held_us = elapsed(now_us, window->slots[head].ready_us);
		if(window->waiting) event.wait_us = elapsed(now_us, window->waiting_since_us);
		window->slots[head].used = false;
		window->waiting = false;
		return event;
	}
	/* No successor: do not time out the frame still being received. The
	 * normal stream-output deadline covers a completely silent connection. */
	if(future < 0) return event;
	if(!window->waiting)
	{
		window->waiting = true;
		window->waiting_since_us = first_future_us;
	}
	uint64_t wait_us = elapsed(now_us, window->waiting_since_us);
	if(!pressure && wait_us < PLUS_VITA_VIDEO_REORDER_GRACE_US) return event;
	event.action = PLUS_VITA_VIDEO_REORDER_DROP;
	event.slot = head;
	event.index = window->next;
	event.skipped = head >= 0 ? 1 : distance;
	event.wait_us = wait_us;
	event.pressure = pressure;
	if(head >= 0) window->slots[head].used = false;
	window->next = (uint16_t)(window->next + event.skipped);
	window->waiting = false;
	return event;
}
