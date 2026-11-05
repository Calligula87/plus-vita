// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_flow.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* Twelve native screen pixels distinguish a deliberate drag from a tap. */
#define TOUCH_DRAG_PIXELS 12
#define MENU_TRANSITION_US 150000u

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

/* All artwork policy lives here so production and controlled-clock journeys agree. */
static bool artwork_tick(PlusVitaUiFlow *flow)
{
	PlusVitaAppView view=plus_vita_app_view(flow->app);
	bool changed=false;
	if(!flow->artwork_initialized || view.account_epoch!=flow->artwork_epoch) {
		plus_vita_covers_reset(flow->covers);flow->artwork_epoch=view.account_epoch;
		flow->artwork_initialized=true;flow->preview_valid=false;changed=true;
	}
	const PlusVitaCatalogEntry *game=NULL;
	if(view.screen==PLUS_VITA_APP_DETAILS) game=view.detail;
	if(view.screen==PLUS_VITA_APP_CATALOGUE && view.selected<view.list_count) game=view.detail;
	bool same=game && flow->preview_valid && !strcmp(flow->preview_id,game->id)
		&& !strcmp(flow->preview_name,game->name) && !strcmp(flow->preview_url,game->cover_url);
	bool dragging=flow->touch_dragging;
	if(!same || dragging || flow->preview_dragging) {
		plus_vita_covers_request(flow->covers,NULL,0);
		flow->preview_eligible=false;flow->preview_requested=false;
		flow->preview_since=flow->now_us;changed|=flow->preview_valid || game!=NULL;
		flow->preview_valid=game!=NULL;
		if(game) {
			snprintf(flow->preview_id,sizeof(flow->preview_id),"%s",game->id);
			snprintf(flow->preview_name,sizeof(flow->preview_name),"%s",game->name);
			snprintf(flow->preview_url,sizeof(flow->preview_url),"%s",game->cover_url);
		}
	}
	if(plus_vita_covers_poll(flow->covers)) { flow->artwork_completed=true;changed=true; }
	if(game && !dragging) {
		const PlusVitaCoverImage *cached=plus_vita_covers_find(flow->covers,game->cover_url);
		bool details_return=same && flow->preview_screen==PLUS_VITA_APP_DETAILS
			&& cached && cached->state==PLUS_VITA_COVER_READY;
		if(flow->now_us<flow->preview_since) flow->preview_since=flow->now_us;
		if(view.screen==PLUS_VITA_APP_DETAILS || details_return
			|| flow->now_us-flow->preview_since>=3000000u) {
			if(!flow->preview_eligible) { flow->preview_eligible=true;changed=true; }
			if(!flow->preview_requested) {
				plus_vita_covers_retry(flow->covers,game->cover_url);flow->preview_requested=true;
			}
			const char *url=game->cover_url;
			plus_vita_covers_request(flow->covers,&url,1);
		}
	}
	flow->preview_screen=view.screen;flow->preview_dragging=dragging;
	flow->dirty|=changed;return changed;
}

void plus_vita_ui_flow_start(PlusVitaUiFlow *flow,PlusVitaBranding *branding,uint64_t now_us,bool log_available)
{
	if(flow->startup_started) return;
	flow->startup_started=true;
	flow->foreground=true;flow->animation_tick_us=now_us;
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
	PlusVitaPresentation preferences=plus_vita_app_presentation(flow->app);
	if(preferences.reduced_motion) flow->transition_active=false;
	if(flow->foreground && !flow->splash_active && !preferences.reduced_motion
		&& now_us>=flow->animation_tick_us && now_us-flow->animation_tick_us>=50000u) {
		/* Long blocking dialogs/sleep never turn into an animation catch-up jump. */
		flow->animation_us+=50000u;flow->animation_tick_us=now_us;
		flow->dirty=true;changed=true;
	}
	if(flow->splash_active && now_us>=flow->splash_started_us
		&& now_us-flow->splash_started_us>=1000000u) { finish_splash(flow);changed=true; }
	if(flow->transition_active) {
		if(now_us<flow->transition_started_us) flow->transition_started_us=now_us;
		if(now_us-flow->transition_started_us>=MENU_TRANSITION_US) flow->transition_active=false;
		flow->dirty=true;changed=true;
	}
	return artwork_tick(flow) || changed;
}

void plus_vita_ui_flow_foreground(PlusVitaUiFlow *flow,bool foreground,uint64_t now_us)
{
	if(flow->foreground==foreground) return;
	flow->foreground=foreground;flow->animation_tick_us=now_us;
	flow->transition_active=false;flow->touch_captured=false;flow->touch_list=false;
	flow->touch_dragging=false;flow->dirty=true;
}

static bool flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us)
{
	flow->cancel_account=false;
	if(sampled_us>flow->now_us) flow->now_us=sampled_us;
	if(plus_vita_app_presentation(flow->app).reduced_motion) flow->transition_active=false;
	if(flow->transition_active && event.kind==PLUS_VITA_EVENT_PAD) {
		plus_vita_app_dispatch(flow->app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD_RESET,.pad=event.pad});
		return false;
	}
	PlusVitaAppView touch_view=plus_vita_app_view(flow->app);
	if(event.kind==PLUS_VITA_EVENT_TOUCH_DOWN) {
		if(flow->touch_held) { flow->touch_captured=false;flow->touch_list=false;flow->dirty=true;return false; }
		flow->touch_held=true;
		flow->touch_dragging=false;flow->touch_list=false;
		flow->touch_start_x=event.x;flow->touch_start_y=event.y;
		flow->touch_first=touch_view.list_first;
		if(flow->transition_active) return false;
		if(flow->splash_active) { finish_splash(flow);return true; }
		for(size_t i=0;i<touch_view.action_count;i++) if(inside(&touch_view.actions[i],event.x,event.y)) {
			flow->touch_captured=true;flow->touch_index=i;
			flow->touch_target=touch_view.actions[i];flow->touch_screen=touch_view.screen;
			flow->touch_operation=touch_view.operation;
			flow->touch_list=i<touch_view.list_count;flow->dirty=true;return true;
		}
		return false;
	}
	if(event.kind==PLUS_VITA_EVENT_TOUCH_MOVE && flow->touch_held && flow->touch_list) {
		if(touch_view.screen!=flow->touch_screen || touch_view.operation!=flow->touch_operation) {
			flow->touch_list=false;flow->touch_captured=false;return false;
		}
		int dy=flow->touch_start_y-event.y;
		if(abs(dy)>=TOUCH_DRAG_PIXELS || abs(event.x-flow->touch_start_x)>=TOUCH_DRAG_PIXELS)
			flow->touch_dragging=true;
		if(flow->touch_dragging) {
			flow->touch_captured=false;
			int rows=dy/(touch_view.row_step ? touch_view.row_step : 44);
			int desired=(int)flow->touch_first+rows;
			plus_vita_app_dispatch(flow->app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SCROLL,
				.y=desired-(int)touch_view.list_first});
			flow->dirty=true;return true;
		}
	}
	if(event.kind==PLUS_VITA_EVENT_TOUCH_MOVE || event.kind==PLUS_VITA_EVENT_TOUCH_CANCEL) {
		if(event.kind==PLUS_VITA_EVENT_TOUCH_CANCEL) flow->touch_list=false;
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
		flow->touch_held=false;flow->touch_captured=false;flow->touch_list=false;flow->touch_dragging=false;flow->dirty|=repaint;
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
	if(event.kind==PLUS_VITA_EVENT_INTERRUPTED || event.kind==PLUS_VITA_EVENT_PAD_RESET) {
		flow->touch_list=false;flow->touch_captured=false;flow->touch_dragging=false;
	}
	PlusVitaAppView before = plus_vita_app_view(flow->app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(flow->app, event);
	PlusVitaAppView after = plus_vita_app_view(flow->app);
	flow->cancel_account = before.operation && after.operation != before.operation;
	bool changed = effect.kind != PLUS_VITA_EFFECT_NONE || memcmp(&before, &after, sizeof(before)) != 0;
	if(flow->touch_captured && (changed || event.kind==PLUS_VITA_EVENT_INTERRUPTED)) {
		flow->touch_captured=false;flow->touch_list=false;flow->touch_dragging=false;flow->dirty=true;
	}
	if(before.screen!=after.screen && !flow->splash_active && !flow->splash_release
		&& after.screen!=PLUS_VITA_APP_CLOSED) {
		flow->touch_list=false;flow->touch_dragging=false;
		flow->transition_active=!plus_vita_app_presentation(flow->app).reduced_motion
			&& event.kind!=PLUS_VITA_EVENT_INTERRUPTED;
		flow->transition_started_us=flow->now_us;
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

bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow,PlusVitaAppEvent event,uint64_t sampled_us)
{
	PlusVitaPresentation before=plus_vita_app_presentation(flow->app);
	bool changed=flow_event(flow,event,sampled_us);
	PlusVitaPresentation after=plus_vita_app_presentation(flow->app);
	if(before.reduced_motion!=after.reduced_motion || before.muted!=after.muted || before.volume!=after.volume) {
		flow->dirty=true;changed=true;
		if(after.reduced_motion) flow->transition_active=false;
	}
	/* Sleep and context replacement cancel a settled interval even without a screen change. */
	if(event.kind==PLUS_VITA_EVENT_INTERRUPTED) {
		flow->transition_active=false;
		plus_vita_covers_request(flow->covers,NULL,0);flow->preview_valid=false;
	}
	return artwork_tick(flow) || changed;
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
	view.preview_eligible=flow->preview_eligible;
	view.touch_pressed=capture_current(flow,&view);
	view.pressed_index=flow->touch_index;
	unsigned opacity=255;
	if(flow->transition_active) {
		uint64_t elapsed=flow->now_us>=flow->transition_started_us ? flow->now_us-flow->transition_started_us : 0;
		opacity=elapsed>=MENU_TRANSITION_US ? 255 : (unsigned)(elapsed*255/MENU_TRANSITION_US);
	}
	PlusVitaPresentation preferences=plus_vita_app_presentation(flow->app);
	plus_vita_app_paint_frame(font,pixels,pitch,&view,flow->covers,flow->branding,
		flow->animation_us,flow->foreground && !preferences.reduced_motion,opacity);
	flow->dirty = false;
	return true;
}
