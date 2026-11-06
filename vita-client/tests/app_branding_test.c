// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_flow.h"
#include "branding.h"
#include <psp2/pvf.h>
#include <debugScreen.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pvf_fixture.h"
static uint32_t pixels[1024*544];

static void activate_visible(PlusVitaApp *app,PlusVitaAppAction action)
{
	PlusVitaAppView view=plus_vita_app_view(app);
	for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==action) {
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=view.actions[i].x+10,.y=view.actions[i].y+10});return;
	}
	assert(!"expected visible action");
}

static void appearance_journey(void)
{
	PlusVitaApp *app=plus_vita_app_create();
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN});
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE});
	PlusVitaAppView view=plus_vita_app_view(app);
	bool found=false;
	for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==PLUS_VITA_ACTION_APPEARANCE) {
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=view.actions[i].x+10,.y=view.actions[i].y+10});found=true;break;
	}
	assert(found && plus_vita_app_view(app).screen==PLUS_VITA_APP_APPEARANCE);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE});
	assert(plus_vita_app_presentation_pending(app));
	PlusVitaPresentation prefs=plus_vita_app_presentation(app);
	assert(prefs.reduced_motion);
	assert(plus_vita_presentation_save("appearance-test.dat",&prefs));
	PlusVitaPresentation loaded;
	assert(plus_vita_presentation_load("appearance-test.dat",&loaded));
	assert(loaded.reduced_motion && loaded.volume==20 && !loaded.muted);
	activate_visible(app,PLUS_VITA_ACTION_MUSIC_MUTE);
	activate_visible(app,PLUS_VITA_ACTION_MUSIC_LOUDER);
	prefs=plus_vita_app_presentation(app);assert(prefs.muted && prefs.volume==30);
	assert(plus_vita_presentation_save("appearance-test.dat",&prefs));
	PlusVitaApp *reopened=plus_vita_app_create();
	assert(plus_vita_presentation_load("appearance-test.dat",&loaded));
	plus_vita_app_dispatch(reopened,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_LOADED,.presentation=loaded});
	prefs=plus_vita_app_presentation(reopened);assert(prefs.reduced_motion && prefs.muted && prefs.volume==30);
	plus_vita_app_destroy(reopened);
	FILE *file=fopen("appearance-test.dat","ab");assert(file && fputc(0,file)!=EOF && !fclose(file));
	assert(!plus_vita_presentation_load("appearance-test.dat",&loaded));
	assert(!loaded.reduced_motion && !loaded.muted && loaded.volume==20);
	file=fopen("appearance-test.dat","wb");assert(file && fwrite("bad",1,3,file)==3 && !fclose(file));
	assert(!plus_vita_presentation_load("appearance-test.dat",&loaded));
	assert(!plus_vita_presentation_save("missing-directory/style.dat",&prefs));
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_SAVED,.status=PLUS_VITA_RESULT_STORAGE_ERROR});
	assert(strstr(plus_vita_app_view(app).body,"Could not save") && !plus_vita_app_presentation_pending(app));
	activate_visible(app,PLUS_VITA_ACTION_MUSIC_MUTE);
	assert(plus_vita_app_presentation_pending(app));
	assert(remove("appearance-test.dat")==0);
	plus_vita_app_destroy(app);
}

static void living_background_journey(PlusVitaFont *font,PlusVitaBranding *branding)
{
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_start(&flow,branding,100,true);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	uint32_t before[960];memcpy(before,pixels+350*1024,sizeof(before));
	plus_vita_ui_flow_tick(&flow,500100);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(memcmp(before,pixels+350*1024,sizeof(before)));
	assert(pixels[20*1024+64]==0xff0099ff);
	PlusVitaPresentation prefs=plus_vita_app_presentation(app);prefs.reduced_motion=true;
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_LOADED,.presentation=prefs},0);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	memcpy(before,pixels+350*1024,sizeof(before));
	plus_vita_ui_flow_tick(&flow,1000100);
	flow.dirty=true;assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(!memcmp(before,pixels+350*1024,sizeof(before)));
	/* Reduced motion removes the transition input wait as well as the effect. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK},1000101);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN},1000102);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE},1000103);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_SETTINGS);
	plus_vita_ui_flow_foreground(&flow,false,1000104);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	plus_vita_ui_flow_tick(&flow,9000104);
	assert(!plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	/* Re-enabling motion after a saved reduced-motion choice restores both effects. */
	prefs.reduced_motion=false;
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PRESENTATION_LOADED,.presentation=prefs},0);
	plus_vita_ui_flow_foreground(&flow,true,9000105);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	memcpy(before,pixels+350*1024,sizeof(before));
	plus_vita_ui_flow_tick(&flow,9050105);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(memcmp(before,pixels+350*1024,sizeof(before)));
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK},9050106);
	assert(flow.transition_active);
	plus_vita_ui_flow_tick(&flow,9200106);
	assert(!flow.transition_active);
	plus_vita_app_destroy(app);
}

static void image_fixture(const char *name,unsigned width,unsigned height,uint32_t color)
{
	FILE *file=fopen(name,"wb"); assert(file);
	unsigned char header[16]="PVIMAGE1";
	for(unsigned i=0;i<4;i++) { header[8+i]=(width>>(i*8))&255; header[12+i]=(height>>(i*8))&255; }
	assert(fwrite(header,1,16,file)==16);
	for(size_t i=0;i<(size_t)width*height;i++) assert(fwrite(&color,sizeof(color),1,file)==1);
	assert(!fclose(file));
}
static void splash_journey(PlusVitaFont *font)
{
	image_fixture("splash.pvi",960,544,0xffaa7733);
	PlusVitaBranding *branding=plus_vita_branding_load("."); assert(branding);
	PlusVitaApp *app=plus_vita_app_create(); assert(app);
	PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_start(&flow,branding,100,true);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[0]==0xffaa7733 && pixels[543*1024+959]==0xffaa7733);
	assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_NONE);
	assert(!plus_vita_ui_flow_tick(&flow,1000099));
	assert(plus_vita_ui_flow_tick(&flow,1000100));
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[0]!=0xffaa7733);
	PlusVitaAppEffect load=plus_vita_ui_flow_take(&flow);
	assert(load.kind==PLUS_VITA_EFFECT_LOAD_ACCOUNT);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
		.operation=load.operation,.status=PLUS_VITA_RESULT_MISSING,.restore_blocked=true},0);
	plus_vita_ui_flow_start(&flow,branding,2000000,true);
	assert(plus_vita_ui_flow_tick(&flow,3000000)); /* Live menus repaint, splash does not restart. */
	plus_vita_branding_free(branding);plus_vita_app_destroy(app);assert(remove("splash.pvi")==0);
}
static void splash_dismissal(PlusVitaFont *font)
{
	for(unsigned touch=0;touch<2;touch++) {
		image_fixture("splash.pvi",960,544,0xffaa7733);
		PlusVitaBranding *branding=plus_vita_branding_load(".");
		PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};
		plus_vita_ui_flow_start(&flow,branding,100,true);
		PlusVitaAppPad held={.buttons=PLUS_VITA_MENU_CROSS,.left_x=128,.left_y=128,.time_us=200};
		assert(plus_vita_ui_flow_event(&flow,touch ? (PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=200,.y=320}
			: (PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=held},200));
		assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024) && pixels[0]!=0xffaa7733);
		PlusVitaAppEffect load=plus_vita_ui_flow_take(&flow);assert(load.kind==PLUS_VITA_EFFECT_LOAD_ACCOUNT);
		plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
			.operation=load.operation,.status=PLUS_VITA_RESULT_MISSING,.restore_blocked=true},0);
		assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_WELCOME);
		plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=held},300);
		assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_NONE);
		held.buttons=0;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=held},400);
		held.buttons=PLUS_VITA_MENU_CROSS;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=held},500);
		assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_PREPARE_SETUP);
		plus_vita_branding_free(branding);plus_vita_app_destroy(app);assert(remove("splash.pvi")==0);
	}
	/* Bad/truncated package artwork must still permit normal startup. */
	image_fixture("menu.pvi",1,1,0xff000000);
	image_fixture("wordmark.pvi",1,1,0xff000000);
	image_fixture("splash.pvi",1,1,0xff000000);
	PlusVitaBranding *branding=plus_vita_branding_load(".");
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_start(&flow,branding,100,false);
	assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_LOAD_ACCOUNT);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	plus_vita_branding_free(branding);plus_vita_app_destroy(app);
	assert(remove("menu.pvi")==0 && remove("splash.pvi")==0 && remove("wordmark.pvi")==0);
}
static void cancelled_touch_journey(void)
{
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=2,.y=2},100);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=80,.y=420},200);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=420},300);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_WELCOME);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=420},310);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,.status=PLUS_VITA_RESULT_SLEEP},320);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=420},330);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_WELCOME);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=420},400);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START},500);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},600);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_AUTHORIZING);
	assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_LOAD_ACCOUNT);
	plus_vita_app_destroy(app);
}

static bool flow_action(PlusVitaUiFlow *flow,PlusVitaAppAction action,uint64_t *clock)
{
	*clock+=150000;plus_vita_ui_flow_tick(flow,*clock);
	PlusVitaAppView view=plus_vita_app_view(flow->app);
	for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==action) {
		plus_vita_ui_flow_event(flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=view.actions[i].x+10,.y=view.actions[i].y+10},*clock);
		return plus_vita_ui_flow_event(flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=view.actions[i].x+10,.y=view.actions[i].y+10},++*clock);
	}
	assert(!"Expected visible flow action");return false;
}

static void bindings_page_journey(PlusVitaFont *font)
{
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};uint64_t clock=100;
	assert(flow_action(&flow,PLUS_VITA_ACTION_SETTINGS,&clock));
	assert(flow_action(&flow,PLUS_VITA_ACTION_CONTROL_LAYOUT,&clock));
	assert(flow_action(&flow,PLUS_VITA_ACTION_BINDINGS,&clock));
	PlusVitaAppView view=plus_vita_app_view(app);
	assert(view.list_total==PLUS_VITA_INPUT_COUNT && view.list_first==0);
	assert(view.list_count==5 && view.focus==0);
	for(unsigned i=0;i<100;i++) {
		plus_vita_ui_flow_tick(&flow,clock+=150000);
		plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN},clock);
	}
	view=plus_vita_app_view(app);
	assert(view.focus==PLUS_VITA_INPUT_COUNT-1 && view.list_first+view.list_count==view.list_total);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[(view.list_y+view.list_height-2)*1024+view.list_x+view.list_width+9]==0xff5bc6f6);
	plus_vita_ui_flow_tick(&flow,clock+=150000);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=280},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=80,.y=430},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=430},++clock);
	view=plus_vita_app_view(app);
	assert(view.screen==PLUS_VITA_APP_BINDINGS && view.list_first+view.list_count<view.list_total);
	assert(view.focus>=view.list_first && view.focus<view.list_first+view.list_count);
	size_t retained_first=view.list_first, retained_focus=view.focus;
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE},++clock);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_BIND_ACTIONS);
	plus_vita_ui_flow_tick(&flow,clock+=150000);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK},++clock);
	view=plus_vita_app_view(app);
	assert(view.screen==PLUS_VITA_APP_BINDINGS && view.focus==retained_focus && view.list_first==retained_first);
	plus_vita_app_destroy(app);
}

static void ordinary_menu_journey(PlusVitaFont *font)
{
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};uint64_t clock=100;
	flow_action(&flow,PLUS_VITA_ACTION_SETTINGS,&clock);
	plus_vita_ui_flow_tick(&flow,clock+=150000);
	PlusVitaAppView view=plus_vita_app_view(app);
	assert(view.list_total==6 && view.list_count==5 && view.list_first==0);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_UP},++clock);
	assert(plus_vita_app_view(app).focus==0);
	/* Combined D-pad/stick samples make one decision at the accepted repeat cadence. */
	PlusVitaAppPad pad={.buttons=PLUS_VITA_MENU_DOWN,.left_x=128,.left_y=255,.time_us=clock};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=pad},clock);
	assert(plus_vita_app_view(app).focus==1);
	pad.time_us=clock+399999;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=pad},pad.time_us);
	assert(plus_vita_app_view(app).focus==1);
	pad.time_us=clock+400000;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=pad},pad.time_us);
	assert(plus_vita_app_view(app).focus==2);
	pad.time_us+=159999;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=pad},pad.time_us);
	assert(plus_vita_app_view(app).focus==2);
	pad.time_us++;plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PAD,.pad=pad},pad.time_us);
	assert(plus_vita_app_view(app).focus==3);
	for(unsigned i=0;i<20;i++) plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN},++pad.time_us);
	view=plus_vita_app_view(app);assert(view.focus==5);
	assert(view.actions[view.selected].id==PLUS_VITA_ACTION_BACK);
	assert(view.actions[view.selected].y+view.actions[view.selected].height<=502);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[(view.list_y+10)*1024+905]!=0xff5bc6f6);
	clock=pad.time_us+1;
	PlusVitaAppButton row=view.actions[0];
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=row.x+30,.y=row.y+10},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=row.x+60,.y=row.y+10},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=row.x+60,.y=row.y+10},++clock);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_SETTINGS);
	/* A second contact cancels the first, including a release over its target. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=row.x+30,.y=row.y+10},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=row.x+30,.y=row.y+10},++clock);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=row.x+30,.y=row.y+10},++clock);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_SETTINGS);
	plus_vita_app_destroy(app);
}

static void touch_release_journey(PlusVitaFont *font)
{
	PlusVitaApp *app=plus_vita_app_create();
	PlusVitaUiFlow flow={.app=app,.dirty=true};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=420},100);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_WELCOME);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[410*1024+70]==0xfff4b44d); /* Visible pressed state, before release. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=420},200);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	/* The same release cannot activate the destination screen. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},300);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[0]==0xff5bc6f6); /* Persistent header never fades. */
	uint32_t initial=pixels[450*1024+70];
	assert(initial!=0xff5bc6f6); /* Transition is visible, not an instant replacement. */
	plus_vita_ui_flow_tick(&flow,60200);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[0]==0xff5bc6f6);
	assert(pixels[450*1024+70]!=initial && pixels[450*1024+70]!=0xff5bc6f6);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},60300);
	plus_vita_ui_flow_tick(&flow,150200);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024) && pixels[450*1024+70]==0xff5bc6f6);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},150300);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	/* Leaving and returning to the original target keeps the gesture cancelled. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},150400);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=2,.y=2},150500);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=80,.y=460},150600);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},150700);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},150800);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_CANCEL},150900);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},151000);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},121100);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},121200);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_WELCOME);
	plus_vita_app_destroy(app);
}

int main(void)
{
	memset(bitmap,255,sizeof(bitmap));
	image_fixture("menu.pvi",960,544,0xff443322);
	image_fixture("wordmark.pvi",224,40,0xff0099ff);
	FILE *wordmark=fopen("wordmark.pvi","r+b");assert(wordmark);
	uint32_t edge[]={0x8099bbdd,0};
	assert(!fseek(wordmark,20,SEEK_SET) && fwrite(edge,sizeof(*edge),2,wordmark)==2 && !fclose(wordmark));
	/* An obsolete logo resource must never appear in the new menu painter. */
	image_fixture("logo.pvi",192,192,0xffccbb99);
	PlusVitaBranding *branding=plus_vita_branding_load("."); assert(branding);
	PlusVitaApp *app=plus_vita_app_create(); assert(app);
	PlusVitaFont *font=plus_vita_font_create(); assert(font);
	living_background_journey(font,branding);
	touch_release_journey(font);
	appearance_journey();
	cancelled_touch_journey();
	bindings_page_journey(font);
	ordinary_menu_journey(font);
	PlusVitaAppView view=plus_vita_app_view(app);
	assert(!strcmp(view.title,"Plus-Vita"));
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[20*1024+64]==0xff0099ff && pixels[20*1024+65]==0xff6f7780 && pixels[20*1024+66]==0xff443322);
	assert(pixels[238*1024+674]==0xff443322 && pixels[429*1024+865]==0xff443322);
	assert(pixels[500*1024+959]==0xff443322);
	assert(remove("menu.pvi")==0 && remove("logo.pvi")==0 && remove("wordmark.pvi")==0);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN});
	view=plus_vita_app_view(app);
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[300*1024+700]==0xff443322);
	assert(pixels[(view.actions[view.selected].y+20)*1024+70]==0xff5bc6f6);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE});
	view=plus_vita_app_view(app);assert(view.screen==PLUS_VITA_APP_SETTINGS);
	assert(strstr(view.body,"v000") && !strstr(view.body,"beta"));
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[20*1024+64]==0xff0099ff);
	assert(pixels[500*1024+959]==0xff443322);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=420});
	view=plus_vita_app_view(app);assert(view.screen==PLUS_VITA_APP_ABOUT);
	assert(strstr(view.body,"Use your own Sony account") && strstr(view.body,"v000"));
	assert(!strstr(view.body,"independent") && !strstr(view.body,"Final Vita") && !strstr(view.body,"beta"));
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[20*1024+64]==0xff0099ff);
	splash_journey(font);
	splash_dismissal(font);
	plus_vita_branding_free(branding);plus_vita_app_destroy(app);plus_vita_font_free(font);
	puts("branding journey: full-screen menu background, no logo, focus, cached resources and splash passed");
}
