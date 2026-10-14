// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_INPUT_VITA_H
#define PLUS_VITA_INPUT_VITA_H

#include "input_worker.h"
#include <psp2/ctrl.h>
#include <psp2/touch.h>

typedef struct {
	bool rear_ready, front_ready;
	int rear_mid_x, rear_mid_y;
	SceTouchPanelInfo front_info;
} PlusVitaInputVita;

/* Shared by the original main-loop path and the independent sampler. */
void plus_vita_input_map_pad(const SceCtrlData *pad, ChiakiControllerState *state);
void plus_vita_input_map_rear(const SceTouchData *touch, int mid_x, int mid_y,
	ChiakiControllerState *state);
void plus_vita_input_map_front(const SceTouchData *touch,
	const SceTouchPanelInfo *panel, const ChiakiControllerState *previous,
	ChiakiControllerState *state);
unsigned int plus_vita_input_touch_count(const ChiakiControllerState *state);
int plus_vita_input_vita_read(void *user, const ChiakiControllerState *previous,
	PlusVitaInputSample *sample);

#endif
