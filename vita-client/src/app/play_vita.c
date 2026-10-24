// SPDX-License-Identifier: AGPL-3.0-only
#include "play_vita.h"
#include "../platform/diagnostic_log.h"
#include "../platform/request_cancel.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct plus_vita_play_t {
	pthread_mutex_t mutex;
	pthread_t thread;
	atomic_bool cancelled;
	bool running, complete;
	unsigned stage;
	PlusVitaAppEffect job;
	ChiakiCloudProvisionResult provision;
	ChiakiErrorCode error;
};

static void wipe(void *memory, size_t size)
{
	volatile unsigned char *p=memory;
	while(size--) *p++=0;
}

void plus_vita_play_result_clear(ChiakiCloudProvisionResult *result)
{
	if(!result) return;
	char *private_values[]={result->handshake_key,result->launch_spec,
		result->session_id,result->datacenter_pings,result->error_message};
	for(unsigned i=0;i<sizeof(private_values)/sizeof(private_values[0]);i++)
		if(private_values[i]) wipe(private_values[i],strlen(private_values[i]));
	chiaki_cloud_provision_result_fini(result);
	wipe(result,sizeof(*result));
}

static unsigned fixed_stage(const char *stage)
{
	if(!stage) return 0;
	if(strstr(stage,"Cloud Auth") || strstr(stage,"Authorizing")
		|| strstr(stage,"Getting Client") || strstr(stage,"Getting Configuration")
		|| strstr(stage,"Getting Tokens")) return 1;
	if(strstr(stage,"Resolving Game") || strstr(stage,"License")) return 2;
	if(strstr(stage,"Datacenter") || strstr(stage,"Pinging")) return 3;
	if(strstr(stage,"Allocating") || strstr(stage,"Starting Session")
		|| strstr(stage,"Creating Session")) return 4;
	return 0;
}

static void progress(const char *stage, void *context)
{
	PlusVitaPlay *play=context;
	unsigned next=fixed_stage(stage);
	pthread_mutex_lock(&play->mutex);
	if(next && next!=play->stage) {
		play->stage=next;
		plus_vita_diagnostic_event("allocation.stage",(int)next);
	}
	pthread_mutex_unlock(&play->mutex);
}

static bool cancelled(void *context)
{
	return atomic_load(&((PlusVitaPlay *)context)->cancelled);
}

static void *allocate(void *context)
{
	PlusVitaPlay *play=context;
	const PlusVitaAppEffect *job=&play->job;
	ChiakiCloudProvisionConfig config={
		.service_type="psnow", .game_identifier=job->game.id,
		.game_name="", .npsso=job->credential.value,
		.store_country=job->store_country,
		.store_lang=job->store_language,
		.owned_entitlement_id="", .owned_platform="",
		.catalog_is_foreign=false, .skip_account_attr_check=true,
		.forced_datacenter="Auto", .prior_datacenters_json="",
		.game_language=job->language, .resolution=720, .bitrate_kbps=8000,
		.progress=progress, .is_cancelled=cancelled, .user=play
	};
	ChiakiLog log;
	chiaki_log_init(&log,CHIAKI_LOG_INFO | CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR,
		plus_vita_diagnostic_chiaki_log,NULL);
	plus_vita_diagnostic_event("allocation.begin",0);
	ChiakiErrorCode error=chiaki_cloud_provision_session(&config,&play->provision,&log);
	plus_vita_diagnostic_event("allocation.complete",(int)error);
	pthread_mutex_lock(&play->mutex);
	play->error=error;
	play->complete=true;
	pthread_mutex_unlock(&play->mutex);
	return NULL;
}

PlusVitaPlay *plus_vita_play_create(void)
{
	PlusVitaPlay *play=calloc(1,sizeof(*play));
	if(!play) return NULL;
	if(pthread_mutex_init(&play->mutex,NULL)!=0) { free(play); return NULL; }
	atomic_init(&play->cancelled,false);
	return play;
}

bool plus_vita_play_start(PlusVitaPlay *play,const PlusVitaAppEffect *effect)
{
	if(!play || play->running || !effect || effect->kind!=PLUS_VITA_EFFECT_PLAY
		|| !effect->game.launch_id_valid || !effect->store_country[0]
		|| !effect->store_language[0] || !effect->language[0]
		|| !effect->credential.length) return false;
	play->job=*effect;
	play->stage=0;
	play->complete=false;
	atomic_store(&play->cancelled,false);
	plus_vita_request_cancel_set(false);
	pthread_attr_t attr;
	bool ready=pthread_attr_init(&attr)==0;
	int error=ready ? pthread_attr_setstacksize(&attr,512*1024) : -1;
	if(!error) error=pthread_create(&play->thread,&attr,allocate,play);
	if(ready) pthread_attr_destroy(&attr);
	if(error) { wipe(&play->job,sizeof(play->job)); return false; }
	play->running=true;
	return true;
}

bool plus_vita_play_busy(const PlusVitaPlay *play)
{
	return play && play->running;
}

void plus_vita_play_cancel(PlusVitaPlay *play)
{
	if(!play || !play->running) return;
	atomic_store(&play->cancelled,true);
	plus_vita_request_cancel_set(true);
}

unsigned plus_vita_play_stage(PlusVitaPlay *play)
{
	if(!play || !play->running) return 0;
	pthread_mutex_lock(&play->mutex);
	unsigned stage=play->stage;
	pthread_mutex_unlock(&play->mutex);
	return stage;
}

bool plus_vita_play_ready(PlusVitaPlay *play)
{
	if(!play || !play->running) return false;
	pthread_mutex_lock(&play->mutex);
	bool ready=play->complete;
	pthread_mutex_unlock(&play->mutex);
	return ready;
}

bool plus_vita_play_take(PlusVitaPlay *play,ChiakiCloudProvisionResult *result,
	ChiakiErrorCode *error,uint64_t *operation)
{
	if(!plus_vita_play_ready(play)) return false;
	pthread_join(play->thread,NULL);
	*result=play->provision;
	memset(&play->provision,0,sizeof(play->provision));
	*error=play->error;
	*operation=play->job.operation;
	wipe(&play->job,sizeof(play->job));
	play->running=false;
	return true;
}

bool plus_vita_play_close(PlusVitaPlay *play)
{
	if(!play) return true;
	plus_vita_play_cancel(play);
	if(play->running) {
		ChiakiCloudProvisionResult result={0};
		ChiakiErrorCode error;
		uint64_t operation;
		if(!plus_vita_play_take(play,&result,&error,&operation)) return false;
		plus_vita_play_result_clear(&result);
	}
	pthread_mutex_destroy(&play->mutex);
	wipe(play,sizeof(*play));
	free(play);
	return true;
}
