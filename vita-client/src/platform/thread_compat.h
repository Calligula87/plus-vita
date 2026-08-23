// SPDX-License-Identifier: AGPL-3.0-only

#ifndef PLUS_VITA_THREAD_COMPAT_H
#define PLUS_VITA_THREAD_COMPAT_H

#ifdef __vita__

#include <pthread.h>
#include <time.h>

/* Vita pthreads have no GNU clock-based timed join. */
int plus_vita_pthread_clockjoin(pthread_t thread, void **retval,
	clockid_t clock_id, const struct timespec *timeout);

#define pthread_clockjoin_np plus_vita_pthread_clockjoin

#endif

#endif
