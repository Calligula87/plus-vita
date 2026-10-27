// SPDX-License-Identifier: AGPL-3.0-only
#include "render.h"
#include <string.h>
#include "../platform/build_identity.h"
#define WIDTH 960
#define HEIGHT 544
#define RGB(r,g,b) (0xff000000u | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (r))
/* Native pixel sizes from the hardware-selected A setup and tile views.
 * All screens share this painter and the system PVF path. Keep compact
 * Account/tile text at 18px and the accepted heading at 36px. */
typedef enum {
	TEXT_MICRO=14, TEXT_HINT=16, TEXT_COMPACT=18, TEXT_BODY=20,
	TEXT_CONTROL=22, TEXT_BRAND=24, TEXT_HEADING=36
} TextRole;
typedef struct { uint32_t *pixels; int pitch; PlusVitaFont *font; } Canvas;
static void rectangle(Canvas *display, int x, int y, int width, int height, uint32_t color)
{
	int right = x + width, bottom = y + height;
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(right > WIDTH) right = WIDTH;
	if(bottom > HEIGHT) bottom = HEIGHT;
	for(int row = y; row < bottom; row++)
		for(int col = x; col < right; col++) display->pixels[row * display->pitch + col] = color;
}

static void rounded(Canvas *display, int x, int y, int width, int height, uint32_t color)
{
	for(int row = 0; row < height; row++)
	{
		int inset = row == 0 || row == height - 1 ? 5 : row < 3 || row >= height - 3 ? 2 : 0;
		rectangle(display, x + inset, y + row, width - 2 * inset, 1, color);
	}
}

static void text(Canvas *display, const char *value, int x, int y, TextRole role, int width, int height, bool wrap, uint32_t color)
{
	plus_vita_font_draw(display->font, display->pixels, display->pitch, value,
		(PlusVitaTextBox){x, y, width, height}, role, wrap, color);
}
static void cover(Canvas *display,const PlusVitaCovers *covers,const PlusVitaCatalogEntry *game,int x,int y)
{
	const PlusVitaCoverImage *image=game ? plus_vita_covers_find(covers,game->cover_url) : NULL;
	rectangle(display,x,y,128,128,RGB(9,29,66));
	if(image && image->state==PLUS_VITA_COVER_READY) {
		for(int row=0;row<128;row++) for(int col=0;col<128;col++) display->pixels[(y+row)*display->pitch+x+col]=image->pixels[row*128+col];
	} else text(display,!game || (!game->cover_url[0] && game->cover_metadata==PLUS_VITA_ARTWORK_MISSING) ? "No cover" : !game->cover_url[0] || !covers ? "Cover unavailable" : !image || image->state==PLUS_VITA_COVER_LOADING ? "Loading cover" : "Cover unavailable",x+6,y+48,TEXT_MICRO,116,58,true,RGB(195,211,239));
}

static void wordmark(Canvas *display,const PlusVitaBranding *branding)
{
	const uint32_t *image=plus_vita_branding_wordmark(branding);
	if(!image) {
		text(display,"plus-vita",64,25,TEXT_BRAND,400,38,false,RGB(246,198,91));
		return;
	}
	for(unsigned y=0;y<40;y++) for(unsigned x=0;x<224;x++) {
		uint32_t source=image[y*224+x], alpha=source>>24;
		uint32_t *destination=&display->pixels[(20+y)*display->pitch+64+x];
		if(alpha==255) *destination=source;
		else if(alpha) {
			uint32_t result=0xff000000u;
			for(unsigned shift=0;shift<24;shift+=8)
				result|=(((((source>>shift)&255)*alpha+((*destination>>shift)&255)*(255-alpha)+127)/255)<<shift);
			*destination=result;
		}
	}
}

void plus_vita_app_paint_with_resources(PlusVitaFont *font,uint32_t *pixels,int pitch,const PlusVitaAppView *view,const PlusVitaCovers *covers,const PlusVitaBranding *branding)
{
	Canvas canvas = {pixels, pitch, font}; Canvas *display = &canvas;
	const uint32_t *background=plus_vita_branding_background(branding);
	for(int y = 0; y < HEIGHT; y++) {
		if(background) memcpy(pixels+y*pitch,background+y*WIDTH,WIDTH*sizeof(*pixels));
		else rectangle(display, 0, y, WIDTH, 1, RGB(0, 25 + y / 20, 70 + y / 6));
	}
	rectangle(display, 0, 0, WIDTH, 4, RGB(246, 198, 91));
	wordmark(display,branding);
	text(display, PLUS_VITA_BUILD_LABEL, 712, 32, TEXT_MICRO, 220, 24, false, RGB(195, 211, 239));
	text(display, view->subtitle, 64, 94, TEXT_HINT, 832, 26, false, RGB(246, 198, 91));
	text(display, view->title, 64, 128, TEXT_HEADING, 832, 54, false, RGB(243, 247, 252));
	int body_bottom = view->action_count ? view->actions[0].y - 12 : 480;
	if(view->screen==PLUS_VITA_APP_CATALOGUE) body_bottom=224;
	if(view->screen==PLUS_VITA_APP_FONT_COMPARISON) body_bottom=view->comparison_page ? 224 : 302;
	text(display, view->body, 64, 196, view->screen == PLUS_VITA_APP_ACCOUNT ? TEXT_COMPACT : TEXT_BODY,
		view->screen == PLUS_VITA_APP_WELCOME ? 520 : view->screen==PLUS_VITA_APP_DETAILS ? 624 : 832, body_bottom - 196, view->screen!=PLUS_VITA_APP_CATALOGUE, RGB(210, 224, 247));
	if(view->screen==PLUS_VITA_APP_DETAILS) cover(display,covers,view->detail,744,216);
	if(view->screen==PLUS_VITA_APP_FONT_COMPARISON) {
		if(view->comparison_page) {
			static const PlusVitaCatalogEntry samples[]={
				{.name="Agjpqy: Complete letters"},{.name="Müller 日本"},
				{.name="A long game title with complete letters and deliberate overflow"}};
			for(unsigned i=0;i<3;i++) {
				int x=64+(int)i*286;
				rounded(display,x,228,260,204,i==1 ? RGB(246,198,91) : RGB(17,57,125));
				cover(display,NULL,&samples[i],x+66,238);
				text(display,samples[i].name,x+12,374,TEXT_COMPACT,236,48,true,i==1 ? RGB(9,29,66) : RGB(243,247,252));
			}
		} else {
			text(display,"14 px: Agjpqy é Å Müller",64,310,TEXT_MICRO,832,24,false,RGB(210,224,247));
			text(display,"16 px: Agjpqy é Å Müller",64,342,TEXT_HINT,832,28,false,RGB(210,224,247));
			text(display,"18 px: Agjpqy é Å Müller 日本",64,376,TEXT_COMPACT,832,32,false,RGB(210,224,247));
		}
	}
	for(size_t i = 0; i < view->action_count; i++)
	{
		const PlusVitaAppButton *button = &view->actions[i];
		bool selected = i == view->selected;
		rounded(display, button->x, button->y, button->width, button->height,
			selected ? RGB(246, 198, 91) : RGB(17, 57, 125));
		if(view->screen==PLUS_VITA_APP_CATALOGUE && button->id==PLUS_VITA_ACTION_GAME) {
			cover(display,covers,view->games[i],button->x+66,button->y+10);
			text(display,button->label,button->x+12,button->y+146,TEXT_COMPACT,button->width-24,48,true,selected ? RGB(9,29,66) : RGB(243,247,252));
		} else {
			bool compact = button->height < 48;
			int inset = compact ? 6 : 8;
			text(display, button->label, button->x + 20, button->y + inset,
				compact ? TEXT_COMPACT : TEXT_CONTROL, button->width - 40,
				button->height - 2 * inset, false,
				selected ? RGB(9, 29, 66) : RGB(243, 247, 252));
		}
	}
	text(display, view->hint, 64, 509, TEXT_HINT, 832, 28, false, RGB(195, 211, 239));
}

void plus_vita_app_paint(PlusVitaFont *font,uint32_t *pixels,int pitch,const PlusVitaAppView *view)
{ plus_vita_app_paint_with_covers(font,pixels,pitch,view,NULL); }

void plus_vita_app_paint_with_covers(PlusVitaFont *font,uint32_t *pixels,int pitch,const PlusVitaAppView *view,const PlusVitaCovers *covers)
{ plus_vita_app_paint_with_resources(font,pixels,pitch,view,covers,NULL); }
