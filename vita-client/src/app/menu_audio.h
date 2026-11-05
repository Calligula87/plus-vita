// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_MENU_AUDIO_H
#define PLUS_VITA_MENU_AUDIO_H
#include "presentation.h"
typedef struct plus_vita_menu_audio_t PlusVitaMenuAudio;
/* UI-thread lifecycle owner. Output worker never owns stream audio. */
PlusVitaMenuAudio *plus_vita_menu_audio_create(const char *path);
void plus_vita_menu_audio_update(PlusVitaMenuAudio *audio,PlusVitaPresentation preferences,bool foreground);
void plus_vita_menu_audio_close(PlusVitaMenuAudio *audio);
#endif
