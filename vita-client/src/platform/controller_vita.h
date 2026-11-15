// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_CONTROLLER_VITA_H
#define PLUS_VITA_CONTROLLER_VITA_H
#include "control_layout.h"
#include <psp2/ctrl.h>

/* One phase/thread owns this reader. Menu and gameplay ownership never overlap. */
typedef struct {
	bool initialized, tv, external, ready, neutral_required;
	int port;
	PlusVitaControlDevice device;
} PlusVitaController;

/* 1=raw sample (or confirmed disconnected neutral), 0=no new sample, -1=fatal.
 * ready gates gameplay/menu actions, but physical exit detection may use raw input. */
int plus_vita_controller_read(PlusVitaController *controller,SceCtrlData *pad);
#endif
