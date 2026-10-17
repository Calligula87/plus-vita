// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { PlusVitaTextBox box; unsigned drawn, dots; int last_baseline; } Output;
static void metrics(void *context, int size, int *ascent, int *descent)
{ (void)context; *ascent = size; *descent = 5; }
static PlusVitaGlyph measure(void *context, unsigned code, int size)
{
	(void)context;
	int width = code == 'W' ? 25 : code == 'i' ? 4 : 11;
	return (PlusVitaGlyph){ .advance = width, .left = code == 'j' ? -2 : 0, .top = size,
		.width = width, .height = size + 5 };
}
static void ink(void *context, unsigned code, int x, int baseline, int size, const PlusVitaTextBox *clip)
{
	Output *output = context;
	PlusVitaGlyph glyph = measure(context, code, size);
	assert(x + glyph.left >= clip->x && x + glyph.left + glyph.width <= clip->x + clip->width);
	assert(baseline - glyph.top >= clip->y && baseline - glyph.top + glyph.height <= clip->y + clip->height);
	assert(clip->y >= output->box.y && clip->y + clip->height <= output->box.y + output->box.height);
	output->drawn++;
	output->dots = code == '.' ? output->dots + 1 : 0;
	output->last_baseline = baseline;
}
int main(void)
{
	PlusVitaApp *app = plus_vita_app_create();
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	PlusVitaAppView view = plus_vita_app_view(app);
	Output output = { .box = {64, 196, 832, 236} };
	PlusVitaTextFont font = { &output, metrics, measure, ink };
	assert(plus_vita_text_draw(&font, view.body, output.box, 20, true));
	unsigned expected = 0;
	for(const char *p = view.body; *p; p++) if(*p != '\n' && *p != ' ') expected++;
	assert(output.drawn == expected);
	/* A long unbroken title must end deliberately, never draw outside its button. */
	char long_title[250]; memset(long_title, 'W', sizeof(long_title) - 1); long_title[249] = 0;
	output = (Output){ .box = {84, 326, 792, 32} };
	assert(!plus_vita_text_draw(&font, long_title, output.box, 22, false));
	assert(output.dots == 3 && output.last_baseline == 348);
	/* Bearings and descenders remain inside the box, including the first glyph. */
	output = (Output){ .box = {64, 196, 80, 85} };
	assert(plus_vita_text_draw(&font, "jump iii WWW", output.box, 20, true));
	assert(output.drawn == 10);
	plus_vita_app_destroy(app);
	puts("app text: complete About output, measured wrapping, bearing/descender bounds and explicit title ellipsis passed");
	return 0;
}
