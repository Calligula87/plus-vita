// SPDX-License-Identifier: AGPL-3.0-only
#include "cover_download.h"
#include "cover_resolve.h"
#include <psp2/net/net.h>
#undef curl_easy_setopt
#undef curl_easy_getinfo
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cover_http_fixture.h"
int main(void)
{
 const char *url="https://vulcan.dl.playstation.net/fixture.png";
 PlusVitaArtworkResult reason; atomic_bool cancel=false; unsigned char *data=NULL; size_t size=0;
 assert(plus_vita_cover_download(url,&data,&size,&cancel,&reason) && size==4096); free(data);
 cover_mode=1; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason) && !data && !size);
 assert(reason==PLUS_VITA_ARTWORK_LIMIT);
 cover_mode=2; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason) && !data && !size);
 assert(reason==PLUS_VITA_ARTWORK_CANCELLED);
 atomic_store(&cancel,false); cover_mode=3; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason));
 cover_mode=0; status=302; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason));
 unsigned before=cover_requests;
 status=200; cover_mode=4; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason));
 assert(reason==PLUS_VITA_ARTWORK_DNS);
 cover_mode=5; dns_cancel=&cancel; assert(!plus_vita_cover_download(url,&data,&size,&cancel,&reason));
 assert(cover_requests==before); atomic_store(&cancel,false);
 assert(!plus_vita_cover_download("http://127.0.0.1/private",&data,&size,&cancel,&reason));
 assert(cover_requests==before);
 puts("cover HTTP boundary: bounded native DNS, TLS, no redirects, size bounds, overflow, status and cancellation passed");
}


