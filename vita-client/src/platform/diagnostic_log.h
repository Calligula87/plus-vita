#ifndef PLUS_VITA_DIAGNOSTIC_LOG_H
#define PLUS_VITA_DIAGNOSTIC_LOG_H

#include <chiaki/log.h>

#include <stdbool.h>
#include "latency_stats.h"
#include "artwork_status.h"

#ifdef __cplusplus
extern "C" {
#endif

bool plus_vita_diagnostic_begin(void);
void plus_vita_diagnostic_event(const char *event, int code);
void plus_vita_diagnostic_progress(const char *stage);
void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,
	const char *message, void *user);
void plus_vita_diagnostic_end(void);
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
