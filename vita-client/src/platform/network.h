// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_NETWORK_H
#define PLUS_VITA_NETWORK_H

#include <chiaki/common.h>
#include <stdbool.h>

ChiakiErrorCode plus_vita_network_init(void);
void plus_vita_network_fini(void);
bool plus_vita_network_is_initialized(void);

#endif
