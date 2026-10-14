// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_FEEDBACK_LATENCY_H
#define PLUS_VITA_FEEDBACK_LATENCY_H

#include "latency_stats.h"
#include <chiaki/feedbacksender.h>

typedef struct {
	uint64_t published, consumed, coalesced, send_failures;
	PlusVitaLatencyStats sample_to_publish, publish_to_wake;
	PlusVitaLatencyStats sample_to_state_send, sample_to_history_send;
	PlusVitaLatencyStats state_send_call, history_send_call;
} PlusVitaFeedbackLatency;

/* Local adaptation of the existing sender; same wire behavior. sampled_us is
 * the beginning of the Vita poll. Zero excludes synthetic neutral updates
 * from sample-to-send timings. One active sender, as in the current client. */
ChiakiErrorCode plus_vita_feedback_sender_set_controller_state_sampled(
	ChiakiFeedbackSender *sender, ChiakiControllerState *state, uint64_t sampled_us);
/* Only after sender_fini has joined the worker, before another sender_init. */
void plus_vita_feedback_latency_get_stopped(PlusVitaFeedbackLatency *result);
/* Existing sender mutex protects observed local transport failures. */
ChiakiErrorCode plus_vita_feedback_sender_error(ChiakiFeedbackSender *sender);
bool plus_vita_feedback_sender_neutral_consumed(ChiakiFeedbackSender *sender);

#endif
