// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CURL_RESOLVE_H
#define PLUS_VITA_CURL_RESOLVE_H

#include <curl/curl.h>

CURLcode plus_vita_curl_prepare_resolve(CURL *curl, const char *url,
	struct curl_slist **resolve_list);

#endif
