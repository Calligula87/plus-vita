// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_PRESENTATION_H
#define PLUS_VITA_PRESENTATION_H
#include <stdbool.h>
#define PLUS_VITA_PRESENTATION_PATH "ux0:data/plus-vita-presentation.dat"
typedef struct { bool reduced_motion, muted; unsigned volume; } PlusVitaPresentation;
void plus_vita_presentation_defaults(PlusVitaPresentation *preferences);
bool plus_vita_presentation_load(const char *path,PlusVitaPresentation *preferences);
bool plus_vita_presentation_save(const char *path,const PlusVitaPresentation *preferences);
#endif
