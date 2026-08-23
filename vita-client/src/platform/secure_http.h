// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_SECURE_HTTP_H
#define PLUS_VITA_SECURE_HTTP_H

#include <chiaki/common.h>
#include <stdbool.h>

ChiakiErrorCode plus_vita_secure_http_init(void);
void plus_vita_secure_http_fini(void);
bool plus_vita_secure_http_is_initialized(void);

#endif
