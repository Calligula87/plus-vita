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
	assert(!plus_vita_ui_flow_tick(&flow,3000000));
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
	assert(flow_action(&flow,PLUS_VITA_ACTION_BIND_NEXT,&clock));
	assert(!strcmp(plus_vita_app_view(app).actions[0].label,"Triangle: Triangle"));
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(flow_action(&flow,PLUS_VITA_ACTION_BIND_PREVIOUS,&clock));
	assert(!strcmp(plus_vita_app_view(app).actions[0].label,"Cross: Cross"));
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
	uint32_t initial=pixels[0];
	assert(initial!=0xff5bc6f6); /* Transition is visible, not an instant replacement. */
	plus_vita_ui_flow_tick(&flow,60200);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	assert(pixels[0]!=initial && pixels[0]!=0xff5bc6f6);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},60300);
	plus_vita_ui_flow_tick(&flow,120200);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024) && pixels[0]==0xff5bc6f6);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},120300);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	/* Leaving and returning to the original target keeps the gesture cancelled. */
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},120400);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=2,.y=2},120500);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_MOVE,.x=80,.y=460},120600);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},120700);
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_DOWN,.x=80,.y=460},120800);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_CANCEL},120900);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH_UP,.x=80,.y=460},121000);
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
	touch_release_journey(font);
	cancelled_touch_journey();
	bindings_page_journey(font);
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
