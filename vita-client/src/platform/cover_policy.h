// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_COVER_POLICY_H
#define PLUS_VITA_COVER_POLICY_H
#include <stdbool.h>
#include <string.h>
/* Public artwork only. No credentials, redirects, alternate hosts or ports. */
static inline const char *plus_vita_cover_url_host(const char *url)
{
	static const char *const hosts[] = {
		"image.api.playstation.com", "gmedia.playstation.com",
		"vulcan.dl.playstation.net", "apollo2.dl.playstation.net",
		"gs2-sec.ww.prod.dl.playstation.net"
	};
	if(!url || strncmp(url,"https://",8)!=0) return NULL;
	const char *host = NULL;
	for(unsigned i=0;i<sizeof(hosts)/sizeof(hosts[0]);i++) {
		size_t length = strlen(hosts[i]);
		if(strncmp(url+8,hosts[i],length)==0 && url[8+length]=='/') { host=hosts[i]; break; }
	}
	if(!host) return NULL;
	for(unsigned i=0;i<512;i++) {
		unsigned char c=(unsigned char)url[i];
		if(!c) return host;
		if(c<=32 || c==127) return NULL;
	}
	return NULL;
}
static inline bool plus_vita_cover_url_allowed(const char *url) { return plus_vita_cover_url_host(url)!=NULL; }
#endif
