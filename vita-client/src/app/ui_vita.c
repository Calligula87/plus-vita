// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_vita.h"
#include "app.h"
#include "account_vita.h"
#include "font_vita.h"
#include "ui_flow.h"
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include "../platform/credential_ime.h"
#include "../platform/diagnostic_log.h"
#include "../platform/latency_clock.h"
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

static bool draw(UiDisplay *display, PlusVitaUiFlow *flow)
{
	uint64_t started = plus_vita_latency_now_us();
	plus_vita_ui_flow_paint(flow, display->font, display->pixels[display->back], PITCH);
	uint64_t painted = plus_vita_latency_now_us();
	if(painted >= started) plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_DRAW, painted-started);
	SceDisplayFrameBuf frame = {
		.size = sizeof(frame), .base = display->pixels[display->back], .pitch = PITCH,
		.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8, .width = WIDTH, .height = HEIGHT
	};
	if(sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME) < 0) return false;
	display->displayed = true;
	if(sceDisplayWaitSetFrameBuf() < 0) return false;
	uint64_t presented = plus_vita_latency_now_us();
	if(presented >= painted) plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_PRESENT, presented-painted);
	display->back ^= 1u;
	return true;
}

static bool handle_input(PlusVitaUiFlow *flow, PlusVitaAccount *account, PlusVitaAppEvent event)
{
	bool changed = plus_vita_ui_flow_event(flow, event, plus_vita_latency_now_us());
	if(flow->cancel_account) plus_vita_account_cancel(account);
	return changed;
}
static bool storage_effect(PlusVitaAppEffectKind kind)
{
	return kind == PLUS_VITA_EFFECT_LOAD_ACCOUNT || kind == PLUS_VITA_EFFECT_SAVE_ACCOUNT
		|| kind == PLUS_VITA_EFFECT_DELETE_ACCOUNT || kind == PLUS_VITA_EFFECT_ENABLE_AUTO;
}

static PlusVitaAppEventKind completion(PlusVitaAppEffectKind kind)
{
	switch(kind) {
		case PLUS_VITA_EFFECT_LOAD_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_LOADED;
		case PLUS_VITA_EFFECT_SAVE_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_SAVED;
		case PLUS_VITA_EFFECT_DELETE_ACCOUNT: return PLUS_VITA_EVENT_ACCOUNT_DELETED;
		case PLUS_VITA_EFFECT_ENABLE_AUTO: return PLUS_VITA_EVENT_AUTO_ENABLED;
		case PLUS_VITA_EFFECT_READ_IMPORT: return PLUS_VITA_EVENT_IMPORT_READ;
		default: return PLUS_VITA_EVENT_AUTHORIZED;
	}
}

int plus_vita_app_run(void)
{
	plus_vita_diagnostic_ui_begin();
	UiDisplay *display = calloc(1, sizeof(*display));
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	PlusVitaAccount *storage = plus_vita_account_create();
	if(!display || !app || !account || !storage)
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); plus_vita_diagnostic_ui_end(); return 1; }
	if(!display_open(display))
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); plus_vita_diagnostic_ui_end(); return 1; }
	SceCtrlPadInputMode previous_mode = SCE_CTRL_MODE_DIGITAL;
	bool restore_mode = sceCtrlGetSamplingMode(&previous_mode) == 0;
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
	SceTouchSamplingState previous_touch = SCE_TOUCH_SAMPLING_STATE_STOP;
	bool restore_touch = sceTouchGetSamplingState(SCE_TOUCH_PORT_FRONT, &previous_touch) == 0;
	SceTouchPanelInfo panel = { 0 };
	bool touch_ready = sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panel) == 0
		&& panel.maxAaX > panel.minAaX && panel.maxAaY > panel.minAaY
		&& sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START) == 0;
	bool touched = false;
	PlusVitaUiFlow flow = { .app = app, .dirty = true };
	plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }, 0);
	int result = 0;
	uint64_t previous_poll = 0, last_report = plus_vita_latency_now_us();
	while(plus_vita_app_view(app).screen != PLUS_VITA_APP_CLOSED)
	{
		if(flow.dirty)
		{
			if(!draw(display, &flow)) { result = 1; break; }
			uint64_t shown = plus_vita_latency_now_us();
			if(flow.input_since && shown >= flow.input_since) plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_INPUT_PRESENT, shown-flow.input_since);
			flow.input_since = 0;
			flow.dirty = false;
		}
		SceCtrlData pad = { 0 };
		uint64_t polled = plus_vita_latency_now_us();
		if(previous_poll && polled >= previous_poll) plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_POLL_GAP, polled-previous_poll);
		previous_poll = polled;
		bool input_changed = false;
		if(sceCtrlPeekBufferPositive(0, &pad, 1) > 0)
		{
			input_changed = handle_input(&flow, account, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD,
				.pad = { .buttons = pad.buttons, .left_y = pad.ly, .time_us = sceKernelGetProcessTimeWide() } });
		}
		SceTouchData touch = { 0 };
		if(touch_ready && sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0)
		{
			if(!input_changed && touch.reportNum && !touched)
			{
				input_changed = handle_input(&flow, account, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH,
					.x = (touch.report[0].x - panel.minAaX) * (WIDTH - 1) / (panel.maxAaX - panel.minAaX),
					.y = (touch.report[0].y - panel.minAaY) * (HEIGHT - 1) / (panel.maxAaY - panel.minAaY) });
			}
			touched = touch.reportNum != 0;
		}
		flow.dirty = flow.dirty || input_changed;
		if(input_changed && !flow.input_since) flow.input_since = polled;
		if(!input_changed && flow.pending.kind != PLUS_VITA_EFFECT_NONE
			&& (!storage_effect(flow.pending.kind) || plus_vita_account_idle(storage)))
		{
			PlusVitaAppEffect effect = plus_vita_ui_flow_take(&flow);
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
					plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LABEL_ENTERED,
						.operation = effect.operation, .status = status, .data = label, .data_size = strlen(label) }, 0);
					memset(label, 0, sizeof(label));
					SceCtrlData after = {0};
					sceCtrlPeekBufferPositive(0, &after, 1);
					plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD_RESET,
						.pad = { .buttons = after.buttons, .left_y = after.ly, .time_us = sceKernelGetProcessTimeWide() } });
					touched = true;
					previous_poll = flow.input_since = 0; /* Human keyboard time is not menu latency. */
				}
				else if(!plus_vita_account_start(storage_effect(effect.kind) ? storage : account, &effect))
				{
					plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){
						.kind = completion(effect.kind), .operation = effect.operation,
						.status = storage_effect(effect.kind) ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_BUSY }, 0);
				}
			}
			plus_vita_credential_clear(&effect.credential);
			flow.dirty = true;
		}
		PlusVitaAccount *workers[] = {account, storage};
		for(unsigned i = 0; i < 2; i++) {
			PlusVitaAppEvent service_result;
			if(plus_vita_account_poll(workers[i], &service_result)) {
				plus_vita_ui_flow_event(&flow, service_result, 0);
				plus_vita_account_release_result(workers[i]);
				flow.dirty = true;
			}
		}
		uint64_t now = plus_vita_latency_now_us();
		if(now >= last_report && now-last_report >= 1000000u) {
			plus_vita_diagnostic_ui_flush(); last_report = now;
		}
		sceDisplayWaitVblankStart();
	}
	if(restore_touch) sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, previous_touch);
	if(restore_mode) sceCtrlSetSamplingMode(previous_mode);
	plus_vita_credential_clear(&flow.pending.credential);
	plus_vita_account_close(account);
	plus_vita_account_close(storage);
	plus_vita_app_destroy(app);
	if(display_close(display)) free(display);
	plus_vita_diagnostic_ui_end();
	/* On failed scanout restoration leave its buffers alive until process exit. */
	return result;
}
