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
static bool fail_open;
FILE *__real_fopen(const char *,const char *);
FILE *__wrap_fopen(const char *path,const char *mode)
{
	if(fail_open) { errno=ENOSPC; return NULL; }
	return __real_fopen(path,mode);
}
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
	plus_vita_diagnostic_event("allocation.complete", -1);
	plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_INFO, "Stream probe access_token=synthetic-secret", NULL);
	atomic_store(&produced, true);
	return NULL;
}
static void *stop_sink(void *unused) { (void)unused; plus_vita_diagnostic_end(); return NULL; }
static void privacy_entrypoints(void)
{
	assert(plus_vita_diagnostic_begin());
	static const char *const canaries[]={
		"unmarked-canary-credential", "private-label-canary", "192.0.2.42:4242",
		"https://example.invalid/canary?private=1", "Takion arbitrary-canary",
		"Stream probe remote-canary", "allocation.complete\npayload-canary",
		"NPSSO=synthetic-token-canary", "Cookie: synthetic-cookie-canary"
	};
	for(unsigned i=0;i<sizeof(canaries)/sizeof(canaries[0]);i++) {
		plus_vita_diagnostic_event(canaries[i],123456789);
		plus_vita_diagnostic_progress(canaries[i]);
		plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_ERROR,canaries[i],NULL);
		PlusVitaLatencyStats stats={.count=1,.total_us=2,.max_us=2};
		plus_vita_diagnostic_metric(canaries[i],&stats);
		plus_vita_diagnostic_counter(canaries[i],123456789);
	}
	char oversized[8192]; memset(oversized,'Q',sizeof(oversized)-1); oversized[sizeof(oversized)-1]=0;
	plus_vita_diagnostic_event(oversized,123456789);
	plus_vita_diagnostic_progress(oversized);
	plus_vita_diagnostic_chiaki_log(CHIAKI_LOG_WARNING,oversized,NULL);
	plus_vita_diagnostic_event("allocation.complete",-1);
	plus_vita_diagnostic_end();
	FILE *f=fopen("plus-vita-allocation.log","rb"); assert(f);
	char line[2048]; bool complete=false;
	while(fgets(line,sizeof(line),f)) {
		for(unsigned i=0;i<sizeof(canaries)/sizeof(canaries[0]);i++) assert(!strstr(line,canaries[i]));
		assert(!strstr(line,"canary") && !strstr(line,"QQQQ") && !strstr(line,"123456789"));
		if(strstr(line,"allocation.complete")) complete=true;
	}
	assert(!ferror(f)); fclose(f); assert(complete);
}
static void bounded_file(void)
{
	assert(plus_vita_diagnostic_begin());
	PlusVitaLatencyStats stats={.count=3,.total_us=321,.max_us=123};
	/* Flush batches so this measures the whole file, not only queue capacity. */
	for(unsigned batch=0;batch<3000;batch++) {
		for(unsigned i=0;i<8;i++) plus_vita_diagnostic_metric("submitted_first_packet_to_present",&stats);
		struct timespec delay={.tv_nsec=1000000}; nanosleep(&delay,NULL);
	}
	plus_vita_diagnostic_metric("decode",&stats);
	plus_vita_diagnostic_event("stream_probe.complete",6);
	plus_vita_diagnostic_end();
	FILE *f=fopen("plus-vita-allocation.log","rb"); assert(f);
	assert(!fseek(f,0,SEEK_END)); long length=ftell(f);
	assert(length>0 && length<=PLUS_VITA_DIAGNOSTIC_MAX_BYTES);
	rewind(f); char line[2048]; bool final=false,metric=false,limited=false;
	unsigned long records=0,reported_persisted=0;
	while(fgets(line,sizeof(line),f)) {
		unsigned long sequence;
		if(sscanf(line,"%lu",&sequence)==1) records++;
		char *persisted_field=strstr(line," persisted=");
		if(persisted_field) assert(sscanf(persisted_field," persisted=%lu",&reported_persisted)==1);
		if(strstr(line,"stream_probe.complete")) final=true;
		if(strstr(line,"metric=decode count=3 total_us=321 max_us=123")) metric=true;
		if(strstr(line,"budget_dropped=") && !strstr(line,"budget_dropped=0")) limited=true;
	}
	fclose(f); assert(final && metric && limited && records==reported_persisted);
}
int main(void)
{
	privacy_entrypoints();
	bounded_file();
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
		if(strstr(line, "allocation.complete")) critical = true;
		if(strstr(line, "library.info")) redacted = true;
		if(strstr(line, "regular_dropped=")) overflow = true;
	}
	fclose(f); assert(critical && redacted && overflow);
	assert(plus_vita_diagnostic_persistence_ok());
	assert(plus_vita_diagnostic_ui_begin());
	const unsigned artwork[]={2,3,4,5};
	plus_vita_diagnostic_artwork_metadata(artwork);
	plus_vita_diagnostic_artwork_result(PLUS_VITA_ARTWORK_OK);
	plus_vita_diagnostic_artwork_result(PLUS_VITA_ARTWORK_DNS);
	plus_vita_diagnostic_artwork_result((PlusVitaArtworkResult)999999);
	for(unsigned i=0;i<500;i++) {
		plus_vita_diagnostic_ui_sample(PLUS_VITA_UI_DRAW, 123);
		plus_vita_diagnostic_ui_sample((PlusVitaUiTiming)999, 999999);
		plus_vita_diagnostic_ui_flush();
	}
	plus_vita_diagnostic_ui_end();
	f = fopen("plus-vita-allocation.log", "r"); assert(f);
	bool ui_build=false, ui_timing=false, artwork_summary=false; size_t bytes=0;
	while(fgets(line,sizeof(line),f)) {
		bytes+=strlen(line);
		assert(!strstr(line,"allocation.complete") && !strstr(line,"999999"));
		if(strstr(line,"build=recovery-diagnostics-1")) ui_build=true;
		if(strstr(line,"draw count=500")) ui_timing=true;
		if(strstr(line,"metadata_missing=2 available=3 unsupported=4 rejected=5 loaded=1 dns=1")) artwork_summary=true;
	}
	fclose(f); assert(ui_build && ui_timing && artwork_summary && bytes < 65536);
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
	for(unsigned i = 0; i < 100; i++) plus_vita_diagnostic_event("allocation.complete", -1);
	plus_vita_diagnostic_event("stream_probe.complete", 0);
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
		if(strstr(line, "stream_probe.complete")) latest = true;
		if(strstr(line, "sink deferred") && !strstr(line, "critical_dropped=0")
			&& strstr(line, "truncated=0")) accounted = true;
	}
	fclose(f); assert(latest && accounted);
	/* A device call that never finishes cannot keep end waiting indefinitely.
	 * Its worker retains ownership until that call returns; restart is safe. */
	assert(plus_vita_diagnostic_begin());
	pthread_mutex_lock(&gate); blocked = true; entered = false; pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_event("allocation.begin", -1);
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
	plus_vita_diagnostic_event("allocation.complete", -1);
	plus_vita_diagnostic_end();
	assert(!plus_vita_diagnostic_persistence_ok());
	pthread_mutex_lock(&gate); fail = false; pthread_mutex_unlock(&gate);
	assert(plus_vita_diagnostic_begin());
	plus_vita_diagnostic_event("allocation.begin", 0);
	plus_vita_diagnostic_end();
	assert(plus_vita_diagnostic_persistence_ok());
	/* Unavailable/full storage fails closed, including during initial open. */
	fail_open=true;assert(plus_vita_diagnostic_begin());plus_vita_diagnostic_end();
	assert(!plus_vita_diagnostic_persistence_ok());fail_open=false;
	/* Startup I/O is deferred too. A stuck header cannot block begin or
	 * let reset reuse its file/buffers while the old writer still owns them. */
	pthread_mutex_lock(&gate);blocked=true;entered=false;pthread_mutex_unlock(&gate);
	clock_gettime(CLOCK_MONOTONIC,&before);
	assert(plus_vita_diagnostic_begin());
	clock_gettime(CLOCK_MONOTONIC,&after);
	elapsed=(after.tv_sec-before.tv_sec)+(after.tv_nsec-before.tv_nsec)/1e9;
	assert(elapsed<1.0);
	pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&changed,&gate);pthread_mutex_unlock(&gate);
	assert(!plus_vita_diagnostic_persistence_ok());
	assert(!plus_vita_diagnostic_begin());
	plus_vita_diagnostic_event("recovery.sleep",0);
	pthread_mutex_lock(&gate);blocked=false;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
	plus_vita_diagnostic_end();assert(plus_vita_diagnostic_begin());
	plus_vita_diagnostic_event("allocation.begin",0);plus_vita_diagnostic_end();
	f=fopen("plus-vita-allocation.log","rb");assert(f);bool first=false;
	while(fgets(line,sizeof(line),f)) {
		assert(!strstr(line,"recovery.sleep"));
		if(strstr(line,"000001 elapsed_ms=") && strstr(line,"BUILD")) first=true;
	}
	fclose(f);assert(first && plus_vita_diagnostic_persistence_ok());
	puts("deferred log: real producers return during blocked I/O; overflow, ordering, reserved errors, redaction, failed writer and restart passed");
}
