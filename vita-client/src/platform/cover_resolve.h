// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_COVER_RESOLVE_H
#define PLUS_VITA_COVER_RESOLVE_H
#include <curl/curl.h>
#include <stdatomic.h>
CURLcode plus_vita_cover_resolve(CURL *curl, const char *url,
	struct curl_slist **list, const atomic_bool *cancel);
#endif
