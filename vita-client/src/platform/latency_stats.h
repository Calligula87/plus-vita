// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_LATENCY_STATS_H
#define PLUS_VITA_LATENCY_STATS_H

#include <stdbool.h>
#include <stdint.h>

#define PLUS_VITA_LATENCY_BUCKETS 19

/* Bounded, allocation-free durations. Caller owns synchronization. No input
 * values, packet contents, or absolute timestamps are retained here. */
typedef struct {
	uint64_t count, total_us, max_us, invalid;
	uint64_t buckets[PLUS_VITA_LATENCY_BUCKETS];
} PlusVitaLatencyStats;

void plus_vita_latency_add(PlusVitaLatencyStats *stats, uint64_t duration_us);
void plus_vita_latency_span(PlusVitaLatencyStats *stats, uint64_t start_us,
	uint64_t end_us);
/* Conservative bucket upper bound, capped by the observed maximum. Zero for
 * empty/invalid requests. This is an estimate, not an exact percentile. */
uint64_t plus_vita_latency_percentile_upper(const PlusVitaLatencyStats *stats,
	unsigned int percentile);

#endif
