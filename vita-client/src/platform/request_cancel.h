// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_REQUEST_CANCEL_H
#define PLUS_VITA_REQUEST_CANCEL_H
#include <stdbool.h>
#include <curl/curl.h>
/* The account worker is the sole request owner. Reset only after its previous
 * job has completed; cancelling never destroys a handle owned by that worker. */
void plus_vita_request_cancel_set(bool cancelled);
bool plus_vita_request_cancelled(void);
int plus_vita_request_progress(void *user, curl_off_t total_in, curl_off_t received,
	curl_off_t total_out, curl_off_t sent);
#endif
