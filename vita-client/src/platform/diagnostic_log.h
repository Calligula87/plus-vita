#ifndef PLUS_VITA_DIAGNOSTIC_LOG_H
#define PLUS_VITA_DIAGNOSTIC_LOG_H

#include <chiaki/log.h>

#include <stdbool.h>
#include "latency_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

bool plus_vita_diagnostic_begin(void);
void plus_vita_diagnostic_event(const char *event, int code);
void plus_vita_diagnostic_progress(const char *stage);
void plus_vita_diagnostic_chiaki_log(ChiakiLogLevel level,
	const char *message, void *user);
void plus_vita_diagnostic_end(void);
void plus_vita_diagnostic_stream_timing_begin(void);
void plus_vita_diagnostic_stream_timing_end(PlusVitaLatencyStats *write_call,
	PlusVitaLatencyStats *mutex_wait);

#ifdef __cplusplus
}
#endif

#endif
