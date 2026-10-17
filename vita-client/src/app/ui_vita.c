// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_vita.h"
#include "app.h"
#include "account_vita.h"
#include "font_vita.h"
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include "../platform/credential_ime.h"
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

_Static_assert((unsigned)PLUS_VITA_MENU_CROSS == (unsigned)SCE_CTRL_CROSS, "Menu Cross bit must match VitaSDK");
_Static_assert((unsigned)PLUS_VITA_MENU_CIRCLE == (unsigned)SCE_CTRL_CIRCLE, "Menu Circle bit must match VitaSDK");

typedef struct {
	SceUID blocks[2];
	uint32_t *pixels[2];
	unsigned back;
	SceDisplayFrameBuf saved;
	bool displayed;
	PlusVitaFont *font;
} UiDisplay;

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
	plus_vita_font_free(display->font);
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
	display->font = plus_vita_font_create();
	if(!display->font) { display_close(display); return false; }
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

static void text(UiDisplay *display, const char *value, int x, int y, int size, int width, int height, bool wrap, uint32_t color)
{
	plus_vita_font_draw(display->font, display->pixels[display->back], PITCH, value,
		(PlusVitaTextBox){x, y, width, height}, size, wrap, color);
}
static bool draw(UiDisplay *display, const PlusVitaAppView *view)
{
	for(int y = 0; y < HEIGHT; y++)
		rectangle(display, 0, y, WIDTH, 1, RGB(0, 25 + y / 20, 70 + y / 6));
	rectangle(display, 0, 0, WIDTH, 4, RGB(246, 198, 91));
	text(display, "plus vita", 64, 25, 24, 400, 38, false, RGB(236, 244, 250));
	text(display, "DEVELOPMENT BUILD", 712, 32, 14, 220, 24, false, RGB(195, 211, 239));
	text(display, view->subtitle, 64, 94, 16, 832, 26, false, RGB(246, 198, 91));
	text(display, view->title, 64, 128, 36, 832, 54, false, RGB(243, 247, 252));
	int body_bottom = view->action_count ? view->actions[0].y - 12 : 480;
	text(display, view->body, 64, 196, view->screen == PLUS_VITA_APP_ACCOUNT ? 18 : 20,
		view->screen == PLUS_VITA_APP_WELCOME ? 520 : 832, body_bottom - 196, true, RGB(210, 224, 247));
	if(view->screen == PLUS_VITA_APP_WELCOME)
	{
		rounded(display, 674, 238, 192, 192, RGB(18, 53, 113));
		text(display, "APP LOGO", 712, 301, 18, 150, 30, false, RGB(210, 224, 247));
		text(display, "Coming later", 709, 335, 16, 150, 28, false, RGB(195, 211, 239));
	}
	for(size_t i = 0; i < view->action_count; i++)
	{
		const PlusVitaAppButton *button = &view->actions[i];
		bool selected = i == view->selected;
		rounded(display, button->x, button->y, button->width, button->height,
			selected ? RGB(246, 198, 91) : RGB(17, 57, 125));
		text(display, button->label, button->x + 20, button->y + 8, 22, button->width - 40, button->height - 16, false,
			selected ? RGB(9, 29, 66) : RGB(243, 247, 252));
	}
	text(display, view->hint, 64, 509, 16, 832, 28, false, RGB(195, 211, 239));
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

static bool handle_input(PlusVitaApp *app, PlusVitaAccount *account, PlusVitaAppEffect *pending, PlusVitaAppEvent event)
{
	PlusVitaAppView before = plus_vita_app_view(app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app, event);
	PlusVitaAppView after = plus_vita_app_view(app);
	if(before.operation && after.operation != before.operation) plus_vita_account_cancel(account);
	bool changed = effect.kind != PLUS_VITA_EFFECT_NONE || memcmp(&before, &after, sizeof(before)) != 0;
	queue_effect(pending, effect);
	plus_vita_credential_clear(&effect.credential);
	return changed;
}

static bool storage_effect(PlusVitaAppEffectKind kind)
{
	return kind == PLUS_VITA_EFFECT_LOAD_ACCOUNT || kind == PLUS_VITA_EFFECT_SAVE_ACCOUNT || kind == PLUS_VITA_EFFECT_DELETE_ACCOUNT;
}

static PlusVitaAppEventKind completion(PlusVitaAppEffectKind kind)
{
	switch(kind) {
		case PLUS_VITA_EFFECT_LOAD_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_LOADED;
		case PLUS_VITA_EFFECT_SAVE_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_SAVED;
		case PLUS_VITA_EFFECT_DELETE_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_DELETED;
		case PLUS_VITA_EFFECT_READ_IMPORT: return PLUS_VITA_EVENT_IMPORT_READ;
		default: return PLUS_VITA_EVENT_AUTHORIZED;
	}
}

int plus_vita_app_run(void)
{
	UiDisplay *display = calloc(1, sizeof(*display));
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	PlusVitaAccount *storage = plus_vita_account_create();
	if(!display || !app || !account || !storage)
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); return 1; }
	if(!display_open(display))
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); return 1; }
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
	PlusVitaAppEffect pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START });
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
		bool input_changed = false;
		if(sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
		{
			input_changed = handle_input(app, account, &pending, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD,
				.pad = { .buttons = pad.buttons, .left_y = pad.ly, .time_us = sceKernelGetProcessTimeWide() } });
		}
		SceTouchData touch = { 0 };
		if(touch_ready && sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0)
		{
			if(!input_changed && touch.reportNum && !touched)
			{
				input_changed = handle_input(app, account, &pending, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH,
					.x = (touch.report[0].x - panel.minAaX) * (WIDTH - 1) / (panel.maxAaX - panel.minAaX),
					.y = (touch.report[0].y - panel.minAaY) * (HEIGHT - 1) / (panel.maxAaY - panel.minAaY) });
			}
			touched = touch.reportNum != 0;
		}
		dirty = dirty || input_changed;
		if(!input_changed && pending.kind != PLUS_VITA_EFFECT_NONE
			&& (!storage_effect(pending.kind) || plus_vita_account_idle(storage)))
		{
			PlusVitaAppEffect effect = pending;
			plus_vita_credential_clear(&pending.credential);
			pending.kind = PLUS_VITA_EFFECT_NONE;
			if(effect.operation && effect.operation == plus_vita_app_view(app).operation)
			{
				if(effect.kind == PLUS_VITA_EFFECT_PREPARE_SETUP)
					plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
				else if(effect.kind == PLUS_VITA_EFFECT_EDIT_LABEL)
				{
					char label[129]; PlusVitaCredentialImeResult keyboard;
					plus_vita_account_label_read(label, &keyboard);
					PlusVitaAppResult status = !keyboard.initialized ? PLUS_VITA_RESULT_STORAGE_ERROR
						: !keyboard.accepted ? PLUS_VITA_RESULT_CANCELLED : !keyboard.validated ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_OK;
					queue_effect(&pending, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LABEL_ENTERED,
						.operation = effect.operation, .status = status, .data = label, .data_size = strlen(label) }));
					memset(label, 0, sizeof(label));
					SceCtrlData after = {0};
					sceCtrlPeekBufferPositive(0, &after, 1);
					plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD_RESET,
						.pad = { .buttons = after.buttons, .left_y = after.ly, .time_us = sceKernelGetProcessTimeWide() } });
					touched = true;
				}
				else if(!plus_vita_account_start(storage_effect(effect.kind) ? storage : account, &effect))
				{
					queue_effect(&pending, plus_vita_app_dispatch(app, (PlusVitaAppEvent){
						.kind = completion(effect.kind), .operation = effect.operation,
						.status = storage_effect(effect.kind) ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_BUSY }));
				}
			}
			plus_vita_credential_clear(&effect.credential);
			dirty = true;
		}
		PlusVitaAccount *workers[] = {account, storage};
		for(unsigned i = 0; i < 2; i++) {
			PlusVitaAppEvent service_result;
			if(plus_vita_account_poll(workers[i], &service_result)) {
				queue_effect(&pending, plus_vita_app_dispatch(app, service_result));
				plus_vita_account_release_result(workers[i]);
				dirty = true;
			}
		}
		sceDisplayWaitVblankStart();
	}
	if(restore_touch) sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, previous_touch);
	if(restore_mode) sceCtrlSetSamplingMode(previous_mode);
	plus_vita_credential_clear(&pending.credential);
	plus_vita_account_close(account);
	plus_vita_account_close(storage);
	plus_vita_app_destroy(app);
	if(display_close(display)) free(display);
	/* On failed scanout restoration leave its buffers alive until process exit. */
	return result;
}
