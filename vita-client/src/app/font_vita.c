// SPDX-License-Identifier: AGPL-3.0-only
#include "font_vita.h"
#include <psp2/pvf.h>
#include <psp2/sysmodule.h>
#include <debugScreen.h>
#include <stdlib.h>
#include <string.h>
#if PLUS_VITA_FONT_COMPARISON || PLUS_VITA_FONT_DEFAULT_FREETYPE
#include "font_freetype.h"
#endif

#define GLYPH_SLOTS 256u
#define GLYPH_BYTES (512u * 1024u)
typedef struct {
	unsigned code;
	int size;
	PlusVitaGlyph metrics;
	unsigned char *alpha;
} CachedGlyph;
struct plus_vita_font_t {
#if PLUS_VITA_FONT_COMPARISON || PLUS_VITA_FONT_DEFAULT_FREETYPE
	PlusVitaFtFont *alternative;
#endif
	ScePvfLibId library;
	ScePvfFontId latin, japanese;
	bool owned_module;
	int size, pitch;
	uint32_t *pixels, color;
	unsigned char glyph[128 * 128];
	CachedGlyph cache[GLYPH_SLOTS];
	unsigned next;
	size_t cached_bytes;
	struct { int size, ascent, descent; } lines[8];
	unsigned next_line;
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
#if PLUS_VITA_FONT_COMPARISON || PLUS_VITA_FONT_DEFAULT_FREETYPE
	plus_vita_freetype_free(font->alternative);
#endif
	for(unsigned i = 0; i < GLYPH_SLOTS; i++) free(font->cache[i].alpha);
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

static void evict(PlusVitaFont *font, CachedGlyph *entry)
{
	if(entry->alpha) font->cached_bytes -= (size_t)entry->metrics.width * entry->metrics.height;
	free(entry->alpha);
	memset(entry, 0, sizeof(*entry));
}

static CachedGlyph *resolve(PlusVitaFont *font, unsigned code, int size)
{
	for(unsigned i = 0; i < GLYPH_SLOTS; i++)
		if(font->cache[i].size == size && font->cache[i].code == code) return &font->cache[i];
	CachedGlyph *entry = &font->cache[font->next++ % GLYPH_SLOTS];
	evict(font, entry);
	entry->size = size; entry->code = code;
	ScePvfFontId selected; ScePvfCharInfo info;
	bool outlined = outline(font, &code, size, &selected, &info, &entry->metrics);
	if(!outlined) {
		int sx = size / 12, sy = size / 8; if(sx < 1) sx = 1; if(sy < 1) sy = 1;
		entry->metrics = (PlusVitaGlyph){ .advance = 8*sx, .width = 8*sx, .height = 8*sy, .top = 8*sy };
	}
	PlusVitaGlyph glyph = entry->metrics;
	size_t bytes = (size_t)glyph.width * glyph.height;
	if(!bytes || bytes > GLYPH_BYTES) return entry;
	while(font->cached_bytes + bytes > GLYPH_BYTES) evict(font, &font->cache[font->next++ % GLYPH_SLOTS]);
	entry->alpha = calloc(bytes, 1);
	if(!entry->alpha) return entry;
	font->cached_bytes += bytes;
	if(outlined) {
		memset(font->glyph, 0, sizeof(font->glyph));
		ScePvfUserImageBufferRec image = { .pixelFormat = SCE_PVF_USERIMAGE_DIRECT8,
			.xPos64 = 128 - info.glyphMetrics.horizontalBearingX64,
			.yPos64 = 128 + info.glyphMetrics.horizontalBearingY64,
			.rect = {128,128}, .bytesPerLine = 128, .buffer = font->glyph };
		if(scePvfGetCharGlyphImage(selected, (ScePvfCharCode)code, &image) == 0) {
			for(int row = 0; row < glyph.height; row++)
				memcpy(entry->alpha + row * glyph.width, font->glyph + (row+2)*128+2, glyph.width);
			return entry;
		}
		/* Keep the measured ink bounds on external raster failure. */
		for(int row = 0; row < glyph.height; row++) for(int col = 0; col < glyph.width; col++)
			if(!row || !col || row == glyph.height-1 || col == glyph.width-1) entry->alpha[row*glyph.width+col] = 255;
	} else {
		PsvDebugScreenFont *bitmap = psvDebugScreenGetFont();
		if(code > 127) code = '?';
		int sx = glyph.width / 8, sy = glyph.height / 8;
		for(int row=0; row<8; row++) for(int col=0; col<8; col++)
			if(bitmap->glyphs[code*8+row] & (0x80 >> col)) for(int dy=0; dy<sy; dy++) for(int dx=0; dx<sx; dx++)
				entry->alpha[(row*sy+dy)*glyph.width+col*sx+dx] = 255;
	}
	return entry;
}

static PlusVitaGlyph measure(void *context, unsigned code, int size)
{
	return resolve(context, code, size)->metrics;
}

static void metrics(void *context, int size, int *ascent, int *descent)
{
	PlusVitaFont *font = context;
	for(unsigned i = 0; i < 8; i++) if(font->lines[i].size == size) {
		*ascent = font->lines[i].ascent; *descent = font->lines[i].descent; return;
	}
	*ascent = size; *descent = size / 4;
	ScePvfFontInfo info;
	if(resize(font, size) && scePvfGetFontInfo(font->latin, &info) == 0) {
		int a = (info.maxIGlyphMetrics.ascender64 + 63) / 64;
		int d = abs(info.maxIGlyphMetrics.descender64) / 64 + 1;
		if(a > 0 && a <= size * 2 && d <= size) { *ascent = a; *descent = d; }
	}
	unsigned slot = font->next_line++ % 8;
	font->lines[slot].size = size; font->lines[slot].ascent = *ascent; font->lines[slot].descent = *descent;
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
	CachedGlyph *entry = resolve(font, code, size);
	PlusVitaGlyph glyph = entry->metrics;
	for(int row = 0; row < glyph.height; row++) for(int col = 0; col < glyph.width; col++) {
		unsigned alpha = entry->alpha ? entry->alpha[row*glyph.width+col]
			: (!row || !col || row == glyph.height-1 || col == glyph.width-1 ? 255 : 0);
		if(alpha) pixel(font, x+glyph.left+col, baseline-glyph.top+row, alpha, clip);
	}
}

bool plus_vita_font_draw(PlusVitaFont *font, uint32_t *pixels, int pitch, const char *text,
	PlusVitaTextBox box, int size, bool wrap, uint32_t color)
{
	font->pixels = pixels; font->pitch = pitch; font->color = color;
#if PLUS_VITA_FONT_COMPARISON || PLUS_VITA_FONT_DEFAULT_FREETYPE
	if(font->alternative) return plus_vita_freetype_draw(font->alternative,pixels,pitch,text,box,size,wrap,color);
#endif
	PlusVitaTextFont adapter = { font, metrics, measure, draw };
	return plus_vita_text_draw(&adapter, text, box, size, wrap);
}
PlusVitaFont *plus_vita_font_create_alternative(const char *path)
{
#if PLUS_VITA_FONT_COMPARISON || PLUS_VITA_FONT_DEFAULT_FREETYPE
	PlusVitaFont *font=plus_vita_font_create(); if(!font) return NULL;
	font->alternative=plus_vita_freetype_create(path,(PlusVitaTextFont){font,metrics,measure,draw});
	if(!font->alternative) { plus_vita_font_free(font); return NULL; }
	return font;
#else
	(void)path; return NULL;
#endif
}
