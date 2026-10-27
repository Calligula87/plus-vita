// SPDX-License-Identifier: AGPL-3.0-only
#include "font_freetype.h"
#include "font_vita.h"
#include "app.h"
#include "render.h"
#include "comparison_capture.h"
#include <psp2/pvf.h>
#include <debugScreen.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pvf_fixture.h"
static uint32_t pixels[960*544], previous[960*544];
static bool button_has_label(const PlusVitaAppButton *button, uint32_t color)
{
 for(int y=button->y;y<button->y+button->height;y++)
  for(int x=button->x;x<button->x+button->width;x++)
   if(pixels[y*960+x]==color) return true;
 return false;
}
static void assert_button_labels(const PlusVitaAppView *view)
{
 for(size_t i=0;i<view->action_count;i++)
  assert(button_has_label(&view->actions[i],i==view->selected ? 0xff421d09u : 0xfffcf7f3u));
}
int main(void)
{
 const char *path="vita-client/assets/fonts/NotoSans-Regular.ttf";
 PlusVitaFtFont *font=plus_vita_freetype_create(path,(PlusVitaTextFont){0}); assert(font);
 FT_Library reference; FT_Face face; assert(!FT_Init_FreeType(&reference));
 assert(!FT_New_Face(reference,path,0,&face));
 const unsigned sizes[]={14,16,18,20,22,24,36};
 const FT_ULong codes[]={'T','g','j',0xe9,0xc5};
 for(unsigned s=0;s<sizeof(sizes)/sizeof(sizes[0]);s++) {
  unsigned size=sizes[s]; unsigned long expected=0,actual=0;
  assert(!FT_Set_Pixel_Sizes(face,0,size));
  for(unsigned c=0;c<sizeof(codes)/sizeof(codes[0]);c++) {
   assert(!FT_Load_Char(face,codes[c],FT_LOAD_RENDER|FT_LOAD_TARGET_NORMAL));
   FT_Bitmap *image=&face->glyph->bitmap;
   assert(image->pixel_mode==FT_PIXEL_MODE_GRAY);
   for(unsigned y=0;y<image->rows;y++) for(unsigned x=0;x<image->width;x++) expected+=image->buffer[y*image->pitch+x];
  }
  memset(pixels,0,sizeof(pixels));
  assert(plus_vita_freetype_draw(font,pixels,960,"T g j é Å",(PlusVitaTextBox){20,20,800,90},size,false,0xffffffff));
  for(unsigned y=0;y<544;y++) for(unsigned x=0;x<960;x++) if(pixels[y*960+x]) {
   assert(x>=20 && x<820 && y>=20 && y<110); actual+=pixels[y*960+x]&255;
  }
  assert(expected>0 && actual==expected); /* Independent native raster retains every alpha sample. */
  memcpy(previous,pixels,sizeof(pixels)); memset(pixels,0,sizeof(pixels));
  assert(plus_vita_freetype_draw(font,pixels,960,"T g j é Å",(PlusVitaTextBox){20,20,800,90},size,false,0xffffffff));
  assert(!memcmp(previous,pixels,sizeof(pixels)));
 }
 memset(pixels,0,sizeof(pixels));
 assert(!plus_vita_freetype_draw(font,pixels,960,"Text",(PlusVitaTextBox){20,20,800,1},20,false,0xffffffff));
 for(unsigned i=0;i<960*544;i++) assert(!pixels[i]);
	/* More glyphs than cache slots, at the largest admitted size, exercise
	 * eviction and the fixed byte budget under the actual rasterizer. */
	for(unsigned code=0x100;code<0x400;code++) {
		char utf8[]={(char)(0xc0|(code>>6)),(char)(0x80|(code&63)),0};
		assert(plus_vita_freetype_draw(font,pixels,960,utf8,(PlusVitaTextBox){20,20,800,120},64,false,0xffffffff));
	}
 plus_vita_freetype_free(font);
 assert(!plus_vita_freetype_create("build/nonexistent-font.ttf",(PlusVitaTextFont){0}));
 FT_Done_Face(face); FT_Done_FreeType(reference);
 PlusVitaApp *app=plus_vita_app_create(); assert(app);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=410});
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE});
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_FONT_COMPARISON);
 PlusVitaFont *a=plus_vita_font_create(), *b=plus_vita_font_create_alternative(path); assert(a && b);
 /* Normal graphical screens now use the same B font context as comparison.
  * Verify actual painter output differs from A and remains stable when warm. */
 PlusVitaApp *normal=plus_vita_app_create(); assert(normal);
 PlusVitaAppView normal_view=plus_vita_app_view(normal);
 plus_vita_app_paint(a,previous,960,&normal_view);
 plus_vita_app_paint(b,pixels,960,&normal_view);
 assert(memcmp(previous,pixels,sizeof(pixels)));
 assert_button_labels(&normal_view);
 plus_vita_app_dispatch(normal,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=360});
 normal_view=plus_vita_app_view(normal); assert(normal_view.screen==PLUS_VITA_APP_SETTINGS);
 plus_vita_app_paint(b,pixels,960,&normal_view); assert_button_labels(&normal_view);
 PlusVitaBranding *branding=plus_vita_branding_load("vita-client/assets/branding"); assert(branding);
 PlusVitaApp *preview=plus_vita_app_create(); assert(preview);
 PlusVitaAppView preview_view=plus_vita_app_view(preview);
 plus_vita_app_paint_with_resources(b,pixels,960,&preview_view,NULL,branding);
 assert(plus_vita_comparison_capture(pixels,960,"build/debug/app-freetype-host/branded-welcome.ppm"));
 plus_vita_app_dispatch(preview,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=420});
 preview_view=plus_vita_app_view(preview);assert(preview_view.screen==PLUS_VITA_APP_ABOUT);
 plus_vita_app_paint_with_resources(b,pixels,960,&preview_view,NULL,branding);
 assert(plus_vita_comparison_capture(pixels,960,"build/debug/app-freetype-host/v000-about.ppm"));
 const uint32_t *splash=plus_vita_branding_splash(branding); assert(splash);
 assert(plus_vita_comparison_capture(splash,960,"build/debug/app-freetype-host/branded-splash.ppm"));
 plus_vita_branding_free(branding);
 plus_vita_app_destroy(preview);
 plus_vita_app_paint(b,pixels,960,&normal_view);
 memcpy(previous,pixels,sizeof(pixels)); api_calls=0;
 plus_vita_app_paint(b,pixels,960,&normal_view);
 assert(!memcmp(previous,pixels,sizeof(pixels)) && api_calls==0);
 plus_vita_app_destroy(normal);
 normal=plus_vita_app_create(); assert(normal);
 PlusVitaAppEffect load=plus_vita_app_dispatch(normal,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
 const char saved[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
 PlusVitaAppEffect auth=plus_vita_app_dispatch(normal,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
  .operation=load.operation,.data=saved,.data_size=sizeof(saved)-1,.retained_json=true});
 PlusVitaCatalogEntry title={.name="Game",.id="FAKECUSA",.launch_id_valid=true};
 plus_vita_app_dispatch(normal,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,
  .operation=auth.operation,.entries=&title,.entry_count=1});
 plus_vita_app_dispatch(normal,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 normal_view=plus_vita_app_view(normal); assert(normal_view.screen==PLUS_VITA_APP_WELCOME && normal_view.action_count==5);
 plus_vita_app_paint(b,pixels,960,&normal_view); assert_button_labels(&normal_view);
 plus_vita_app_destroy(normal);
 for(unsigned page=0;page<2;page++) for(unsigned mode=0;mode<2;mode++) {
  PlusVitaAppView view=plus_vita_app_view(app);
  assert(view.comparison_page==page && view.alternative_font==(mode!=0));
  plus_vita_app_paint(mode ? b : a,pixels,960,&view);
  char output[100]; snprintf(output,sizeof(output),"build/debug/app-freetype-host/%c-%s.ppm",mode ? 'B':'A',page ? "tiles":"setup");
  assert(plus_vita_comparison_capture(pixels,960,output));
  memcpy(previous,pixels,sizeof(pixels)); api_calls=0;
  plus_vita_app_paint(mode ? b : a,pixels,960,&view);
  assert(!memcmp(previous,pixels,sizeof(pixels)) && api_calls==0);
  plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=80,.y=460});
  if(mode==1) plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,.x=370,.y=460});
 }
 plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
 assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_ABOUT);
 plus_vita_app_destroy(app);
 /* Same reverse ownership order as display_close: the alternative borrows
  * the module loaded by A. The fixture rejects unload with live libraries. */
 assert(live_libraries==2 && module_loaded);
 plus_vita_font_free(b); assert(live_libraries==1 && module_loaded);
 plus_vita_font_free(a); assert(live_libraries==0 && !module_loaded);
 puts("FreeType: real pinned raster preserves full alpha at all UI sizes; cache and bounds passed");
}
