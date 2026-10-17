// SPDX-License-Identifier: AGPL-3.0-only
#include "diagnostic_log.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static bool blocked, entered, fail;
static atomic_bool produced;
static bool fail_create;
int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*run)(void *), void *user)
{
	if(fail_create) return EAGAIN;
	return __real_pthread_create(thread, attr, run, user);
}
int __real_fflush(FILE *f);
int __wrap_fflush(FILE *f)
{
	pthread_mutex_lock(&gate);
	if(blocked) { entered = true; pthread_cond_broadcast(&changed); }
	while(blocked) pthread_cond_wait(&changed, &gate);
	bool failing = fail;
	pthread_mutex_unlock(&gate);
	if(failing) { errno = EIO; return EOF; }
	return __real_fflush(f);
}
static void *produce(void *unused)
{
	(void)unused;
	for(unsigned i = 0; i < 2000; i++)
		plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe synthetic queued", NULL);
	plus_vita_diagnostic_event("synthetic critical failure", -1);
	plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe access_token=synthetic-secret", NULL);
	atomic_store(&produced, true);
	return NULL;
}
static void *stop_sink(void *unused) { (void)unused; plus_vita_diagnostic_end(); return NULL; }
int main(void)
{
	assert(plus_vita_diagnostic_begin());
	plus_vita_diagnostic_stream_timing_begin();
	pthread_mutex_lock(&gate); blocked = true; pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_progress("synthetic writer gate");
	pthread_mutex_lock(&gate);
	while(!entered) pthread_cond_wait(&changed, &gate);
	pthread_mutex_unlock(&gate);
	pthread_t producer;
	assert(!pthread_create(&producer, NULL, produce, NULL));
	/* The actual entrypoints must finish even while persistence is blocked. */
	for(unsigned i = 0; i < 1000 && !atomic_load(&produced); i++) {
		struct timespec delay = { .tv_nsec = 1000000 }; nanosleep(&delay, NULL);
	}
	assert(atomic_load(&produced));
	assert(!pthread_join(producer, NULL));
	pthread_mutex_lock(&gate); blocked = false; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&gate);
	PlusVitaLatencyStats writes, waits;
	plus_vita_diagnostic_stream_timing_end(&writes, &waits);
	plus_vita_diagnostic_end();
	FILE *f = fopen("plus-vita-allocation.log", "r"); assert(f);
	char line[2048]; unsigned long last = 0; bool critical = false, redacted = false, overflow = false;
	while(fgets(line, sizeof(line), f)) {
		assert(!strstr(line, "synthetic-secret"));
		unsigned long sequence;
		if(sscanf(line, "%lu", &sequence) == 1) { assert(sequence > last); last = sequence; }
		if(strstr(line, "synthetic critical failure")) critical = true;
		if(strstr(line, "sensitive diagnostic entry omitted")) redacted = true;
		if(strstr(line, "regular_dropped=")) overflow = true;
	}
	fclose(f); assert(critical && redacted && overflow);
	assert(plus_vita_diagnostic_persistence_ok());
	assert(plus_vita_diagnostic_ui_begin());
	for(unsigned i=0;i<500;i++) {
		plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_DRAW, 123);
		plus_vita_diagnostic_ui_sample((PlusVitaUiTiming)999, 999999);
		plus_vita_diagnostic_ui_flush();
	}
	plus_vita_diagnostic_ui_end();
	f = fopen("plus-vita-allocation.log", "r"); assert(f);
	bool ui_build=false, ui_timing=false; size_t bytes=0;
	while(fgets(line,sizeof(line),f)) {
		bytes+=strlen(line);
		assert(!strstr(line,"synthetic critical failure") && !strstr(line,"999999"));
		if(strstr(line,"build=ui-font-response-1")) ui_build=true;
		if(strstr(line,"draw count=500")) ui_timing=true;
	}
	fclose(f); assert(ui_build && ui_timing && bytes < 65536);
	/* Full critical reserve keeps the latest final records, with explicit loss;
	 * producer entrypoints remain callable during a blocked stop/drain. */
	assert(plus_vita_diagnostic_begin());
	pthread_mutex_lock(&gate); blocked = true; entered = false; pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_progress("second gate");
	pthread_mutex_lock(&gate);
	while(!entered) pthread_cond_wait(&changed, &gate);
	pthread_mutex_unlock(&gate);
	char large[2048]; memset(large, 'x', sizeof(large)-1); large[sizeof(large)-1] = 0;
	plus_vita_diagnostic_event(large, 0);
	for(unsigned i = 0; i < 100; i++) plus_vita_diagnostic_event("synthetic critical saturation", -1);
	plus_vita_diagnostic_event("synthetic latest final summary", 0);
	pthread_t stopper; assert(!pthread_create(&stopper, NULL, stop_sink, NULL));
	atomic_store(&produced, false);
	assert(!pthread_create(&producer, NULL, produce, NULL));
	for(unsigned i = 0; i < 1000 && !atomic_load(&produced); i++) {
		struct timespec delay = { .tv_nsec = 1000000 }; nanosleep(&delay, NULL);
	}
	assert(atomic_load(&produced)); assert(!pthread_join(producer, NULL));
	pthread_mutex_lock(&gate); blocked = false; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&gate);
	assert(!pthread_join(stopper, NULL));
	f = fopen("plus-vita-allocation.log", "r"); assert(f);
	bool latest = false, accounted = false;
	while(fgets(line, sizeof(line), f)) {
		if(strstr(line, "synthetic latest final summary")) latest = true;
		if(strstr(line, "sink deferred") && !strstr(line, "critical_dropped=0")
			&& strstr(line, "truncated=1")) accounted = true;
	}
	fclose(f); assert(latest && accounted);
	/* A device call that never finishes cannot keep end waiting indefinitely.
	 * Its worker retains ownership until that call returns; restart is safe. */
	assert(plus_vita_diagnostic_begin());
	pthread_mutex_lock(&gate); blocked = true; entered = false; pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_event("stuck device call", -1);
	pthread_mutex_lock(&gate);
	while(!entered) pthread_cond_wait(&changed, &gate);
	pthread_mutex_unlock(&gate);
	struct timespec before, after; clock_gettime(CLOCK_MONOTONIC, &before);
	plus_vita_diagnostic_end(); clock_gettime(CLOCK_MONOTONIC, &after);
	double elapsed = (after.tv_sec - before.tv_sec) + (after.tv_nsec - before.tv_nsec) / 1e9;
	assert(elapsed >= 1.9 && elapsed < 3.0);
	assert(!plus_vita_diagnostic_persistence_ok());
	plus_vita_diagnostic_event("late producer returns during stuck writer", 0);
	pthread_mutex_lock(&gate); blocked = false; pthread_cond_broadcast(&changed); pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_end();
	fail_create = true; assert(!plus_vita_diagnostic_begin()); fail_create = false;
	plus_vita_diagnostic_event("ignored after failed thread init", 0);
	plus_vita_diagnostic_end();
	assert(plus_vita_diagnostic_begin());
	pthread_mutex_lock(&gate); fail = true; pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_event("synthetic I/O error", -1);
	plus_vita_diagnostic_end();
	assert(!plus_vita_diagnostic_persistence_ok());
	pthread_mutex_lock(&gate); fail = false; pthread_mutex_unlock(&gate);
	assert(plus_vita_diagnostic_begin());
	plus_vita_diagnostic_event("synthetic restarted session", 0);
	plus_vita_diagnostic_end();
	assert(plus_vita_diagnostic_persistence_ok());
	puts("deferred log: real producers return during blocked I/O; overflow, ordering, reserved errors, redaction, failed writer and restart passed");
}
