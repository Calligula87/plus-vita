// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include "utf8.h"
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
	uint64_t account_epoch;
	PlusVitaAppEffectKind waiting_for;
	PlusVitaCredential credential;
	PlusVitaCatalogEntry *catalogue;
	size_t catalogue_count, catalogue_selection;
	size_t matches[4096], matches_count;
	bool search_focus;
	char search[129], search_button[160], catalogue_status[512], detail_status[512];
	bool imported_valid, connected;
	bool remember, saved, started, retry_account, retry_save, retry_delete, forgetting, expired;
	PlusVitaAppScreen storage_return;
	char label[129], online_id[129], account_status[512];
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
		app->view.body = app->saved ? "Your sign-in is remembered on this Vita.\nOpen your cloud catalogue and keep browsing."
			: "Open your cloud catalogue and keep browsing.\nYour sign-in stays connected until you exit the app.";
		app->view.actions[0].id = PLUS_VITA_ACTION_CATALOGUE;
		app->view.actions[0].label = "Open catalogue";
	}
	else if(app->saved)
	{
		app->view.body = "Your saved sign-in is available.\nReconnect when you are ready.";
		app->view.actions[0].id = PLUS_VITA_ACTION_RESTORE;
		app->view.actions[0].label = "Reconnect saved account";
	}
	if(app->connected || app->saved)
	{
		app->view.actions[0].y = 284;
		app->view.actions[1] = (PlusVitaAppButton){ PLUS_VITA_ACTION_ACCOUNT, "Account", 64, 340, 500, 48 };
		app->view.actions[2] = (PlusVitaAppButton){ PLUS_VITA_ACTION_ABOUT, "About Plus Vita", 64, 396, 500, 48 };
		app->view.actions[3] = (PlusVitaAppButton){ PLUS_VITA_ACTION_EXIT, "Exit", 64, 452, 500, 48 };
		app->view.actions[0].height = 48;
		app->view.action_count = 4;
		if(app->view.selected >= 4) app->view.selected = 0;
	}
}

PlusVitaApp *plus_vita_app_create(void)
{
	PlusVitaApp *app = calloc(1, sizeof(*app));
	if(!app) return NULL;
	app->remember = true;
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
PlusVitaAppView plus_vita_app_view(const PlusVitaApp *app) { PlusVitaAppView view=app->view; view.account_epoch=app->account_epoch; return view; }

static void setup_view(PlusVitaApp *app)
{
	app->pending_operation = 0;
	app->waiting_for = PLUS_VITA_EFFECT_NONE;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_SETUP,
		.title = "Import your sign-in JSON.", .subtitle = "CONNECT TO SONY",
		.body = "Save plus-vita-auth.json in ux0:data with VitaShell.\nThe JSON stays there and can sign you in next launch.\nRemember Off pauses automatic sign-in; files are locally readable.",
		.hint = "X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_IMPORT, "Import JSON", 64, 316, 832, 52 },
			{ PLUS_VITA_ACTION_REMEMBER, app->remember ? "Remember this account: On" : "Remember this account: Off", 64, 380, 832, 52 },
			{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 }
		}, .action_count = 3
	};
}

static void account_error(PlusVitaApp *app, const char *message)
{
	app->pending_operation = 0;
	app->waiting_for = PLUS_VITA_EFFECT_NONE;
	app->view = (PlusVitaAppView){
		.screen = PLUS_VITA_APP_ERROR, .title = "Let's try that again.",
		.subtitle = "ACCOUNT SETUP", .body = message, .hint = "X  Select     O  Back",
		.actions = {{ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 }}, .action_count = 1
	};
	if(app->retry_account || app->retry_save || app->retry_delete)
	{
		app->view.actions[0] = (PlusVitaAppButton){ PLUS_VITA_ACTION_RETRY, app->retry_delete ? "Retry removing saved account" : "Retry", 64, 380, 832, 52 };
		app->view.actions[1] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 };
		app->view.action_count = 2;
	}
	else if(app->saved || app->forgetting)
	{
		app->view.actions[0] = (PlusVitaAppButton){ app->forgetting ? PLUS_VITA_ACTION_CONFIRM_FORGET : PLUS_VITA_ACTION_FORGET,
			app->forgetting ? "Retry removing account" : "Forget saved account", 64, 380, 832, 52 };
		app->view.actions[1] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 };
		app->view.action_count = 2;
	}
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

static const char *account_name(const PlusVitaApp *app)
{
	return app->label[0] ? app->label : app->online_id[0] ? app->online_id : "Your account.";
}

static PlusVitaAppEffect operation(PlusVitaApp *app, PlusVitaAppEffectKind kind, const char *message, bool cancel)
{
	app->waiting_for = kind;
	app->pending_operation = ++app->next_operation;
	app->view = (PlusVitaAppView){ .screen = PLUS_VITA_APP_AUTHORIZING,
		.title = account_name(app), .subtitle = app->label[0] ? "ACCOUNT LABEL" : "ACCOUNT", .body = message,
		.hint = cancel ? "O  Cancel" : "Please wait", .operation = app->pending_operation };
	if(cancel) { app->view.actions[0] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Cancel", 64, 444, 832, 52 }; app->view.action_count = 1; }
	if(kind==PLUS_VITA_EFFECT_EDIT_SEARCH) { app->view.title="Search your games."; app->view.subtitle="CATALOGUE SEARCH"; }
	PlusVitaAppEffect effect = { .kind = kind, .operation = app->pending_operation, .credential = app->credential, .forget = app->forgetting };
	memcpy(effect.label, app->label, sizeof(effect.label));
	return effect;
}

static void json_space(const char **cursor, const char *end)
{
	while(*cursor < end && (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n')) ++*cursor;
}

static bool json_text_valid(const char *text, size_t size)
{
	const char *cursor = text;
	while(cursor < text + size) if(plus_vita_utf8_next(&cursor, text + size) == UINT32_MAX) return false;
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

static bool parse_import_ascii(PlusVitaCredential *credential, const char *data, size_t size, char *label)
{
	const char *p = data, *end = data + size;
	/* Accept a UTF-8 BOM emitted by some save-response tools. */
	if(size >= 3 && memcmp(p, "\xef\xbb\xbf", 3) == 0) p += 3;
	json_space(&p, end);
	if(p == end || *p++ != '{') return false;
	struct json_tokener *parser = json_tokener_new_ex(16);
	if(!parser) return false;
	/* UTF-8 is validated once above, independently of tokener versions and
	 * incremental parsing of individual root keys/values. */
	json_tokener_set_flags(parser, JSON_TOKENER_STRICT | JSON_TOKENER_ALLOW_TRAILING_CHARS);
	PlusVitaCredential parsed = { 0 };
	struct json_object *value = NULL;
	bool found = false, valid = false;
	bool label_found = false;
	char parsed_label[129] = { 0 };
	/* Parse root members individually: a normal object parse would silently
	 * overwrite duplicate npsso keys, including escaped spellings of the key.
	 * json-c still validates/decodes each complete key and nested value. */
	for(;;)
	{
		json_space(&p, end);
		if(p == end || *p != '"' || !json_value(parser, &p, end, &value)) goto done;
		bool npsso = json_object_is_type(value, json_type_string)
			&& json_object_get_string_len(value) == 5 && memcmp(json_object_get_string(value), "npsso", 5) == 0;
		bool label_key = label && json_object_is_type(value, json_type_string)
			&& json_object_get_string_len(value) == 5 && memcmp(json_object_get_string(value), "label", 5) == 0;
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
		if(label_key)
		{
			if(label_found || !json_object_is_type(value, json_type_string) || json_object_get_string_len(value) > 128) goto done;
			int length = json_object_get_string_len(value);
			const char *name = json_object_get_string(value);
			if(!plus_vita_utf8_label(name, (size_t)length)) goto done;
			memcpy(parsed_label, name, (size_t)length);
			label_found = true;
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
	if(valid && label) memcpy(label, parsed_label, sizeof(parsed_label));
done:
	json_object_put(value);
	json_tokener_free(parser);
	plus_vita_credential_clear(&parsed);
	return valid;
}

static bool parse_import(PlusVitaCredential *credential, const char *data, size_t size, char *label)
{
	if(!data || !size || size > 4096 || !json_text_valid(data, size)) return false;
	/* Normalize UTF-8 to JSON's UTF-16 escapes before incremental parsing.
	 * This keeps raw Unicode handling identical across host/system json-c versions. */
	char *normalized = malloc(size * 3 + 1);
	if(!normalized) return false;
	const char *p = data, *end = data + size;
	if(size >= 3 && memcmp(p, "\xef\xbb\xbf", 3) == 0) p += 3;
	char *out = normalized;
	while(p < end)
	{
		uint32_t code = plus_vita_utf8_next(&p, end);
		if(code < 0x80) *out++ = (char)code;
		else if(code <= 0xffff) { snprintf(out, 7, "\\u%04x", (unsigned)code); out += 6; }
		else { code -= 0x10000; snprintf(out, 13, "\\u%04x\\u%04x", 0xd800u + (unsigned)((code >> 10) & 1023), 0xdc00u + (unsigned)(code & 1023)); out += 12; }
	}
	*out = 0;
	bool valid = parse_import_ascii(credential, normalized, (size_t)(out - normalized), label);
	volatile char *wipe = normalized;
	for(size_t i = 0; i < size * 3 + 1; i++) wipe[i] = 0;
	free(normalized);
	return valid;
}

static int compare_titles(const void *left, const void *right)
{
	const unsigned char *a = (const unsigned char *)((const PlusVitaCatalogEntry *)left)->name;
	const unsigned char *b = (const unsigned char *)((const PlusVitaCatalogEntry *)right)->name;
	while(*a && tolower(*a) == tolower(*b)) { a++; b++; }
	return tolower(*a) - tolower(*b);
}

static void account_view(PlusVitaApp *app)
{
	snprintf(app->account_status, sizeof(app->account_status),
		"Remember: %s. Local files are readable with file access.\nAccount label (entered by you): %s\nSony account name: %s",
		app->saved ? "On" : "Off", app->label[0] ? app->label : "Not set", app->online_id[0] ? app->online_id : "Unavailable");
	app->view = (PlusVitaAppView){ .screen = PLUS_VITA_APP_ACCOUNT, .title = account_name(app),
		.subtitle = "ACCOUNT", .body = app->account_status, .hint = "Left stick  Move     X  Select     O  Back",
		.actions = {
			{ PLUS_VITA_ACTION_REMEMBER, app->retry_delete ? "Retry removing saved account" : app->saved ? "Remember: On (turn off)" : "Remember: Off (turn on)", 64, 284, 832, 48 },
			{ PLUS_VITA_ACTION_LABEL, "Edit account label", 64, 340, 832, 48 },
			{ PLUS_VITA_ACTION_FORGET, "Forget account", 64, 396, 832, 48 },
			{ PLUS_VITA_ACTION_BACK, "Back", 64, 452, 832, 48 }
		}, .action_count = 4 };
	if(!app->connected)
	{
		app->view.actions[0] = app->view.actions[2];
		app->view.actions[0].y = 396;
		app->view.actions[1] = app->view.actions[3];
		app->view.action_count = 2;
	}
}

static void clear_runtime(PlusVitaApp *app)
{
	app->account_epoch++;
	plus_vita_credential_clear(&app->credential);
	free(app->catalogue);
	app->catalogue = NULL;
	app->catalogue_count = app->catalogue_selection = 0;
	memset(app->label, 0, sizeof(app->label));
	memset(app->online_id, 0, sizeof(app->online_id));
	memset(app->account_status, 0, sizeof(app->account_status));
	memset(app->catalogue_status, 0, sizeof(app->catalogue_status));
	memset(app->search, 0, sizeof(app->search));
	memset(app->search_button,0,sizeof(app->search_button)); memset(app->detail_status,0,sizeof(app->detail_status));
	app->matches_count = 0; app->search_focus = false;
	app->connected = app->retry_account = app->retry_save = false;
}

static PlusVitaAppEffect remove_saved(PlusVitaApp *app, bool forget, bool expired)
{
	app->forgetting = forget;
	app->expired = expired;
	app->storage_return = PLUS_VITA_APP_ACCOUNT;
	if(forget || expired) clear_runtime(app);
	return operation(app, PLUS_VITA_EFFECT_DELETE_ACCOUNT, "Removing saved sign-in from this Vita.", false);
}

static bool title_matches(const char *title, const char *query)
{
	if(!*query) return true;
	for(; *title; title++) {
		const unsigned char *a=(const unsigned char *)title, *b=(const unsigned char *)query;
		while(*a && *b && tolower(*a)==tolower(*b)) { a++; b++; }
		if(!*b) return true;
	}
	return false;
}

static int menu_direction(const PlusVitaApp *app, PlusVitaAppPad pad)
{
	int axis = pad.left_y;
	if(app->view.screen == PLUS_VITA_APP_CATALOGUE
		&& abs((int)pad.left_x - 128) > abs(axis - 128)) axis = pad.left_x;
	return axis > 180 ? 1 : axis < 76 ? -1 : 0;
}

static void filter_catalogue(PlusVitaApp *app)
{
	app->matches_count = 0;
	for(size_t i=0;i<app->catalogue_count;i++) if(title_matches(app->catalogue[i].name,app->search)) app->matches[app->matches_count++]=i;
	app->catalogue_selection = 0;
}

static void catalogue_view(PlusVitaApp *app)
{
	PlusVitaAppView view = {
		.screen = PLUS_VITA_APP_CATALOGUE, .title = "Choose your next game.",
		.subtitle = "CLOUD CATALOGUE", .body = app->catalogue_status,
		.hint = "Left stick  Browse     X  Details     Triangle  Search     O  Back"
	};
	snprintf(app->catalogue_status, sizeof(app->catalogue_status),
		"%u of %u games%s", (unsigned)app->matches_count, (unsigned)app->catalogue_count,
		app->search[0] ? " match your search." : " available.");
	if(!app->catalogue_count)
	{
		view.title = "No cloud games found.";
		view.actions[0] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 500, 52 };
		view.action_count = 1;
	}
	else if(!app->matches_count)
	{
		view.title = "No search results.";
		view.actions[0] = (PlusVitaAppButton){ PLUS_VITA_ACTION_SEARCH, "Edit search", 64, 228, 832, 42 };
		view.actions[1] = (PlusVitaAppButton){ PLUS_VITA_ACTION_CLEAR_SEARCH, "Clear search", 64, 380, 832, 52 };
		view.actions[2] = (PlusVitaAppButton){ PLUS_VITA_ACTION_BACK, "Back", 64, 444, 832, 52 };
		view.action_count = 3;
	}
	else
	{
		size_t first = app->catalogue_selection / 3 * 3;
		for(size_t i = first; i < app->matches_count && i < first + 3; i++) {
			const PlusVitaCatalogEntry *game=&app->catalogue[app->matches[i]];
			view.games[view.action_count]=game;
			view.actions[view.action_count++] = (PlusVitaAppButton){
				PLUS_VITA_ACTION_GAME, game->name, 64 + (int)(i-first)*286, 282, 260, 204 };
		}
		snprintf(app->search_button,sizeof(app->search_button),"Search: %s",app->search[0] ? app->search : "All games");
		size_t search_index = view.action_count;
		view.actions[view.action_count++] = (PlusVitaAppButton){PLUS_VITA_ACTION_SEARCH,app->search_button,64,228,832,42};
		if(app->matches_count > 3) {
			view.actions[view.action_count-1].width = 548;
			view.actions[view.action_count++] = (PlusVitaAppButton){PLUS_VITA_ACTION_PREVIOUS_PAGE,"Prev",624,228,128,42};
			view.actions[view.action_count++] = (PlusVitaAppButton){PLUS_VITA_ACTION_NEXT_PAGE,"Next",764,228,132,42};
		}
		view.selected = app->catalogue_selection - first;
		if(app->search_focus) view.selected = search_index;
	}
	app->view = view;
}

static void detail_view(PlusVitaApp *app, const char *notice)
{
	const PlusVitaCatalogEntry *game=&app->catalogue[app->matches[app->catalogue_selection]];
	snprintf(app->detail_status,sizeof(app->detail_status),"Platform: %s\n%s",game->platform[0] ? game->platform : "Not specified",
		notice ? notice : game->launch_id_valid ? "Cloud catalogue title. Streaming eligibility is checked at launch." : "Play unavailable: required launch information is missing.");
	app->view=(PlusVitaAppView){.screen=PLUS_VITA_APP_DETAILS,.title=game->name,.subtitle="GAME DETAILS",.body=app->detail_status,.hint="X  Select     O  Back",.detail=game,
		.actions={{game->launch_id_valid ? PLUS_VITA_ACTION_PLAY : PLUS_VITA_ACTION_DISABLED,game->launch_id_valid ? "Play" : "Play unavailable",64,380,832,52},{PLUS_VITA_ACTION_BACK,"Back",64,444,832,52}},.action_count=2};
}

static PlusVitaAppEffect import_account(PlusVitaApp *app)
{
	clear_runtime(app);
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
		.body = app->saved ? "Your sign-in is remembered on this Vita.\nThe app will validate it again next time you open it."
			: app->connected ? "Exiting ends this sign-in.\nTurn on Remember in Account to keep it for the next launch."
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
	if(event.kind == PLUS_VITA_EVENT_START)
	{
		if(app->started) return effect;
		app->started = true;
		return operation(app, PLUS_VITA_EFFECT_LOAD_ACCOUNT, "Checking for a remembered account.", true);
	}
	if(event.kind == PLUS_VITA_EVENT_PAD_RESET)
	{
		app->previous_buttons = event.pad.buttons;
		app->previous_direction = menu_direction(app, event.pad);
		app->repeat_at = event.pad.time_us + 400000u;
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_PAD)
	{
		PlusVitaAppPad pad = event.pad;
		uint32_t pressed = pad.buttons & ~app->previous_buttons;
		int direction = menu_direction(app, pad);
		bool navigate = direction && (direction != app->previous_direction || pad.time_us >= app->repeat_at);
		if(navigate) app->repeat_at = pad.time_us + (direction != app->previous_direction ? 400000u : 160000u);
		app->previous_direction = direction;
		app->previous_buttons = pad.buttons;
		if(pressed & PLUS_VITA_MENU_CIRCLE) event.kind = PLUS_VITA_EVENT_BACK;
		else if(pressed & PLUS_VITA_MENU_TRIANGLE) event.kind = PLUS_VITA_EVENT_SEARCH;
		else if(pressed & PLUS_VITA_MENU_CROSS) event.kind = PLUS_VITA_EVENT_ACTIVATE;
		else if(navigate) event.kind = direction > 0 ? PLUS_VITA_EVENT_DOWN : PLUS_VITA_EVENT_UP;
		else return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_IMPORT_READ
		|| event.kind == PLUS_VITA_EVENT_AUTHORIZED || event.kind == PLUS_VITA_EVENT_ACCOUNT_LOADED
		|| event.kind == PLUS_VITA_EVENT_ACCOUNT_SAVED || event.kind == PLUS_VITA_EVENT_ACCOUNT_DELETED
		|| event.kind == PLUS_VITA_EVENT_AUTO_ENABLED)
	{
		if(view->screen != PLUS_VITA_APP_AUTHORIZING || !event.operation
			|| event.operation != app->pending_operation) return effect;
		if(event.kind == PLUS_VITA_EVENT_AUTO_ENABLED && app->waiting_for == PLUS_VITA_EFFECT_ENABLE_AUTO) {
			if(event.status != PLUS_VITA_RESULT_OK) account_error(app, "Automatic sign-in could not be enabled.\nCheck storage and try again from setup.");
			else { app->remember = true; setup_view(app); app->view.selected = 1; }
			return effect;
		}
		if(event.kind == PLUS_VITA_EVENT_ACCOUNT_LOADED && app->waiting_for == PLUS_VITA_EFFECT_LOAD_ACCOUNT)
		{
			if(event.status == PLUS_VITA_RESULT_MISSING) {
				app->pending_operation = 0; app->waiting_for = PLUS_VITA_EFFECT_NONE;
				app->saved = false; app->remember = !event.restore_blocked; clear_runtime(app); welcome(app); return effect;
			}
			if(event.status != PLUS_VITA_RESULT_OK || !parse_import(&app->credential, event.data, event.data_size,
				event.retained_json ? NULL : app->label))
			{
				app->saved = true; /* Presence/error is not proof of a usable sign-in. */
				account_error(app, "The saved account could not be read.\nCheck local storage or forget the saved account and import again.");
				return effect;
			}
			app->saved = app->remember = true;
			return operation(app, PLUS_VITA_EFFECT_AUTHORIZE, "Restoring your saved sign-in and loading cloud games.", true);
		}
		if(event.kind == PLUS_VITA_EVENT_ACCOUNT_SAVED && app->waiting_for == PLUS_VITA_EFFECT_SAVE_ACCOUNT)
		{
			app->pending_operation = 0;
			app->waiting_for = PLUS_VITA_EFFECT_NONE;
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				app->retry_save = true;
				account_error(app, "Your account changes could not be saved.\nYou can keep using this app run, or retry saving before exiting.");
				return effect;
			}
			app->saved = app->remember = true;
			app->retry_save = false;
			if(app->storage_return == PLUS_VITA_APP_ACCOUNT) account_view(app); else catalogue_view(app);
			return effect;
		}
		if(event.kind == PLUS_VITA_EVENT_ACCOUNT_DELETED && app->waiting_for == PLUS_VITA_EFFECT_DELETE_ACCOUNT)
		{
			app->pending_operation = 0;
			app->waiting_for = PLUS_VITA_EFFECT_NONE;
			if(event.restore_blocked) app->saved = app->remember = false;
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				app->retry_delete = true;
				account_error(app, "Account files could not all be removed.\nCheck storage and retry cleanup. Removal is not complete.");
				return effect;
			}
			app->saved = app->remember = false;
			app->retry_delete = false;
			if(app->expired) { app->expired = false; account_error(app, result_message(PLUS_VITA_RESULT_REJECTED)); }
			else if(app->forgetting) { app->forgetting = false; app->welcome_selection = 0; setup_view(app); }
			else if(app->storage_return == PLUS_VITA_APP_SETUP) { setup_view(app); app->view.selected = 1; }
			else if(app->storage_return == PLUS_VITA_APP_CATALOGUE) catalogue_view(app);
			else account_view(app);
			return effect;
		}
		if(event.kind == PLUS_VITA_EVENT_IMPORT_READ && app->waiting_for == PLUS_VITA_EFFECT_READ_IMPORT)
		{
			if(event.status != PLUS_VITA_RESULT_OK)
			{
				account_error(app, result_message(event.status));
				return effect;
			}
			app->imported_valid = parse_import(&app->credential, event.data, event.data_size, NULL);
			if(!app->imported_valid)
			{
				account_error(app, "The JSON sign-in data was not valid. The file was kept.\nReplace it with a fresh browser JSON response and try again.");
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
				app->retry_account = event.status != PLUS_VITA_RESULT_REJECTED;
				if(event.status == PLUS_VITA_RESULT_REJECTED) return remove_saved(app, false, true);
				account_error(app, result_message(event.status));
				return effect;
			}
			if(event.entry_count > 4096 || (event.entry_count && !event.entries))
				account_error(app, "The catalogue could not be loaded. Please try again later.");
			else
			{
				const char *end = memchr(event.account_name, 0, sizeof(event.account_name));
				memset(app->online_id, 0, sizeof(app->online_id));
				if(end && plus_vita_utf8_label(event.account_name, (size_t)(end-event.account_name)))
					memcpy(app->online_id, event.account_name, (size_t)(end-event.account_name));
				if(event.entry_count)
				{
					app->catalogue = malloc(event.entry_count * sizeof(*app->catalogue));
					if(!app->catalogue) { account_error(app, "Not enough memory to load cloud games."); return effect; }
					memcpy(app->catalogue, event.entries, event.entry_count * sizeof(*app->catalogue));
					for(size_t i = 0; i < event.entry_count; i++) app->catalogue[i].name[PLUS_VITA_CATALOG_NAME_MAX - 1] = 0;
					qsort(app->catalogue, event.entry_count, sizeof(*app->catalogue), compare_titles);
				}
				app->catalogue_count = event.entry_count;
				filter_catalogue(app);
				app->connected = true;
				app->pending_operation = 0;
				app->waiting_for = PLUS_VITA_EFFECT_NONE;
				catalogue_view(app);
				app->retry_account = false;
				if(app->remember && !app->saved) {
					app->storage_return = PLUS_VITA_APP_CATALOGUE;
					return operation(app, PLUS_VITA_EFFECT_SAVE_ACCOUNT, "Saving your account on this Vita.", false);
				}
			}
		}
		return effect;
	}
	if(event.kind == PLUS_VITA_EVENT_SEARCH_ENTERED)
	{
		if(app->waiting_for!=PLUS_VITA_EFFECT_EDIT_SEARCH || !event.operation || event.operation!=app->pending_operation) return effect;
		app->pending_operation=0; app->waiting_for=PLUS_VITA_EFFECT_NONE;
		if(event.status==PLUS_VITA_RESULT_CANCELLED) { catalogue_view(app); return effect; }
		if(event.status!=PLUS_VITA_RESULT_OK || !plus_vita_utf8_label(event.data,event.data_size)) {
			catalogue_view(app); app->view.body="Search could not be entered. Please try again."; return effect;
		}
		memset(app->search,0,sizeof(app->search)); memcpy(app->search,event.data,event.data_size);
		filter_catalogue(app); app->search_focus=false; catalogue_view(app); return effect;
	}
	if(event.kind==PLUS_VITA_EVENT_PLAY_ENDED)
	{
		if(app->waiting_for!=PLUS_VITA_EFFECT_PLAY || !event.operation || event.operation!=app->pending_operation) return effect;
		app->pending_operation=0; app->waiting_for=PLUS_VITA_EFFECT_NONE;
		detail_view(app,"Streaming is not connected in this development build yet."); return effect;
	}
	if(event.kind==PLUS_VITA_EVENT_SEARCH && view->screen==PLUS_VITA_APP_CATALOGUE && app->catalogue_count) {
		app->search_focus=true;
		return operation(app,PLUS_VITA_EFFECT_EDIT_SEARCH,"Opening title search.",true);
	}
	if(event.kind == PLUS_VITA_EVENT_LABEL_ENTERED)
	{
		if(app->waiting_for != PLUS_VITA_EFFECT_EDIT_LABEL || !event.operation || event.operation != app->pending_operation) return effect;
		app->pending_operation = 0; app->waiting_for = PLUS_VITA_EFFECT_NONE;
		if(event.status == PLUS_VITA_RESULT_CANCELLED) { account_view(app); return effect; }
		bool valid = event.status == PLUS_VITA_RESULT_OK && plus_vita_utf8_label(event.data, event.data_size);
		if(!valid) { account_error(app, "The label could not be entered.\nUse up to 32 characters and try again from Account."); return effect; }
		memset(app->label, 0, sizeof(app->label)); memcpy(app->label, event.data, event.data_size);
		if(app->saved) { app->storage_return = PLUS_VITA_APP_ACCOUNT; return operation(app, PLUS_VITA_EFFECT_SAVE_ACCOUNT, "Saving your account label.", false); }
		account_view(app); return effect;
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
				{ app->catalogue_selection = app->catalogue_selection / 3 * 3 + i; app->search_focus=false; }
				return plus_vita_app_dispatch(app,
					(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
			}
		}
		return effect;
	}
	if(view->screen == PLUS_VITA_APP_CATALOGUE && app->matches_count
		&& (event.kind == PLUS_VITA_EVENT_DOWN || event.kind == PLUS_VITA_EVENT_UP))
	{
		if(app->search_focus) { app->search_focus=false; app->catalogue_selection=event.kind==PLUS_VITA_EVENT_DOWN ? 0 : app->matches_count-1; }
		else if(event.kind==PLUS_VITA_EVENT_DOWN) { if(app->catalogue_selection+1==app->matches_count) app->search_focus=true; else app->catalogue_selection++; }
		else { if(!app->catalogue_selection) app->search_focus=true; else app->catalogue_selection--; }
		catalogue_view(app);
	}
	else if(event.kind == PLUS_VITA_EVENT_DOWN && view->action_count)
		view->selected = (view->selected + 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_UP && view->action_count)
		view->selected = (view->selected + view->action_count - 1) % view->action_count;
	else if(event.kind == PLUS_VITA_EVENT_BACK)
	{
		if(app->waiting_for == PLUS_VITA_EFFECT_SAVE_ACCOUNT || app->waiting_for == PLUS_VITA_EFFECT_DELETE_ACCOUNT
			|| app->waiting_for == PLUS_VITA_EFFECT_ENABLE_AUTO) return effect;
		if(view->screen == PLUS_VITA_APP_FORGET_CONFIRM) { account_view(app); return effect; }
		if(view->screen==PLUS_VITA_APP_DETAILS || app->waiting_for==PLUS_VITA_EFFECT_EDIT_SEARCH || app->waiting_for==PLUS_VITA_EFFECT_PLAY) {
			app->pending_operation=0; app->waiting_for=PLUS_VITA_EFFECT_NONE; catalogue_view(app); return effect;
		}
		app->pending_operation = 0;
		app->waiting_for = PLUS_VITA_EFFECT_NONE;
		if(!app->connected) plus_vita_credential_clear(&app->credential);
		app->retry_account = app->retry_save = false;
		if(view->screen == PLUS_VITA_APP_ERROR && !app->connected && !app->saved) setup_view(app);
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
		if((action==PLUS_VITA_ACTION_PREVIOUS_PAGE || action==PLUS_VITA_ACTION_NEXT_PAGE)
			&& view->screen==PLUS_VITA_APP_CATALOGUE && app->matches_count) {
			size_t page = app->catalogue_selection / 3, pages = (app->matches_count + 2) / 3;
			page = action==PLUS_VITA_ACTION_NEXT_PAGE ? (page+1)%pages : (page+pages-1)%pages;
			app->catalogue_selection = page * 3; app->search_focus = false;
			catalogue_view(app);
		}
		else if(action==PLUS_VITA_ACTION_SEARCH) return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_SEARCH});
		else if(action==PLUS_VITA_ACTION_CLEAR_SEARCH) { app->search[0]=0; filter_catalogue(app); app->search_focus=false; catalogue_view(app); }
		else if(action==PLUS_VITA_ACTION_GAME && view->screen==PLUS_VITA_APP_CATALOGUE) detail_view(app,NULL);
		else if(action==PLUS_VITA_ACTION_PLAY && view->screen==PLUS_VITA_APP_DETAILS) {
			PlusVitaCatalogEntry game=*view->detail;
			effect=operation(app,PLUS_VITA_EFFECT_PLAY,"Preparing to play.",true); effect.game=game; return effect;
		}
		else if(action == PLUS_VITA_ACTION_CATALOGUE)
		{
			app->welcome_selection = view->selected;
			catalogue_view(app);
		}
		else if(action == PLUS_VITA_ACTION_IMPORT)
			return import_account(app);
		else if(action == PLUS_VITA_ACTION_REMEMBER)
		{
			if(view->screen == PLUS_VITA_APP_ACCOUNT)
			{
				if(app->retry_delete) return remove_saved(app, app->forgetting, app->expired);
				if(app->saved) return remove_saved(app, false, false);
				app->storage_return = PLUS_VITA_APP_ACCOUNT;
				return operation(app, PLUS_VITA_EFFECT_SAVE_ACCOUNT, "Saving sign-in in locally readable files on this Vita.", false);
			}
			if(app->remember) {
				PlusVitaAppEffect disable = remove_saved(app, false, false);
				app->storage_return = PLUS_VITA_APP_SETUP;
				return disable;
			}
			return operation(app, PLUS_VITA_EFFECT_ENABLE_AUTO, "Enabling automatic use of your retained JSON.", false);
		}
		else if(action == PLUS_VITA_ACTION_RESTORE)
			return operation(app, PLUS_VITA_EFFECT_LOAD_ACCOUNT, "Loading your remembered account.", true);
		else if(action == PLUS_VITA_ACTION_RETRY)
			return operation(app, app->retry_delete ? PLUS_VITA_EFFECT_DELETE_ACCOUNT : app->retry_save ? PLUS_VITA_EFFECT_SAVE_ACCOUNT : PLUS_VITA_EFFECT_AUTHORIZE,
				app->retry_delete ? "Removing saved sign-in from this Vita." : app->retry_save ? "Saving your account on this Vita." : "Checking your sign-in and loading cloud games.", !app->retry_save && !app->retry_delete);
		else if(action == PLUS_VITA_ACTION_ACCOUNT) account_view(app);
		else if(action == PLUS_VITA_ACTION_LABEL)
			return operation(app, PLUS_VITA_EFFECT_EDIT_LABEL, "Opening the account-label keyboard.", true);
		else if(action == PLUS_VITA_ACTION_FORGET)
		{
			app->view = (PlusVitaAppView){ .screen = PLUS_VITA_APP_FORGET_CONFIRM, .title = "Forget this account?",
				.subtitle = "ACCOUNT", .body = "This clears active sign-in and saved account data.\nYour JSON stays in place, but automatic sign-in is paused.\nChoose Import JSON explicitly to connect again.", .hint = "X  Select     O  Back",
				.actions = {{ PLUS_VITA_ACTION_BACK, "Keep account", 64, 380, 832, 52 },
				{ PLUS_VITA_ACTION_CONFIRM_FORGET, "Forget account", 64, 444, 832, 52 }}, .action_count = 2 };
		}
		else if(action == PLUS_VITA_ACTION_CONFIRM_FORGET) return remove_saved(app, true, false);
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
			if(view->screen==PLUS_VITA_APP_DETAILS) { catalogue_view(app); return effect; }
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
