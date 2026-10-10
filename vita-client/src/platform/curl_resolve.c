// SPDX-License-Identifier: AGPL-3.0-only

#include "curl_resolve.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * VitaSDK's threaded libcurl resolver can leave pthread-embedded with an
 * invalid handle and crash while curl tears down its Happy Eyeballs filter.
 * Resolve the known HTTPS endpoint synchronously and seed CURLOPT_RESOLVE so
 * libcurl never starts that resolver thread. TLS still uses the original host
 * name for SNI and certificate validation.
 */
CURLcode plus_vita_curl_prepare_resolve(CURL *curl, const char *url,
	struct curl_slist **resolve_list)
{
	if(!curl || !url || !resolve_list)
		return CURLE_BAD_FUNCTION_ARGUMENT;
	*resolve_list = NULL;

	const char *scheme_end = strstr(url, "://");
	if(!scheme_end)
		return CURLE_URL_MALFORMAT;
	const char *host_begin = scheme_end + 3;
	const char *host_end = host_begin;
	while(*host_end && *host_end != ':' && *host_end != '/' &&
		*host_end != '?' && *host_end != '#')
		host_end++;
	if(host_end == host_begin || *host_begin == '[')
		return CURLE_URL_MALFORMAT;

	size_t host_size = (size_t)(host_end - host_begin);
	char *host = malloc(host_size + 1);
	if(!host)
		return CURLE_OUT_OF_MEMORY;
	memcpy(host, host_begin, host_size);
	host[host_size] = 0;

	unsigned int port = strncmp(url, "https://", 8) == 0 ? 443U : 80U;
	if(*host_end == ':')
	{
		char *port_end = NULL;
		unsigned long parsed = strtoul(host_end + 1, &port_end, 10);
		if(port_end == host_end + 1 || parsed == 0 || parsed > 65535)
		{
			free(host);
			return CURLE_URL_MALFORMAT;
		}
		port = (unsigned int)parsed;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo *addresses = NULL;
	int gai_result = getaddrinfo(host, NULL, &hints, &addresses);
	if(gai_result != 0 || !addresses)
	{
		if(addresses)
			freeaddrinfo(addresses);
		free(host);
		return CURLE_COULDNT_RESOLVE_HOST;
	}

	char address[INET_ADDRSTRLEN];
	struct sockaddr_in *resolved = (struct sockaddr_in *)addresses->ai_addr;
	bool converted = inet_ntop(AF_INET, &resolved->sin_addr,
		address, sizeof(address)) != NULL;
	freeaddrinfo(addresses);
	if(!converted)
	{
		free(host);
		return CURLE_COULDNT_RESOLVE_HOST;
	}

	size_t entry_size = host_size + 1 + 5 + 1 + strlen(address) + 1;
	char *entry = malloc(entry_size);
	if(!entry)
	{
		free(host);
		return CURLE_OUT_OF_MEMORY;
	}
	snprintf(entry, entry_size, "%s:%u:%s", host, port, address);
	free(host);

	*resolve_list = curl_slist_append(NULL, entry);
	free(entry);
	if(!*resolve_list)
		return CURLE_OUT_OF_MEMORY;

	CURLcode result = curl_easy_setopt(curl, CURLOPT_IPRESOLVE,
		CURL_IPRESOLVE_V4);
	if(result == CURLE_OK)
		result = curl_easy_setopt(curl, CURLOPT_RESOLVE, *resolve_list);
	if(result != CURLE_OK)
	{
		curl_slist_free_all(*resolve_list);
		*resolve_list = NULL;
	}
	return result;
}
