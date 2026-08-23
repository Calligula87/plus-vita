// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CC_HTTP_TEST_H
#define PLUS_VITA_CC_HTTP_TEST_H

#include <stdbool.h>

void plus_vita_http_test_block_network(bool blocked);
unsigned int plus_vita_http_test_network_attempts(void);

#endif
