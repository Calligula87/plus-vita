// SPDX-License-Identifier: AGPL-3.0-only
#include "play_vita.h"
#include "ui_flow.h"
#include "recovery_vita.h"
#include "request_cancel.h"
#include "diagnostic_log.h"
#include "cloudsession_internal.h"
#include "curl_http.h"
#include <psp2/power.h>
#include <psp2/kernel/threadmgr/callback.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static bool blocked, entered;
static long http_status=200;
static ChiakiErrorCode transport=CHIAKI_ERR_SUCCESS;
static unsigned allocations;
static int flags,net_state=3;
static SceKernelCallbackFunction callback;
int sceKernelCreateCallback(const char *name,unsigned attr,SceKernelCallbackFunction fn,void *user)
{ (void)name;(void)attr;(void)user;callback=fn;return 1; }
int sceKernelDeleteCallback(int id) { (void)id;callback=NULL;return 0; }
int sceKernelCheckCallback(void) { if(flags) { int pending=flags;flags=0;callback(0,1,pending,NULL); } return 0; }
int scePowerRegisterCallback(int id) { (void)id;return 0; }
int scePowerUnregisterCallback(int id) { (void)id;return 0; }
bool scePowerIsSuspendRequired(void) { return false; }
int sceNetCtlInetGetState(int *state) { *state=net_state;return 0; }
void plus_vita_diagnostic_event(const char *event,int code) { (void)event;(void)code; }
void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,const char *message,void *user)
{ (void)level;(void)message;(void)user; }
ChiakiErrorCode chiaki_holepunch_generate_client_device_uid(char *buf,size_t *size)
{ assert(*size>=10);strcpy(buf,"synthetic");*size=9;return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode cc_http_make_cookie_header(char **out,const char *name,const char *value)
{ (void)name;(void)value;*out=strdup("synthetic cookie");return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode cc_http_perform(ChiakiLog *log,const CCHttpRequest *request,CCHttpResponse *response)
{
	(void)log; (void)request;
	pthread_mutex_lock(&gate);
	entered=true;pthread_cond_broadcast(&changed);
	while(blocked) pthread_cond_wait(&changed,&gate);
	pthread_mutex_unlock(&gate);
	response->status_code=http_status;
	return transport;
}
void cc_http_response_fini(CCHttpResponse *response) { memset(response,0,sizeof(*response)); }
ChiakiErrorCode cc_kamaji_resolve(ChiakiLog *log,const ChiakiCloudProvisionConfig *cfg,
	const char *uid,char id[128],char platform[8],char **error)
{ (void)log;(void)cfg;(void)uid;(void)error;strcpy(id,"synthetic");strcpy(platform,"ps4");return CHIAKI_ERR_SUCCESS; }
ChiakiErrorCode cc_gaikai_allocate(ChiakiLog *log,const ChiakiCloudProvisionConfig *cfg,
	const char *uid,const char *platform,const char *id,ChiakiCloudProvisionResult *out)
{ (void)log;(void)cfg;(void)uid;(void)platform;(void)id;allocations++;out->session_id=strdup("private-session-canary");return CHIAKI_ERR_SUCCESS; }

static PlusVitaAppEffect choose(PlusVitaApp *app,PlusVitaAppAction action)
{
	PlusVitaAppView view=plus_vita_app_view(app);
	for(unsigned i=0;i<view.action_count;i++) if(view.actions[i].id==action)
		return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,
			.x=view.actions[i].x+1,.y=view.actions[i].y+1});
	assert(!"Expected action");return (PlusVitaAppEffect){0};
}
static PlusVitaApp *connected_app(void)
{
	PlusVitaApp *app=plus_vita_app_create();assert(app);
	PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
	const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
	PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
		.operation=load.operation,.data=json,.data_size=sizeof(json)-1});
	PlusVitaCatalogEntry entry={.name="Synthetic",.id="FAKECUSA1",.launch_id_valid=true};
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,
		.entries=&entry,.entry_count=1,.store_country="CH",.store_language="de",.language="de-CH"});
	choose(app,PLUS_VITA_ACTION_GAME);return app;
}
static void wait_ready(PlusVitaPlay *play)
{
	for(unsigned i=0;i<3000 && !plus_vita_play_ready(play);i++) {
		struct timespec delay={.tv_nsec=1000000};nanosleep(&delay,NULL);
	}
	assert(plus_vita_play_ready(play));
}
int main(void)
{
	/* Match the real adapter order: input, recovery poll, pending local effect,
	 * then its completion. Offline state must not prevent opening setup. */
	PlusVitaApp *offline=plus_vita_app_create();assert(offline);
	PlusVitaUiFlow flow={.app=offline};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE},0);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,
		.status=PLUS_VITA_RESULT_NETWORK_ERROR},0);
	PlusVitaAppEffect setup=plus_vita_ui_flow_take(&flow);
	assert(setup.kind==PLUS_VITA_EFFECT_PREPARE_SETUP);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SETUP_READY,.operation=setup.operation},0);
	assert(plus_vita_app_view(offline).screen==PLUS_VITA_APP_SETUP_GUIDE);
	plus_vita_app_destroy(offline);
	assert(plus_vita_recovery_init());
	assert(plus_vita_recovery_poll()==PLUS_VITA_RECOVERY_NONE);
	flags=SCE_POWER_CB_SYSTEM_RESUME;
	assert(plus_vita_recovery_poll()==PLUS_VITA_RECOVERY_SLEEP);
	assert(plus_vita_recovery_poll()==PLUS_VITA_RECOVERY_SLEEP);
	plus_vita_recovery_acknowledge();
	net_state=0;assert(plus_vita_recovery_poll()==PLUS_VITA_RECOVERY_WIFI);net_state=3;
	const long statuses[]={401,403,429,503};
	const PlusVitaAppResult expected[]={PLUS_VITA_RESULT_REJECTED,PLUS_VITA_RESULT_SERVICE_ERROR,
		PLUS_VITA_RESULT_SERVICE_ERROR,PLUS_VITA_RESULT_SERVICE_ERROR};
	for(unsigned i=0;i<4;i++) {
		PlusVitaApp *app=connected_app();PlusVitaPlay *play=plus_vita_play_create();assert(play);
		PlusVitaAppEffect effect=choose(app,PLUS_VITA_ACTION_PLAY);
		http_status=statuses[i];assert(plus_vita_play_start(play,&effect));wait_ready(play);
		ChiakiCloudProvisionResult result={0};ChiakiErrorCode error;uint64_t operation;
		assert(plus_vita_play_take(play,&result,&error,&operation));
		assert(plus_vita_play_failure(error,&result)==expected[i]);
		PlusVitaAppEffect next=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,
			.operation=operation,.status=plus_vita_play_failure(error,&result)});
		assert(i ? plus_vita_app_view(app).account_connected : next.kind==PLUS_VITA_EFFECT_DELETE_ACCOUNT);
		assert(!allocations);plus_vita_play_result_clear(&result);
		assert(plus_vita_play_close(play));plus_vita_app_destroy(app);
	}
	/* Cancellation while a real preflight response is outstanding cannot
	 * proceed to allocation, even when that response eventually succeeds. */
	PlusVitaApp *app=connected_app();PlusVitaPlay *play=plus_vita_play_create();
	PlusVitaAppEffect effect=choose(app,PLUS_VITA_ACTION_PLAY);
	http_status=200;blocked=true;entered=false;assert(plus_vita_play_start(play,&effect));
	pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&changed,&gate);pthread_mutex_unlock(&gate);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_BACK});
	plus_vita_play_cancel(play);assert(!plus_vita_play_close(play));
	pthread_mutex_lock(&gate);blocked=false;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
	wait_ready(play);ChiakiCloudProvisionResult result={0};ChiakiErrorCode error;uint64_t operation;
	assert(plus_vita_play_take(play,&result,&error,&operation));assert(error==CHIAKI_ERR_CANCELED && !allocations);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_PLAY_ENDED,.operation=operation,.status=PLUS_VITA_RESULT_OK});
	assert(plus_vita_app_view(app).screen==PLUS_VITA_APP_CATALOGUE);
	plus_vita_play_result_clear(&result);assert(plus_vita_play_close(play));plus_vita_app_destroy(app);
	plus_vita_recovery_fini();
	puts("recovery: real preflight status mapping, blocked cancellation, stale results and power/Wi-Fi adapter passed");
}
