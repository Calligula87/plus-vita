// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

struct plus_vita_app_t {
	PlusVitaAppView view;
	size_t welcome_selection;
	uint64_t next_operation, pending_operation;
	PlusVitaAppEffectKind waiting_for;
	PlusVitaCredential credential;
	PlusVitaCatalogEntry *catalogue;
	size_t catalogue_count, catalogue_selection;
	char catalogue_status[128];
	bool imported_valid;
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

void plus_vita_app_destroy(PlusVitaApp *app)
{
	if(!app) return;
	plus_vita_credential_clear(&app->credential);
	free(app->catalogue);
	free(app);
}
PlusVitaAppView plus_vita_app_view(const PlusVitaApp *app) { return app->view; }

static void setup_view(PlusVitaApp *app)
{
	app->pending_operation = 0;
	app->waiting_for = PLUS_VITA_EFFECT_NONE;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_SETUP,
		.title = "Bring your own account.", .subtitle = "CONNECT TO SONY",
		.body = "Create a JSON file: {\"npsso\":\"your 64-character token\"}\nTransfer to ux0:data/plus-vita-auth.json with VitaShell.\nThe file is removed after import. Sign-in is not saved.",
		.hint = "X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_IMPORT, "Import token file", 64, 316, 832, 52 },
			{ PLUS_VITA_ACTION_ENTER_TOKEN, "Enter token", 64, 380, 832, 52 },
			{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 }
		}, .action_count = 3
	};
}

static void account_error(PlusVitaApp *app, const char *message)
{
	plus_vita_credential_clear(&app->credential);
	app->pending_operation = 0;
	app->waiting_for = PLUS_VITA_EFFECT_NONE;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_ERROR, .title = "Let's try that again.",
		.subtitle = "ACCOUNT SETUP", .body = message, .hint = "X  Select     O  Back",
		.actions = {{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 }}, .action_count = 1
	};
}

static const char *result_message(PlusVitaAppResult result)
{
	switch(result)
	{
		case PLUS_VITA_RESULT_MISSING:
			return "No token file was found. Transfer it with VitaShell\nor return to setup and enter the token with the Vita keyboard.";
		case PLUS_VITA_RESULT_STORAGE_ERROR:
			return "The token file could not be read. Check it with VitaShell and try again.";
		case PLUS_VITA_RESULT_REJECTED:
			return "Your sign-in was rejected or has expired.\nSign in to Sony again and import a fresh token.";
		case PLUS_VITA_RESULT_NETWORK_ERROR:
			return "Sony could not be reached. Check Wi-Fi and try again.\nYour account details have not been displayed or logged.";
		case PLUS_VITA_RESULT_CATALOGUE_ERROR:
			return "The cloud catalogue is unavailable for this account right now.\nCheck your eligible subscription and try again later.";
		case PLUS_VITA_RESULT_BUSY:
			return "The previous request is still finishing.\nReturn to setup and try again in a moment.";
		case PLUS_VITA_RESULT_INPUT_ERROR:
			return "The Vita keyboard could not be opened.\nReturn to setup and try again, or import a file with VitaShell.";
		default:
			return "Account setup could not be completed. Please try again.";
	}
}

static bool parse_import(PlusVitaCredential *credential, const char *data, size_t size)
{
	if(!data || size > 4096) return false;
	const char *p = data, *end = data + size;
	#define SKIP_SPACE() while(p < end && isspace((unsigned char)*p)) p++
	SKIP_SPACE();
	if(p == end || *p++ != '{') return false;
	SKIP_SPACE();
	if(end - p < 7 || memcmp(p, "\"npsso\"", 7)) return false;
	p += 7;
	SKIP_SPACE();
	if(p == end || *p++ != ':') return false;
	SKIP_SPACE();
	if(p == end || *p++ != '"' || end - p < 65) return false;
	const char *token = p;
	for(unsigned i = 0; i < 64; i++) if(!isalnum((unsigned char)*p++)) return false;
	if(*p++ != '"') return false;
	SKIP_SPACE();
	if(p == end || *p++ != '}') return false;
	SKIP_SPACE();
	#undef SKIP_SPACE
	return p == end && plus_vita_credential_set(credential, token, 64);
}

static int compare_titles(const void *left, const void *right)
{
	const unsigned char *a = (const unsigned char *)((const PlusVitaCatalogEntry *)left)->name;
	const unsigned char *b = (const unsigned char *)((const PlusVitaCatalogEntry *)right)->name;
	while(*a && tolower(*a) == tolower(*b)) { a++; b++; }
	return tolower(*a) - tolower(*b);
}

static void catalogue_view(PlusVitaApp *app)
{
	PlusVitaAppView view = {
		.screen = PLUS_VITA_APP_CATALOGUE, .title = "Choose your next game.",
		.subtitle = "CLOUD CATALOGUE", .body = app->catalogue_status,
		.hint = "D-pad / stick  Browse     O  Back"
	};
	snprintf(app->catalogue_status, sizeof(app->catalogue_status),
		"%u cloud games available.\nYour sign-in is kept for this app session only.", (unsigned)app->catalogue_count);
	if(!app->catalogue_count)
	{
		view.title = "No cloud games found.";
		view.actions[0] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 };
		view.action_count = 1;
	}
	else
	{
		size_t first = app->catalogue_selection / 3 * 3;
		for(size_t i = first; i < app->catalogue_count && i < first + 3; i++)
			view.actions[view.action_count++] = (PlusVitaAppButton){
				PLUS_VITA_ACTION_GAME, app->catalogue[i].name, 64, 316 + (int)(i - first) * 64, 832, 52 };
		view.selected = app->catalogue_selection - first;
	}
	app->view = view;
}

static PlusVitaAppEffect import_account(PlusVitaApp *app, bool manual)
{
	plus_vita_credential_clear(&app->credential);
	free(app->catalogue);
	app->catalogue = NULL;
	app->catalogue_count = app->catalogue_selection = 0;
	app->pending_operation = ++app->next_operation;
	app->waiting_for = manual ? PLUS_VITA_EFFECT_ENTER_TOKEN : PLUS_VITA_EFFECT_READ_IMPORT;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_AUTHORIZING, .title = "Connecting your account.",
		.subtitle = "ACCOUNT SETUP", .body = manual ? "Enter your sign-in token using the Vita keyboard."
			: "Reading your token file. You can cancel at any time.",
		.hint = "O  Cancel", .actions = {{ PLUS_VITA_ACTION_BACK, "Cancel", 64, 444, 500, 52 }},
		.action_count = 1
	};
	app->view.operation = app->pending_operation;
	return (PlusVitaAppEffect){ .kind = app->waiting_for, .operation = app->pending_operation };
}

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
	if(event.kind == PLUS_VITA_EVENT_IMPORT_READ || event.kind == PLUS_VITA_EVENT_IMPORT_REMOVED
		|| event.kind == PLUS_VITA_EVENT_AUTHORIZED || event.kind == PLUS_VITA_EVENT_TOKEN_ENTERED)
	{
		if(view->screen != PLUS_VITA_APP_AUTHORIZING || !event.operation
			|| event.operation != app->pending_operation) return effect;
		if((event.kind == PLUS_VITA_EVENT_IMPORT_READ && app->waiting_for == PLUS_VITA_EFFECT_READ_IMPORT)
			|| (event.kind == PLUS_VITA_EVENT_TOKEN_ENTERED && app->waiting_for == PLUS_VITA_EFFECT_ENTER_TOKEN))
		{
			if(event.kind == PLUS_VITA_EVENT_TOKEN_ENTERED && event.status == PLUS_VITA_RESULT_CANCELLED)
			{
				plus_vita_credential_clear(&app->credential);
				setup_view(app);
				return effect;
			}
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				account_error(app, result_message(event.status));
				return effect;
			}
			bool valid;
			if(event.kind == PLUS_VITA_EVENT_TOKEN_ENTERED)
			{
				valid = event.data && event.data_size == 64;
				for(size_t i = 0; valid && i < event.data_size; i++) valid = isalnum((unsigned char)event.data[i]) != 0;
				if(valid) valid = plus_vita_credential_set(&app->credential, event.data, event.data_size);
			}
			else valid = parse_import(&app->credential, event.data, event.data_size);
			app->imported_valid = valid;
			if(!valid && event.kind == PLUS_VITA_EVENT_TOKEN_ENTERED)
				account_error(app, "The sign-in token is not valid. Use exactly 64 letters and numbers.");
			else
			{
				app->waiting_for = PLUS_VITA_EFFECT_REMOVE_IMPORT;
				view->body = "Removing the imported token file before sign-in.";
				effect = (PlusVitaAppEffect){ .kind = PLUS_VITA_EFFECT_REMOVE_IMPORT, .operation = event.operation };
			}
		}
		else if(event.kind == PLUS_VITA_EVENT_IMPORT_REMOVED && app->waiting_for == PLUS_VITA_EFFECT_REMOVE_IMPORT)
		{
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				account_error(app, "The app could not remove the imported token file.\nSign-in was not attempted. Remove it with VitaShell and try again.");
				return effect;
			}
			if(!app->imported_valid)
			{
				account_error(app, "The token file was removed, but its format was not valid.\nCreate a new file using the setup guide, or enter the token.");
				return effect;
			}
			app->waiting_for = PLUS_VITA_EFFECT_AUTHORIZE;
			view->body = "Checking your sign-in and loading cloud games.\nYou can cancel while this is working.";
			effect = (PlusVitaAppEffect){ .kind = PLUS_VITA_EFFECT_AUTHORIZE, .operation = event.operation, .credential = app->credential };
		}
		else if(event.kind == PLUS_VITA_EVENT_AUTHORIZED && app->waiting_for == PLUS_VITA_EFFECT_AUTHORIZE)
		{
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				account_error(app, result_message(event.status));
				return effect;
			}
			if(event.entry_count > 4096 || (event.entry_count && !event.entries))
				account_error(app, "The catalogue could not be loaded. Please try again later.");
			else
			{
				if(event.entry_count)
				{
					app->catalogue = malloc(event.entry_count * sizeof(*app->catalogue));
					if(!app->catalogue) { account_error(app, "Not enough memory to load cloud games."); return effect; }
					memcpy(app->catalogue, event.entries, event.entry_count * sizeof(*app->catalogue));
					for(size_t i = 0; i < event.entry_count; i++) app->catalogue[i].name[PLUS_VITA_CATALOG_NAME_MAX - 1] = 0;
					qsort(app->catalogue, event.entry_count, sizeof(*app->catalogue), compare_titles);
				}
				app->catalogue_count = event.entry_count;
				app->pending_operation = 0;
				app->waiting_for = PLUS_VITA_EFFECT_NONE;
				catalogue_view(app);
			}
		}
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_SETUP_READY)
	{
		if(view->screen == PLUS_VITA_APP_PREPARING && event.operation
			&& event.operation == app->pending_operation)
		{
			app->pending_operation = 0;
			*view = (PlusVitaAppView){
				.screen = PLUS_VITA_APP_SETUP_GUIDE, .title = "Sign in on your phone or PC.",
				.subtitle = "YOUR SONY ACCOUNT", .hint = "X  Select     O  Back",
				.body = "1. Sign in at playstation.com in your browser.\n2. Open ca.account.sony.com/api/v1/ssocookie.\n3. Copy only the npsso value. Keep it private.\nComplete Sony's password, passkey or 2-step prompts.",
				.actions = {
					{ PLUS_VITA_ACTION_CONTINUE, "Continue", 64, 380, 832, 52 },
					{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 }
				}, .action_count = 2
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
				if(view->screen == PLUS_VITA_APP_CATALOGUE && button->id == PLUS_VITA_ACTION_GAME)
					app->catalogue_selection = app->catalogue_selection / 3 * 3 + i;
				return plus_vita_app_dispatch(app,
					(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
			}
		}
		return effect;
	}
	if(view->screen == PLUS_VITA_APP_CATALOGUE && app->catalogue_count
		&& (event.kind == PLUS_VITA_EVENT_DOWN || event.kind == PLUS_VITA_EVENT_UP))
	{
		if(event.kind == PLUS_VITA_EVENT_DOWN) app->catalogue_selection = (app->catalogue_selection + 1) % app->catalogue_count;
		else app->catalogue_selection = (app->catalogue_selection + app->catalogue_count - 1) % app->catalogue_count;
		catalogue_view(app);
	}
	else if(event.kind == PLUS_VITA_EVENT_DOWN && view->action_count)
		view->selected = (view->selected + 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_UP && view->action_count)
		view->selected = (view->selected + view->action_count - 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_BACK)
	{
		app->pending_operation = 0;
		app->waiting_for = PLUS_VITA_EFFECT_NONE;
		plus_vita_credential_clear(&app->credential);
		if(view->screen == PLUS_VITA_APP_ERROR) setup_view(app);
		else if(view->screen == PLUS_VITA_APP_WELCOME)
		{
			app->welcome_selection = view->selected;
			confirm_exit(app);
		}
		else welcome(app);
	}
	else if(event.kind == PLUS_VITA_EVENT_ACTIVATE && view->action_count)
	{
		PlusVitaAppAction action = view->actions[view->selected].id;
		if(action == PLUS_VITA_ACTION_IMPORT || action == PLUS_VITA_ACTION_ENTER_TOKEN)
			return import_account(app, action == PLUS_VITA_ACTION_ENTER_TOKEN);
		else if(action == PLUS_VITA_ACTION_CONTINUE) setup_view(app);
		else if(action == PLUS_VITA_ACTION_SETUP)
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
			view->operation = app->pending_operation;
			effect = (PlusVitaAppEffect){ .kind = PLUS_VITA_EFFECT_PREPARE_SETUP, .operation = app->pending_operation };
		}
		else if(action == PLUS_VITA_ACTION_ABOUT)
		{
			app->welcome_selection = view->selected;
			*view = (PlusVitaAppView){
				.screen = PLUS_VITA_APP_ABOUT,
				.title = "Made for your handheld.", .subtitle = "ABOUT PLUS VITA",
				.body = "An independent cloud gaming client for PlayStation Vita.\nUse your own Sony account and eligible subscription.\n\nDevelopment build. Not a public release.",
				.hint = "X  Select     O  Back",
				.actions = {{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 }},
				.action_count = 1
			};
		}
		else if(action == PLUS_VITA_ACTION_BACK)
		{
			app->pending_operation = 0;
			app->waiting_for = PLUS_VITA_EFFECT_NONE;
			plus_vita_credential_clear(&app->credential);
			if(view->screen == PLUS_VITA_APP_ERROR) setup_view(app);
			else welcome(app);
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
