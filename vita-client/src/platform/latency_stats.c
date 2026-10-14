// SPDX-License-Identifier: AGPL-3.0-only
#include "latency_stats.h"

static const uint64_t upper_us[PLUS_VITA_LATENCY_BUCKETS] = {
	250, 500, 1000, 2000, 4000, 8000, 12000, 16000, 20000, 25000,
	33000, 50000, 75000, 100000, 150000, 250000, 500000, 1000000,
	UINT64_MAX
};

void plus_vita_latency_add(PlusVitaLatencyStats *stats, uint64_t duration_us)
{
	stats->count++;
	stats->total_us += duration_us;
	if(duration_us > stats->max_us) stats->max_us = duration_us;
	unsigned int bucket = 0;
	while(duration_us > upper_us[bucket]) bucket++;
	stats->buckets[bucket]++;
}

void plus_vita_latency_span(PlusVitaLatencyStats *stats, uint64_t start_us,
	uint64_t end_us)
{
	if(end_us < start_us) { stats->invalid++; return; }
	plus_vita_latency_add(stats, end_us - start_us);
}

uint64_t plus_vita_latency_percentile_upper(const PlusVitaLatencyStats *stats,
	unsigned int percentile)
{
	if(!stats->count || !percentile || percentile > 100) return 0;
	uint64_t rank = stats->count / 100 * percentile
		+ (stats->count % 100 * percentile + 99) / 100;
	uint64_t cumulative = 0;
	for(unsigned int i = 0; i < PLUS_VITA_LATENCY_BUCKETS; i++)
	{
		cumulative += stats->buckets[i];
		if(cumulative >= rank)
			return upper_us[i] < stats->max_us ? upper_us[i] : stats->max_us;
	}
	return stats->max_us;
}
