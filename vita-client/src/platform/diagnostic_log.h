#ifndef PLUS_VITA_DIAGNOSTIC_LOG_H
#define PLUS_VITA_DIAGNOSTIC_LOG_H

#include <chiaki/log.h>

#include <stdbool.h>
#include "latency_stats.h"
#include "artwork_status.h"
#define PLUS_VITA_DIAGNOSTIC_MAX_BYTES (1024u * 1024u)

#ifdef __cplusplus
extern "C" {
#endif

/* Deferred mode: true means a writer was started. Availability stays false
 * until that worker has opened and initialized storage successfully. */
bool plus_vita_diagnostic_begin(void);
void plus_vita_diagnostic_event(const char *event, int code);
/* These values are persisted as startup status codes. Append new values. */
typedef enum {
	PLUS_VITA_STREAM_STARTUP_TRANSPORT = 0,
	PLUS_VITA_STREAM_STARTUP_HANDSHAKE,
	PLUS_VITA_STREAM_STARTUP_STREAMINFO,
	PLUS_VITA_STREAM_STARTUP_DECODER,
	PLUS_VITA_STREAM_STARTUP_READY,
	PLUS_VITA_STREAM_STARTUP_TRANSPORT_FAILED,
	PLUS_VITA_STREAM_STARTUP_HANDSHAKE_FAILED,
	PLUS_VITA_STREAM_STARTUP_STREAMINFO_INTERRUPTED,
	PLUS_VITA_STREAM_STARTUP_STREAMINFO_INVALID,
	PLUS_VITA_STREAM_STARTUP_PROFILE_UNAVAILABLE,
	PLUS_VITA_STREAM_STARTUP_AUDIO_INIT_FAILED,
	PLUS_VITA_STREAM_STARTUP_VIDEO_HEADER_INVALID,
	PLUS_VITA_STREAM_STARTUP_HEADER_GUARD_REJECTED,
	PLUS_VITA_STREAM_STARTUP_DECODER_INIT_FAILED,
	PLUS_VITA_STREAM_STARTUP_STREAMINFO_REPLY_FAILED,
	PLUS_VITA_STREAM_STARTUP_PROVISION_INVALID,
	PLUS_VITA_STREAM_STARTUP_MEDIA_DISCONNECTED,
	PLUS_VITA_STREAM_STARTUP_MEDIA_DECRYPT_FAILED,
	PLUS_VITA_STREAM_STARTUP_MEDIA_INVALID,
	PLUS_VITA_STREAM_STARTUP_FIRST_FRAME_TIMEOUT,
	PLUS_VITA_STREAM_STARTUP_PS3_FEEDBACK,
} PlusVitaStreamStartup;
void plus_vita_diagnostic_stream_startup(PlusVitaStreamStartup status);
void plus_vita_diagnostic_progress(const char *stage);
void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,
	const char *message, void *user);
void plus_vita_diagnostic_end(void);
/* Exact metric/counter-name allowlists; unknown names are discarded. Only
 * aggregate durations/counts cross this boundary, never free-form values. */
void plus_vita_diagnostic_metric(const char *name, const PlusVitaLatencyStats *stats);
void plus_vita_diagnostic_counter(const char *name, uint64_t count);
/* File I/O success for the most recently started session. Overflow is reported
 * separately in its bounded sink summary. Safe to query after end. */
bool plus_vita_diagnostic_persistence_ok(void);
/* Graphical UI owner only: typed durations, no text/account payload entrypoint.
 * Samples are allocation/I/O free. Deferred snapshots are capped; end writes
 * a final aggregate. Values measure local stages, not physical/cloud latency. */
typedef enum {
	PLUS_VITA_UI_DRAW, PLUS_VITA_UI_PRESENT, PLUS_VITA_UI_INPUT_PRESENT,
	PLUS_VITA_UI_POLL_GAP, PLUS_VITA_UI_FONT_A_DRAW, PLUS_VITA_UI_FONT_B_DRAW, PLUS_VITA_UI_TIMINGS
} PlusVitaUiTiming;
bool plus_vita_diagnostic_ui_begin(void);
void plus_vita_diagnostic_ui_sample(PlusVitaUiTiming stage, uint64_t duration_us);
void plus_vita_diagnostic_ui_flush(void);
void plus_vita_diagnostic_ui_end(void);
/* UI owner only; fixed enums/counts, never artwork URLs or title/account text. */
void plus_vita_diagnostic_artwork_metadata(const unsigned counts[PLUS_VITA_ARTWORK_METADATA_STATES]);
void plus_vita_diagnostic_artwork_result(PlusVitaArtworkResult result);
/* One active feedback sender. Suppress its nested diagnostic callbacks while
 * it holds transport/state locks; return only a bounded aggregate after join. */
void plus_vita_diagnostic_feedback_begin(void);
uint64_t plus_vita_diagnostic_feedback_end(void);
void plus_vita_diagnostic_stream_timing_begin(void);
void plus_vita_diagnostic_stream_timing_end(PlusVitaLatencyStats *write_call,
	PlusVitaLatencyStats *mutex_wait);

#ifdef __cplusplus
}
#endif

#endif
