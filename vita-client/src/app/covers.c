// SPDX-License-Identifier: AGPL-3.0-only
#include "covers.h"
#include "../platform/cover_download.h"
#include "../platform/cover_policy.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
struct plus_vita_covers_t {
	PlusVitaCoverImage slots[6], output;
	pthread_t thread;
	atomic_bool complete,cancel;
	bool running;
	unsigned next, generation, job_generation;
};
static void *load_cover(void *context)
{
	PlusVitaCovers *covers=context; unsigned char *data=NULL; size_t size=0;
	bool ok=plus_vita_cover_download(covers->output.url,&data,&size,&covers->cancel)
		&& !atomic_load(&covers->cancel) && plus_vita_cover_decode(data,size,covers->output.pixels);
	free(data); covers->output.state=ok ? PLUS_VITA_COVER_READY : PLUS_VITA_COVER_FAILED;
	atomic_store(&covers->complete,true); return NULL;
}
PlusVitaCovers *plus_vita_covers_create(void)
{
	PlusVitaCovers *covers=calloc(1,sizeof(*covers));
	if(covers) { atomic_init(&covers->complete,false); atomic_init(&covers->cancel,false); }
	return covers;
}
const PlusVitaCoverImage *plus_vita_covers_find(const PlusVitaCovers *covers,const char *url)
{
	if(!covers || !url || !*url) return NULL;
	for(unsigned i=0;i<6;i++) if(strcmp(covers->slots[i].url,url)==0) return &covers->slots[i];
	return NULL;
}
bool plus_vita_covers_poll(PlusVitaCovers *covers)
{
	if(!covers || !covers->running || !atomic_load(&covers->complete)) return false;
	pthread_join(covers->thread,NULL); covers->running=false;
	if(covers->generation==covers->job_generation)
		for(unsigned i=0;i<6;i++) if(strcmp(covers->slots[i].url,covers->output.url)==0) {
			if(atomic_load(&covers->cancel)) memset(&covers->slots[i],0,sizeof(covers->slots[i]));
			else covers->slots[i]=covers->output;
		}
	return true;
}
void plus_vita_covers_request(PlusVitaCovers *covers,const char *const *urls,size_t count)
{
	if(!covers) return;
	bool visible=false;
	for(size_t i=0;i<count;i++) if(urls[i] && strcmp(urls[i],covers->output.url)==0) visible=true;
	if(covers->running) { if(!visible) atomic_store(&covers->cancel,true); return; }
	for(size_t i=0;i<count;i++) {
		const char *url=urls[i];
		if(!plus_vita_cover_url_allowed(url) || plus_vita_covers_find(covers,url)) continue;
		PlusVitaCoverImage *slot=&covers->slots[covers->next++ % 6];
		memset(slot,0,sizeof(*slot)); strcpy(slot->url,url); slot->state=PLUS_VITA_COVER_LOADING;
		memset(&covers->output,0,sizeof(covers->output)); strcpy(covers->output.url,url);
		covers->job_generation=covers->generation; atomic_store(&covers->complete,false); atomic_store(&covers->cancel,false);
		pthread_attr_t attr; bool ready=pthread_attr_init(&attr)==0;
		int result=ready ? pthread_attr_setstacksize(&attr,512*1024) : -1;
		if(!result) result=pthread_create(&covers->thread,&attr,load_cover,covers);
		if(ready) pthread_attr_destroy(&attr);
		covers->running=result==0;
		if(result) slot->state=PLUS_VITA_COVER_FAILED;
		return;
	}
}
void plus_vita_covers_reset(PlusVitaCovers *covers)
{
	if(!covers) return;
	atomic_store(&covers->cancel,true); covers->generation++;
	memset(covers->slots,0,sizeof(covers->slots));
}
bool plus_vita_covers_close(PlusVitaCovers *covers)
{
	if(!covers) return true;
	atomic_store(&covers->cancel,true); plus_vita_covers_poll(covers);
	if(covers->running) return false; /* Live worker retains bounded ownership until process exit. */
	free(covers); return true;
}
