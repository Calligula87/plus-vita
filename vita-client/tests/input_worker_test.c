// SPDX-License-Identifier: AGPL-3.0-only
#include "input_worker.h"
#include "input_vita.h"
#include "latency_clock.h"
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
	/* The caller stands in for the video owner and does no sampling while
	 * blocked. New controls must still reach the actual sender/serialization. */
	pthread_mutex_t video_mutex = PTHREAD_MUTEX_INITIALIZER;
	assert(!pthread_mutex_lock(&video_mutex));
	pthread_mutex_lock(&device_mutex);
	device_pad.lx = 255;
	pthread_mutex_unlock(&device_mutex);
	wait_x(32767);
	assert(!pthread_mutex_unlock(&video_mutex));
	PlusVitaInputStats stats;
	assert(plus_vita_input_worker_stop(worker, &stats) == CHIAKI_ERR_SUCCESS);
	wait_x(0); /* Neutral reaches the sender before it can be destroyed. */
	chiaki_feedback_sender_fini(&sender);
	PlusVitaFeedbackLatency feedback;
	plus_vita_feedback_latency_get_stopped(&feedback);
	assert(stats.updates == 1 && stats.neutral_consumed);
	assert(feedback.published == 2 && feedback.consumed == 2);
	pthread_mutex_destroy(&video_mutex);
	lifecycle_cases();
	puts("input worker: native mappings/contact continuity, real feedback during blocked video and neutral release passed");
	puts("input lifecycle: partial init, missed polls, fatal device, actual send failure, stop chord and restart passed");
	return 0;
}
