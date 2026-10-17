// SPDX-License-Identifier: AGPL-3.0-only
#include "font_vita.h"
#include <psp2/pvf.h>
#include <psp2/sysmodule.h>
#include <debugScreen.h>
#include <stdlib.h>
#include <string.h>

struct plus_vita_font_t {
	ScePvfLibId library;
	ScePvfFontId latin, japanese;
	bool owned_module;
	int size, pitch;
	uint32_t *pixels, color;
	unsigned char glyph[128 * 128];
};
static void *allocate(void *user, unsigned size) { (void)user; return malloc(size); }
static void *reallocate(void *user, void *old, unsigned size) { (void)user; return realloc(old, size); }
static void release(void *user, void *memory) { (void)user; free(memory); }

PlusVitaFont *plus_vita_font_create(void)
{
	PlusVitaFont *font = calloc(1, sizeof(*font));
	if(!font) return NULL;
	int result = sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
	font->owned_module = result == 0;
	if(result < 0 && sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF) < 0) return font;
	ScePvfInitRec params = { .maxNumFonts = 2, .allocFunc = allocate, .reallocFunc = reallocate, .freeFunc = release };
	ScePvfError error = 0;
	font->library = scePvfNewLib(&params, &error);
	if(error || !font->library) { font->library = NULL; return font; }
	/* System outline fonts only; no Sony font asset is bundled. The resolution
	 * and EM setup follow the public libvita2d PVF adapter's system-font path. */
	if(scePvfSetEM(font->library, 72.0f / (10.125f * 128.0f)) < 0
		|| scePvfSetResolution(font->library, 128, 128) < 0) return font;
	ScePvfFontStyleInfo style = { .languageCode = SCE_PVF_LANGUAGE_LATIN,
		.familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_REGULAR };
	int index = scePvfFindOptimumFont(font->library, &style, &error);
	if(!error && index >= 0) font->latin = scePvfOpen(font->library, index, 0, &error);
	if(error) font->latin = NULL;
	memset(&style, 0, sizeof(style)); style.languageCode = SCE_PVF_LANGUAGE_J;
	index = scePvfFindOptimumFont(font->library, &style, &error);
	if(!error && index >= 0) font->japanese = scePvfOpen(font->library, index, 0, &error);
	if(error) font->japanese = NULL;
	return font;
}

void plus_vita_font_free(PlusVitaFont *font)
{
	if(!font) return;
	if(font->latin) scePvfClose(font->latin);
	if(font->japanese) scePvfClose(font->japanese);
	if(font->library) scePvfDoneLib(font->library);
	if(font->owned_module) sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);
	free(font);
}

static bool resize(PlusVitaFont *font, int size)
{
	if(!font->latin) return false;
	if(font->size != size) {
		float points = size * 72.0f / 128.0f;
		if(scePvfSetCharSize(font->latin, points, points) < 0) return false;
		if(font->japanese && scePvfSetCharSize(font->japanese, points, points) < 0) { scePvfClose(font->japanese); font->japanese = NULL; }
		font->size = size;
	}
	return true;
}

static ScePvfFontId face(PlusVitaFont *font, unsigned *code, int size)
{
	if(!resize(font, size)) return NULL;
	if(*code > 0xffff) *code = '?';
	if(scePvfIsElement(font->latin, (ScePvfCharCode)*code)) return font->latin;
	if(font->japanese && scePvfIsElement(font->japanese, (ScePvfCharCode)*code)) return font->japanese;
	*code = '?'; return font->latin;
}

static bool outline(PlusVitaFont *font, unsigned *code, int size, ScePvfFontId *selected,
	ScePvfCharInfo *info, PlusVitaGlyph *glyph)
{
	*selected = face(font, code, size);
	ScePvfIrect rect;
	if(*selected && scePvfGetCharInfo(*selected, (ScePvfCharCode)*code, info) == 0
		&& scePvfGetCharImageRect(*selected, (ScePvfCharCode)*code, &rect) == 0 && rect.width <= 124 && rect.height <= 124) {
		*glyph = (PlusVitaGlyph){ .advance = (info->glyphMetrics.horizontalAdvance64 + 63) / 64,
			.left = info->glyphMetrics.horizontalBearingX64 / 64, .top = (info->glyphMetrics.horizontalBearingY64 + 63) / 64,
			.width = rect.width, .height = rect.height };
		return true;
	}
	return false;
}

static PlusVitaGlyph measure(void *context, unsigned code, int size)
{
	ScePvfFontId selected; ScePvfCharInfo info; PlusVitaGlyph glyph;
	if(outline(context, &code, size, &selected, &info, &glyph)) return glyph;
	int horizontal = size / 12; if(horizontal < 1) horizontal = 1;
	int vertical = size / 8; if(vertical < 1) vertical = 1;
	return (PlusVitaGlyph){ .advance = 8 * horizontal, .width = 8 * horizontal, .height = 8 * vertical, .top = 8 * vertical };
}

static void metrics(void *context, int size, int *ascent, int *descent)
{
	PlusVitaFont *font = context;
	*ascent = size; *descent = size / 4;
	ScePvfFontInfo info;
	if(resize(font, size) && scePvfGetFontInfo(font->latin, &info) == 0) {
		int a = (info.maxIGlyphMetrics.ascender64 + 63) / 64;
		int d = abs(info.maxIGlyphMetrics.descender64) / 64 + 1;
		if(a > 0 && a <= size * 2 && d <= size) { *ascent = a; *descent = d; }
	}
}

static void pixel(PlusVitaFont *font, int x, int y, unsigned alpha, const PlusVitaTextBox *clip)
{
	if(x < clip->x || x >= clip->x + clip->width || y < clip->y || y >= clip->y + clip->height
		|| x < 0 || x >= 960 || y < 0 || y >= 544) return;
	uint32_t *target = &font->pixels[y * font->pitch + x], color = 0xff000000;
	for(unsigned shift = 0; shift < 24; shift += 8)
		color |= ((((*target >> shift) & 255) * (255 - alpha) + ((font->color >> shift) & 255) * alpha) / 255) << shift;
	*target = color;
}

static void draw(void *context, unsigned code, int x, int baseline, int size, const PlusVitaTextBox *clip)
{
	PlusVitaFont *font = context;
	PlusVitaGlyph glyph; ScePvfFontId selected; ScePvfCharInfo info;
	if(outline(font, &code, size, &selected, &info, &glyph)) {
		memset(font->glyph, 0, sizeof(font->glyph));
		ScePvfUserImageBufferRec image = { .pixelFormat = SCE_PVF_USERIMAGE_DIRECT8,
			.xPos64 = 128 - info.glyphMetrics.horizontalBearingX64,
			.yPos64 = 128 + info.glyphMetrics.horizontalBearingY64,
			.rect = {128,128}, .bytesPerLine = 128, .buffer = font->glyph };
		if(scePvfGetCharGlyphImage(selected, (ScePvfCharCode)code, &image) == 0) {
			for(int row = 0; row < glyph.height; row++) for(int col = 0; col < glyph.width; col++)
				pixel(font, x + glyph.left + col, baseline - glyph.top + row, font->glyph[(row + 2) * 128 + col + 2], clip);
			return;
		}
		/* A raster failure must not substitute wider bitmap metrics after layout.
		 * Draw a bounded missing-glyph outline inside the measured ink region. */
		for(int row = 0; row < glyph.height; row++) for(int col = 0; col < glyph.width; col++)
			if(!row || !col || row == glyph.height - 1 || col == glyph.width - 1)
				pixel(font, x + glyph.left + col, baseline - glyph.top + row, 255, clip);
		return;
	}
	PsvDebugScreenFont *bitmap = psvDebugScreenGetFont();
	if(code > 127) code = '?';
	int sx = size / 12, sy = size / 8; if(sx < 1) sx = 1; if(sy < 1) sy = 1;
	for(int row = 0; row < 8; row++) for(int col = 0; col < 8; col++)
		if(bitmap->glyphs[code * 8 + row] & (0x80 >> col)) for(int dy = 0; dy < sy; dy++) for(int dx = 0; dx < sx; dx++)
			pixel(font, x + col * sx + dx, baseline - 8 * sy + row * sy + dy, 255, clip);
}

bool plus_vita_font_draw(PlusVitaFont *font, uint32_t *pixels, int pitch, const char *text,
	PlusVitaTextBox box, int size, bool wrap, uint32_t color)
{
	font->pixels = pixels; font->pitch = pitch; font->color = color;
	PlusVitaTextFont adapter = { font, metrics, measure, draw };
	return plus_vita_text_draw(&adapter, text, box, size, wrap);
}
