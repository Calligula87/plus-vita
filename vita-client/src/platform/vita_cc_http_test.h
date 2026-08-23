// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CC_HTTP_TEST_H
#define PLUS_VITA_CC_HTTP_TEST_H

#include <stdbool.h>

void plus_vita_http_test_block_network(bool blocked);
unsigned int plus_vita_http_test_network_attempts(void);
const char *plus_vita_http_test_last_stage(void);
int plus_vita_http_test_last_native_result(void);
int plus_vita_http_test_last_error(void);
int plus_vita_http_test_last_status(void);
int plus_vita_http_test_last_ssl_error(void);
unsigned int plus_vita_http_test_last_ssl_detail(void);

#endif
