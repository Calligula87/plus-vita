// SPDX-License-Identifier: AGPL-3.0-only
#include "render.h"
#define WIDTH 960
#define HEIGHT 544
#define RGB(r,g,b) (0xff000000u | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (r))
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

static void text(Canvas *display, const char *value, int x, int y, int size, int width, int height, bool wrap, uint32_t color)
{
	plus_vita_font_draw(display->font, display->pixels, display->pitch, value,
		(PlusVitaTextBox){x, y, width, height}, size, wrap, color);
}
void plus_vita_app_paint(PlusVitaFont *font, uint32_t *pixels, int pitch, const PlusVitaAppView *view)
{
	Canvas canvas = {pixels, pitch, font}; Canvas *display = &canvas;
	for(int y = 0; y < HEIGHT; y++)
		rectangle(display, 0, y, WIDTH, 1, RGB(0, 25 + y / 20, 70 + y / 6));
	rectangle(display, 0, 0, WIDTH, 4, RGB(246, 198, 91));
	text(display, "plus vita", 64, 25, 24, 400, 38, false, RGB(236, 244, 250));
	text(display, "DEVELOPMENT BUILD", 712, 32, 14, 220, 24, false, RGB(195, 211, 239));
	text(display, view->subtitle, 64, 94, 16, 832, 26, false, RGB(246, 198, 91));
	text(display, view->title, 64, 128, 36, 832, 54, false, RGB(243, 247, 252));
	int body_bottom = view->action_count ? view->actions[0].y - 12 : 480;
	text(display, view->body, 64, 196, view->screen == PLUS_VITA_APP_ACCOUNT ? 18 : 20,
		view->screen == PLUS_VITA_APP_WELCOME ? 520 : 832, body_bottom - 196, true, RGB(210, 224, 247));
	if(view->screen == PLUS_VITA_APP_WELCOME)
	{
		rounded(display, 674, 238, 192, 192, RGB(18, 53, 113));
		text(display, "APP LOGO", 712, 301, 18, 150, 30, false, RGB(210, 224, 247));
		text(display, "Coming later", 709, 335, 16, 150, 28, false, RGB(195, 211, 239));
	}
	for(size_t i = 0; i < view->action_count; i++)
	{
		const PlusVitaAppButton *button = &view->actions[i];
		bool selected = i == view->selected;
		rounded(display, button->x, button->y, button->width, button->height,
			selected ? RGB(246, 198, 91) : RGB(17, 57, 125));
		text(display, button->label, button->x + 20, button->y + 8, 22, button->width - 40, button->height - 16, false,
			selected ? RGB(9, 29, 66) : RGB(243, 247, 252));
	}
	text(display, view->hint, 64, 509, 16, 832, 28, false, RGB(195, 211, 239));
}
