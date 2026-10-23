// SPDX-License-Identifier: AGPL-3.0-only
#include "cover_resolve.h"
#include "cover_policy.h"
#include <psp2/net/net.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* Artwork has a separate bounded resolver. Keep the historical Sony/stream
 * transport unchanged and never start libcurl's problematic resolver thread. */
CURLcode plus_vita_cover_resolve(CURL *curl, const char *url,
	struct curl_slist **list, const atomic_bool *cancel)
{
	*list = NULL;
	const char *host = plus_vita_cover_url_host(url);
	if(!host || atomic_load(cancel)) return CURLE_ABORTED_BY_CALLBACK;
	int resolver = sceNetResolverCreate("PlusVitaCover", NULL, 0);
	if(resolver < 0) return CURLE_COULDNT_RESOLVE_HOST;
	SceNetInAddr address = {0};
	/* Native API uses microseconds. One retry bounds the non-interruptible
	 * DNS phase; cancellation is checked before and immediately after it. */
	int result = sceNetResolverStartNtoa(resolver,host,&address,1000000,1,0);
	sceNetResolverDestroy(resolver);
	if(atomic_load(cancel)) return CURLE_ABORTED_BY_CALLBACK;
	if(result < 0) return CURLE_COULDNT_RESOLVE_HOST;
	char ip[INET_ADDRSTRLEN], entry[80];
	if(!inet_ntop(AF_INET,&address,ip,sizeof(ip))) return CURLE_COULDNT_RESOLVE_HOST;
	snprintf(entry,sizeof(entry),"%s:443:%s",host,ip);
	*list = curl_slist_append(NULL,entry);
	if(!*list) return CURLE_OUT_OF_MEMORY;
	CURLcode error = curl_easy_setopt(curl,CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4);
	if(error == CURLE_OK) error = curl_easy_setopt(curl,CURLOPT_RESOLVE,*list);
	if(error != CURLE_OK) { curl_slist_free_all(*list); *list=NULL; }
	return error;
}
