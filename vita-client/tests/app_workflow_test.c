// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	PlusVitaApp *app = plus_vita_app_create();
	assert(app);
	PlusVitaAppView view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_WELCOME);
	assert(view.action_count == 3);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_SETUP);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_ABOUT);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ABOUT);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_WELCOME);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_ABOUT);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_EXIT_CONFIRM);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_BACK);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_EXIT_CONFIRM);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app,
		(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(effect.kind == PLUS_VITA_EFFECT_EXIT);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CLOSED);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH, .x = 2, .y = 2 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH, .x = 200, .y = 400 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ABOUT);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(effect.kind == PLUS_VITA_EFFECT_PREPARE_SETUP && effect.operation != 0);
	uint64_t cancelled_operation = effect.operation;
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_PREPARING);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = cancelled_operation });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(effect.operation != cancelled_operation);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = cancelled_operation });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_PREPARING);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP_GUIDE);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_destroy(app);
	puts("app workflow: welcome, navigation, About, focus, exit, touch and cancelled/stale setup results passed");
	return 0;
}
