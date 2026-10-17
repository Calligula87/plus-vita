// SPDX-License-Identifier: AGPL-3.0-only
#include "text.h"
#include "utf8.h"
#include <string.h>

static unsigned character(const char **p, const char *end)
{
	unsigned code = plus_vita_utf8_next(p, end);
	return code == UINT32_MAX || (code < 32 && code != '\n') || code == 127 ? '?' : code;
}

static int width(const PlusVitaTextFont *font, const char *p, const char *end, int size, bool word)
{
	int advance = 0, right = 0;
	while(p < end && *p != '\n' && (!word || *p != ' ')) {
		PlusVitaGlyph glyph = font->measure(font->context, character(&p, end), size);
		int edge = advance + glyph.left + glyph.width;
		if(edge > right) right = edge;
		advance += glyph.advance;
	}
	return advance > right ? advance : right;
}

bool plus_vita_text_draw(const PlusVitaTextFont *font, const char *text, PlusVitaTextBox box, int size, bool wrap)
{
	if(!text || !*text) return true;
	if(box.width <= 0 || box.height <= 0 || size <= 0) return false;
	int ascent, descent;
	font->metrics(font->context, size, &ascent, &descent);
	int line = ascent + descent + 4;
	if(ascent + descent > box.height) return false;
	int x = box.x, baseline = box.y + ascent;
	const char *p = text, *end = text + strlen(text);
	PlusVitaGlyph dot = font->measure(font->context, '.', size);
	while(p < end)
	{
		bool last = !wrap || baseline + line + descent > box.y + box.height;
		if(wrap && !last && x != box.x && *p != ' ' && *p != '\n'
			&& (p == text || p[-1] == ' ') && width(font, p, end, size, true) > box.x + box.width - x) {
			x = box.x; baseline += line; continue;
		}
		bool elide = last && (width(font, p, end, size, false) > box.x + box.width - x || strchr(p, '\n'));
		const char *next = p;
		unsigned code = character(&next, end);
		if(code == '\n') {
			if(last) elide = true;
			else { x = box.x; baseline += line; p = next; continue; }
		}
		PlusVitaGlyph glyph = font->measure(font->context, code, size);
		if(x == box.x && glyph.left < 0) x -= glyph.left;
		int extent = glyph.advance > glyph.left + glyph.width ? glyph.advance : glyph.left + glyph.width;
		int limit = box.x + box.width - (elide ? 3 * dot.advance : 0);
		if(code == '\n' || x + extent > limit) {
			if(!last) { x = box.x; baseline += line; continue; }
			for(unsigned i = 0; i < 3 && x + dot.left + dot.width <= box.x + box.width; i++, x += dot.advance)
				font->draw(font->context, '.', x, baseline, size, &box);
			return false;
		}
		if(code != ' ') font->draw(font->context, code, x, baseline, size, &box);
		x += glyph.advance;
		p = next;
		while(wrap && x == box.x && p < end && *p == ' ') p++;
	}
	return true;
}
