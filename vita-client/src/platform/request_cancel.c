// SPDX-License-Identifier: AGPL-3.0-only
#include "request_cancel.h"
#include <stdatomic.h>
static atomic_bool request_cancelled;
void plus_vita_request_cancel_set(bool cancelled) { atomic_store(&request_cancelled, cancelled); }
bool plus_vita_request_cancelled(void) { return atomic_load(&request_cancelled); }
int plus_vita_request_progress(void *user, curl_off_t total_in, curl_off_t received,
	curl_off_t total_out, curl_off_t sent)
{
	(void)user; (void)total_in; (void)received; (void)total_out; (void)sent;
	return plus_vita_request_cancelled() ? 1 : 0;
}
