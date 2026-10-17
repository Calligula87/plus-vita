// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_FONT_VITA_H
#define PLUS_VITA_FONT_VITA_H
#include "text.h"
#include <stdint.h>
typedef struct plus_vita_font_t PlusVitaFont;
PlusVitaFont *plus_vita_font_create(void);
void plus_vita_font_free(PlusVitaFont *font);
bool plus_vita_font_draw(PlusVitaFont *font, uint32_t *pixels, int pitch, const char *text,
	PlusVitaTextBox box, int size, bool wrap, uint32_t color);
#endif
