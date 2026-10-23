// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void menu_pad(PlusVitaApp *app, uint32_t buttons, unsigned char y, uint64_t time)
{
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_PAD,
		.pad = { .buttons = buttons, .left_x = 128, .left_y = y, .time_us = time } });
}

static void joystick_menu(void)
{
	PlusVitaApp *app = plus_vita_app_create();
	assert(app);
	/* Vita D-pad bits (up/right/down/left) must not navigate the UI. */
	menu_pad(app, 0xf0, 128, 0);
	assert(plus_vita_app_view(app).selected == 0);
	menu_pad(app, 0, 180, 1);
	assert(plus_vita_app_view(app).selected == 0);
	menu_pad(app, 0, 181, 100);
	assert(plus_vita_app_view(app).selected == 1);
	menu_pad(app, 0, 255, 400099);
	assert(plus_vita_app_view(app).selected == 1);
	menu_pad(app, 0, 255, 400100);
	assert(plus_vita_app_view(app).selected == 2);
	menu_pad(app, 0, 255, 560100);
	assert(plus_vita_app_view(app).selected == 3);
	menu_pad(app, 0, 76, 560101);
	assert(plus_vita_app_view(app).selected == 3);
	menu_pad(app, 0, 75, 560102);
	assert(plus_vita_app_view(app).selected == 2);
	menu_pad(app, 0, 128, 560103);
	menu_pad(app, 0, 255, 560104);
	assert(plus_vita_app_view(app).selected == 3);
	menu_pad(app, 0x2000, 128, 560105); /* Circle */
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_EXIT_CONFIRM);
	menu_pad(app, 0x2000, 128, 560104);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_EXIT_CONFIRM);
	menu_pad(app, 0, 128, 560105);
	menu_pad(app, 0x4000, 128, 560106); /* Cross: Stay here */
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	menu_pad(app, 0x4000, 128, 560107);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_destroy(app);
}

int main(void)
{
	joystick_menu();
	PlusVitaApp *app = plus_vita_app_create();
	assert(app);
	PlusVitaAppView view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_WELCOME);
	assert(view.action_count == 4);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_SETUP);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LOG_STATUS, .log_available = true });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(view.actions[view.selected].id == PLUS_VITA_ACTION_SETTINGS);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_SETTINGS);
	assert(view.action_count == 4 && view.actions[0].id == PLUS_VITA_ACTION_SETUP);
	assert(strstr(view.body, "ux0:data/plus-vita-allocation.log"));
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CONTROLS);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETTINGS);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_REPORTING && strstr(view.body, "Never share sign-in JSON"));
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LOG_STATUS, .log_available = false });
	assert(strstr(plus_vita_app_view(app).body, "unavailable"));
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
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
