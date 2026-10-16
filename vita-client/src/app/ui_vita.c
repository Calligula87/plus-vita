// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_vita.h"
#include "app.h"
#include "account_vita.h"
#include "../platform/credential_ime.h"
#include <debugScreen.h> /* SDK bitmap font fallback; no terminal initialization. */
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/pgf.h>
#include <psp2/sysmodule.h>
#include <psp2/touch.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 960
#define HEIGHT 544
#define PITCH 1024
#define FRAME_BYTES ((PITCH * HEIGHT * 4u + 0x3ffffu) & ~0x3ffffu)
#define RGB(r,g,b) (0xff000000u | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (r))

typedef struct {
	SceUID blocks[2];
	uint32_t *pixels[2];
	unsigned back;
	SceDisplayFrameBuf saved;
	bool displayed, font_module_owned;
	SceFontLibHandle font_library;
	SceFontHandle font;
	unsigned char glyph[128 * 128];
} UiDisplay;

static void *font_alloc(void *user, unsigned size) { (void)user; return malloc(size); }
static void font_free(void *user, void *memory) { (void)user; free(memory); }

static void font_init(UiDisplay *display)
{
	int loaded = sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
	display->font_module_owned = loaded == 0;
	if(loaded < 0 && sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF) < 0) return;
	SceFontNewLibParams params = { .numFonts = 1, .allocFunc = font_alloc, .freeFunc = font_free };
	unsigned error = 0;
	display->font_library = sceFontNewLib(&params, &error);
	if(error || !display->font_library) return;
	SceFontStyle style = { .fontH = 10, .fontV = 10, .fontLanguage = SCE_FONT_LANGUAGE_LATIN };
	int index = sceFontFindOptimumFont(display->font_library, &style, &error);
	if(!error) display->font = sceFontOpen(display->font_library, index, 0, &error);
	if(error) display->font = NULL;
}

/* Never release a buffer until scanout has moved away from it. */
static bool display_close(UiDisplay *display)
{
	if(display->displayed)
	{
		const SceDisplayFrameBuf *restore = display->saved.base ? &display->saved : NULL;
		if(sceDisplaySetFrameBuf(restore, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0
			|| sceDisplayWaitSetFrameBuf() < 0) return false;
	}
	for(unsigned i = 0; i < 2; i++)
		if(display->blocks[i] >= 0) sceKernelFreeMemBlock(display->blocks[i]);
	if(display->font) sceFontClose(display->font);
	if(display->font_library) sceFontDoneLib(display->font_library);
	if(display->font_module_owned) sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);
	return true;
}

static bool display_open(UiDisplay *display)
{
	memset(display, 0, sizeof(*display));
	display->blocks[0] = display->blocks[1] = -1;
	display->saved.size = sizeof(display->saved);
	if(sceDisplayGetFrameBuf(&display->saved, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0)
		display->saved.base = NULL;
	for(unsigned i = 0; i < 2; i++)
	{
		display->blocks[i] = sceKernelAllocMemBlock("PlusVitaUI", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, FRAME_BYTES, NULL);
		if(display->blocks[i] < 0 || sceKernelGetMemBlockBase(display->blocks[i], (void **)&display->pixels[i]) < 0)
		{
			display_close(display);
			return false;
		}
		memset(display->pixels[i], 0, FRAME_BYTES);
	}
	font_init(display);
	return true;
}

static void rectangle(UiDisplay *display, int x, int y, int width, int height, uint32_t color)
{
	int right = x + width, bottom = y + height;
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(right > WIDTH) right = WIDTH;
	if(bottom > HEIGHT) bottom = HEIGHT;
	for(int row = y; row < bottom; row++)
		for(int col = x; col < right; col++) display->pixels[display->back][row * PITCH + col] = color;
}

static void rounded(UiDisplay *display, int x, int y, int width, int height, uint32_t color)
{
	for(int row = 0; row < height; row++)
	{
		int inset = row == 0 || row == height - 1 ? 5 : row < 3 || row >= height - 3 ? 2 : 0;
		rectangle(display, x + inset, y + row, width - 2 * inset, 1, color);
	}
}

static void blend(UiDisplay *display, int x, int y, uint32_t color, unsigned alpha)
{
	if(x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
	uint32_t *pixel = &display->pixels[display->back][y * PITCH + x];
	uint32_t result = 0xff000000u;
	for(unsigned shift = 0; shift < 24; shift += 8)
		result |= (((((*pixel >> shift) & 255u) * (255u - alpha) + ((color >> shift) & 255u) * alpha) / 255u) << shift);
	*pixel = result;
}

static int draw_character(UiDisplay *display, unsigned character, int x, int baseline, int size, uint32_t color)
{
	SceFontCharInfo info;
	if(display->font && sceFontGetCharInfo(display->font, character, &info) == 0
		&& (info.sfp26AdvanceH > 0 || info.bitmapWidth > 0)
		&& info.bitmapWidth <= 128 && info.bitmapHeight <= 128)
	{
		memset(display->glyph, 0, sizeof(display->glyph));
		SceFontGlyphImage image = {
			.pixelFormat = SCE_FONT_PIXELFORMAT_8, .bufWidth = 128, .bufHeight = 128,
			.bytesPerLine = 128, .bufferPtr = (unsigned)display->glyph
		};
		if(sceFontGetCharGlyphImage(display->font, character, &image) == 0)
		{
			float scale = size / 20.0f;
			int width = (int)(info.bitmapWidth * scale), height = (int)(info.bitmapHeight * scale);
			int left = (int)((int)info.bitmapLeft * scale), top = (int)((int)info.bitmapTop * scale);
			for(int row = 0; row < height; row++)
				for(int col = 0; col < width; col++)
					blend(display, x + left + col, baseline - top + row, color,
						display->glyph[(int)(row / scale) * 128 + (int)(col / scale)]);
			return (int)((info.sfp26AdvanceH / 64.0f) * scale);
		}
	}
	/* SDK BSD-licensed 8x8 font remains readable if the system font is unavailable. */
	PsvDebugScreenFont *font = psvDebugScreenGetFont();
	if(character > 127) character = '?';
	int scale = size / 8;
	if(scale < 1) scale = 1;
	int horizontal_scale = size / 12;
	if(horizontal_scale < 1) horizontal_scale = 1;
	const unsigned char *glyph = font->glyphs + character * 8;
	for(int row = 0; row < 8; row++)
		for(int col = 0; col < 8; col++)
			if(glyph[row] & (0x80u >> col)) rectangle(display, x + col * horizontal_scale,
				baseline - 8 * scale + row * scale, horizontal_scale, scale, color);
	return 8 * horizontal_scale;
}

static void text(UiDisplay *display, const char *value, int x, int y, int size, int max_width, uint32_t color)
{
	int pen = x;
	if(!value) return;
	const unsigned char *p = (const unsigned char *)value;
	while(*p)
	{
		unsigned character = *p++;
		if(character >= 0x80)
		{
			unsigned following = character >= 0xc2 && character <= 0xdf ? 1
				: character >= 0xe0 && character <= 0xef ? 2 : character >= 0xf0 && character <= 0xf4 ? 3 : 0;
			unsigned decoded = character & (following == 1 ? 0x1fu : following == 2 ? 0x0fu : 0x07u);
			bool valid = following != 0;
			for(unsigned i = 0; valid && i < following; i++)
			{
				if((p[i] & 0xc0u) != 0x80u) valid = false;
				else decoded = (decoded << 6) | (p[i] & 0x3fu);
			}
			unsigned minimum = following == 1 ? 0x80 : following == 2 ? 0x800 : 0x10000;
			valid = valid && decoded >= minimum && decoded <= 0x10ffff && !(decoded >= 0xd800 && decoded <= 0xdfff);
			if(valid) { character = decoded; p += following; }
			else character = '?';
		}
		if(character == '\n') { pen = x; y += size + 10; continue; }
		if(pen + size > x + max_width) { pen = x; y += size + 10; }
		pen += draw_character(display, character, pen, y, size, color);
	}
}

static bool draw(UiDisplay *display, const PlusVitaAppView *view)
{
	for(int y = 0; y < HEIGHT; y++)
		rectangle(display, 0, y, WIDTH, 1, RGB(12 + y / 60, 17 + y / 45, 29 + y / 32));
	rectangle(display, 0, 0, WIDTH, 4, RGB(103, 228, 181));
	text(display, "plus vita", 64, 52, 24, 400, RGB(236, 244, 250));
	text(display, "DEVELOPMENT BUILD", 712, 49, 14, 220, RGB(145, 164, 185));
	text(display, view->subtitle, 64, 114, 16, 832, RGB(103, 228, 181));
	text(display, view->title, 64, 164, 36, 832, RGB(243, 247, 252));
	text(display, view->body, 64, 216, 20, view->screen == PLUS_VITA_APP_WELCOME ? 520 : 832, RGB(162, 180, 201));
	if(view->screen == PLUS_VITA_APP_WELCOME)
	{
		rounded(display, 636, 235, 262, 146, RGB(37, 56, 72));
		rounded(display, 681, 248, 170, 120, RGB(13, 24, 37));
		rectangle(display, 650, 300, 21, 6, RGB(103, 228, 181));
		rectangle(display, 658, 292, 6, 22, RGB(103, 228, 181));
		rounded(display, 865, 293, 10, 10, RGB(103, 228, 181));
		rounded(display, 878, 306, 10, 10, RGB(103, 228, 181));
		text(display, "A new way to play.", 656, 414, 18, 262, RGB(145, 164, 185));
	}
	for(size_t i = 0; i < view->action_count; i++)
	{
		const PlusVitaAppButton *button = &view->actions[i];
		bool selected = i == view->selected;
		rounded(display, button->x, button->y, button->width, button->height,
			selected ? RGB(103, 228, 181) : RGB(33, 45, 61));
		text(display, button->label, button->x + 20, button->y + 34, 22, button->width - 40,
			selected ? RGB(10, 28, 25) : RGB(226, 234, 244));
	}
	text(display, view->hint, 64, 529, 16, 832, RGB(145, 164, 185));
	SceDisplayFrameBuf frame = {
		.size = sizeof(frame), .base = display->pixels[display->back], .pitch = PITCH,
		.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8, .width = WIDTH, .height = HEIGHT
	};
	if(sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0) return false;
	display->displayed = true;
	if(sceDisplayWaitSetFrameBuf() < 0) return false;
	display->back ^= 1u;
	return true;
}

static void queue_effect(PlusVitaAppEffect *pending, PlusVitaAppEffect effect)
{
	if(effect.kind != PLUS_VITA_EFFECT_NONE)
	{
		plus_vita_credential_clear(&pending->credential);
		*pending = effect;
	}
	plus_vita_credential_clear(&effect.credential);
}

int plus_vita_app_run(void)
{
	UiDisplay *display = calloc(1, sizeof(*display));
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	if(!display || !app || !account)
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); return 1; }
	if(!display_open(display))
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); return 1; }
	SceCtrlPadInputMode previous_mode = SCE_CTRL_MODE_DIGITAL;
	bool restore_mode = sceCtrlGetSamplingMode(&previous_mode) == 0;
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	SceTouchSamplingState previous_touch = SCE_TOUCH_SAMPLING_STATE_STOP;
	bool restore_touch = sceTouchGetSamplingState(SCE_TOUCH_PORT_FRONT, &previous_touch) == 0;
	SceTouchPanelInfo panel = { 0 };
	bool touch_ready = sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) == 0
		&& panel.maxAaX > panel.minAaX && panel.maxAaY > panel.minAaY
		&& sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START) == 0;
	bool touched = false, dirty = true;
	uint32_t previous_buttons = 0;
	int previous_direction = 0;
	uint64_t repeat_at = 0;
	PlusVitaAppEffect pending = { .kind = PLUS_VITA_EFFECT_NONE };
	int result = 0;
	while(plus_vita_app_view(app).screen != PLUS_VITA_APP_CLOSED)
	{
		if(dirty)
		{
			PlusVitaAppView view = plus_vita_app_view(app);
			if(!draw(display, &view)) { result = 1; break; }
			dirty = false;
		}
		SceCtrlData pad = { 0 };
		PlusVitaAppEvent event = { 0 };
		bool has_event = false;
		if(sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
		{
			uint32_t pressed = pad.buttons & ~previous_buttons;
			int direction = (pad.buttons & SCE_CTRL_DOWN) || pad.ly > 180 ? 1
				: (pad.buttons & SCE_CTRL_UP) || pad.ly < 76 ? -1 : 0;
			uint64_t now = sceKernelGetProcessTimeWide();
			if(pressed & SCE_CTRL_CIRCLE) { event.kind = PLUS_VITA_EVENT_BACK; has_event = true; }
			else if(pressed & SCE_CTRL_CROSS) { event.kind = PLUS_VITA_EVENT_ACTIVATE; has_event = true; }
			else if(direction && (direction != previous_direction || now >= repeat_at))
			{
				event.kind = direction > 0 ? PLUS_VITA_EVENT_DOWN : PLUS_VITA_EVENT_UP;
				has_event = true;
				repeat_at = now + (direction != previous_direction ? 400000u : 160000u);
			}
			previous_direction = direction;
			previous_buttons = pad.buttons;
		}
		SceTouchData touch = { 0 };
		if(touch_ready && sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0)
		{
			if(!has_event && touch.reportNum && !touched)
			{
				event = (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH,
					.x = (touch.report[0].x - panel.minAaX) * (WIDTH - 1) / (panel.maxAaX - panel.minAaX),
					.y = (touch.report[0].y - panel.minAaY) * (HEIGHT - 1) / (panel.maxAaY - panel.minAaY) };
				has_event = true;
			}
			touched = touch.reportNum != 0;
		}
		if(has_event)
		{
			uint64_t previous_operation = plus_vita_app_view(app).operation;
			PlusVitaAppEffect effect = plus_vita_app_dispatch(app, event);
			if(previous_operation && plus_vita_app_view(app).operation != previous_operation)
				plus_vita_account_cancel(account);
			queue_effect(&pending, effect);
			plus_vita_credential_clear(&effect.credential);
			dirty = true;
		}
		else if(pending.kind != PLUS_VITA_EFFECT_NONE)
		{
			PlusVitaAppEffect effect = pending;
			plus_vita_credential_clear(&pending.credential);
			pending.kind = PLUS_VITA_EFFECT_NONE;
			if(effect.operation && effect.operation == plus_vita_app_view(app).operation)
			{
				if(effect.kind == PLUS_VITA_EFFECT_PREPARE_SETUP)
					plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
				else if(effect.kind == PLUS_VITA_EFFECT_ENTER_TOKEN)
				{
					PlusVitaCredential credential;
					PlusVitaCredentialImeResult keyboard;
					plus_vita_credential_ime_read_masked(&credential, &keyboard);
					PlusVitaAppResult status = !keyboard.initialized ? PLUS_VITA_RESULT_INPUT_ERROR
						: !keyboard.accepted ? PLUS_VITA_RESULT_CANCELLED : PLUS_VITA_RESULT_OK;
					queue_effect(&pending, plus_vita_app_dispatch(app, (PlusVitaAppEvent){
						.kind = PLUS_VITA_EVENT_TOKEN_ENTERED, .operation = effect.operation, .status = status,
						.data = credential.value, .data_size = credential.length }));
					plus_vita_credential_clear(&credential);
					/* The keyboard consumed these presses; do not replay Enter/Cancel. */
					SceCtrlData after_keyboard = { 0 };
					if(sceCtrlPeekBufferPositive(0, &after_keyboard, 1) > 0) previous_buttons = after_keyboard.buttons;
					previous_direction = 0;
					touched = true;
				}
				else if(!plus_vita_account_start(account, &effect))
				{
					PlusVitaAppEventKind kind = effect.kind == PLUS_VITA_EFFECT_READ_IMPORT ? PLUS_VITA_EVENT_IMPORT_READ
						: effect.kind == PLUS_VITA_EFFECT_REMOVE_IMPORT ? PLUS_VITA_EVENT_IMPORT_REMOVED : PLUS_VITA_EVENT_AUTHORIZED;
					queue_effect(&pending, plus_vita_app_dispatch(app, (PlusVitaAppEvent){
						.kind = kind, .operation = effect.operation, .status = PLUS_VITA_RESULT_BUSY }));
				}
			}
			plus_vita_credential_clear(&effect.credential);
			dirty = true;
		}
		PlusVitaAppEvent service_result;
		if(plus_vita_account_poll(account, &service_result))
		{
			queue_effect(&pending, plus_vita_app_dispatch(app, service_result));
			plus_vita_account_release_result(account);
			dirty = true;
		}
		sceDisplayWaitVblankStart();
	}
	if(restore_touch) sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, previous_touch);
	if(restore_mode) sceCtrlSetSamplingMode(previous_mode);
	plus_vita_credential_clear(&pending.credential);
	plus_vita_account_close(account);
	plus_vita_app_destroy(app);
	if(display_close(display)) free(display);
	/* On failed scanout restoration leave its buffers alive until process exit. */
	return result;
}
