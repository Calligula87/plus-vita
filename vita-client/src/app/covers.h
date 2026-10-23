// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_COVERS_H
#define PLUS_VITA_COVERS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../platform/artwork_status.h"
#define PLUS_VITA_COVER_SIDE 128
typedef enum { PLUS_VITA_COVER_LOADING, PLUS_VITA_COVER_READY, PLUS_VITA_COVER_FAILED } PlusVitaCoverState;
typedef struct { char url[512]; PlusVitaCoverState state; PlusVitaArtworkResult result; uint32_t pixels[128*128]; } PlusVitaCoverImage;
typedef struct plus_vita_covers_t PlusVitaCovers;
/* All calls except the private downloader run on the UI owner thread. */
PlusVitaCovers *plus_vita_covers_create(void);
void plus_vita_covers_request(PlusVitaCovers *covers,const char *const *urls,size_t count);
bool plus_vita_covers_poll(PlusVitaCovers *covers);
PlusVitaArtworkResult plus_vita_covers_last_result(const PlusVitaCovers *covers);
const PlusVitaCoverImage *plus_vita_covers_find(const PlusVitaCovers *covers,const char *url);
void plus_vita_covers_reset(PlusVitaCovers *covers);
bool plus_vita_covers_close(PlusVitaCovers *covers);
#endif
