// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_BRANDING_H
#define PLUS_VITA_BRANDING_H
#include <stdint.h>
#include <stdbool.h>
typedef struct plus_vita_branding_t PlusVitaBranding;
/* Bounded local package resources. A failed image remains independently absent. */
PlusVitaBranding *plus_vita_branding_load(const char *directory);
const uint32_t *plus_vita_branding_logo(const PlusVitaBranding *branding);
const uint32_t *plus_vita_branding_splash(const PlusVitaBranding *branding);
void plus_vita_branding_release_splash(PlusVitaBranding *branding);
void plus_vita_branding_free(PlusVitaBranding *branding);
#endif
