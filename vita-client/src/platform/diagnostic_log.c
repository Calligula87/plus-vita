#include "diagnostic_log.h"
#include "latency_clock.h"
#include "build_identity.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#ifndef PLUS_VITA_DEFERRED_LOG
#define PLUS_VITA_DEFERRED_LOG 0
#endif

#ifdef __vita__
#define PLUS_VITA_DIAGNOSTIC_PATH "ux0:data/plus-vita-allocation.log"
#else
#define PLUS_VITA_DIAGNOSTIC_PATH "plus-vita-allocation.log"
#endif

static FILE *diagnostic_file;
static unsigned long diagnostic_sequence;
static pthread_mutex_t diagnostic_write_mutex = PTHREAD_MUTEX_INITIALIZER;
/* begin/end own the file lifetime. Producers never acquire this mutex in the
 * deferred build; the queue mutex is never held across file I/O. */
static pthread_mutex_t diagnostic_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool diagnostic_stream_timing;
static PlusVitaLatencyStats diagnostic_write_call, diagnostic_mutex_wait;
static bool diagnostic_io_ok;
static uint64_t diagnostic_started_us;
#define RECORD_SIZE 1024u
#define TAIL_CAPACITY 64u
#define BODY_LIMIT (PLUS_VITA_DIAGNOSTIC_MAX_BYTES - (TAIL_CAPACITY + 2u) * RECORD_SIZE)
static char diagnostic_tail[TAIL_CAPACITY][RECORD_SIZE];
static unsigned tail_head, tail_count;
static size_t file_bytes;
static unsigned long budget_dropped;

/* Writer-owned, or called under the write mutex in the synchronous profile.
 * Keep the newest terminal/error records after the body quota is exhausted. */
static bool persist_record(const char *line, bool critical)
{
	size_t length = strlen(line);
	if(file_bytes + length > BODY_LIMIT) {
		budget_dropped++;
		if(critical) {
			if(tail_count == TAIL_CAPACITY) { tail_head=(tail_head+1)%TAIL_CAPACITY; tail_count--; }
			memcpy(diagnostic_tail[(tail_head+tail_count)%TAIL_CAPACITY],line,length+1);
			tail_count++;
		}
		return true;
	}
	file_bytes += length;
	return fputs(line,diagnostic_file)>=0 && fflush(diagnostic_file)==0;
}
#if PLUS_VITA_INPUT_WORKER
static pthread_mutex_t feedback_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t feedback_log_thread;
static bool feedback_log_active;
static uint64_t feedback_logs_suppressed;

void plus_vita_diagnostic_feedback_begin(void)
{
	pthread_mutex_lock(&feedback_log_mutex);
	feedback_log_thread = pthread_self();
	feedback_logs_suppressed = 0;
	feedback_log_active = true;
	pthread_mutex_unlock(&feedback_log_mutex);
}

uint64_t plus_vita_diagnostic_feedback_end(void)
{
	pthread_mutex_lock(&feedback_log_mutex);
	uint64_t count = feedback_logs_suppressed;
	feedback_log_active = false;
	pthread_mutex_unlock(&feedback_log_mutex);
	return count;
}

static bool feedback_diagnostic_suppressed(void)
{
	pthread_mutex_lock(&feedback_log_mutex);
	bool suppressed = feedback_log_active && pthread_equal(feedback_log_thread, pthread_self());
	if(suppressed && feedback_logs_suppressed != UINT64_MAX) feedback_logs_suppressed++;
	pthread_mutex_unlock(&feedback_log_mutex);
	return suppressed;
}
#endif

#if PLUS_VITA_DEFERRED_LOG
#define REGULAR_CAPACITY 128u
#define CRITICAL_CAPACITY 64u
#define DRAIN_BUDGET_US 2000000u
typedef struct { unsigned long sequence; bool critical; char line[RECORD_SIZE]; } DiagnosticRecord;
typedef struct {
	DiagnosticRecord *records;
	unsigned head, count, capacity;
} DiagnosticQueue;
static DiagnosticRecord regular_records[REGULAR_CAPACITY], critical_records[CRITICAL_CAPACITY];
static DiagnosticQueue regular_queue, critical_queue;
static pthread_cond_t diagnostic_pending = PTHREAD_COND_INITIALIZER;
static pthread_t diagnostic_thread;
static bool diagnostic_accepting, diagnostic_stopping, diagnostic_thread_started, diagnostic_thread_finished;
static bool diagnostic_timed_out;
static uint64_t diagnostic_drain_deadline;
static unsigned long regular_dropped, critical_dropped, truncated, drain_dropped, persisted;
static PlusVitaLatencyStats enqueue_call, persistence_call;
static void diagnostic_finish_file(void);

static bool critical_record(const char *kind, const char *message, int code)
{
	return strcmp(kind, "EVENT") == 0 || strcmp(kind, "STAGE") == 0
		|| strcmp(kind,"METRIC")==0 || strcmp(kind,"COUNTER")==0 || strcmp(kind,"BUILD")==0
		|| (strcmp(kind, "LIB") == 0 && (code & (CHIAKI_LOG_ERROR | CHIAKI_LOG_WARNING)))
		|| strncmp(message,"final",5)==0;
}

static void *diagnostic_writer(void *unused)
{
	(void)unused;
	/* Storage can stall even during open/header writes. The UI publishes only
	 * this bounded worker; all device I/O belongs to it. */
	FILE *opened=fopen(PLUS_VITA_DIAGNOSTIC_PATH,"w");
	bool storage_ok=opened && fputs("plus-vita structured diagnostic v2\n",opened)>=0 && fflush(opened)==0;
	pthread_mutex_lock(&diagnostic_write_mutex);
	diagnostic_file=opened;
	diagnostic_io_ok=storage_ok && !diagnostic_timed_out;
	pthread_mutex_unlock(&diagnostic_write_mutex);
	for(;;) {
		pthread_mutex_lock(&diagnostic_write_mutex);
		while(!diagnostic_stopping && !regular_queue.count && !critical_queue.count)
			pthread_cond_wait(&diagnostic_pending, &diagnostic_write_mutex);
		if(diagnostic_stopping && plus_vita_latency_now_us() >= diagnostic_drain_deadline) {
			drain_dropped += regular_queue.count + critical_queue.count;
			regular_queue.count = critical_queue.count = 0;
		}
		if(!regular_queue.count && !critical_queue.count && diagnostic_stopping) {
			pthread_mutex_unlock(&diagnostic_write_mutex); break;
		}
		DiagnosticQueue *q = &regular_queue;
		if(!q->count || (critical_queue.count && critical_queue.records[critical_queue.head].sequence
			< q->records[q->head].sequence)) q = &critical_queue;
		DiagnosticRecord record = q->records[q->head];
		q->head = (q->head + 1) % q->capacity; q->count--;
		pthread_mutex_unlock(&diagnostic_write_mutex);
		uint64_t started = plus_vita_latency_now_us();
		bool ok = storage_ok && persist_record(record.line, record.critical);
		storage_ok=storage_ok && ok;
		uint64_t done = plus_vita_latency_now_us();
		pthread_mutex_lock(&diagnostic_write_mutex);
		diagnostic_io_ok = diagnostic_io_ok && ok;
		if(ok) persisted++;
		plus_vita_latency_span(&persistence_call, started, done);
		pthread_mutex_unlock(&diagnostic_write_mutex);
	}
	diagnostic_finish_file();
	pthread_mutex_lock(&diagnostic_write_mutex);
	diagnostic_thread_finished = true;
	pthread_cond_broadcast(&diagnostic_pending);
	pthread_mutex_unlock(&diagnostic_write_mutex);
	return NULL;
}
#endif

void plus_vita_diagnostic_stream_timing_begin(void)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	memset(&diagnostic_write_call, 0, sizeof(diagnostic_write_call));
	memset(&diagnostic_mutex_wait, 0, sizeof(diagnostic_mutex_wait));
	diagnostic_stream_timing = true;
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

void plus_vita_diagnostic_stream_timing_end(PlusVitaLatencyStats *write_call,
	PlusVitaLatencyStats *mutex_wait)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	diagnostic_stream_timing = false;
	*write_call = diagnostic_write_call;
	*mutex_wait = diagnostic_mutex_wait;
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

static void write_line(const char *kind, const char *message, int code)
{
	if(!message) message = "";

	uint64_t start_us = plus_vita_latency_now_us();
	pthread_mutex_lock(&diagnostic_write_mutex);
	uint64_t locked_us = plus_vita_latency_now_us();
#if PLUS_VITA_DEFERRED_LOG
	if(!diagnostic_accepting) { pthread_mutex_unlock(&diagnostic_write_mutex); return; }
	diagnostic_sequence++;
	DiagnosticQueue *q = critical_record(kind, message, code) ? &critical_queue : &regular_queue;
	if(q->count == q->capacity) {
		if(q == &critical_queue) {
			/* Preserve the most recent errors/final summaries in reserved space. */
			q->head = (q->head + 1) % q->capacity; q->count--; critical_dropped++;
		} else {
			regular_dropped++;
			plus_vita_latency_span(&enqueue_call, start_us, plus_vita_latency_now_us());
			pthread_mutex_unlock(&diagnostic_write_mutex); return;
		}
	}
	DiagnosticRecord *record = &q->records[(q->head + q->count) % q->capacity];
	record->sequence = diagnostic_sequence;
	record->critical = q == &critical_queue;
	int length = snprintf(record->line, sizeof(record->line), "%06lu elapsed_ms=%llu %-8s code=%d %s\n",
		diagnostic_sequence, (unsigned long long)((start_us-diagnostic_started_us)/1000u), kind, code, message);
	if(length < 0 || (size_t)length >= sizeof(record->line)) {
		truncated++;
		/* Keep every persisted record on a separate line. */
		record->line[sizeof(record->line)-2] = '\n'; record->line[sizeof(record->line)-1] = 0;
	}
	q->count++;
	if(diagnostic_stream_timing) plus_vita_latency_span(&diagnostic_mutex_wait, start_us, locked_us);
	plus_vita_latency_span(&enqueue_call, start_us, plus_vita_latency_now_us());
	pthread_cond_signal(&diagnostic_pending);
#else
	if(!diagnostic_file)
	{
		pthread_mutex_unlock(&diagnostic_write_mutex);
		return;
	}
	diagnostic_sequence++;
	char line[RECORD_SIZE];
	snprintf(line,sizeof(line),"%06lu elapsed_ms=%llu %-8s code=%d %s\n",diagnostic_sequence,
		(unsigned long long)((start_us-diagnostic_started_us)/1000u),kind,code,message);
	if(!persist_record(line,strcmp(kind,"EVENT")==0 || strstr(message,"final")!=NULL)) diagnostic_io_ok=false;
	if(diagnostic_stream_timing)
	{
		plus_vita_latency_span(&diagnostic_mutex_wait, start_us, locked_us);
		plus_vita_latency_span(&diagnostic_write_call, locked_us, plus_vita_latency_now_us());
	}
#endif
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

/* Only the writer (or failed initialization with no worker) owns this call. */
static void diagnostic_finish_file(void)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	if(diagnostic_file) {
		bool io_ok = diagnostic_io_ok;
#if PLUS_VITA_DEFERRED_LOG
		/* Rejected late producers must not wait for shutdown persistence. This
		 * writer (or failed-init cleanup with no writer) exclusively owns I/O. */
		pthread_mutex_unlock(&diagnostic_write_mutex);
#endif
		for(unsigned i=0;i<tail_count;i++)
			if(fputs(diagnostic_tail[(tail_head+i)%TAIL_CAPACITY],diagnostic_file)<0) io_ok=false;
		if(fprintf(diagnostic_file,"sink budget_dropped=%lu limit_bytes=%u\n",budget_dropped,PLUS_VITA_DIAGNOSTIC_MAX_BYTES)<0) io_ok=false;
#if PLUS_VITA_DEFERRED_LOG
		if(fprintf(diagnostic_file, "sink deferred regular_dropped=%lu critical_dropped=%lu truncated=%lu drain_dropped=%lu persisted=%lu io_ok=%d enqueue_count=%llu enqueue_total_us=%llu enqueue_max_us=%llu persistence_count=%llu persistence_total_us=%llu persistence_max_us=%llu\n",
			regular_dropped, critical_dropped, truncated, drain_dropped, persisted, io_ok,
			(unsigned long long)enqueue_call.count, (unsigned long long)enqueue_call.total_us,
			(unsigned long long)enqueue_call.max_us, (unsigned long long)persistence_call.count,
			(unsigned long long)persistence_call.total_us, (unsigned long long)persistence_call.max_us) < 0)
			io_ok = false;
#endif
		if(fflush(diagnostic_file) != 0) io_ok = false;
		if(fclose(diagnostic_file) != 0) io_ok = false;
#if PLUS_VITA_DEFERRED_LOG
		pthread_mutex_lock(&diagnostic_write_mutex);
#endif
		diagnostic_io_ok = diagnostic_io_ok && io_ok;
		diagnostic_file = NULL;
	}
	pthread_mutex_unlock(&diagnostic_write_mutex);
}

/* Caller holds lifecycle mutex. A stuck device write retains this one bounded
 * worker/file/buffer set; never free/reopen it or wait indefinitely for I/O. */
static bool diagnostic_close(void)
{
#if PLUS_VITA_DEFERRED_LOG
	pthread_mutex_lock(&diagnostic_write_mutex);
	diagnostic_accepting = false;
	diagnostic_stopping = true;
	diagnostic_drain_deadline = plus_vita_latency_now_us() + DRAIN_BUDGET_US;
	pthread_cond_broadcast(&diagnostic_pending);
	struct timespec deadline = { 0 };
	int wait_error = clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += 2;
	while(diagnostic_thread_started && !diagnostic_thread_finished && !wait_error)
		wait_error = pthread_cond_timedwait(&diagnostic_pending, &diagnostic_write_mutex, &deadline);
	bool finished = !diagnostic_thread_started || diagnostic_thread_finished;
	if(!finished) { diagnostic_io_ok = false; diagnostic_timed_out=true; }
	pthread_mutex_unlock(&diagnostic_write_mutex);
	if(!finished) return false;
	if(diagnostic_thread_started) {
		pthread_join(diagnostic_thread, NULL);
		diagnostic_thread_started = false;
	} else diagnostic_finish_file();
#else
	diagnostic_finish_file();
#endif
	return true;
}

bool plus_vita_diagnostic_begin(void)
{
	pthread_mutex_lock(&diagnostic_lifecycle_mutex);
	if(!diagnostic_close()) {
		pthread_mutex_unlock(&diagnostic_lifecycle_mutex); return false;
	}
	/* The deferred writer owns opening as well as subsequent persistence. */
#if !PLUS_VITA_DEFERRED_LOG
	FILE *opened = fopen(PLUS_VITA_DIAGNOSTIC_PATH, "w");
	bool opened_ok = opened != NULL;
	if(opened && (fputs("plus-vita allocation diagnostic v1\nSecrets and HTTP payloads are intentionally excluded.\n", opened) < 0
		|| fflush(opened) != 0)) opened_ok = false;
#endif
	pthread_mutex_lock(&diagnostic_write_mutex);
#if PLUS_VITA_DEFERRED_LOG
	diagnostic_file = NULL;
	diagnostic_io_ok = false;
#else
	diagnostic_file = opened;
	diagnostic_io_ok = opened_ok;
#endif
	diagnostic_sequence = 0;
	diagnostic_started_us = plus_vita_latency_now_us();
	file_bytes=256; tail_head=tail_count=0; budget_dropped=0;
	diagnostic_stream_timing = false;
#if PLUS_VITA_DEFERRED_LOG
	regular_queue = (DiagnosticQueue){ .records = regular_records, .capacity = REGULAR_CAPACITY };
	critical_queue = (DiagnosticQueue){ .records = critical_records, .capacity = CRITICAL_CAPACITY };
	regular_dropped = critical_dropped = truncated = drain_dropped = persisted = 0;
	memset(&enqueue_call, 0, sizeof(enqueue_call));
	memset(&persistence_call, 0, sizeof(persistence_call));
	diagnostic_stopping = false;
	diagnostic_timed_out = false;
	diagnostic_thread_finished = false;
	{
		pthread_attr_t attr;
		bool attr_ready = pthread_attr_init(&attr) == 0;
		int r = attr_ready ? pthread_attr_setstacksize(&attr, 128 * 1024) : -1;
		if(!r) r = pthread_create(&diagnostic_thread, &attr, diagnostic_writer, NULL);
		if(attr_ready) pthread_attr_destroy(&attr);
		diagnostic_thread_started = r == 0;
	}
	diagnostic_accepting = diagnostic_thread_started;
	bool ready = diagnostic_thread_started;
#else
	bool ready = diagnostic_io_ok;
#endif
	pthread_mutex_unlock(&diagnostic_write_mutex);
	if(!ready) diagnostic_close();
	pthread_mutex_unlock(&diagnostic_lifecycle_mutex);
	if(ready) write_line("BUILD", "build=" PLUS_VITA_APP_BUILD " profile=" PLUS_VITA_DIAGNOSTIC_PROFILE, 0);
	return ready;
}

void plus_vita_diagnostic_event(const char *event, int code)
{
	static const char *const names[]={"allocation.begin","allocation.complete","allocation.stage",
		"allocation.network.init","allocation.network.release","stream_probe.begin","stream_probe.complete",
		"replay.begin","replay.complete","recovery.wifi","recovery.sleep","app.start","app.stop",
		"ui.stage","account.complete"};
	if(!event) return;
	for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)
		if(strncmp(event,names[i],strlen(names[i])+1)==0) {
			if(!strcmp(names[i],"ui.stage")) code=code>=0 && code<=17 ? code : 0;
			else if(!strcmp(names[i],"allocation.stage")) code=code>=0 && code<=5 ? code : 0;
			else code=code>=-1 && code<=21 ? code : -1;
			write_line("EVENT",names[i],code); return;
		}
}

void plus_vita_diagnostic_progress(const char *stage)
{
	(void)stage;
	write_line("STAGE", "working", 0);
}

static const char *known_name(const char *name, const char *const *names, unsigned count)
{
	if(!name) return NULL;
	for(unsigned i=0;i<count;i++)
		if(strncmp(name,names[i],strlen(names[i])+1)==0) return names[i];
	return NULL;
}

void plus_vita_diagnostic_metric(const char *name, const PlusVitaLatencyStats *stats)
{
	static const char *const names[]={"input_poll_gap","input_publish_call","main_mutex_wait",
		"callback_mutex_wait","callback_work","frame_assembly","ready_to_enqueue","queue_wait",
		"decode_prepare","decode","display_wait","submitted_first_packet_to_present",
		"submitted_ready_to_present","log_write_call","log_mutex_wait","feedback.sample_to_publish",
		"feedback.publish_to_wake","feedback.sample_to_state_send","feedback.sample_to_history_send",
		"feedback.state_send_call","feedback.history_send_call"};
	const char *fixed=known_name(name,names,sizeof(names)/sizeof(names[0]));
	if(!fixed || !stats) return;
	char record[512];
	snprintf(record,sizeof(record),"final metric=%s count=%llu total_us=%llu max_us=%llu invalid=%llu p50_us=%llu p95_us=%llu p99_us=%llu",
		fixed,(unsigned long long)stats->count,(unsigned long long)stats->total_us,
		(unsigned long long)stats->max_us,(unsigned long long)stats->invalid,
		(unsigned long long)plus_vita_latency_percentile_upper(stats,50),
		(unsigned long long)plus_vita_latency_percentile_upper(stats,95),
		(unsigned long long)plus_vita_latency_percentile_upper(stats,99));
	write_line("METRIC",record,0);
}

void plus_vita_diagnostic_counter(const char *name, uint64_t count)
{
	static const char *const names[]={"displayed","submitted","audio_frames","fec_repaired",
		"fec_failed","reorder_late","reorder_missing","input_updates","input_poll_failures",
		"feedback_published","feedback_consumed","feedback_coalesced","feedback_send_failures",
		"feedback_logs_suppressed","input_neutral_consumed","input_neutral_failed","recovery_episodes"};
	const char *fixed=known_name(name,names,sizeof(names)/sizeof(names[0]));
	if(!fixed) return;
	char record[128];
	snprintf(record,sizeof(record),"final counter=%s count=%llu",fixed,(unsigned long long)count);
	write_line("COUNTER",record,0);
}

void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,
	const char *message, void *user)
{
	(void)user;
#if PLUS_VITA_INPUT_WORKER
	if(feedback_diagnostic_suppressed()) return;
#endif
	(void)message;
	/* Text is untrusted, including library-owned prefixes and disconnect reasons. */
	if(level==CHIAKI_LOG_ERROR) write_line("LIB","library.error",CHIAKI_LOG_ERROR);
	else if(level==CHIAKI_LOG_WARNING) write_line("LIB","library.warning",CHIAKI_LOG_WARNING);
	else if(level==CHIAKI_LOG_INFO) write_line("LIB","library.info",CHIAKI_LOG_INFO);
}

void plus_vita_diagnostic_end(void)
{
	pthread_mutex_lock(&diagnostic_lifecycle_mutex);
	diagnostic_close();
	pthread_mutex_unlock(&diagnostic_lifecycle_mutex);
}

bool plus_vita_diagnostic_persistence_ok(void)
{
	pthread_mutex_lock(&diagnostic_write_mutex);
	bool ok = diagnostic_io_ok;
	pthread_mutex_unlock(&diagnostic_write_mutex);
	return ok;
}

static PlusVitaLatencyStats ui_timings[PLUS_VITA_UI_TIMINGS];
static unsigned ui_snapshots;
static bool ui_active;
static unsigned artwork_metadata[PLUS_VITA_ARTWORK_METADATA_STATES];
static uint64_t artwork_results[PLUS_VITA_ARTWORK_RESULTS];

void plus_vita_diagnostic_artwork_metadata(const unsigned counts[PLUS_VITA_ARTWORK_METADATA_STATES])
{
	if(!ui_active || !counts) return;
	for(unsigned i=0;i<PLUS_VITA_ARTWORK_METADATA_STATES;i++) artwork_metadata[i]=counts[i]>4096 ? 4096 : counts[i];
}
void plus_vita_diagnostic_artwork_result(PlusVitaArtworkResult result)
{
	if(ui_active && (unsigned)result<PLUS_VITA_ARTWORK_RESULTS && artwork_results[result]!=UINT64_MAX)
		artwork_results[result]++;
}

bool plus_vita_diagnostic_ui_begin(void)
{
	ui_active = false;
	memset(ui_timings, 0, sizeof(ui_timings)); ui_snapshots = 0;
	memset(artwork_metadata,0,sizeof(artwork_metadata)); memset(artwork_results,0,sizeof(artwork_results));
	if(!plus_vita_diagnostic_begin()) return false;
	ui_active = true;
	write_line("UI", "app.start graphical=1 glyph_budget_bytes=524288", 0);
	return true;
}

void plus_vita_diagnostic_ui_sample(PlusVitaUiTiming stage, uint64_t duration_us)
{
	if(ui_active && (unsigned)stage < PLUS_VITA_UI_TIMINGS) plus_vita_latency_add(&ui_timings[stage], duration_us);
}

static void ui_report(bool final)
{
	static const char *const names[] = {"draw", "present", "sampled_input_to_present", "poll_gap", "font_A_draw", "font_B_draw"};
	if(!ui_active) return;
	for(unsigned i = 0; i < PLUS_VITA_UI_TIMINGS; i++) {
		char record[256];
		snprintf(record, sizeof(record), "%s%s count=%llu total_us=%llu max_us=%llu invalid=%llu",
			final ? "final summary " : "", names[i], (unsigned long long)ui_timings[i].count,
			(unsigned long long)ui_timings[i].total_us, (unsigned long long)ui_timings[i].max_us,
			(unsigned long long)ui_timings[i].invalid);
		write_line("UI", record, 0);
	}
	char record[512];
	snprintf(record,sizeof(record),"%s summary metadata_missing=%u available=%u unsupported=%u rejected=%u loaded=%llu dns=%llu http=%llu tls=%llu transport=%llu limit=%llu decode=%llu cancelled=%llu unavailable=%llu policy=%llu",
		final ? "final" : "current",artwork_metadata[0],artwork_metadata[1],artwork_metadata[2],artwork_metadata[3],
		(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_OK],(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_DNS],
		(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_HTTP],(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_TLS],
		(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_TRANSPORT],(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_LIMIT],
		(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_DECODE],(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_CANCELLED],
		(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_UNAVAILABLE],(unsigned long long)artwork_results[PLUS_VITA_ARTWORK_POLICY]);
	write_line("ARTWORK",record,0);
}

void plus_vita_diagnostic_ui_flush(void)
{
#if PLUS_VITA_DEFERRED_LOG
	if(ui_snapshots >= 64) return;
	ui_snapshots++;
	ui_report(false);
#endif
}

void plus_vita_diagnostic_ui_end(void)
{
	ui_report(true);
	ui_active = false;
	plus_vita_diagnostic_end();
}
