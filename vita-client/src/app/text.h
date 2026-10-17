// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_TEXT_H
#define PLUS_VITA_TEXT_H
#include <stdbool.h>
typedef struct { int x, y, width, height; } PlusVitaTextBox;
typedef struct { int advance, left, top, width, height; } PlusVitaGlyph;
typedef struct {
	void *context;
	void (*metrics)(void *, int size, int *ascent, int *descent);
	PlusVitaGlyph (*measure)(void *, unsigned code, int size);
	void (*draw)(void *, unsigned code, int x, int baseline, int size, const PlusVitaTextBox *clip);
} PlusVitaTextFont;
/* Complete output returns true; overflow ends in an explicit ellipsis. */
bool plus_vita_text_draw(const PlusVitaTextFont *font, const char *text, PlusVitaTextBox box, int size, bool wrap);
#endif
