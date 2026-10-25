// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_flow.h"
#include <string.h>

void plus_vita_ui_flow_start(PlusVitaUiFlow *flow,PlusVitaBranding *branding,uint64_t now_us,bool log_available)
{
	if(flow->startup_started) return;
	flow->startup_started=true;
	flow->branding=branding;
	flow->splash_active=plus_vita_branding_splash(branding)!=NULL;
	flow->splash_started_us=now_us;
	flow->dirty=true;
	plus_vita_ui_flow_event(flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START,.log_available=log_available},0);
}
static void finish_splash(PlusVitaUiFlow *flow)
{
	flow->splash_active=false;
	flow->splash_release=true;
	plus_vita_branding_release_splash(flow->branding);
	flow->dirty=true;
}
bool plus_vita_ui_flow_tick(PlusVitaUiFlow *flow,uint64_t now_us)
{
	if(!flow->splash_active || now_us<flow->splash_started_us || now_us-flow->splash_started_us<1000000u) return false;
	finish_splash(flow);return true;
}

bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us)
{
	flow->cancel_account=false;
	if(event.kind==PLUS_VITA_EVENT_PAD && (flow->splash_active || flow->splash_release)) {
		plus_vita_app_dispatch(flow->app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD_RESET,.pad=event.pad});
		if(flow->splash_active && (event.pad.buttons&PLUS_VITA_MENU_CROSS)) { finish_splash(flow);return true; }
		if(!event.pad.buttons && event.pad.left_x>=76 && event.pad.left_x<=180
			&& event.pad.left_y>=76 && event.pad.left_y<=180) flow->splash_release=false;
		return false;
	}
	if(flow->splash_active && (event.kind==PLUS_VITA_EVENT_TOUCH || event.kind==PLUS_VITA_EVENT_ACTIVATE)) {
		finish_splash(flow);return true;
	}
	if(flow->splash_active && (event.kind==PLUS_VITA_EVENT_BACK || event.kind==PLUS_VITA_EVENT_UP
		|| event.kind==PLUS_VITA_EVENT_DOWN || event.kind==PLUS_VITA_EVENT_SEARCH)) return false;
	if(flow->splash_active && event.kind==PLUS_VITA_EVENT_INTERRUPTED && event.status==PLUS_VITA_RESULT_SLEEP)
		finish_splash(flow);
	PlusVitaAppView before = plus_vita_app_view(flow->app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(flow->app, event);
	PlusVitaAppView after = plus_vita_app_view(flow->app);
	flow->cancel_account = before.operation && after.operation != before.operation;
	bool changed = effect.kind != PLUS_VITA_EFFECT_NONE || memcmp(&before, &after, sizeof(before)) != 0;
	if(effect.kind != PLUS_VITA_EFFECT_NONE) {
		plus_vita_credential_clear(&flow->pending.credential);
		flow->pending = effect;
	}
	plus_vita_credential_clear(&effect.credential);
	flow->dirty = flow->dirty || changed;
	if(changed && sampled_us && !flow->input_since) flow->input_since = sampled_us;
	return changed;
}

PlusVitaAppEffect plus_vita_ui_flow_take(PlusVitaUiFlow *flow)
{
	if(flow->splash_active) return (PlusVitaAppEffect){0};
	PlusVitaAppEffect effect = flow->pending;
	plus_vita_credential_clear(&flow->pending.credential);
	flow->pending.kind = PLUS_VITA_EFFECT_NONE;
	if(!effect.operation || effect.operation != plus_vita_app_view(flow->app).operation) {
		plus_vita_credential_clear(&effect.credential);
		return (PlusVitaAppEffect){0};
	}
	return effect;
}

bool plus_vita_ui_flow_paint(PlusVitaUiFlow *flow, PlusVitaFont *font, uint32_t *pixels, int pitch)
{
	if(!flow->dirty) return false;
	const uint32_t *splash=flow->splash_active ? plus_vita_branding_splash(flow->branding) : NULL;
	if(splash) {
		for(unsigned y=0;y<544;y++) memcpy(pixels+y*pitch,splash+y*960,960*sizeof(*pixels));
		flow->dirty=false;return true;
	}
	PlusVitaAppView view = plus_vita_app_view(flow->app);
	plus_vita_app_paint_with_resources(font, pixels, pitch, &view, flow->covers, flow->branding);
	flow->dirty = false;
	return true;
}
