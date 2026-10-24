// SPDX-License-Identifier: AGPL-3.0-only
#include "input_worker.h"
#include "input_vita.h"
#include "latency_clock.h"
#include "diagnostic_log.h"
#include "video_decoder_vita.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t device_cond = PTHREAD_COND_INITIALIZER;
static SceCtrlData device_pad;
static SceTouchData device_touch[2];
static unsigned int reads;
static int16_t sent_x;
static unsigned int state_packets;
static int device_mode=1;
static bool fail_send, fatal_device, fail_create;
static unsigned int history_packets;
static pthread_mutex_t boundary_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t boundary_cond = PTHREAD_COND_INITIALIZER;
static bool video_entered, video_released, disk_blocked, disk_entered;

uint64_t sceKernelGetProcessTimeWide(void) { return plus_vita_latency_now_us(); }
int sceAvcdecDecode(SceAvcdecCtrl *ctrl, const SceAvcdecAu *au, SceAvcdecArrayPicture *pictures)
{
	(void)ctrl; assert(au->es.size == 1 && pictures->numOfElm == 1);
	pthread_mutex_lock(&boundary_mutex);
	video_entered = true; pthread_cond_broadcast(&boundary_cond);
	while(!video_released) pthread_cond_wait(&boundary_cond, &boundary_mutex);
	pthread_mutex_unlock(&boundary_mutex);
	pictures->numOfOutput = 0;
	return 0;
}
static void *blocked_video(void *unused)
{
	(void)unused;
	PlusVitaVideoDecoder decoder = { .decoder_created = true, .width = 16, .height = 16 };
	uint8_t packet = 0; uint32_t output[16 * 16]; bool produced = true;
	assert(plus_vita_video_decoder_decode(&decoder, NULL, 0, &packet, 1,
		output, 16, 16, 16, &produced) == 0 && !produced);
	return NULL;
}
int __real_fflush(FILE *file);
int __wrap_fflush(FILE *file)
{
	pthread_mutex_lock(&boundary_mutex);
	if(disk_blocked) { disk_entered = true; pthread_cond_broadcast(&boundary_cond); }
	while(disk_blocked) pthread_cond_wait(&boundary_cond, &boundary_mutex);
	pthread_mutex_unlock(&boundary_mutex);
	return __real_fflush(file);
}
static void *blocked_disk(void *unused)
{ (void)unused; plus_vita_diagnostic_event("allocation.begin", 0); return NULL; }

int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*entry)(void *), void *user)
{
	if(fail_create) return EAGAIN;
	return __real_pthread_create(thread,attr,entry,user);
}

ChiakiErrorCode chiaki_takion_send_feedback_state(ChiakiTakion *takion,
	ChiakiSeqNum16 seq, ChiakiFeedbackState *state)
{
	(void)takion; (void)seq;
	pthread_mutex_lock(&device_mutex);
	sent_x = state->left_x;
	state_packets++;
	bool fail=fail_send; fail_send=false;
	pthread_cond_broadcast(&device_cond);
	pthread_mutex_unlock(&device_mutex);
	if(fail) CHIAKI_LOGE(takion->log, "Takion synthetic network error");
	return fail ? CHIAKI_ERR_NETWORK : CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode chiaki_takion_send_feedback_history(ChiakiTakion *takion,
	ChiakiSeqNum16 seq, uint8_t *payload, size_t size)
{
	(void)takion; (void)seq; (void)payload; (void)size;
	pthread_mutex_lock(&device_mutex); history_packets++; pthread_mutex_unlock(&device_mutex);
	return CHIAKI_ERR_SUCCESS;
}

int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad, int count)
{
	assert(port == 0 && count == 1);
	pthread_mutex_lock(&device_mutex);
	*pad = device_pad;
	reads++;
	int mode=fatal_device ? -1 : device_mode;
	pthread_cond_broadcast(&device_cond);
	pthread_mutex_unlock(&device_mutex);
	return mode;
}

int sceTouchPeek(uint32_t port, SceTouchData *touch, uint32_t count)
{
	assert(port < 2 && count == 1);
	pthread_mutex_lock(&device_mutex);
	*touch = device_touch[port];
	pthread_mutex_unlock(&device_mutex);
	return 1;
}

static void adapter_contract(void)
{
	PlusVitaInputVita vita = { .front_ready=true, .rear_ready=true,
		.rear_mid_x=960, .rear_mid_y=544,
		.front_info={ .minAaX=0, .minAaY=0, .maxAaX=1920, .maxAaY=1088 } };
	device_pad = (SceCtrlData){ .timeStamp=123, .lx=0,.ly=255,.rx=128,.ry=140,
		.buttons=SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE | SCE_CTRL_TRIANGLE
			| SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT
			| SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER | SCE_CTRL_START | SCE_CTRL_SELECT };
	device_touch[1].reportNum=4;
	device_touch[1].report[0]=(SceTouchReport){.x=0,.y=0};
	device_touch[1].report[1]=(SceTouchReport){.x=1919,.y=0};
	device_touch[1].report[2]=(SceTouchReport){.x=0,.y=1087};
	device_touch[1].report[3]=(SceTouchReport){.x=1919,.y=1087};
	device_touch[0].reportNum=2;
	device_touch[0].report[0]=(SceTouchReport){.id=4,.x=1920,.y=1088};
	device_touch[0].report[1]=(SceTouchReport){.id=7,.x=0,.y=0};
	ChiakiControllerState previous;
	chiaki_controller_state_set_idle(&previous);
	PlusVitaInputSample sample;
	assert(plus_vita_input_vita_read(&vita,&previous,&sample)==1);
	assert(sample.stop_chord && sample.native_timestamp==123);
	assert(sample.state.left_x==-32768 && sample.state.left_y==32767);
	assert(sample.state.right_x==0 && sample.state.right_y==3212);
	assert(sample.state.l2_state==255 && sample.state.r2_state==255);
	uint32_t expected = CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_MOON
		| CHIAKI_CONTROLLER_BUTTON_BOX | CHIAKI_CONTROLLER_BUTTON_PYRAMID
		| CHIAKI_CONTROLLER_BUTTON_DPAD_UP | CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN
		| CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT | CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT
		| CHIAKI_CONTROLLER_BUTTON_L1 | CHIAKI_CONTROLLER_BUTTON_R1
		| CHIAKI_CONTROLLER_BUTTON_L3 | CHIAKI_CONTROLLER_BUTTON_R3
		| CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
	assert(sample.state.buttons==expected); /* Exit chord never becomes Options/Share. */
	assert(sample.state.touches[0].id==4 && sample.state.touches[0].x==1920
		&& sample.state.touches[0].y==942 && sample.state.touches[1].id==7);
	previous=sample.state;
	device_touch[0].report[0]=(SceTouchReport){.id=7,.x=960,.y=544};
	device_touch[0].report[1]=(SceTouchReport){.id=9,.x=0,.y=0};
	assert(plus_vita_input_vita_read(&vita,&previous,&sample)==1);
	assert(sample.state.touches[1].id==7 && sample.state.touches[1].x==960
		&& sample.state.touches[1].y==471 && sample.state.touches[0].id==9);
	previous=sample.state;
	device_touch[0].reportNum=device_touch[1].reportNum=0;
	device_pad.buttons=SCE_CTRL_START;
	assert(plus_vita_input_vita_read(&vita,&previous,&sample)==1);
	assert(!sample.stop_chord && sample.state.buttons==CHIAKI_CONTROLLER_BUTTON_OPTIONS);
	assert(!sample.state.l2_state && !sample.state.r2_state);
	assert(sample.state.touches[0].id<0 && sample.state.touches[1].id<0);
}

static void wait_x(int16_t expected)
{
	struct timespec limit;
	assert(!clock_gettime(CLOCK_REALTIME, &limit));
	limit.tv_sec += 3;
	pthread_mutex_lock(&device_mutex);
	while(!state_packets || sent_x != expected)
		assert(!pthread_cond_timedwait(&device_cond, &device_mutex, &limit));
	pthread_mutex_unlock(&device_mutex);
}

static void wait_reads(unsigned int count)
{
	struct timespec limit;
	assert(!clock_gettime(CLOCK_REALTIME,&limit)); limit.tv_sec+=3;
	pthread_mutex_lock(&device_mutex);
	while(reads<count) assert(!pthread_cond_timedwait(&device_cond,&device_mutex,&limit));
	pthread_mutex_unlock(&device_mutex);
}

static int scripted_read(void *user, const ChiakiControllerState *previous,
	PlusVitaInputSample *sample)
{
	return plus_vita_input_vita_read(user,previous,sample);
}

static PlusVitaInputStatus wait_done(PlusVitaInputWorker *worker)
{
	uint64_t deadline=plus_vita_latency_now_us()+5000000;
	for(;;)
	{
		PlusVitaInputStatus status=plus_vita_input_worker_status(worker);
		if(status.done) return status;
		assert(plus_vita_latency_now_us()<deadline);
		struct timespec delay={.tv_nsec=1000000}; nanosleep(&delay,NULL);
	}
}

static void send_failure_during_blocked_disk(void)
{
	assert(plus_vita_diagnostic_begin());
	pthread_mutex_lock(&boundary_mutex); disk_blocked = true; disk_entered = false; pthread_mutex_unlock(&boundary_mutex);
	pthread_t disk; assert(!pthread_create(&disk, NULL, blocked_disk, NULL));
	pthread_mutex_lock(&boundary_mutex);
	while(!disk_entered) pthread_cond_wait(&boundary_cond, &boundary_mutex);
	pthread_mutex_unlock(&boundary_mutex);
	ChiakiLog log; chiaki_log_init(&log, CHIAKI_LOG_ALL, plus_vita_diagnostic_chiaki_log, NULL);
	ChiakiTakion takion = { .log = &log }; ChiakiFeedbackSender sender = { 0 };
	device_pad = (SceCtrlData){ .timeStamp=42, .lx=255, .ly=128, .rx=128, .ry=128 };
	fail_send = true; fatal_device = false; device_mode = 1;
	assert(chiaki_feedback_sender_init(&sender, &takion) == CHIAKI_ERR_SUCCESS);
	PlusVitaInputVita vita = { 0 }; PlusVitaInputWorker *worker = NULL;
	assert(plus_vita_input_worker_start(&worker, &sender, plus_vita_input_vita_read, &vita) == CHIAKI_ERR_SUCCESS);
	assert(wait_done(worker).error == CHIAKI_ERR_NETWORK);
	PlusVitaInputStats stats;
	assert(plus_vita_input_worker_stop(worker, &stats) == CHIAKI_ERR_NETWORK && stats.neutral_consumed);
	chiaki_feedback_sender_fini(&sender);
	PlusVitaFeedbackLatency feedback; plus_vita_feedback_latency_get_stopped(&feedback);
	assert(feedback.send_failures == 1 && feedback.suppressed_sender_logs >= 2);
	pthread_mutex_lock(&boundary_mutex); disk_blocked = false; pthread_cond_broadcast(&boundary_cond); pthread_mutex_unlock(&boundary_mutex);
	assert(!pthread_join(disk, NULL)); plus_vita_diagnostic_end();
}

static void lifecycle_cases(void)
{
	for(unsigned int scenario=0;scenario<4;scenario++)
	{
		device_pad=(SceCtrlData){.timeStamp=42,.lx=128,.ly=128,.rx=128,.ry=128};
		device_mode=1; fatal_device=fail_send=false; reads=state_packets=history_packets=0;
		ChiakiLog log={0}; ChiakiTakion takion={.log=&log}; ChiakiFeedbackSender sender={0};
		assert(chiaki_feedback_sender_init(&sender,&takion)==CHIAKI_ERR_SUCCESS);
		PlusVitaInputVita vita={0}; PlusVitaInputWorker *worker=NULL;
		if(scenario==0)
		{
			assert(plus_vita_input_worker_start(&worker,&sender,NULL,&vita)==CHIAKI_ERR_INVALID_DATA && !worker);
			fail_create=true;
			assert(plus_vita_input_worker_start(&worker,&sender,scripted_read,&vita)==CHIAKI_ERR_THREAD && !worker);
			fail_create=false;
			device_mode=0;
		}
		if(scenario==3) device_pad.buttons=SCE_CTRL_START|SCE_CTRL_SELECT;
		assert(plus_vita_input_worker_start(&worker,&sender,scripted_read,&vita)==CHIAKI_ERR_SUCCESS);
		wait_reads(3);
		PlusVitaInputStats stats;
		if(scenario==0)
		{
			assert(!plus_vita_input_worker_status(worker).done);
			pthread_mutex_lock(&device_mutex); device_mode=1; device_pad.lx=255; pthread_mutex_unlock(&device_mutex);
			wait_x(32767);
			assert(plus_vita_input_worker_stop(worker,&stats)==CHIAKI_ERR_SUCCESS);
			assert(stats.poll_failures>=3 && stats.native_repeats+stats.native_changes>0);
		}
		else if(scenario==1)
		{
			pthread_mutex_lock(&device_mutex); device_pad.lx=255; pthread_mutex_unlock(&device_mutex); wait_x(32767);
			pthread_mutex_lock(&device_mutex); fatal_device=true; pthread_mutex_unlock(&device_mutex);
			assert(wait_done(worker).error==CHIAKI_ERR_UNKNOWN);
			assert(plus_vita_input_worker_stop(worker,&stats)==CHIAKI_ERR_UNKNOWN);
		}
		else if(scenario==2)
		{
			pthread_mutex_lock(&device_mutex); fail_send=true; device_pad.lx=255; pthread_mutex_unlock(&device_mutex);
			assert(wait_done(worker).error==CHIAKI_ERR_NETWORK);
			assert(plus_vita_input_worker_stop(worker,&stats)==CHIAKI_ERR_NETWORK);
		}
		else
		{
			uint64_t start=plus_vita_latency_now_us();
			PlusVitaInputStatus status=wait_done(worker);
			assert(status.user_stopped && status.error==CHIAKI_ERR_SUCCESS);
			assert(plus_vita_latency_now_us()-start>=1800000);
			assert(plus_vita_input_worker_stop(worker,&stats)==CHIAKI_ERR_SUCCESS);
			assert(history_packets==0); /* Chord never leaks Options or Share. */
		}
		assert(stats.neutral_consumed);
		chiaki_feedback_sender_fini(&sender);
	}
}

int main(void)
{
	adapter_contract();
	ChiakiLog log = { 0 };
	ChiakiTakion takion = { .log = &log };
	ChiakiFeedbackSender sender = { 0 };
	device_pad=(SceCtrlData){.timeStamp=123,.lx=128,.ly=128,.rx=128,.ry=128};
	reads=0;
	assert(chiaki_feedback_sender_init(&sender, &takion) == CHIAKI_ERR_SUCCESS);
	PlusVitaInputWorker *worker = NULL;
	PlusVitaInputVita vita={0};
	assert(plus_vita_input_worker_start(&worker, &sender, plus_vita_input_vita_read, &vita) == CHIAKI_ERR_SUCCESS);
	/* Stall the actual production decoder at its native SDK boundary. */
	pthread_t video; assert(!pthread_create(&video, NULL, blocked_video, NULL));
	pthread_mutex_lock(&boundary_mutex);
	while(!video_entered) pthread_cond_wait(&boundary_cond, &boundary_mutex);
	pthread_mutex_unlock(&boundary_mutex);
	pthread_mutex_lock(&device_mutex);
	device_pad.lx = 255;
	pthread_mutex_unlock(&device_mutex);
	wait_x(32767);
	PlusVitaInputStats stats;
	/* A terminal stop also completes while the synchronous decoder is stalled. */
	assert(plus_vita_input_worker_stop(worker, &stats) == CHIAKI_ERR_SUCCESS);
	wait_x(0); /* Neutral reaches the sender before it can be destroyed. */
	chiaki_feedback_sender_fini(&sender);
	PlusVitaFeedbackLatency feedback;
	plus_vita_feedback_latency_get_stopped(&feedback);
	assert(stats.updates == 1 && stats.neutral_consumed);
	assert(feedback.published == 2 && feedback.consumed == 2);
	pthread_mutex_lock(&boundary_mutex); video_released = true; pthread_cond_broadcast(&boundary_cond); pthread_mutex_unlock(&boundary_mutex);
	assert(!pthread_join(video, NULL));
	lifecycle_cases();
	send_failure_during_blocked_disk();
	puts("input worker: native mappings/contact continuity, real feedback during blocked video and neutral release passed");
	puts("input lifecycle: partial init, missed polls, fatal device, actual send failure, stop chord and restart passed");
	return 0;
}
