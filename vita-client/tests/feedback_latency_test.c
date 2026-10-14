// SPDX-License-Identifier: AGPL-3.0-only
#include "feedback_latency.h"
#include "latency_clock.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t capture_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t capture_cond = PTHREAD_COND_INITIALIZER;
static unsigned int history_count, state_count;
static int16_t last_left_x;
static bool stress, fail_history;

/* Replace transport only. The actual generated/upstream sender, controller
 * diffing, history serialization and pthread lifecycle are exercised. */
ChiakiErrorCode chiaki_takion_send_feedback_state(ChiakiTakion *takion,
	ChiakiSeqNum16 seq, ChiakiFeedbackState *state)
{
	(void)takion; (void)seq;
	pthread_mutex_lock(&capture_mutex);
	if(!state_count || state->left_x != last_left_x)
	{
		state_count++;
		last_left_x = state->left_x;
		if(!stress) printf("state %d %d %d %d\n", state->left_x,
			state->left_y, state->right_x, state->right_y);
	}
	pthread_cond_broadcast(&capture_cond);
	pthread_mutex_unlock(&capture_mutex);
	return CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode chiaki_takion_send_feedback_history(ChiakiTakion *takion,
	ChiakiSeqNum16 seq, uint8_t *payload, size_t size)
{
	(void)takion; (void)seq;
	pthread_mutex_lock(&capture_mutex);
	history_count++;
	if(!stress)
	{
		printf("history");
		for(size_t i = 0; i < size; i++) printf(" %02x", payload[i]);
		putchar('\n');
	}
	bool failed = fail_history;
	fail_history = false;
	pthread_cond_broadcast(&capture_cond);
	pthread_mutex_unlock(&capture_mutex);
	return failed ? CHIAKI_ERR_NETWORK : CHIAKI_ERR_SUCCESS;
}

static void wait_output(unsigned int states, unsigned int histories, int16_t left_x)
{
	struct timespec deadline;
	assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
	deadline.tv_sec += 3;
	pthread_mutex_lock(&capture_mutex);
	while(state_count < states || history_count < histories || last_left_x != left_x)
		assert(pthread_cond_timedwait(&capture_cond, &capture_mutex, &deadline) == 0);
	pthread_mutex_unlock(&capture_mutex);
}

static void publish(ChiakiFeedbackSender *sender, ChiakiControllerState *state)
{
#ifdef PLUS_VITA_INSTRUMENTED
	/* Artificial 2.5ms poll/mapping delay: must survive into timing stats. */
	assert(plus_vita_feedback_sender_set_controller_state_sampled(sender, state,
		plus_vita_latency_now_us() - 2500) == CHIAKI_ERR_SUCCESS);
#else
	assert(chiaki_feedback_sender_set_controller_state(sender, state) == CHIAKI_ERR_SUCCESS);
#endif
}

static void sequence(void)
{
	ChiakiLog log = { 0 };
	ChiakiTakion takion = { .log = &log };
	ChiakiFeedbackSender sender = { 0 };
	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);
	assert(chiaki_feedback_sender_init(&sender, &takion) == CHIAKI_ERR_SUCCESS);
	state.left_x = 1234; publish(&sender, &state); wait_output(1, 0, 1234);
	state.buttons = CHIAKI_CONTROLLER_BUTTON_CROSS;
	publish(&sender, &state); wait_output(1, 1, 1234);
	pthread_mutex_lock(&capture_mutex); fail_history = true; pthread_mutex_unlock(&capture_mutex);
	state.l2_state = 255; state.r2_state = 127;
	publish(&sender, &state); wait_output(1, 3, 1234);
	state.touches[0].id = 0; state.touches[0].x = 400; state.touches[0].y = 300;
	publish(&sender, &state); wait_output(1, 4, 1234);
	state.touches[0].id = 1; state.touches[0].x = 500; state.touches[0].y = 200;
	publish(&sender, &state); wait_output(1, 6, 1234);
	chiaki_controller_state_set_idle(&state);
	publish(&sender, &state); wait_output(2, 10, 0);
	publish(&sender, &state); /* Duplicate must not publish a new update. */
	chiaki_feedback_sender_fini(&sender);
#ifdef PLUS_VITA_INSTRUMENTED
	PlusVitaFeedbackLatency timing;
	plus_vita_feedback_latency_get_stopped(&timing);
	assert(timing.published == 6 && timing.consumed == 6 && timing.coalesced == 0);
	assert(timing.send_failures == 1 && timing.sample_to_publish.count == 6);
	assert(timing.sample_to_publish.max_us >= 2500 && timing.publish_to_wake.count == 6);
	assert(timing.history_send_call.count == 10 && timing.sample_to_history_send.count == 9);
	assert(timing.sample_to_state_send.count == 2 && !timing.sample_to_state_send.invalid);
#endif
}

static void stress_session(void)
{
	stress = true;
	history_count = state_count = 0;
	ChiakiLog log = { 0 };
	ChiakiTakion takion = { .log = &log };
	ChiakiFeedbackSender sender = { 0 };
	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);
	assert(chiaki_feedback_sender_init(&sender, &takion) == CHIAKI_ERR_SUCCESS);
	for(unsigned int i = 0; i < 1000; i++)
	{
		state.left_x = (int16_t)(1 + i % 2);
		publish(&sender, &state);
	}
	state.left_x = 321; publish(&sender, &state);
	wait_output(1, 0, 321);
	chiaki_feedback_sender_fini(&sender);
#ifdef PLUS_VITA_INSTRUMENTED
	PlusVitaFeedbackLatency timing;
	plus_vita_feedback_latency_get_stopped(&timing);
	assert(timing.published == 1001 && timing.published == timing.consumed + timing.coalesced);
	assert(timing.sample_to_publish.count == 1001 && !timing.sample_to_publish.invalid);
	assert(timing.publish_to_wake.count == timing.consumed);
	assert(timing.history_send_call.count == 0 && timing.send_failures == 0);
#endif
}

int main(void)
{
	sequence();
	stress_session();
	puts("feedback: wire sequence, failure observation, coalescing and session reset passed");
	return 0;
}
