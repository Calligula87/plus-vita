// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_UTF8_H
#define PLUS_VITA_UTF8_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Decode one scalar within bounds; invalid input advances one byte and returns UINT32_MAX. */
uint32_t plus_vita_utf8_next(const char **cursor, const char *end);
/* Bounded visible labels; empty is allowed, controls and invalid UTF-8 are not. */
bool plus_vita_utf8_label(const char *text, size_t bytes);
#endif
