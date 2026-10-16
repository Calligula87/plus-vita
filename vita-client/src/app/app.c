// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <stdlib.h>

struct plus_vita_app_t {
	PlusVitaAppView view;
	size_t welcome_selection;
	uint64_t next_operation, pending_operation;
};

static void welcome(PlusVitaApp *app)
{
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_WELCOME,
		.title = "Your games. Your Vita.",
		.subtitle = "WELCOME TO PLUS VITA",
		.body = "Play your cloud games on the handheld you love.\nConnect your own Sony account to get started.",
		.hint = "D-pad / stick  Move     X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_SETUP, "Get started", 64, 316, 500, 52 },
			{ PLUS_VITA_ACTION_ABOUT, "About Plus Vita", 64, 380, 500, 52 },
			{ PLUS_VITA_ACTION_EXIT, "Exit", 64, 444, 500, 52 }
		},
		.action_count = 3, .selected = app->welcome_selection
	};
}

PlusVitaApp *plus_vita_app_create(void)
{
	PlusVitaApp *app = calloc(1, sizeof(*app));
	if(!app) return NULL;
	welcome(app);
	return app;
}

void plus_vita_app_destroy(PlusVitaApp *app) { free(app); }
PlusVitaAppView plus_vita_app_view(const PlusVitaApp *app) { return app->view; }

static void confirm_exit(PlusVitaApp *app)
{
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_EXIT_CONFIRM,
		.title = "See you next time?", .subtitle = "EXIT PLUS VITA",
		.body = "You can come back whenever you are ready to play.",
		.hint = "X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_BACK, "Stay here", 64, 380, 500, 52 },
			{ PLUS_VITA_ACTION_EXIT, "Exit Plus Vita", 64, 444, 500, 52 }
		}, .action_count = 2
	};
}

PlusVitaAppEffect plus_vita_app_dispatch(PlusVitaApp *app, PlusVitaAppEvent event)
{
	PlusVitaAppView *view = &app->view;
	PlusVitaAppEffect effect = { .kind = PLUS_VITA_EFFECT_NONE };
	if(view->screen == PLUS_VITA_APP_CLOSED) return effect;
	if(event.kind == PLUS_VITA_EVENT_SETUP_READY)
	{
		if(view->screen == PLUS_VITA_APP_PREPARING && event.operation
			&& event.operation == app->pending_operation)
		{
			app->pending_operation = 0;
			*view = (PlusVitaAppView){
				.screen = PLUS_VITA_APP_SETUP,
				.title = "Bring your own account.", .subtitle = "CONNECT TO SONY",
				.body = "Sign in to Sony on your phone or computer.\nYour password stays with Sony.\n\nAccount import is not included in this startup preview.",
				.hint = "X  Select     O  Back",
				.actions = {{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 }},
				.action_count = 1
			};
		}
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_TOUCH)
	{
		for(size_t i = 0; i < view->action_count; i++)
		{
			const PlusVitaAppButton *button = &view->actions[i];
			if(event.x >= button->x && event.x < button->x + button->width
				&& event.y >= button->y && event.y < button->y + button->height)
			{
				view->selected = i;
				return plus_vita_app_dispatch(app,
					(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
			}
		}
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_DOWN && view->action_count)
		view->selected = (view->selected + 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_UP && view->action_count)
		view->selected = (view->selected + view->action_count - 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_BACK)
	{
		app->pending_operation = 0;
		if(view->screen == PLUS_VITA_APP_WELCOME)
		{
			app->welcome_selection = view->selected;
			confirm_exit(app);
		}
		else welcome(app);
	}
	else if(event.kind == PLUS_VITA_EVENT_ACTIVATE && view->action_count)
	{
		PlusVitaAppAction action = view->actions[view->selected].id;
		if(action == PLUS_VITA_ACTION_SETUP)
		{
			app->welcome_selection = view->selected;
			app->pending_operation = ++app->next_operation;
			*view = (PlusVitaAppView){
				.screen = PLUS_VITA_APP_PREPARING,
				.title = "Getting things ready.", .subtitle = "ACCOUNT SETUP",
				.body = "Preparing account setup. You can cancel at any time.",
				.hint = "O  Cancel",
				.actions = {{ PLUS_VITA_ACTION_BACK, "Cancel", 64, 444, 500, 52 }},
				.action_count = 1
			};
			effect = (PlusVitaAppEffect){ PLUS_VITA_EFFECT_PREPARE_SETUP, app->pending_operation };
		}
		else if(action == PLUS_VITA_ACTION_ABOUT)
		{
			app->welcome_selection = view->selected;
			*view = (PlusVitaAppView){
				.screen = PLUS_VITA_APP_ABOUT,
				.title = "Made for your handheld.", .subtitle = "ABOUT PLUS VITA",
				.body = "An independent cloud gaming client for PlayStation Vita.\nUse your own Sony account and eligible subscription.\n\nGraphical startup preview. Not a public release.",
				.hint = "X  Select     O  Back",
				.actions = {{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 }},
				.action_count = 1
			};
		}
		else if(action == PLUS_VITA_ACTION_BACK)
		{
			app->pending_operation = 0;
			welcome(app);
		}
		else if(action == PLUS_VITA_ACTION_EXIT)
		{
			if(view->screen == PLUS_VITA_APP_EXIT_CONFIRM)
			{
				*view = (PlusVitaAppView){ .screen = PLUS_VITA_APP_CLOSED };
				effect.kind = PLUS_VITA_EFFECT_EXIT;
			}
			else
			{
				app->welcome_selection = view->selected;
				confirm_exit(app);
			}
		}
	}
	return effect;
}
