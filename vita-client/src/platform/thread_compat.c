// SPDX-License-Identifier: AGPL-3.0-only

#include "thread_compat.h"

#ifdef __vita__

int plus_vita_pthread_clockjoin(pthread_t thread, void **retval,
	clockid_t clock_id, const struct timespec *timeout)
{
	/*
	 * pthread-embedded does not expose a non-blocking join primitive. Preserve
	 * correctness with a blocking join until the Chiaki thread representation
	 * is migrated to native SceKernelThread APIs, where a real timeout can be
	 * implemented. Callers must not rely on the timeout during this milestone.
	 */
	(void)clock_id;
	(void)timeout;
	return pthread_join(thread, retval);
}

#endif
