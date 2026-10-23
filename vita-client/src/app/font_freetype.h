// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_FONT_FREETYPE_H
#define PLUS_VITA_FONT_FREETYPE_H
#include "text.h"
#include <stdint.h>
typedef struct plus_vita_ft_font_t PlusVitaFtFont;
PlusVitaFtFont *plus_vita_freetype_create(const char *path, PlusVitaTextFont fallback);
void plus_vita_freetype_free(PlusVitaFtFont *font);
bool plus_vita_freetype_draw(PlusVitaFtFont *font, uint32_t *pixels, int pitch,
	const char *text, PlusVitaTextBox box, int size, bool wrap, uint32_t color);
#endif
