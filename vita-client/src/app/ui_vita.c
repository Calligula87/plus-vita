// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_vita.h"
#include "app.h"
#include "account_vita.h"
#include "font_vita.h"
#include "ui_flow.h"
#include "play_vita.h"
#include "attempt_log.h"
#if PLUS_VITA_FONT_COMPARISON
#include "comparison_capture.h"
#endif
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include "../platform/credential_ime.h"
#include "../platform/diagnostic_log.h"
#include "../platform/latency_clock.h"
#include "../platform/network.h"
#include "../platform/request_cancel.h"
#include "../platform/recovery_vita.h"
#include "../platform/stream_probe.h"
#include <curl/curl.h>
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
_Static_assert((unsigned)PLUS_VITA_MENU_TRIANGLE == (unsigned)SCE_CTRL_TRIANGLE, "Menu Triangle bit must match VitaSDK");

typedef struct {
	SceUID blocks[2];
	uint32_t *pixels[2];
	unsigned back;
	SceDisplayFrameBuf saved;
	bool displayed;
	PlusVitaFont *font;
	PlusVitaFont *alternative;
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
	/* The base font owns PGF; close its alternative's PVF fallback first. */
	plus_vita_font_free(display->alternative);
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
	/* B was the second, preferred device comparison; preserve A/B in the
	 * explicit comparison profile while making B the normal renderer. */
#if PLUS_VITA_FONT_DEFAULT_FREETYPE && !PLUS_VITA_FONT_COMPARISON
	display->font = plus_vita_font_create_alternative("app0:/fonts/NotoSans-Regular.ttf");
#else
	display->font = plus_vita_font_create();
#endif
	if(!display->font) { display_close(display); return false; }
#if PLUS_VITA_FONT_COMPARISON
	display->alternative=plus_vita_font_create_alternative("app0:/fonts/NotoSans-Regular.ttf");
#endif
	return true;
}

static bool draw(UiDisplay *display, PlusVitaUiFlow *flow)
{
	uint64_t started = plus_vita_latency_now_us();
	PlusVitaAppView view=plus_vita_app_view(flow->app);
	if(view.screen==PLUS_VITA_APP_FONT_COMPARISON && view.alternative_font && !display->alternative) {
		view.subtitle="B: FREETYPE UNAVAILABLE";
		view.body="The comparison font could not be loaded.\nA remains available. This is not a successful B result.";
		plus_vita_app_paint(display->font,display->pixels[display->back],PITCH,&view); flow->dirty=false;
	} else plus_vita_ui_flow_paint(flow,view.screen==PLUS_VITA_APP_FONT_COMPARISON && view.alternative_font ? display->alternative : display->font,
		display->pixels[display->back],PITCH);
	uint64_t painted = plus_vita_latency_now_us();
	if(painted >= started) plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_DRAW, painted-started);
	if(view.screen==PLUS_VITA_APP_FONT_COMPARISON && painted>=started && (!view.alternative_font || display->alternative))
		plus_vita_diagnostic_ui_sample(view.alternative_font ? PLUS_VITA_UI_FONT_B_DRAW : PLUS_VITA_UI_FONT_A_DRAW,painted-started);
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

static bool handle_input(PlusVitaUiFlow *flow, PlusVitaAccount *account,
	PlusVitaPlay *play, PlusVitaAppEvent event)
{
	bool changed = plus_vita_ui_flow_event(flow, event, plus_vita_latency_now_us());
	if(flow->cancel_account) {
		plus_vita_account_cancel(account);
		plus_vita_play_cancel(play);
	}
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

/* Keep Circle responsive while the cloud transport is connecting. The stream
 * takes over the display and input only after this stage has completed. */
static bool play_connect_pump(void *user)
{
	(void)user;
	SceCtrlData pad={0};
	if(plus_vita_recovery_poll()!=PLUS_VITA_RECOVERY_NONE) return false;
	return sceCtrlPeekBufferPositive(0,&pad,1) <= 0
		|| !(pad.buttons & SCE_CTRL_CIRCLE);
}

int plus_vita_app_run(void)
{
	plus_vita_diagnostic_ui_begin();
	bool log_available = plus_vita_diagnostic_persistence_ok();
	UiDisplay *display = calloc(1, sizeof(*display));
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	PlusVitaAccount *storage = plus_vita_account_create();
	PlusVitaPlay *play = plus_vita_play_create();
	PlusVitaCovers *covers = NULL;
	if(!display || !app || !account || !storage || !play)
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); plus_vita_play_close(play); plus_vita_covers_close(covers); plus_vita_diagnostic_ui_end(); return 1; }
	if(!display_open(display))
	{ free(display); plus_vita_app_destroy(app); plus_vita_account_close(account); plus_vita_account_close(storage); plus_vita_play_close(play); plus_vita_covers_close(covers); plus_vita_diagnostic_ui_end(); return 1; }
	/* Keep a process-lifetime reference while account HTTP and artwork overlap.
	 * Individual account requests balance their own curl reference as before. */
	bool network_owned = !plus_vita_network_is_initialized();
	bool network_ready = plus_vita_network_init() == CHIAKI_ERR_SUCCESS;
	bool curl_ready = network_ready && curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	bool recovery_ready = plus_vita_recovery_init();
	if(curl_ready) covers = plus_vita_covers_create();
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
#if PLUS_VITA_FONT_COMPARISON
	uint32_t capture_buttons=0;
#endif
	PlusVitaUiFlow flow = { .app = app, .dirty = true, .covers = covers };
	uint64_t artwork_epoch = UINT64_MAX;
	unsigned reported_play_stage=0;
	bool metadata_reported=false;
	PlusVitaAppScreen reported_screen=PLUS_VITA_APP_CLOSED;
	plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START,
		.log_available = log_available }, 0);
	int result = 0;
	uint64_t previous_poll = 0, last_report = plus_vita_latency_now_us();
	while(plus_vita_app_view(app).screen != PLUS_VITA_APP_CLOSED)
	{
		bool workers_idle=plus_vita_account_idle(account) && !plus_vita_play_busy(play);
		if(plus_vita_attempt_log_prepare(&flow,workers_idle)) {
			log_available=plus_vita_diagnostic_persistence_ok();
			reported_screen=PLUS_VITA_APP_CLOSED;
		}
		PlusVitaAppView current=plus_vita_app_view(app);
		if(current.screen!=reported_screen) {
			plus_vita_diagnostic_event("ui.stage",(int)current.screen);
			reported_screen=current.screen;
		}
		PlusVitaRecovery interruption=plus_vita_recovery_poll();
		if(interruption==PLUS_VITA_RECOVERY_SLEEP && !current.operation)
			plus_vita_recovery_acknowledge();
		if(interruption!=PLUS_VITA_RECOVERY_NONE && current.operation) {
			plus_vita_diagnostic_event(interruption==PLUS_VITA_RECOVERY_SLEEP ? "recovery.sleep" : "recovery.wifi",0);
			handle_input(&flow,account,play,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,
				.status=interruption==PLUS_VITA_RECOVERY_SLEEP ? PLUS_VITA_RESULT_SLEEP : PLUS_VITA_RESULT_NETWORK_ERROR});
			if(flow.cancel_account && workers_idle) plus_vita_diagnostic_ui_end();
			plus_vita_recovery_acknowledge();
			current=plus_vita_app_view(app);
		}
		if(current.account_epoch!=artwork_epoch) {
			plus_vita_covers_reset(covers); artwork_epoch=current.account_epoch; metadata_reported=false;
			plus_vita_diagnostic_artwork_metadata(current.artwork_metadata);
		}
		if(current.account_connected && !metadata_reported) {
			plus_vita_diagnostic_artwork_metadata(current.artwork_metadata); metadata_reported=true;
		}
		if(plus_vita_covers_poll(covers)) {
			plus_vita_diagnostic_artwork_result(plus_vita_covers_last_result(covers)); flow.dirty=true;
		}
		const char *urls[3]={NULL,NULL,NULL}; size_t url_count=0;
		if(current.screen==PLUS_VITA_APP_CATALOGUE) for(unsigned i=0;i<3;i++) if(current.games[i]) urls[url_count++]=current.games[i]->cover_url;
		if(current.screen==PLUS_VITA_APP_DETAILS && current.detail) urls[url_count++]=current.detail->cover_url;
		plus_vita_covers_request(covers,urls,url_count);
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
			input_changed = handle_input(&flow, account, play, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD,
				.pad = { .buttons = pad.buttons, .left_x = pad.lx, .left_y = pad.ly, .time_us = sceKernelGetProcessTimeWide() } });
#if PLUS_VITA_FONT_COMPARISON
			if(!input_changed && current.screen==PLUS_VITA_APP_FONT_COMPARISON
				&& (pad.buttons & ~capture_buttons & SCE_CTRL_SQUARE)) {
				const char *path=current.alternative_font
					? (current.comparison_page ? "ux0:data/plus-vita-font-B-tiles.ppm" : "ux0:data/plus-vita-font-B-setup.ppm")
					: (current.comparison_page ? "ux0:data/plus-vita-font-A-tiles.ppm" : "ux0:data/plus-vita-font-A-setup.ppm");
				bool saved=plus_vita_comparison_capture(display->pixels[display->back^1u],PITCH,path);
				plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_FONT_CAPTURED,
					.status=saved ? PLUS_VITA_RESULT_OK : PLUS_VITA_RESULT_STORAGE_ERROR},0);
				previous_poll=flow.input_since=0;
			}
#endif
#if PLUS_VITA_FONT_COMPARISON
			capture_buttons=pad.buttons;
#endif
		}
		SceTouchData touch = { 0 };
		if(touch_ready && sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0)
		{
			if(!input_changed && touch.reportNum && !touched)
			{
				input_changed = handle_input(&flow, account, play, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH,
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
				else if(effect.kind == PLUS_VITA_EFFECT_EDIT_LABEL || effect.kind == PLUS_VITA_EFFECT_EDIT_SEARCH)
				{
					char label[129]; PlusVitaCredentialImeResult keyboard;
					if(effect.kind==PLUS_VITA_EFFECT_EDIT_SEARCH) plus_vita_catalogue_search_read(label,&keyboard);
					else plus_vita_account_label_read(label, &keyboard);
					PlusVitaAppResult status = !keyboard.initialized ? PLUS_VITA_RESULT_STORAGE_ERROR
						: !keyboard.accepted ? PLUS_VITA_RESULT_CANCELLED : !keyboard.validated ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_OK;
					plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){ .kind = effect.kind==PLUS_VITA_EFFECT_EDIT_SEARCH ? PLUS_VITA_EVENT_SEARCH_ENTERED : PLUS_VITA_EVENT_LABEL_ENTERED,
						.operation = effect.operation, .status = status, .data = label, .data_size = strlen(label) }, 0);
					memset(label, 0, sizeof(label));
					SceCtrlData after = {0};
					sceCtrlPeekBufferPositive(0, &after, 1);
					plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD_RESET,
						.pad = { .buttons = after.buttons, .left_x = after.lx, .left_y = after.ly, .time_us = sceKernelGetProcessTimeWide() } });
					touched = true;
					previous_poll = flow.input_since = 0; /* Human keyboard time is not menu latency. */
				}
				else if(effect.kind==PLUS_VITA_EFFECT_PLAY)
				{
					if(plus_vita_play_busy(play) || !plus_vita_account_idle(account))
						plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,
							.operation=effect.operation,.status=PLUS_VITA_RESULT_BUSY},0);
					else {
						plus_vita_recovery_acknowledge();
						reported_play_stage=0;
						if(!network_ready || !curl_ready || !recovery_ready || !plus_vita_play_start(play,&effect)) {
							plus_vita_diagnostic_event("allocation.complete",(int)CHIAKI_ERR_NETWORK);
							plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,
								.operation=effect.operation,.status=!recovery_ready ? PLUS_VITA_RESULT_SERVICE_ERROR : PLUS_VITA_RESULT_NETWORK_ERROR},0);
							plus_vita_diagnostic_ui_end();
						}
					}
				}
				else if((!storage_effect(effect.kind) && plus_vita_play_busy(play))
					|| !plus_vita_account_start(storage_effect(effect.kind) ? storage : account, &effect))
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
				plus_vita_diagnostic_event("account.complete",(int)service_result.status);
				plus_vita_ui_flow_event(&flow, service_result, 0);
				plus_vita_account_release_result(workers[i]);
				flow.dirty = true;
			}
		}
		if(plus_vita_play_ready(play)) {
			ChiakiCloudProvisionResult provision={0};
			ChiakiErrorCode allocation_error=CHIAKI_ERR_UNKNOWN;
			uint64_t operation=0;
			plus_vita_play_take(play,&provision,&allocation_error,&operation);
			if(operation && operation==plus_vita_app_view(app).operation) {
				bool user_stopped=false;
				ChiakiErrorCode stream_error=allocation_error;
				if(allocation_error==CHIAKI_ERR_SUCCESS) {
					plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_PROGRESS,
						.operation=operation,.play_stage=5},0);
					if(!draw(display,&flow)) { result=1; plus_vita_play_result_clear(&provision); break; }
					ChiakiLog stream_log;
					chiaki_log_init(&stream_log,CHIAKI_LOG_INFO | CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR,
						plus_vita_diagnostic_chiaki_log,NULL);
					plus_vita_diagnostic_event("stream_probe.begin",0);
					stream_error=plus_vita_stream_probe_with_pump(&provision,&stream_log,
						&user_stopped,play_connect_pump,NULL);
					plus_vita_diagnostic_event("stream_probe.complete",(int)stream_error);
				}
				PlusVitaRecovery stopped=plus_vita_recovery_poll();
				if(stopped!=PLUS_VITA_RECOVERY_NONE) {
					plus_vita_diagnostic_event(stopped==PLUS_VITA_RECOVERY_SLEEP ? "recovery.sleep" : "recovery.wifi",0);
					plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,
						.status=stopped==PLUS_VITA_RECOVERY_SLEEP ? PLUS_VITA_RESULT_SLEEP : PLUS_VITA_RESULT_NETWORK_ERROR},0);
					plus_vita_recovery_acknowledge();
				} else plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,
					.operation=operation,.status=stream_error==CHIAKI_ERR_SUCCESS && user_stopped
						? PLUS_VITA_RESULT_OK : stream_error==CHIAKI_ERR_CANCELED
						? PLUS_VITA_RESULT_CANCELLED : allocation_error!=CHIAKI_ERR_SUCCESS
						? plus_vita_play_failure(allocation_error,&provision) : PLUS_VITA_RESULT_STREAM_STOPPED},0);
				SceCtrlData released={0};
				sceCtrlPeekBufferPositive(0,&released,1);
				plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD_RESET,
					.pad={.buttons=released.buttons,.left_x=released.lx,.left_y=released.ly,
						.time_us=sceKernelGetProcessTimeWide()}});
				previous_poll=flow.input_since=0;
				flow.dirty=true;
			}
			/* Complete/cancelled attempts stay readable on disk while browsing.
			 * No old allocation worker remains to produce into the next log. */
			plus_vita_diagnostic_ui_end();
			plus_vita_play_result_clear(&provision);
		} else if(play && plus_vita_app_view(app).operation) {
			unsigned stage=plus_vita_play_stage(play);
			if(stage && stage!=reported_play_stage) {
				reported_play_stage=stage;
				plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_PROGRESS,
					.operation=plus_vita_app_view(app).operation,.play_stage=stage},0);
			}
		}
		uint64_t now = plus_vita_latency_now_us();
		if(now >= last_report && now-last_report >= 1000000u) {
			plus_vita_diagnostic_ui_flush(); last_report = now;
			bool current_log = plus_vita_diagnostic_persistence_ok();
			if(current_log != log_available) {
				log_available = current_log;
				plus_vita_ui_flow_event(&flow, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LOG_STATUS,
					.log_available = log_available }, 0);
			}
		}
		sceDisplayWaitVblankStart();
	}
	if(restore_touch) sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, previous_touch);
	if(restore_mode) sceCtrlSetSamplingMode(previous_mode);
	plus_vita_credential_clear(&flow.pending.credential);
	bool account_closed = plus_vita_account_close(account);
	bool storage_closed = plus_vita_account_close(storage);
	bool play_closed = plus_vita_play_close(play);
	bool covers_closed = plus_vita_covers_close(covers);
	if(account_closed && storage_closed && play_closed && covers_closed) {
		if(curl_ready) curl_global_cleanup();
		if(network_ready && network_owned) plus_vita_network_fini();
	} /* A live cancelled worker retains its resources until process exit. */
	plus_vita_app_destroy(app);
	if(recovery_ready) plus_vita_recovery_fini();
	if(display_close(display)) free(display);
	plus_vita_diagnostic_ui_end();
	/* On failed scanout restoration leave its buffers alive until process exit. */
	return result;
}
