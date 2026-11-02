// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_flow.h"
#include <string.h>
#define MENU_TRANSITION_US 120000u

static bool inside(const PlusVitaAppButton *button,int x,int y)
{
	return x>=button->x && x<button->x+button->width
		&& y>=button->y && y<button->y+button->height;
}

static bool capture_current(const PlusVitaUiFlow *flow,const PlusVitaAppView *view)
{
	if(!flow->touch_captured || view->screen!=flow->touch_screen
		|| view->operation!=flow->touch_operation || flow->touch_index>=view->action_count) return false;
	const PlusVitaAppButton *current=&view->actions[flow->touch_index];
	return current->id==flow->touch_target.id && current->x==flow->touch_target.x
		&& current->y==flow->touch_target.y && current->width==flow->touch_target.width
		&& current->height==flow->touch_target.height;
}

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
	flow->now_us=now_us;
	bool changed=false;
	if(flow->splash_active && now_us>=flow->splash_started_us
		&& now_us-flow->splash_started_us>=1000000u) { finish_splash(flow);changed=true; }
	if(flow->transition_active) {
		if(now_us<flow->transition_started_us) flow->transition_started_us=now_us;
		if(now_us-flow->transition_started_us>=MENU_TRANSITION_US) flow->transition_active=false;
		flow->dirty=true;changed=true;
	}
	return changed;
}

bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us)
{
	flow->cancel_account=false;
	if(sampled_us>flow->now_us) flow->now_us=sampled_us;
	if(flow->transition_active && event.kind==PLUS_VITA_EVENT_PAD) {
		plus_vita_app_dispatch(flow->app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD_RESET,.pad=event.pad});
		return false;
	}
	PlusVitaAppView touch_view=plus_vita_app_view(flow->app);
	if(event.kind==PLUS_VITA_EVENT_TOUCH_DOWN) {
		if(flow->touch_held) return false;
		flow->touch_held=true;
		if(flow->transition_active) return false;
		if(flow->splash_active) { finish_splash(flow);return true; }
		for(size_t i=0;i<touch_view.action_count;i++) if(inside(&touch_view.actions[i],event.x,event.y)) {
			flow->touch_captured=true;flow->touch_index=i;
			flow->touch_target=touch_view.actions[i];flow->touch_screen=touch_view.screen;
			flow->touch_operation=touch_view.operation;flow->dirty=true;return true;
		}
		return false;
	}
	if(event.kind==PLUS_VITA_EVENT_TOUCH_MOVE || event.kind==PLUS_VITA_EVENT_TOUCH_CANCEL) {
		if(flow->touch_captured && (event.kind==PLUS_VITA_EVENT_TOUCH_CANCEL
			|| !capture_current(flow,&touch_view) || !inside(&flow->touch_target,event.x,event.y))) {
			flow->touch_captured=false;flow->dirty=true;return true;
		}
		return false;
	}
	if(event.kind==PLUS_VITA_EVENT_TOUCH_UP) {
		bool activate=flow->touch_held && capture_current(flow,&touch_view)
			&& inside(&flow->touch_target,event.x,event.y);
		bool repaint=flow->touch_captured;
		flow->touch_held=false;flow->touch_captured=false;flow->dirty|=repaint;
		if(!activate) return repaint;
		event.kind=PLUS_VITA_EVENT_TOUCH;
	}
	if(flow->transition_active && (event.kind==PLUS_VITA_EVENT_TOUCH || event.kind==PLUS_VITA_EVENT_ACTIVATE
		|| event.kind==PLUS_VITA_EVENT_UP || event.kind==PLUS_VITA_EVENT_DOWN
		|| event.kind==PLUS_VITA_EVENT_BACK || event.kind==PLUS_VITA_EVENT_SEARCH)) return false;
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
	if(flow->touch_captured && (changed || event.kind==PLUS_VITA_EVENT_INTERRUPTED)) {
		flow->touch_captured=false;flow->dirty=true;
	}
	if(before.screen!=after.screen && !flow->splash_active && !flow->splash_release
		&& after.screen!=PLUS_VITA_APP_CLOSED) {
		flow->transition_active=true;flow->transition_started_us=flow->now_us;
	}
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
	view.touch_pressed=capture_current(flow,&view);
	view.pressed_index=flow->touch_index;
	plus_vita_app_paint_with_resources(font, pixels, pitch, &view, flow->covers, flow->branding);
	if(flow->transition_active) {
		uint64_t elapsed=flow->now_us>=flow->transition_started_us ? flow->now_us-flow->transition_started_us : 0;
		unsigned opacity=elapsed>=MENU_TRANSITION_US ? 255 : (unsigned)(elapsed*255/MENU_TRANSITION_US);
		for(int y=0;y<544;y++) for(int x=0;x<960;x++) {
			uint32_t source=pixels[y*pitch+x], blended=0xff000000u;
			for(unsigned shift=0;shift<24;shift+=8) blended|=(((source>>shift)&255)*opacity/255)<<shift;
			pixels[y*pitch+x]=blended;
		}
	}
	flow->dirty = false;
	return true;
}
