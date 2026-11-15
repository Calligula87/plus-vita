// SPDX-License-Identifier: AGPL-3.0-only
#include "play_vita.h"
#include "ui_flow.h"
#include "attempt_log.h"
#include "recovery_vita.h"
#include "request_cancel.h"
#include "diagnostic_log.h"
#include "stream_info.h"
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
#include "pb_utils.h"
#include "takion.pb.h"

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

static void offline_attempt_log(bool retry)
{
	assert(plus_vita_diagnostic_ui_begin());
	plus_vita_diagnostic_event("recovery.sleep",0);
	plus_vita_diagnostic_ui_end();
	PlusVitaApp *app;
	if(!retry) app=connected_app();
	else {
		app=plus_vita_app_create();assert(app);
		PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
		const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
		PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
			.operation=load.operation,.data=json,.data_size=sizeof(json)-1});
		plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,
			.operation=auth.operation,.status=PLUS_VITA_RESULT_NETWORK_ERROR});
	}
	PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE},0);
	assert(!plus_vita_attempt_log_prepare(&flow,false)); /* Earlier worker still owns output. */
	assert(plus_vita_attempt_log_prepare(&flow,true));
	assert(!plus_vita_attempt_log_prepare(&flow,true)); /* Exactly once for this action. */
	plus_vita_diagnostic_event("recovery.wifi",0);
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_INTERRUPTED,.status=PLUS_VITA_RESULT_NETWORK_ERROR},0);
	assert(plus_vita_ui_flow_take(&flow).kind==PLUS_VITA_EFFECT_NONE);
	plus_vita_diagnostic_ui_end();
	FILE *file=fopen("plus-vita-allocation.log","rb");assert(file);
	char line[2048];bool first=false,failed=false;
	while(fgets(line,sizeof(line),file)) {
		assert(!strstr(line,"recovery.sleep"));
		if(strstr(line,"000001 elapsed_ms=") && strstr(line,"BUILD")) first=true;
		if(strstr(line,"recovery.wifi")) failed=true;
	}
	fclose(file);assert(first && failed);plus_vita_app_destroy(app);
}

static PlusVitaStreamStartup startup_failure;

static void live_header_guards(ChiakiLog *log)
{
	const uint8_t ps3_header[] = {
		0,0,0,1,0x67,0x64,0x00,0x29,0xac,0xb1,0x40,0x28,0x02,0xdd,0x21,0x06,
		0x06,0x06,0x40,0x00,0x00,0x03,0x00,0x40,0x00,0x00,0x1e,0x23,0xc6,0x0c,0x8a,0x80,
		0,0,0,1,0x68,0xef,0x3c,0xb0,
	};
	PlusVitaStreamInfo info = { .video_header_size = sizeof(ps3_header) };
	memcpy(info.video_header, ps3_header, sizeof(ps3_header));
	PlusVitaH264Guard guard = { 0 };
	uint8_t patched[64];
	size_t size;
	assert(plus_vita_stream_info_guard_header(&info, log, true, &guard, patched, sizeof(patched), &size));
	assert(guard.ps3);
	assert(!plus_vita_stream_info_guard_header(&info, log, false, &guard, patched, sizeof(patched), &size));
	const uint8_t ps4_header[] = {
		0,0,0,1,0x67,0x4d,0x40,0x28,0x91,0x8a,0x01,0x40,0x16,0xec,0x05,0xa8,0x08,
		0x08,0x0a,0x00,0x00,0x0f,0xa4,0x00,0x07,0x53,0x01,0x13,0x43,0xc7,0xc5,0x40,
		0,0,0,1,0x68,0xee,0x3c,0x80,
	};
	memcpy(info.video_header, ps4_header, sizeof(ps4_header));
	info.video_header_size = sizeof(ps4_header);
	assert(plus_vita_stream_info_guard_header(&info, log, false, &guard, patched, sizeof(patched), &size));
	assert(!guard.ps3);
	assert(!plus_vita_stream_info_guard_header(&info, log, true, &guard, patched, sizeof(patched), &size));
}

static bool encode_resolution(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
	return pb_encode_tag_for_field(stream, field)
		&& pb_encode_submessage(stream, tkproto_ResolutionPayload_fields, *arg);
}

static void receive_streaminfo(ChiakiLog *log, const char *private_reason)
{
	uint8_t other_sps_pps[] = {
		0,0,0,1,0x67,0x4d,0x40,0x28,0x91,0x8a,0x01,0x40,0x16,0xec,0x05,0xa8,0x08,
		0x08,0x0a,0x00,0x00,0x0f,0xa4,0x00,0x07,0x53,0x01,0x13,0x43,0xc7,0xc5,0x40,
		0,0,0,1,0x68,0xee,0x3c,0x81
	};
	uint8_t invalid_sps[] = {0,0,0,1,0x67,0x4d};
	uint8_t audio_bytes[CHIAKI_AUDIO_HEADER_SIZE] = {0};
	ChiakiPBBuf audio = { .size=sizeof(audio_bytes), .buf=audio_bytes };
	ChiakiPBBuf video = { .size=sizeof(other_sps_pps), .buf=other_sps_pps };
	tkproto_ResolutionPayload resolution = { .width=1280, .height=720,
		.video_header={ .arg=&video, .funcs.encode=chiaki_pb_encode_buf } };
	tkproto_TakionMessage msg = { .type=tkproto_TakionMessage_PayloadType_STREAMINFO,
		.has_stream_info_payload=true };
	msg.stream_info_payload.resolution.arg = &resolution;
	msg.stream_info_payload.resolution.funcs.encode = encode_resolution;
	msg.stream_info_payload.audio_header.arg = &audio;
	msg.stream_info_payload.audio_header.funcs.encode = chiaki_pb_encode_buf;
	switch(startup_failure) {
		case PLUS_VITA_STREAM_STARTUP_STREAMINFO_INTERRUPTED:
			msg = (tkproto_TakionMessage){ .type=tkproto_TakionMessage_PayloadType_DISCONNECT,
				.has_disconnect_payload=true };
			msg.disconnect_payload.reason.arg = (void *)private_reason;
			msg.disconnect_payload.reason.funcs.encode = chiaki_pb_encode_string;
			break;
		case PLUS_VITA_STREAM_STARTUP_STREAMINFO_INVALID:
			audio.size--;
			break;
		case PLUS_VITA_STREAM_STARTUP_PROFILE_UNAVAILABLE:
			resolution.width = 960;
			resolution.height = 540;
			break;
		case PLUS_VITA_STREAM_STARTUP_VIDEO_HEADER_INVALID:
			video = (ChiakiPBBuf){ .size=sizeof(invalid_sps), .buf=invalid_sps };
			break;
		default:
			assert(startup_failure==PLUS_VITA_STREAM_STARTUP_HEADER_GUARD_REJECTED);
			break;
	}
	uint8_t bytes[128];
	pb_ostream_t encoded = pb_ostream_from_buffer(bytes, sizeof(bytes));
	assert(pb_encode(&encoded, tkproto_TakionMessage_fields, &msg));
	PlusVitaStreamInfo info;
	bool accepted = plus_vita_stream_info_decode(bytes, encoded.bytes_written, log, &info);
	if(startup_failure==PLUS_VITA_STREAM_STARTUP_HEADER_GUARD_REJECTED) {
		assert(accepted);
		uint8_t patched[PLUS_VITA_STREAM_VIDEO_HEADER_MAX];
		size_t size;
		PlusVitaH264Guard guard = { 0 };
		assert(!plus_vita_stream_info_guard_header(&info, log, false, &guard, patched, sizeof(patched), &size));
	} else {
		assert(!accepted);
		assert(info.disconnected==(startup_failure==PLUS_VITA_STREAM_STARTUP_STREAMINFO_INTERRUPTED));
	}
}

ChiakiErrorCode plus_vita_stream_probe_with_pump(const ChiakiCloudProvisionResult *provision,
	ChiakiLog *log, bool *user_stopped, bool (*connect_pump)(void *), void *pump_user,
	PlusVitaStreamInput *input)
{
	(void)user_stopped;
	(void)connect_pump;
	(void)pump_user;
	(void)input;
	plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_TRANSPORT);
	CHIAKI_LOGE(log, "%s\nstream.startup.ready", provision->session_id);
	plus_vita_diagnostic_event("stream.startup.ready", 0);
	plus_vita_diagnostic_stream_startup((PlusVitaStreamStartup)-1);
	plus_vita_diagnostic_stream_startup((PlusVitaStreamStartup)123456789);
	switch(startup_failure) {
		case PLUS_VITA_STREAM_STARTUP_TRANSPORT_FAILED:
		case PLUS_VITA_STREAM_STARTUP_HANDSHAKE_FAILED:
		case PLUS_VITA_STREAM_STARTUP_DECODER_INIT_FAILED:
			/* These are supplied by the native transport and decoder on Vita. */
			plus_vita_diagnostic_stream_startup(startup_failure);
			break;
		default:
			plus_vita_diagnostic_stream_startup(PLUS_VITA_STREAM_STARTUP_STREAMINFO);
			receive_streaminfo(log, provision->session_id);
			break;
	}
	return CHIAKI_ERR_INVALID_RESPONSE;
}

static void play_startup_attempt_log(PlusVitaStreamStartup status, const char *failure)
{
	startup_failure=status;
	PlusVitaApp *app=connected_app();
	PlusVitaPlay *play=plus_vita_play_create();assert(play);
	PlusVitaUiFlow flow={.app=app};
	plus_vita_ui_flow_event(&flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE},0);
	assert(plus_vita_attempt_log_prepare(&flow,true));
	PlusVitaAppEffect effect=plus_vita_ui_flow_take(&flow);
	assert(effect.kind==PLUS_VITA_EFFECT_PLAY);
	assert(plus_vita_play_start(play,&effect));
	plus_vita_credential_clear(&effect.credential);
	wait_ready(play);
	ChiakiCloudProvisionResult provision={0};
	ChiakiErrorCode error;
	uint64_t operation;
	assert(plus_vita_play_take(play,&provision,&error,&operation));
	assert(error==CHIAKI_ERR_SUCCESS && operation==effect.operation);
	bool user_stopped=false;
	assert(plus_vita_play_stream(&provision,&user_stopped,NULL,NULL,NULL)==CHIAKI_ERR_INVALID_RESPONSE);
	plus_vita_diagnostic_ui_end();
	plus_vita_play_result_clear(&provision);
	assert(plus_vita_play_close(play));
	plus_vita_app_destroy(app);

	const char *const expected[]={"code=0 allocation.complete", "code=0 stream_probe.begin",
		"code=0 stream.startup.transport", failure, "code=16 stream_probe.complete"};
	FILE *file=fopen("plus-vita-allocation.log","rb");assert(file);
	char line[2048];
	size_t next=0;
	while(fgets(line,sizeof(line),file)) {
		assert(!strstr(line,"private-session-canary") && !strstr(line,"stream.startup.ready")
			&& !strstr(line,"123456789"));
		if(next<sizeof(expected)/sizeof(expected[0]) && strstr(line,expected[next])) next++;
	}
	assert(!ferror(file) && next==sizeof(expected)/sizeof(expected[0]));
	fclose(file);
}
int main(void)
{
	ChiakiLog log = { 0 };
	live_header_guards(&log);
	offline_attempt_log(false);
	offline_attempt_log(true);
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
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_TRANSPORT_FAILED, "code=5 stream.startup.transport_failed");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_HANDSHAKE_FAILED, "code=6 stream.startup.handshake_failed");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_STREAMINFO_INTERRUPTED, "code=7 stream.startup.streaminfo_interrupted");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_STREAMINFO_INVALID, "code=8 stream.startup.streaminfo_invalid");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_PROFILE_UNAVAILABLE, "code=9 stream.startup.profile_unavailable");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_VIDEO_HEADER_INVALID, "code=11 stream.startup.video_header_invalid");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_HEADER_GUARD_REJECTED, "code=12 stream.startup.header_guard_rejected");
	play_startup_attempt_log(PLUS_VITA_STREAM_STARTUP_DECODER_INIT_FAILED, "code=13 stream.startup.decoder_init_failed");
	plus_vita_recovery_fini();
	puts("recovery: preflight, cancellation, power/Wi-Fi and persisted Play startup diagnostics passed");
}
