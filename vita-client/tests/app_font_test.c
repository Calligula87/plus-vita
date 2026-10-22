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
static int pixelsize=20;
static unsigned api_calls;
static int glyph_width=10;
static bool unavailable, bad_rect, bad_raster;
static uint32_t pixels[960*544];
static unsigned char bitmap[128*8];
int sceSysmoduleLoadModule(unsigned x) { (void)x; return unavailable ? -1 : 0; }
int sceSysmoduleIsLoaded(unsigned x) { (void)x; return unavailable ? -1 : 0; }
int sceSysmoduleUnloadModule(unsigned x) { (void)x; return 0; }
ScePvfLibId scePvfNewLib(ScePvfInitRec *p, ScePvfError *e) { (void)p; *e=0; return (void *)1; }
int scePvfDoneLib(ScePvfLibId f) { (void)f; return 0; }
int scePvfSetEM(ScePvfLibId f,float x) { (void)f;(void)x; return 0; }
int scePvfSetResolution(ScePvfLibId f,float x,float y) { (void)f;(void)x;(void)y; return 0; }
int scePvfFindOptimumFont(ScePvfLibId f,ScePvfFontStyleInfo *s,ScePvfError *e) { (void)f;(void)s; *e=0; return 0; }
ScePvfFontId scePvfOpen(ScePvfLibId f,int i,unsigned m,ScePvfError *e) { (void)i;(void)m; *e=0; return f; }
int scePvfClose(ScePvfFontId f) { (void)f;return 0; }
int scePvfSetCharSize(ScePvfFontId f,float x,float y) { (void)f;(void)y; pixelsize=(int)(x*128/72+0.5f); api_calls++; return 0; }
int scePvfIsElement(ScePvfFontId f,ScePvfCharCode c) { (void)f;(void)c; api_calls++; return 1; }
int scePvfGetFontInfo(ScePvfFontId f,ScePvfFontInfo *i) { (void)f; memset(i,0,sizeof(*i)); i->maxIGlyphMetrics.ascender64=10*64; i->maxIGlyphMetrics.descender64=-2*64; api_calls++; return 0; }
int scePvfGetCharInfo(ScePvfFontId f,ScePvfCharCode c,ScePvfCharInfo *i) { (void)f; memset(i,0,sizeof(*i)); i->glyphMetrics.horizontalAdvance64=(glyph_width+2)*64; i->glyphMetrics.horizontalBearingY64=(pixelsize-2)*64; i->glyphMetrics.horizontalBearingX64=c=='j' ? -2*64 : 0; api_calls++; return 0; }
int scePvfGetCharImageRect(ScePvfFontId f,ScePvfCharCode c,ScePvfIrect *r) { (void)f;(void)c; *r=(ScePvfIrect){glyph_width,pixelsize}; api_calls++; return bad_rect ? -1 : 0; }
int scePvfGetCharGlyphImage(ScePvfFontId f,ScePvfCharCode c,ScePvfUserImageBufferRec *r) {
 (void)f; api_calls++;
 if(bad_raster) return -1;
 /* External raster contract: x/y are the pen baseline in 26.6 pixels. */
 int left=r->xPos64/64+(c=='j' ? -2 : 0), top=(r->yPos64-(pixelsize-2)*64)/64;
 for(int y=0;y<pixelsize;y++) for(int x=0;x<glyph_width;x++)
  if(left+x>=0 && left+x<(int)r->rect.width && top+y>=0 && top+y<(int)r->rect.height)
   r->buffer[(top+y)*r->bytesPerLine+left+x]=255;
 return 0;
}
PsvDebugScreenFont *psvDebugScreenGetFont(void) { static PsvDebugScreenFont f={bitmap};return &f; }

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
 puts("font adapter: complete requested-size raster ink passed");
}
