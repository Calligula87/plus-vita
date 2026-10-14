// SPDX-License-Identifier: AGPL-3.0-only
#include "latency_stats.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	PlusVitaLatencyStats stats = { 0 };
	assert(!plus_vita_latency_percentile_upper(&stats, 95));
	/* Known distribution with exact boundary hits, not rounded input values. */
	for(unsigned int i = 0; i < 90; i++) plus_vita_latency_add(&stats, 1000);
	for(unsigned int i = 0; i < 9; i++) plus_vita_latency_add(&stats, 8001);
	plus_vita_latency_add(&stats, 1500001);
	assert(stats.count == 100 && stats.total_us == 1662010 && stats.max_us == 1500001);
	assert(plus_vita_latency_percentile_upper(&stats, 50) == 1000);
	assert(plus_vita_latency_percentile_upper(&stats, 95) == 12000);
	assert(plus_vita_latency_percentile_upper(&stats, 99) == 12000);
	assert(plus_vita_latency_percentile_upper(&stats, 100) == 1500001);
	assert(!plus_vita_latency_percentile_upper(&stats, 0));
	assert(!plus_vita_latency_percentile_upper(&stats, 101));
	plus_vita_latency_span(&stats, 3000, 2000);
	assert(stats.invalid == 1 && stats.count == 100);
	PlusVitaLatencyStats spans = { 0 };
	/* Independent fixture: callbacks release frames in bursts and input polls
 * resume after a long display stall. Verify those outliers remain visible. */
	const uint64_t poll_us[] = { 10000, 14000, 18000, 108000, 112000 };
	for(unsigned int i = 1; i < 5; i++)
		plus_vita_latency_span(&spans, poll_us[i - 1], poll_us[i]);
	assert(spans.count == 4 && spans.max_us == 90000 && spans.total_us == 102000);
	assert(plus_vita_latency_percentile_upper(&spans, 50) == 4000);
	assert(plus_vita_latency_percentile_upper(&spans, 95) == 90000);
	PlusVitaLatencyStats large = { 0 };
	for(unsigned int i = 0; i < 1000000; i++) plus_vita_latency_add(&large, 16667);
	assert(large.count == 1000000 && large.total_us == UINT64_C(16667000000));
	assert(plus_vita_latency_percentile_upper(&large, 99) == 16667);
	puts("latency stats: distribution, stall fixture, backward clock and million samples passed");
	return 0;
}
