// SPDX-License-Identifier: AGPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "covers.h"
#include "cover_download.h"
#include "cover_policy.h"
#include "cover_fixture.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static atomic_bool block,entered;
static void pause_poll(void) { const struct timespec t={.tv_nsec=1000000}; nanosleep(&t,NULL); }
bool __wrap_plus_vita_cover_download(const char *url,unsigned char **data,size_t *size,const atomic_bool *cancel,PlusVitaArtworkResult *result)
{
	*result=PLUS_VITA_ARTWORK_OK; atomic_store(&entered,true);
	while(atomic_load(&block) && !atomic_load(cancel)) pause_poll();
	if(atomic_load(cancel)) return false;
	const unsigned char *source=strstr(url,".jpg") ? cover_jpeg : cover_png;
	*size=strstr(url,".jpg") ? sizeof(cover_jpeg) : sizeof(cover_png);
	if(strstr(url,"broken")) *size=3;
	*data=malloc(*size); assert(*data); memcpy(*data,source,*size); return true;
}
static void finish(PlusVitaCovers *covers)
{
	bool done=false;
	for(unsigned i=0;i<5000 && !done;i++) { done=plus_vita_covers_poll(covers); if(!done) pause_poll(); }
	assert(done);
}
int main(void)
{
	assert(!plus_vita_cover_url_allowed("https://image.api.playstation.com.evil.invalid/a.png"));
	assert(!plus_vita_cover_url_allowed("http://image.api.playstation.com/a.png"));
	assert(!plus_vita_cover_url_allowed("https://image.api.playstation.com:443/a.png"));
	assert(plus_vita_cover_url_allowed("https://vulcan.dl.playstation.net/fixture.png"));
	assert(plus_vita_cover_url_allowed("https://apollo2.dl.playstation.net/fixture.png"));
	assert(plus_vita_cover_url_allowed("https://gs2-sec.ww.prod.dl.playstation.net/fixture.png"));
	assert(!plus_vita_cover_url_allowed("https://vulcan.dl.playstation.net.evil.invalid/fixture.png"));
	assert(!plus_vita_cover_url_allowed("https://vulcan.dl.playstation.net@evil.invalid/fixture.png"));
	uint32_t image[128*128];
	assert(plus_vita_cover_decode(cover_png,sizeof(cover_png),image));
	assert(image[64*128+8]==0xff0000ff && image[64*128+100]==0xffff0000);
	assert(plus_vita_cover_decode(cover_jpeg,sizeof(cover_jpeg),image));
	assert((image[64*128+8]&255)>240 && ((image[64*128+100]>>16)&255)>240);
	assert(!plus_vita_cover_decode(cover_png,2*1024*1024u+1,image));
	assert(!plus_vita_cover_decode(cover_png,3,image));
	PlusVitaCovers *covers=plus_vita_covers_create(); assert(covers);
	const char *a="https://image.api.playstation.com/a.png", *b="https://gmedia.playstation.com/b.jpg";
	atomic_store(&block,true); const char *urls[]={a,b}; plus_vita_covers_request(covers,urls,2);
	for(unsigned i=0;i<1000 && !atomic_load(&entered);i++) pause_poll();
	assert(atomic_load(&entered));
	for(unsigned i=0;i<1000;i++) { plus_vita_covers_request(covers,urls,2); assert(!plus_vita_covers_poll(covers)); }
	assert(plus_vita_covers_find(covers,a)->state==PLUS_VITA_COVER_LOADING);
	plus_vita_covers_reset(covers); finish(covers);
	assert(!plus_vita_covers_find(covers,a)); /* Late completion cannot restore forgotten artwork. */
	atomic_store(&block,false); plus_vita_covers_request(covers,urls,2); finish(covers);
	assert(plus_vita_covers_find(covers,a)->state==PLUS_VITA_COVER_READY);
	plus_vita_covers_request(covers,urls,2); finish(covers);
	assert(plus_vita_covers_find(covers,b)->state==PLUS_VITA_COVER_READY);
	const char *broken="https://image.api.playstation.com/broken.png";
	plus_vita_covers_request(covers,&broken,1); finish(covers);
	assert(plus_vita_covers_find(covers,broken)->state==PLUS_VITA_COVER_FAILED);
	assert(plus_vita_covers_close(covers));
	puts("covers: real PNG/JPEG decode, bounded cache, blocked I/O, failure and stale reset passed");
}

