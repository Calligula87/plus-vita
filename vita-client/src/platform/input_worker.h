// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_INPUT_WORKER_H
#define PLUS_VITA_INPUT_WORKER_H

#include "feedback_latency.h"

#define PLUS_VITA_INPUT_PERIOD_US 4000u
#define PLUS_VITA_INPUT_STOP_HOLD_US 2000000u

typedef struct PlusVitaInputWorker PlusVitaInputWorker;
typedef struct {
	ChiakiControllerState state;
	uint64_t native_timestamp;
	bool stop_chord;
} PlusVitaInputSample;

/* Nonblocking platform boundary: 1=sample available, 0=no sample, -1=fatal.
 * previous belongs to this input owner and preserves native touch identity. */
typedef int (*PlusVitaInputRead)(void *user, const ChiakiControllerState *previous,
	PlusVitaInputSample *sample);

typedef struct {
	PlusVitaLatencyStats poll_gap, publish_call;
	uint64_t updates, poll_failures, missed_deadlines;
	uint64_t native_changes, native_repeats;
	ChiakiErrorCode neutral_result;
	bool neutral_consumed;
} PlusVitaInputStats;

typedef struct {
	bool done, user_stopped;
	ChiakiErrorCode error;
} PlusVitaInputStatus;

/* The feedback sender outlives this worker, including stop/join. */
ChiakiErrorCode plus_vita_input_worker_start(PlusVitaInputWorker **result,
	ChiakiFeedbackSender *sender, PlusVitaInputRead read, void *user);
PlusVitaInputStatus plus_vita_input_worker_status(PlusVitaInputWorker *worker);
/* Interrupt sleep, join, copy final stats and release ownership. */
ChiakiErrorCode plus_vita_input_worker_stop(PlusVitaInputWorker *worker,
	PlusVitaInputStats *stats);

#endif
