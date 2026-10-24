// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_PLAY_VITA_H
#define PLUS_VITA_PLAY_VITA_H

#include "app.h"
#include <chiaki/cloudsession.h>
#include <stdbool.h>

typedef struct plus_vita_play_t PlusVitaPlay;

PlusVitaPlay *plus_vita_play_create(void);
bool plus_vita_play_start(PlusVitaPlay *play, const PlusVitaAppEffect *effect);
bool plus_vita_play_busy(const PlusVitaPlay *play);
void plus_vita_play_cancel(PlusVitaPlay *play);
unsigned plus_vita_play_stage(PlusVitaPlay *play);
bool plus_vita_play_ready(PlusVitaPlay *play);
bool plus_vita_play_take(PlusVitaPlay *play, ChiakiCloudProvisionResult *result,
	ChiakiErrorCode *error, uint64_t *operation);
void plus_vita_play_result_clear(ChiakiCloudProvisionResult *result);
bool plus_vita_play_close(PlusVitaPlay *play);
/* Interpret only fixed upstream categories; never display the remote detail. */
PlusVitaAppResult plus_vita_play_failure(ChiakiErrorCode error, const ChiakiCloudProvisionResult *result);

#endif
