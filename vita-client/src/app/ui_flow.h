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
	const PlusVitaCovers *covers;
} PlusVitaUiFlow;
bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us);
PlusVitaAppEffect plus_vita_ui_flow_take(PlusVitaUiFlow *flow);
bool plus_vita_ui_flow_paint(PlusVitaUiFlow *flow, PlusVitaFont *font, uint32_t *pixels, int pitch);
#endif
