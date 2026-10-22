// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_COVER_POLICY_H
#define PLUS_VITA_COVER_POLICY_H
#include <stdbool.h>
#include <string.h>
/* Public artwork only. No credentials, redirects, alternate hosts or ports. */
static inline bool plus_vita_cover_url_allowed(const char *url)
{
	if(!url) return false;
	if(strncmp(url,"https://image.api.playstation.com/",sizeof("https://image.api.playstation.com/")-1)!=0
		&& strncmp(url,"https://gmedia.playstation.com/",sizeof("https://gmedia.playstation.com/")-1)!=0) return false;
	for(unsigned i=0;i<512;i++) {
		unsigned char c=(unsigned char)url[i];
		if(!c) return true;
		if(c<=32 || c==127) return false;
	}
	return false;
}
#endif
