// SPDX-License-Identifier: AGPL-3.0-only
/* Real text/layout/font adapter; only external system-font calls are fixtures.
 * Face-wide metrics need not match the raster ink at a requested char size. */
#include "font_vita.h"
#include "ui_flow.h"
#include <psp2/pvf.h>
#include <debugScreen.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pvf_fixture.h"
static uint32_t pixels[960*544];
static uint32_t previous[960*544];
static PlusVitaAppEffect choose(PlusVitaApp *app,PlusVitaAppAction action)
{
 PlusVitaAppView view=plus_vita_app_view(app);
 for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==action)
  return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=view.actions[i].x+10,.y=view.actions[i].y+10});
 assert(!"Expected visible action"); return (PlusVitaAppEffect){0};
}
static void painted(PlusVitaApp *app,PlusVitaFont *font,PlusVitaAppScreen screen)
{
 PlusVitaAppView view=plus_vita_app_view(app);
 if(view.screen!=screen) fprintf(stderr,"Expected screen %u, got %u\n",(unsigned)screen,(unsigned)view.screen);
 assert(view.screen==screen);
 api_calls=0; plus_vita_app_paint(font,pixels,960,&view);
 assert(api_calls<4000);
 /* Every normal view retains header, small subtitle, body and footer ink. */
 unsigned title=0,subtitle=0,body=0,hint=0;
 for(unsigned y=94;y<537;y++) for(unsigned x=64;x<896;x++) {
  uint32_t ink=pixels[y*960+x];
  if(y<120 && ink==0xff5bc6f6) subtitle++;
  if(y>=128 && y<182 && ink==0xfffcf7f3) title++;
  if(y>=196 && y<280 && ink==0xfff7e0d2) body++;
  if(y>=509 && ink==0xffefd3c3) hint++;
 }
 assert(title && subtitle && body && hint);
 memcpy(previous,pixels,sizeof(pixels)); api_calls=0;
 plus_vita_app_paint(font,pixels,960,&view);
 /* Cross-screen cache pressure may evict a few glyphs; a warmed view must
  * remain identical and do bounded work, rather than rerasterize every row. */
 assert(!memcmp(previous,pixels,sizeof(pixels)) && api_calls<=64);
}
static void screen_journey(PlusVitaFont *font)
{
 PlusVitaApp *app=plus_vita_app_create(); assert(app);
 painted(app,font,PLUS_VITA_APP_WELCOME);
 choose(app,PLUS_VITA_ACTION_ABOUT); painted(app,font,PLUS_VITA_APP_ABOUT);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 PlusVitaAppEffect effect=choose(app,PLUS_VITA_ACTION_SETUP);
 painted(app,font,PLUS_VITA_APP_PREPARING);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SETUP_READY,.operation=effect.operation});
 painted(app,font,PLUS_VITA_APP_SETUP_GUIDE);
 choose(app,PLUS_VITA_ACTION_CONTINUE); painted(app,font,PLUS_VITA_APP_SETUP);
 effect=choose(app,PLUS_VITA_ACTION_IMPORT); painted(app,font,PLUS_VITA_APP_AUTHORIZING);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_IMPORT_READ,.operation=effect.operation,.status=PLUS_VITA_RESULT_MISSING});
 painted(app,font,PLUS_VITA_APP_ERROR);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 plus_vita_app_destroy(app);

 app=plus_vita_app_create(); effect=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
 const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
 effect=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,.operation=effect.operation,.data=json,.data_size=sizeof(json)-1,.retained_json=true});
 PlusVitaCatalogEntry entries[]={{.name="Agjpqy é Å 日本",.id="FAKECUSA1"},{.name="Second title",.id="FAKECUSA2"}};
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=effect.operation,.entries=entries,.entry_count=2});
 painted(app,font,PLUS_VITA_APP_CATALOGUE);
 choose(app,PLUS_VITA_ACTION_GAME); painted(app,font,PLUS_VITA_APP_DETAILS);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 effect=choose(app,PLUS_VITA_ACTION_SEARCH); painted(app,font,PLUS_VITA_APP_AUTHORIZING);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=effect.operation,.status=PLUS_VITA_RESULT_CANCELLED});
 painted(app,font,PLUS_VITA_APP_CATALOGUE);
 effect=choose(app,PLUS_VITA_ACTION_SEARCH);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH_ENTERED,.operation=effect.operation,.data="Agjpqy",.data_size=6});
 painted(app,font,PLUS_VITA_APP_CATALOGUE);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 choose(app,PLUS_VITA_ACTION_ACCOUNT); painted(app,font,PLUS_VITA_APP_ACCOUNT);
 effect=choose(app,PLUS_VITA_ACTION_LABEL);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_LABEL_ENTERED,.operation=effect.operation,.status=PLUS_VITA_RESULT_CANCELLED});
 painted(app,font,PLUS_VITA_APP_ACCOUNT);
 effect=choose(app,PLUS_VITA_ACTION_LABEL);
 effect=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_LABEL_ENTERED,.operation=effect.operation,.data="Müller 日本",.data_size=sizeof("Müller 日本")-1});
 if(effect.kind==PLUS_VITA_EFFECT_SAVE_ACCOUNT)
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_SAVED,.operation=effect.operation});
 painted(app,font,PLUS_VITA_APP_ACCOUNT);
 choose(app,PLUS_VITA_ACTION_FORGET); painted(app,font,PLUS_VITA_APP_FORGET_CONFIRM);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 choose(app,PLUS_VITA_ACTION_EXIT);
 painted(app,font,PLUS_VITA_APP_EXIT_CONFIRM);
 plus_vita_app_destroy(app);
}
static void input_journey(PlusVitaFont *font)
{
 PlusVitaApp *app=plus_vita_app_create(); PlusVitaUiFlow flow={.app=app,.dirty=true};
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 PlusVitaAppEvent pad={.kind=PLUS_VITA_EVENT_PAD,.pad={.left_x=128,.left_y=255,.time_us=1000}};
 assert(plus_vita_ui_flow_event(&flow,pad,1000));
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[400*960+70]==0xff5bc6f6);
 pad.pad.time_us=10000; assert(!plus_vita_ui_flow_event(&flow,pad,10000));
 assert(!plus_vita_ui_flow_paint(&flow,font,pixels,960));
 pad.pad.time_us=401000; assert(plus_vita_ui_flow_event(&flow,pad,401000));
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(pixels[464*960+70]==0xff5bc6f6);
 pad.pad.left_y=128; pad.pad.time_us=402000;
 assert(!plus_vita_ui_flow_event(&flow,pad,402000));
 assert(!plus_vita_ui_flow_paint(&flow,font,pixels,960));
 pad.pad.left_y=0; pad.pad.time_us=403000;
 assert(plus_vita_ui_flow_event(&flow,pad,403000));
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 pad.pad.left_y=128; pad.pad.buttons=PLUS_VITA_MENU_CROSS;
 assert(plus_vita_ui_flow_event(&flow,pad,11000));
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 assert(!plus_vita_ui_flow_event(&flow,pad,12000));
 pad.pad.buttons=PLUS_VITA_MENU_CIRCLE;
 assert(plus_vita_ui_flow_event(&flow,pad,13000));
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 /* Touch starts pending setup. Neutral input cannot lose that effect. */
 assert(plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=330},14000));
 pad.pad.buttons=0; plus_vita_ui_flow_event(&flow,pad,15000);
 PlusVitaAppEffect prepare=plus_vita_ui_flow_take(&flow);
 assert(prepare.kind==PLUS_VITA_EFFECT_PREPARE_SETUP);
 assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_NONE);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SETUP_READY,.operation=prepare.operation},0);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=400},16000);
 plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=330},17000);
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_AUTHORIZING);
 pad.pad.buttons=PLUS_VITA_MENU_CIRCLE;
 plus_vita_ui_flow_event(&flow,pad,18000); assert(flow.cancel_account);
 assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_NONE);
 assert(plus_vita_ui_flow_paint(&flow,font,pixels,960));
 plus_vita_app_destroy(app);
}
int main(void) {
 PlusVitaFont *font=plus_vita_font_create(); assert(font);
 assert(plus_vita_font_draw(font,pixels,960,"T",(PlusVitaTextBox){64,128,832,54},36,false,0xffffffff));
 unsigned ink=0; for(unsigned i=0;i<960*544;i++) if(pixels[i]) ink++;
 printf("complete glyph ink: %u of 360 pixels\n",ink); fflush(stdout);
 assert(ink==360); /* Independent full rectangle from external raster, no clipped top. */
 /* Native roles selected on Vita: independent complete external glyph ink,
  * including negative bearing, accents and supported Japanese. */
 const unsigned sizes[]={14,16,18,20,22,24,36}, heights[]={24,26,32,28,32,38,54};
 const char *glyphs[]={"T","j","é","Å","日"};
 for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++)
  for(unsigned g=0;g<sizeof(glyphs)/sizeof(glyphs[0]);g++) {
   memset(pixels,0,sizeof(pixels));
   assert(plus_vita_font_draw(font,pixels,960,glyphs[g],(PlusVitaTextBox){64,128,832,(int)heights[s]},sizes[s],false,0xffffffff));
   ink=0; for(unsigned i=0;i<960*544;i++) if(pixels[i]) ink++;
   assert(ink==10*sizes[s]);
  }
 char title[97]; for(unsigned i=0;i<96;i++) title[i]='A'+i%26; title[96]=0;
 api_calls=0;
 plus_vita_font_draw(font,pixels,960,title,(PlusVitaTextBox){64,326,792,36},22,false,0xffffffff);
 unsigned cold=api_calls; api_calls=0;
 plus_vita_font_draw(font,pixels,960,title,(PlusVitaTextBox){64,326,792,36},22,false,0xffffffff);
 printf("font work for one 96-character row: cold=%u warm=%u system calls\n",cold,api_calls); fflush(stdout);
 assert(cold <= 400 && api_calls <= 4);
 PlusVitaApp *app=plus_vita_app_create(); assert(app);
 PlusVitaAppView view=plus_vita_app_view(app);
 plus_vita_app_paint(font,pixels,960,&view);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN});
 view=plus_vita_app_view(app); assert(view.selected==1);
 api_calls=0;
 plus_vita_app_paint(font,pixels,960,&view);
 printf("warm welcome focus change: %u system-font calls\n",api_calls); fflush(stdout);
 assert(api_calls==0);
 assert(pixels[400*960+70]==0xff5bc6f6 && pixels[336*960+70]==0xff7d3911);
 input_journey(font);
 screen_journey(font);
 /* The same production painter handles catalogue focus and non-ASCII ink. */
 plus_vita_app_destroy(app); app=plus_vita_app_create();
 PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
 const char source[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
 PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,.operation=load.operation,.data=source,.data_size=sizeof(source)-1,.retained_json=true});
 PlusVitaCatalogEntry titles[]={{.name="jé 日本",.id="a"},{.name="Second game",.id="b"}};
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,.entries=titles,.entry_count=2});
 view=plus_vita_app_view(app); assert(view.screen==PLUS_VITA_APP_CATALOGUE);
 plus_vita_app_paint(font,pixels,960,&view);
 for(unsigned step=0;step<20;step++) {
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_DOWN});
  view=plus_vita_app_view(app); api_calls=0;
  plus_vita_app_paint(font,pixels,960,&view); assert(api_calls==0);
  unsigned y=view.actions[view.selected].y+20;
  assert(pixels[y*960+view.actions[view.selected].x+6]==0xff5bc6f6);
 }
 plus_vita_app_destroy(app);
 /* Working sets larger than both bounded caches remain safe and reusable. */
 glyph_width=120;
 for(unsigned i=0;i<700;i++) {
  unsigned code=0x400+i; char utf8[]={0xc0|(code>>6),0x80|(code&63),0};
  memset(pixels,0,sizeof(pixels));
  assert(plus_vita_font_draw(font,pixels,960,utf8,(PlusVitaTextBox){64,128,832,54},36,false,0xffffffff));
  ink=0; for(unsigned p=0;p<960*544;p++) if(pixels[p]) ink++;
  assert(ink==120*36);
 }
 plus_vita_font_free(font);
 glyph_width=10; memset(bitmap,255,sizeof(bitmap));
 for(unsigned failure=0;failure<3;failure++) {
  unavailable=failure==0; bad_rect=failure==1; bad_raster=failure==2;
  font=plus_vita_font_create(); assert(font); memset(pixels,0,sizeof(pixels));
  assert(plus_vita_font_draw(font,pixels,960,"T",(PlusVitaTextBox){64,128,832,54},36,false,0xffffffff));
  ink=0;
  for(unsigned y=0;y<544;y++) for(unsigned x=0;x<960;x++) if(pixels[y*960+x]) {
   assert(x>=64 && x<896 && y>=128 && y<182); ink++;
  }
  assert(ink==(failure==2 ? 88u : 768u)); /* Outline perimeter or 24x32 SDK bitmap. */
  plus_vita_font_free(font);
 }
 puts("font adapter: all A roles retain complete ink; app screens, keyboard return, warm navigation and fallback passed");
}
