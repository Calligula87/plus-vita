// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <json_object.h>
#include <json_tokener.h>

struct plus_vita_app_t {
	PlusVitaAppView view;
	size_t welcome_selection;
	uint64_t next_operation, pending_operation;
	PlusVitaAppEffectKind waiting_for;
	PlusVitaCredential credential;
	PlusVitaCatalogEntry *catalogue;
	size_t catalogue_count, catalogue_selection;
	char catalogue_status[128];
	bool imported_valid, connected;
	uint32_t previous_buttons;
	int previous_direction;
	uint64_t repeat_at;
};

static void welcome(PlusVitaApp *app)
{
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_WELCOME,
		.title = "Your games. Your Vita.",
		.subtitle = "WELCOME TO PLUS VITA",
		.body = "Play your cloud games on the handheld you love.\nConnect your own Sony account to get started.",
		.hint = "Left stick  Move     X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_SETUP, "Get started", 64, 316, 500, 52 },
			{ PLUS_VITA_ACTION_ABOUT, "About Plus Vita", 64, 380, 500, 52 },
			{ PLUS_VITA_ACTION_EXIT, "Exit", 64, 444, 500, 52 }
		},
		.action_count = 3, .selected = app->welcome_selection
	};
	if(app->connected)
	{
		app->view.subtitle = "YOUR ACCOUNT IS CONNECTED";
		app->view.body = "Open your cloud catalogue and keep browsing.\nYour sign-in stays connected until you exit the app.";
		app->view.actions[0].id = PLUS_VITA_ACTION_CATALOGUE;
		app->view.actions[0].label = "Open catalogue";
	}
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
		.title = "Import your sign-in JSON.", .subtitle = "CONNECT TO SONY",
		.body = "Transfer plus-vita-auth.json to ux0:data with VitaShell.\nChoose Import JSON. The transferred file is then removed.\nYour account stays connected until you exit this app.",
		.hint = "X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_IMPORT, "Import JSON", 64, 316, 832, 52 },
			{ PLUS_VITA_ACTION_BACK, "Back", 64, 380, 832, 52 }
		}, .action_count = 2
	};
}

static void account_error(PlusVitaApp *app, const char *message)
{
	if(!app->connected) plus_vita_credential_clear(&app->credential);
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
			return "No sign-in JSON was found. Transfer plus-vita-auth.json\nto ux0:data with VitaShell, then choose Import JSON.";
		case PLUS_VITA_RESULT_STORAGE_ERROR:
			return "The sign-in JSON could not be read. Check it with VitaShell and try again.";
		case PLUS_VITA_RESULT_REJECTED:
			return "Your sign-in was rejected or has expired.\nSign in to Sony again, save a fresh JSON response and import it.";
		case PLUS_VITA_RESULT_NETWORK_ERROR:
			return "Sony could not be reached. Check Wi-Fi and try again.\nYour account details have not been displayed or logged.";
		case PLUS_VITA_RESULT_CATALOGUE_ERROR:
			return "The cloud catalogue is unavailable for this account right now.\nCheck your eligible subscription and try again later.";
		case PLUS_VITA_RESULT_BUSY:
			return "The previous request is still finishing.\nReturn to setup and try again in a moment.";
		default:
			return "Account setup could not be completed. Please try again.";
	}
}

static void json_space(const char **cursor, const char *end)
{
	while(*cursor < end && (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n')) ++*cursor;
}

static bool json_text_valid(const char *text, size_t size)
{
	/* json-c's strict mode still accepts NaN/Infinity and raw string controls.
	 * These are not JSON. Check their lexical forms before decoding values. */
	bool quoted = false, escaped = false;
	for(size_t i = 0; i < size; i++)
	{
		unsigned char c = (unsigned char)text[i];
		if(c < 0x20 && (quoted || (c != '\t' && c != '\r' && c != '\n'))) return false;
		if(quoted)
		{
			if(escaped) escaped = false;
			else if(c == '\\') escaped = true;
			else if(c == '"') quoted = false;
		}
		else if(c == '"') quoted = true;
		else if(c == 'N' || c == 'I') return false;
	}
	return !quoted;
}

static bool json_value(struct json_tokener *parser, const char **cursor, const char *end, struct json_object **value)
{
	json_tokener_reset(parser);
	*value = json_tokener_parse_ex(parser, *cursor, (int)(end - *cursor));
	if(json_tokener_get_error(parser) != json_tokener_success) return false;
	size_t consumed = json_tokener_get_parse_end(parser);
	if(!consumed) return false;
	*cursor += consumed;
	return true;
}

static bool parse_import(PlusVitaCredential *credential, const char *data, size_t size)
{
	if(!data || !size || size > 4096 || !json_text_valid(data, size)) return false;
	const char *p = data, *end = data + size;
	/* Accept a UTF-8 BOM emitted by some save-response tools. */
	if(size >= 3 && memcmp(p, "\xef\xbb\xbf", 3) == 0) p += 3;
	json_space(&p, end);
	if(p == end || *p++ != '{') return false;
	struct json_tokener *parser = json_tokener_new_ex(16);
	if(!parser) return false;
	json_tokener_set_flags(parser, JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS | JSON_TOKENER_VALIDATE_UTF8);
	PlusVitaCredential parsed = { 0 };
	struct json_object *value = NULL;
	bool found = false, valid = false;
	/* Parse root members individually: a normal object parse would silently
	 * overwrite duplicate npsso keys, including escaped spellings of the key.
	 * json-c still validates/decodes each complete key and nested value. */
	for(;;)
	{
		json_space(&p, end);
		if(p == end || *p != '"' || !json_value(parser, &p, end, &value)) goto done;
		bool npsso = json_object_is_type(value, json_type_string)
			&& json_object_get_string_len(value) == 5 && memcmp(json_object_get_string(value), "npsso", 5) == 0;
		json_object_put(value); value = NULL;
		json_space(&p, end);
		if(p == end || *p++ != ':') goto done;
		json_space(&p, end);
		if(!json_value(parser, &p, end, &value)) goto done;
		if(npsso)
		{
			if(found || !json_object_is_type(value, json_type_string) || json_object_get_string_len(value) != 64) goto done;
			const char *token = json_object_get_string(value);
			for(unsigned i = 0; i < 64; i++)
				if(!((token[i] >= 'A' && token[i] <= 'Z') || (token[i] >= 'a' && token[i] <= 'z')
					|| (token[i] >= '0' && token[i] <= '9'))) goto done;
			found = plus_vita_credential_set(&parsed, token, 64);
		}
		json_object_put(value); value = NULL;
		json_space(&p, end);
		if(p == end) goto done;
		if(*p == '}') { p++; break; }
		if(*p++ != ',') goto done;
	}
	json_space(&p, end);
	valid = found && p == end;
	if(valid) *credential = parsed;
done:
	json_object_put(value);
	json_tokener_free(parser);
	plus_vita_credential_clear(&parsed);
	return valid;
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
		.hint = "Left stick  Browse     O  Back"
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

static PlusVitaAppEffect import_account(PlusVitaApp *app)
{
	plus_vita_credential_clear(&app->credential);
	free(app->catalogue);
	app->catalogue = NULL;
	app->catalogue_count = app->catalogue_selection = 0;
	app->pending_operation = ++app->next_operation;
	app->waiting_for = PLUS_VITA_EFFECT_READ_IMPORT;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_AUTHORIZING, .title = "Connecting your account.",
		.subtitle = "ACCOUNT SETUP", .body = "Reading your sign-in JSON. You can cancel at any time.",
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
		.body = app->connected ? "Exiting ends this sign-in.\nTransfer your JSON again to sign in on the next launch."
			: "You can come back whenever you are ready to play.",
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
	if(event.kind == PLUS_VITA_EVENT_PAD)
	{
		PlusVitaAppPad pad = event.pad;
		uint32_t pressed = pad.buttons & ~app->previous_buttons;
		int direction = pad.left_y > 180 ? 1 : pad.left_y < 76 ? -1 : 0;
		bool navigate = direction && (direction != app->previous_direction || pad.time_us >= app->repeat_at);
		if(navigate) app->repeat_at = pad.time_us + (direction != app->previous_direction ? 400000u : 160000u);
		app->previous_direction = direction;
		app->previous_buttons = pad.buttons;
		if(pressed & PLUS_VITA_MENU_CIRCLE) event.kind = PLUS_VITA_EVENT_BACK;
		else if(pressed & PLUS_VITA_MENU_CROSS) event.kind = PLUS_VITA_EVENT_ACTIVATE;
		else if(navigate) event.kind = direction > 0 ? PLUS_VITA_EVENT_DOWN : PLUS_VITA_EVENT_UP;
		else return effect;
	}
	/* Cleanup is a storage obligation even after sign-in ownership is cancelled.
	 * Report failure without ever accepting the cancelled account result. */
	if(event.kind == PLUS_VITA_EVENT_IMPORT_READ && event.status == PLUS_VITA_RESULT_CLEANUP_ERROR)
	{
		account_error(app, "The app could not remove the imported token file.\nSign-in was not attempted. Remove it with VitaShell and try again.");
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_IMPORT_READ || event.kind == PLUS_VITA_EVENT_IMPORT_REMOVED
		|| event.kind == PLUS_VITA_EVENT_AUTHORIZED)
	{
		if(view->screen != PLUS_VITA_APP_AUTHORIZING || !event.operation
			|| event.operation != app->pending_operation) return effect;
		if(event.kind == PLUS_VITA_EVENT_IMPORT_READ && app->waiting_for == PLUS_VITA_EFFECT_READ_IMPORT)
		{
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				account_error(app, result_message(event.status));
				return effect;
			}
			app->imported_valid = parse_import(&app->credential, event.data, event.data_size);
			app->waiting_for = PLUS_VITA_EFFECT_REMOVE_IMPORT;
			view->body = "Finishing sign-in file cleanup.";
			effect = (PlusVitaAppEffect){ .kind = PLUS_VITA_EFFECT_REMOVE_IMPORT, .operation = event.operation };
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
				account_error(app, "The JSON file was removed, but its sign-in data was not valid.\nSave a fresh browser JSON response and transfer it again.");
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
				app->connected = true;
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
				.body = "1. Sign in at playstation.com on your phone or PC.\n2. Open ca.account.sony.com/api/v1/ssocookie.\n3. Save the response as plus-vita-auth.json.\nKeep this file private. Complete Sony's sign-in prompts.",
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
		if(!app->connected) plus_vita_credential_clear(&app->credential);
		if(view->screen == PLUS_VITA_APP_ERROR && !app->connected) setup_view(app);
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
		if(action == PLUS_VITA_ACTION_CATALOGUE)
		{
			app->welcome_selection = view->selected;
			catalogue_view(app);
		}
		else if(action == PLUS_VITA_ACTION_IMPORT)
			return import_account(app);
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
			if(!app->connected) plus_vita_credential_clear(&app->credential);
			if(view->screen == PLUS_VITA_APP_ERROR && !app->connected) setup_view(app);
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
