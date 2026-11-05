// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_UI_FLOW_H
#define PLUS_VITA_UI_FLOW_H
#include "render.h"
/* UI-thread owner. Pending effects contain credentials: never log this state. */
typedef struct {
	PlusVitaApp *app;
	PlusVitaAppEffect pending;
	bool dirty, cancel_account;
	uint64_t input_since;
	uint64_t diagnostic_operation;
	PlusVitaCovers *covers;
	PlusVitaBranding *branding;
	bool startup_started, splash_active, splash_release;
	uint64_t splash_started_us;
	bool touch_held, touch_captured, touch_list, touch_dragging;
	int touch_start_x, touch_start_y;
	size_t touch_first;
	size_t touch_index;
	PlusVitaAppButton touch_target;
	PlusVitaAppScreen touch_screen;
	uint64_t touch_operation;
	bool transition_active;
	uint64_t now_us, transition_started_us;
	bool foreground;
	uint64_t animation_us, animation_tick_us;
	/* Selection identity belongs to the account, never to a visible row or URL alone. */
	uint64_t artwork_epoch, preview_since;
	bool artwork_initialized, preview_valid, preview_eligible, preview_requested;
	bool preview_dragging, artwork_completed;
	PlusVitaAppScreen preview_screen;
	char preview_id[PLUS_VITA_CATALOG_ID_MAX], preview_name[PLUS_VITA_CATALOG_NAME_MAX], preview_url[512];
} PlusVitaUiFlow;
void plus_vita_ui_flow_start(PlusVitaUiFlow *flow,PlusVitaBranding *branding,uint64_t now_us,bool log_available);
void plus_vita_ui_flow_foreground(PlusVitaUiFlow *flow,bool foreground,uint64_t now_us);
bool plus_vita_ui_flow_tick(PlusVitaUiFlow *flow,uint64_t now_us);
bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us);
PlusVitaAppEffect plus_vita_ui_flow_take(PlusVitaUiFlow *flow);
bool plus_vita_ui_flow_paint(PlusVitaUiFlow *flow, PlusVitaFont *font, uint32_t *pixels, int pitch);
#endif
