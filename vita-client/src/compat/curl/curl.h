// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_CURL_ESCAPE_COMPAT_H
#define PLUS_VITA_CURL_ESCAPE_COMPAT_H

typedef struct plus_vita_curl_t CURL;

CURL *curl_easy_init(void);
char *curl_easy_escape(CURL *curl, const char *text, int length);
void curl_easy_cleanup(CURL *curl);
void curl_free(void *data);

#endif
