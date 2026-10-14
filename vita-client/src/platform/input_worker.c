// SPDX-License-Identifier: AGPL-3.0-only
#include "input_worker.h"
#include "latency_clock.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct PlusVitaInputWorker {
	ChiakiFeedbackSender *sender;
	PlusVitaInputRead read;
	void *user;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t thread;
	bool stop;
	PlusVitaInputStatus status;
	PlusVitaInputStats stats; /* Worker-owned until join. */
};

static int wait_until(PlusVitaInputWorker *w, uint64_t deadline_us, bool interruptible)
{
	int result = 0;
	pthread_mutex_lock(&w->mutex);
	while(!interruptible || !w->stop)
	{
		uint64_t now = plus_vita_latency_now_us();
		if(now >= deadline_us) break;
		uint64_t delay = deadline_us - now;
		struct timespec limit;
		if(clock_gettime(CLOCK_REALTIME, &limit) != 0) { result = EINVAL; break; }
		limit.tv_sec += (time_t)(delay / 1000000u);
		limit.tv_nsec += (long)(delay % 1000000u) * 1000;
		if(limit.tv_nsec >= 1000000000) { limit.tv_sec++; limit.tv_nsec -= 1000000000; }
		result = pthread_cond_timedwait(&w->cond, &w->mutex, &limit);
		if(result == ETIMEDOUT) result = 0;
		if(result) break;
	}
	pthread_mutex_unlock(&w->mutex);
	return result;
}

static void release_controls(PlusVitaInputWorker *w)
{
	ChiakiControllerState idle;
	chiaki_controller_state_set_idle(&idle);
	w->stats.neutral_result = chiaki_feedback_sender_set_controller_state(w->sender, &idle);
	if(w->stats.neutral_result != CHIAKI_ERR_SUCCESS) return;
	/* Bound the consumption check. A successful local send is not a remote
	 * acknowledgement. Keep sender/transport alive until after this join. */
	uint64_t deadline = plus_vita_latency_now_us() + 100000u;
	while(plus_vita_latency_now_us() < deadline)
	{
		if(plus_vita_feedback_sender_neutral_consumed(w->sender))
		{ w->stats.neutral_consumed = true; return; }
		if(wait_until(w, plus_vita_latency_now_us() + 2000u, false))
		{ w->stats.neutral_result = CHIAKI_ERR_THREAD; return; }
	}
	w->stats.neutral_result = CHIAKI_ERR_TIMEOUT;
}

static void *input_thread(void *user)
{
	PlusVitaInputWorker *w = user;
	ChiakiControllerState previous;
	chiaki_controller_state_set_idle(&previous);
	uint64_t next = plus_vita_latency_now_us(), last_poll = 0;
	uint64_t chord_since = 0, last_native = 0;
	bool chord_held = false, native_known = false, user_stopped = false;
	ChiakiErrorCode error = CHIAKI_ERR_SUCCESS;
	for(;;)
	{
		if(wait_until(w, next, true)) { error = CHIAKI_ERR_THREAD; break; }
		pthread_mutex_lock(&w->mutex);
		bool stop = w->stop;
		pthread_mutex_unlock(&w->mutex);
		if(stop) break;
		error = plus_vita_feedback_sender_error(w->sender);
		if(error != CHIAKI_ERR_SUCCESS) break;
		uint64_t sampled_us = plus_vita_latency_now_us();
		if(last_poll) plus_vita_latency_span(&w->stats.poll_gap, last_poll, sampled_us);
		last_poll = sampled_us;
		PlusVitaInputSample sample = { 0 };
		int available = w->read(w->user, &previous, &sample);
		if(available < 0) { error = CHIAKI_ERR_UNKNOWN; break; }
		if(!available)
		{
			w->stats.poll_failures++;
			chord_held = false;
		}
		else
		{
			if(native_known && last_native == sample.native_timestamp) w->stats.native_repeats++;
			else w->stats.native_changes++;
			last_native = sample.native_timestamp;
			native_known = true;
			if(sample.stop_chord)
			{
				if(!chord_held) { chord_since = sampled_us; chord_held = true; }
				if(sampled_us - chord_since >= PLUS_VITA_INPUT_STOP_HOLD_US)
				{ user_stopped = true; break; }
			}
			else chord_held = false;
			if(!chiaki_controller_state_equals(&sample.state, &previous))
			{
				uint64_t start = plus_vita_latency_now_us();
				error = plus_vita_feedback_sender_set_controller_state_sampled(w->sender,
					&sample.state, sampled_us);
				plus_vita_latency_span(&w->stats.publish_call, start, plus_vita_latency_now_us());
				if(error != CHIAKI_ERR_SUCCESS) break;
				previous = sample.state;
				w->stats.updates++;
			}
		}
		next += PLUS_VITA_INPUT_PERIOD_US;
		uint64_t now = plus_vita_latency_now_us();
		if(now >= next)
		{
			w->stats.missed_deadlines++;
			next = now + PLUS_VITA_INPUT_PERIOD_US;
		}
	}
	release_controls(w);
	if(error == CHIAKI_ERR_SUCCESS) error = plus_vita_feedback_sender_error(w->sender);
	if(error == CHIAKI_ERR_SUCCESS && w->stats.neutral_result != CHIAKI_ERR_SUCCESS)
		error = w->stats.neutral_result;
	pthread_mutex_lock(&w->mutex);
	w->status = (PlusVitaInputStatus){ .done = true, .user_stopped = user_stopped, .error = error };
	pthread_mutex_unlock(&w->mutex);
	return NULL;
}

ChiakiErrorCode plus_vita_input_worker_start(PlusVitaInputWorker **result,
	ChiakiFeedbackSender *sender, PlusVitaInputRead read, void *user)
{
	if(!result) return CHIAKI_ERR_INVALID_DATA;
	*result = NULL;
	if(!sender || !read) return CHIAKI_ERR_INVALID_DATA;
	PlusVitaInputWorker *w = calloc(1, sizeof(*w));
	if(!w) return CHIAKI_ERR_MEMORY;
	w->sender = sender; w->read = read; w->user = user;
	if(pthread_mutex_init(&w->mutex, NULL)) { free(w); return CHIAKI_ERR_THREAD; }
	if(pthread_cond_init(&w->cond, NULL)) goto error_mutex;
	pthread_attr_t attr;
	if(pthread_attr_init(&attr)) goto error_cond;
	int r = pthread_attr_setstacksize(&attr, 128 * 1024);
	if(!r) r = pthread_create(&w->thread, &attr, input_thread, w);
	pthread_attr_destroy(&attr);
	if(r) goto error_cond;
	*result = w;
	return CHIAKI_ERR_SUCCESS;
error_cond:
	pthread_cond_destroy(&w->cond);
error_mutex:
	pthread_mutex_destroy(&w->mutex);
	free(w);
	return CHIAKI_ERR_THREAD;
}

PlusVitaInputStatus plus_vita_input_worker_status(PlusVitaInputWorker *w)
{
	pthread_mutex_lock(&w->mutex);
	PlusVitaInputStatus result = w->status;
	pthread_mutex_unlock(&w->mutex);
	return result;
}

ChiakiErrorCode plus_vita_input_worker_stop(PlusVitaInputWorker *w,
	PlusVitaInputStats *stats)
{
	if(!w || !stats) return CHIAKI_ERR_INVALID_DATA;
	pthread_mutex_lock(&w->mutex);
	w->stop = true;
	pthread_cond_signal(&w->cond);
	pthread_mutex_unlock(&w->mutex);
	if(pthread_join(w->thread, NULL)) return CHIAKI_ERR_THREAD;
	*stats = w->stats;
	ChiakiErrorCode result = w->status.error;
	pthread_cond_destroy(&w->cond);
	pthread_mutex_destroy(&w->mutex);
	free(w);
	return result;
}
