// SPDX-License-Identifier: AGPL-3.0-only
#include "attempt_log.h"
#include "../platform/diagnostic_log.h"
bool plus_vita_attempt_log_prepare(PlusVitaUiFlow *flow, bool previous_workers_idle)
{
	const PlusVitaAppEffect *pending=&flow->pending;
	if(!previous_workers_idle || !pending->operation
		|| pending->operation!=plus_vita_app_view(flow->app).operation
		|| pending->operation==flow->diagnostic_operation
		|| (pending->kind!=PLUS_VITA_EFFECT_PLAY && !pending->fresh_attempt)) return false;
	flow->diagnostic_operation=pending->operation;
	plus_vita_diagnostic_ui_flush();
	plus_vita_diagnostic_ui_begin();
	plus_vita_ui_flow_event(flow,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_LOG_STATUS,
		.log_available=plus_vita_diagnostic_persistence_ok()},0);
	return true;
}
