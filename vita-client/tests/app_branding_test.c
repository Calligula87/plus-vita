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
	image_fixture("logo.pvi",1,1,0xff000000);
	image_fixture("splash.pvi",1,1,0xff000000);
	PlusVitaBranding *branding=plus_vita_branding_load(".");
	PlusVitaApp *app=plus_vita_app_create();PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_start(&flow,branding,100,false);
	assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_LOAD_ACCOUNT);
	assert(plus_vita_ui_flow_paint(&flow,font,pixels,1024));
	plus_vita_branding_free(branding);plus_vita_app_destroy(app);
	assert(remove("logo.pvi")==0 && remove("splash.pvi")==0);
}
int main(void)
{
	memset(bitmap,255,sizeof(bitmap));
	image_fixture("logo.pvi",192,192,0xff443322);
	PlusVitaBranding *branding=plus_vita_branding_load("."); assert(branding);
	PlusVitaApp *app=plus_vita_app_create(); assert(app);
	PlusVitaFont *font=plus_vita_font_create(); assert(font);
	PlusVitaAppView view=plus_vita_app_view(app);
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[238*1024+674]==0xff443322 && pixels[429*1024+865]==0xff443322);
	assert(remove("logo.pvi")==0);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN});
	view=plus_vita_app_view(app);
	plus_vita_app_paint_with_resources(font,pixels,1024,&view,NULL,branding);
	assert(pixels[300*1024+700]==0xff443322);
	assert(pixels[(view.actions[view.selected].y+20)*1024+70]==0xff5bc6f6);
	splash_journey(font);
	splash_dismissal(font);
	plus_vita_branding_free(branding);plus_vita_app_destroy(app);plus_vita_font_free(font);
	puts("branding journey: supplied logo replaces placeholder and preserves painted focus without reloading");
}
