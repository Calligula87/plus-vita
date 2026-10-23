// SPDX-License-Identifier: AGPL-3.0-only
#include "cover_download.h"
#include "cover_policy.h"
#include "cover_resolve.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned char *data; size_t size; const atomic_bool *cancel; bool limited; } Download;
static size_t receive(void *data,size_t size,size_t count,void *context)
{
	Download *d=context;
	if(size && count>SIZE_MAX/size) { d->limited=true; return 0; }
	size_t bytes=size*count;
	if(!bytes) return 0;
	if(atomic_load(d->cancel)) return 0;
	if(bytes>2*1024*1024u-d->size) { d->limited=true; return 0; }
	void *grown=realloc(d->data,d->size+bytes);
	if(!grown && bytes) return 0;
	d->data=grown; memcpy(d->data+d->size,data,bytes); d->size+=bytes;
	return bytes;
}
static int progress(void *context,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d)
{ (void)a;(void)b;(void)c;(void)d; return atomic_load((const atomic_bool *)context); }
bool plus_vita_cover_download(const char *url,unsigned char **data,size_t *size,const atomic_bool *cancel,PlusVitaArtworkResult *outcome)
{
	PlusVitaArtworkResult ignored;
	if(!outcome) outcome=&ignored;
	*outcome=PLUS_VITA_ARTWORK_UNAVAILABLE;
	*data=NULL; *size=0;
	if(!plus_vita_cover_url_allowed(url)) { *outcome=PLUS_VITA_ARTWORK_POLICY; return false; }
	if(atomic_load(cancel)) { *outcome=PLUS_VITA_ARTWORK_CANCELLED; return false; }
	CURL *curl=curl_easy_init(); if(!curl) return false;
	Download download={.cancel=cancel}; struct curl_slist *resolved=NULL;
	CURLcode result=plus_vita_cover_resolve(curl,url,&resolved,cancel);
	bool dns_failed=result!=CURLE_OK;
	if(result==CURLE_OK && !atomic_load(cancel)) {
		curl_easy_setopt(curl,CURLOPT_URL,url); curl_easy_setopt(curl,CURLOPT_CAINFO,"app0:/cacert.pem");
		curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L); curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L); curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,5000L); curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,10000L);
		curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive); curl_easy_setopt(curl,CURLOPT_WRITEDATA,&download);
		curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L); curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);
		curl_easy_setopt(curl,CURLOPT_XFERINFODATA,cancel); result=curl_easy_perform(curl);
	}
	long status=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
	curl_easy_cleanup(curl); curl_slist_free_all(resolved);
	if(result!=CURLE_OK || status!=200 || !download.size || atomic_load(cancel)) {
		*outcome=atomic_load(cancel) ? PLUS_VITA_ARTWORK_CANCELLED : dns_failed ? PLUS_VITA_ARTWORK_DNS
			: download.limited ? PLUS_VITA_ARTWORK_LIMIT
			: result==CURLE_PEER_FAILED_VERIFICATION || result==CURLE_SSL_CONNECT_ERROR || result==CURLE_SSL_CERTPROBLEM ? PLUS_VITA_ARTWORK_TLS
			: result!=CURLE_OK ? PLUS_VITA_ARTWORK_TRANSPORT : status!=200 ? PLUS_VITA_ARTWORK_HTTP : PLUS_VITA_ARTWORK_DECODE;
		free(download.data); return false;
	}
	*outcome=PLUS_VITA_ARTWORK_OK;
	*data=download.data; *size=download.size; return true;
}
