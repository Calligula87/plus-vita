// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_RENDER_H
#define PLUS_VITA_RENDER_H
#include "app.h"
#include "font_vita.h"
#include "covers.h"
/* Paint one native 960x544 view into the caller's back buffer. */
void plus_vita_app_paint(PlusVitaFont *font, uint32_t *pixels, int pitch, const PlusVitaAppView *view);
void plus_vita_app_paint_with_covers(PlusVitaFont *font,uint32_t *pixels,int pitch,const PlusVitaAppView *view,const PlusVitaCovers *covers);
#endif
