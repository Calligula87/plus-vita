// SPDX-License-Identifier: AGPL-3.0-only
#include "ui_flow.h"
#include <string.h>

bool plus_vita_ui_flow_event(PlusVitaUiFlow *flow, PlusVitaAppEvent event, uint64_t sampled_us)
{
	PlusVitaAppView before = plus_vita_app_view(flow->app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(flow->app, event);
	PlusVitaAppView after = plus_vita_app_view(flow->app);
	flow->cancel_account = before.operation && after.operation != before.operation;
	bool changed = effect.kind != PLUS_VITA_EFFECT_NONE || memcmp(&before, &after, sizeof(before)) != 0;
	if(effect.kind != PLUS_VITA_EFFECT_NONE) {
		plus_vita_credential_clear(&flow->pending.credential);
		flow->pending = effect;
	}
	plus_vita_credential_clear(&effect.credential);
	flow->dirty = flow->dirty || changed;
	if(changed && sampled_us && !flow->input_since) flow->input_since = sampled_us;
	return changed;
}

PlusVitaAppEffect plus_vita_ui_flow_take(PlusVitaUiFlow *flow)
{
	PlusVitaAppEffect effect = flow->pending;
	plus_vita_credential_clear(&flow->pending.credential);
	flow->pending.kind = PLUS_VITA_EFFECT_NONE;
	if(!effect.operation || effect.operation != plus_vita_app_view(flow->app).operation) {
		plus_vita_credential_clear(&effect.credential);
		return (PlusVitaAppEffect){0};
	}
	return effect;
}

bool plus_vita_ui_flow_paint(PlusVitaUiFlow *flow, PlusVitaFont *font, uint32_t *pixels, int pitch)
{
	if(!flow->dirty) return false;
	PlusVitaAppView view = plus_vita_app_view(flow->app);
	plus_vita_app_paint(font, pixels, pitch, &view);
	flow->dirty = false;
	return true;
}
