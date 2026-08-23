// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_SECURE_HTTP_H
#define PLUS_VITA_SECURE_HTTP_H

#include <chiaki/common.h>
#include <stdbool.h>

ChiakiErrorCode plus_vita_secure_http_init(void);
void plus_vita_secure_http_fini(void);
bool plus_vita_secure_http_is_initialized(void);

#ifdef __vita__
typedef struct plus_vita_tls_probe_result_t
{
	int send_result;
	int ssl_error;
	unsigned int ssl_detail;
} PlusVitaTlsProbeResult;

ChiakiErrorCode plus_vita_secure_http_probe_untrusted(
	const char *url, PlusVitaTlsProbeResult *result);
#endif

#endif
