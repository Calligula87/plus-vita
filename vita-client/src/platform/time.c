// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/time.h>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#else
#include <time.h>
#endif

uint64_t chiaki_time_now_monotonic_us(void)
{
#ifdef __vita__
	return sceKernelGetProcessTimeWide();
#else
	struct timespec now;
	if(clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;
	return (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u;
#endif
}
