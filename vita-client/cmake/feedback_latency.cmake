# Instrument the generated Vita copy only. Fail closed if upstream moves a
# boundary: silently losing a timing probe would invalidate the comparison.
macro(plus_vita_feedback_latency_patch old_text new_text)
  string(FIND "${PLUS_VITA_FEEDBACK_TEXT}" "${old_text}" patch_position)
  if(patch_position EQUAL -1)
    message(FATAL_ERROR "Feedback latency instrumentation boundary was not found: ${old_text}")
  endif()
  string(REPLACE "${old_text}" "${new_text}"
    PLUS_VITA_FEEDBACK_TEXT "${PLUS_VITA_FEEDBACK_TEXT}")
endmacro()

plus_vita_feedback_latency_patch("#include <chiaki/time.h>" [=[
#include <chiaki/time.h>
#include "feedback_latency.h"
#include "latency_clock.h"
#include <string.h>

/* All mutations are under the existing sender state_mutex, except reset before
 * worker creation and snapshot after its join. No additional hot-path locks. */
static PlusVitaFeedbackLatency feedback_latency;
static uint64_t latency_pending_sample_us, latency_pending_publish_us;
static uint64_t latency_active_sample_us;
static bool latency_active_input;

void plus_vita_feedback_latency_get_stopped(PlusVitaFeedbackLatency *result)
{
	*result = feedback_latency;
}
]=])

plus_vita_feedback_latency_patch("feedback_sender->log = takion->log;" [=[
memset(&feedback_latency, 0, sizeof(feedback_latency));
	latency_pending_sample_us = latency_pending_publish_us = 0;
	latency_active_sample_us = 0;
	latency_active_input = false;
	feedback_sender->log = takion->log;
]=])

plus_vita_feedback_latency_patch(
  "CHIAKI_EXPORT ChiakiErrorCode chiaki_feedback_sender_set_controller_state(ChiakiFeedbackSender *feedback_sender, ChiakiControllerState *state)"
  "ChiakiErrorCode plus_vita_feedback_sender_set_controller_state_sampled(ChiakiFeedbackSender *feedback_sender, ChiakiControllerState *state, uint64_t sampled_us)")

plus_vita_feedback_latency_patch("feedback_sender->controller_state_raw = *state;" [=[
uint64_t published_us = plus_vita_latency_now_us();
	feedback_latency.published++;
	if(feedback_sender->controller_state_changed) feedback_latency.coalesced++;
	latency_pending_sample_us = sampled_us;
	latency_pending_publish_us = published_us;
	if(sampled_us)
		plus_vita_latency_span(&feedback_latency.sample_to_publish, sampled_us, published_us);
	feedback_sender->controller_state_raw = *state;
]=])

plus_vita_feedback_latency_patch("bool timeout_wake = !feedback_sender->controller_state_changed;" [=[
bool timeout_wake = !feedback_sender->controller_state_changed;
		latency_active_input = !timeout_wake;
		latency_active_sample_us = latency_active_input ? latency_pending_sample_us : 0;
		if(latency_active_input)
		{
			feedback_latency.consumed++;
			plus_vita_latency_span(&feedback_latency.publish_to_wake,
				latency_pending_publish_us, plus_vita_latency_now_us());
		}
]=])

plus_vita_feedback_latency_patch(
  "ChiakiErrorCode err = chiaki_takion_send_feedback_state(feedback_sender->takion, feedback_sender->state_seq_num++, &state);" [=[
uint64_t send_started_us = plus_vita_latency_now_us();
	ChiakiErrorCode err = chiaki_takion_send_feedback_state(feedback_sender->takion, feedback_sender->state_seq_num++, &state);
	uint64_t send_done_us = plus_vita_latency_now_us();
	plus_vita_latency_span(&feedback_latency.state_send_call, send_started_us, send_done_us);
	if(err != CHIAKI_ERR_SUCCESS) feedback_latency.send_failures++;
	else if(latency_active_input && latency_active_sample_us)
		plus_vita_latency_span(&feedback_latency.sample_to_state_send,
			latency_active_sample_us, send_done_us);
]=])

plus_vita_feedback_latency_patch(
  "chiaki_takion_send_feedback_history(feedback_sender->takion, feedback_sender->history_seq_num++, buf, buf_size);" [=[
uint64_t send_started_us = plus_vita_latency_now_us();
	ChiakiErrorCode send_result = chiaki_takion_send_feedback_history(feedback_sender->takion, feedback_sender->history_seq_num++, buf, buf_size);
	uint64_t send_done_us = plus_vita_latency_now_us();
	plus_vita_latency_span(&feedback_latency.history_send_call, send_started_us, send_done_us);
	if(send_result != CHIAKI_ERR_SUCCESS) feedback_latency.send_failures++;
	else if(latency_active_input && latency_active_sample_us)
		plus_vita_latency_span(&feedback_latency.sample_to_history_send,
			latency_active_sample_us, send_done_us);
]=])

string(APPEND PLUS_VITA_FEEDBACK_TEXT [=[

bool plus_vita_feedback_sender_neutral_consumed(ChiakiFeedbackSender *sender)
{
	if(chiaki_mutex_trylock(&sender->state_mutex) != CHIAKI_ERR_SUCCESS) return false;
	ChiakiControllerState idle;
	chiaki_controller_state_set_idle(&idle);
	bool consumed = !sender->controller_state_changed
		&& chiaki_controller_state_equals(&sender->controller_state_prev, &idle);
	chiaki_mutex_unlock(&sender->state_mutex);
	return consumed;
}

ChiakiErrorCode plus_vita_feedback_sender_error(ChiakiFeedbackSender *sender)
{
	ChiakiErrorCode err = chiaki_mutex_lock(&sender->state_mutex);
	if(err != CHIAKI_ERR_SUCCESS) return err;
	err = feedback_latency.send_failures ? CHIAKI_ERR_NETWORK : CHIAKI_ERR_SUCCESS;
	chiaki_mutex_unlock(&sender->state_mutex);
	return err;
}

CHIAKI_EXPORT ChiakiErrorCode chiaki_feedback_sender_set_controller_state(
	ChiakiFeedbackSender *sender, ChiakiControllerState *state)
{
	return plus_vita_feedback_sender_set_controller_state_sampled(sender, state, 0);
}
]=])
