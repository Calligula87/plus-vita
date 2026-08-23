// SPDX-License-Identifier: AGPL-3.0-only

#include <curl/curl.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct plus_vita_curl_t { int unused; };

CURL *curl_easy_init(void)
{
	return calloc(1, sizeof(CURL));
}

char *curl_easy_escape(CURL *curl, const char *text, int length)
{
	if(!curl || !text || length < 0)
		return NULL;
	size_t input_size = length ? (size_t)length : strlen(text);
	if(input_size > (SIZE_MAX - 1) / 3)
		return NULL;
	char *escaped = malloc(input_size * 3 + 1);
	if(!escaped)
		return NULL;
	static const char hex[] = "0123456789ABCDEF";
	size_t out = 0;
	for(size_t i = 0; i < input_size; i++)
	{
		unsigned char c = (unsigned char)text[i];
		if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_'
			|| c == '.' || c == '~')
			escaped[out++] = (char)c;
		else
		{
			escaped[out++] = '%';
			escaped[out++] = hex[c >> 4];
			escaped[out++] = hex[c & 15];
		}
	}
	escaped[out] = 0;
	return escaped;
}

void curl_easy_cleanup(CURL *curl)
{
	free(curl);
}

void curl_free(void *data)
{
	free(data);
}
