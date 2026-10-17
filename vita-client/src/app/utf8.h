// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_UTF8_H
#define PLUS_VITA_UTF8_H
#include <stdint.h>
/* Decode one scalar within bounds; invalid input advances one byte and returns UINT32_MAX. */
uint32_t plus_vita_utf8_next(const char **cursor, const char *end);
#endif
