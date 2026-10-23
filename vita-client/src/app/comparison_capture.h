// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_COMPARISON_CAPTURE_H
#define PLUS_VITA_COMPARISON_CAPTURE_H
#include <stdbool.h>
#include <stdint.h>
/* Caller must supply the synthetic comparison screen only. */
bool plus_vita_comparison_capture(const uint32_t *pixels,int pitch,const char *path);
#endif
