// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_LATENCY_CLOCK_H
#define PLUS_VITA_LATENCY_CLOCK_H

#include <stdint.h>
#ifdef __vita__
#include <psp2/kernel/processmgr.h>
static inline uint64_t plus_vita_latency_now_us(void)
{
	return sceKernelGetProcessTimeWide();
}
#else
#include <time.h>
static inline uint64_t plus_vita_latency_now_us(void)
{
	struct timespec value;
	if(clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
	return (uint64_t)value.tv_sec * 1000000u + (uint64_t)value.tv_nsec / 1000u;
}
#endif

#endif
