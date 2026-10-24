// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_ATTEMPT_LOG_H
#define PLUS_VITA_ATTEMPT_LOG_H
#include "ui_flow.h"
/* Call before recovery dispatch, while the new request is still pending.
 * A busy predecessor retains its log until its workers have released ownership. */
bool plus_vita_attempt_log_prepare(PlusVitaUiFlow *flow, bool previous_workers_idle);
#endif
