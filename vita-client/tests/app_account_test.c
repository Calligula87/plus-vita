// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void setup(PlusVitaApp *app)
{
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app,
		(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP_GUIDE);
	assert(strstr(plus_vita_app_view(app).body, "ssocookie") != NULL);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(plus_vita_app_view(app).action_count == 3);
	assert(strcmp(plus_vita_app_view(app).actions[0].label, "Import JSON") == 0);
	assert(plus_vita_app_view(app).actions[1].id == PLUS_VITA_ACTION_REMEMBER);
	assert(strstr(plus_vita_app_view(app).actions[1].label, "On"));
	assert(plus_vita_app_view(app).actions[2].id == PLUS_VITA_ACTION_BACK);
}

static void saved_json(const char *data, size_t size, bool valid)
{
	PlusVitaApp *app = plus_vita_app_create();
	assert(app);
	setup(app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	uint64_t operation = effect.operation;
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .data = data, .data_size = size });
	assert(effect.kind == (valid ? PLUS_VITA_EFFECT_AUTHORIZE : PLUS_VITA_EFFECT_NONE));
	if(valid) assert(effect.credential.length == 64);
	else {
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
		assert(plus_vita_credential_is_clear(&effect.credential));
	}
	plus_vita_credential_clear(&effect.credential);
	plus_vita_app_destroy(app);
}

static void saved_response_formats(void)
{
	#define TOKEN "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
	const char *invalid[] = {
		"{}", "[]", "null", "{\"npsso\":null}", "{\"npsso\":64}", "{\"npsso\":\"short\"}",
		"{\"npsso\":\"" TOKEN "\",\"npsso\":\"" TOKEN "\"}",
		"{\"npsso\":\"" TOKEN "\",\"np\\u0073so\":\"" TOKEN "\"}",
		"{\"nested\":{\"npsso\":\"" TOKEN "\"}}",
		"{\"npsso\":\"" TOKEN "\",}", "{\"npsso\":\"" TOKEN "\"} {}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":[1,]}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":/* comment */true}",
		"{\"npsso\":\"" TOKEN "\"}\v", "{\"npsso\":\"" TOKEN "\",\"bad\":\"\xff\"}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":NaN}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":Infinity}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":{\"nested\":NaN}}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":\"raw\nnewline\"}",
		"{\"npsso\":\"" TOKEN "\",\"bad\":\vtrue}"
	};
	for(size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) saved_json(invalid[i], strlen(invalid[i]), false);
	const char escaped[] = " \r\n{\"np\\u0073so\":\"" TOKEN "\",\"ignored\":[true,false,null,2.5,\"a\\\"b\"]}\t";
	saved_json(escaped, sizeof(escaped) - 1, true);
	const char bom[] = "\xef\xbb\xbf{\"npsso\":\"" TOKEN "\"}";
	saved_json(bom, sizeof(bom) - 1, true);
	const char unicode[] = "{\"npsso\":\"" TOKEN "\",\"label\":\"Müller 日本\"}";
	saved_json(unicode, sizeof(unicode) - 1, true);
	const char embedded_nul[] = "{\"npsso\":\"" TOKEN "\"}\0trailing";
	saved_json(embedded_nul, sizeof(embedded_nul) - 1, false);
	char oversized[4097]; memset(oversized, ' ', sizeof(oversized));
	memcpy(oversized, bom, sizeof(bom) - 1);
	saved_json(oversized, sizeof(oversized), false);
	#undef TOKEN
}

int main(void)
{
	saved_response_formats();
	/* Synthetic test credential; never use a real account as a fixture. */
	const char file[] = "{\"extra\":{\"npsso\":\"ignored\",\"values\":[1,true,null]},"
		"\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"other\":\"ignored\"}";
	PlusVitaApp *app = plus_vita_app_create();
	assert(app);
	setup(app);
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app,
		(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(effect.kind == PLUS_VITA_EFFECT_READ_IMPORT);
	uint64_t operation = effect.operation;
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_IMPORT_READ, .operation = operation,
		.status = PLUS_VITA_RESULT_OK, .data = file, .data_size = sizeof(file) - 1 });
	assert(effect.kind == PLUS_VITA_EFFECT_AUTHORIZE);
	assert(effect.credential.length == 64);
	PlusVitaCatalogEntry titles[] = { { .name = "Zulu", .id = "z" }, { .name = "Alpha", .id = "a" } };
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK,
		.entries = titles, .entry_count = 2 });
	PlusVitaAppView view = plus_vita_app_view(app);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACCOUNT_SAVED, .operation = view.operation });
	view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_CATALOGUE);
	assert(view.action_count == 3 && strcmp(view.actions[0].label, "Alpha") == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(strcmp(view.actions[view.selected].label, "Zulu") == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH, .x = 120, .y = 340 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_DETAILS);
	assert(strcmp(plus_vita_app_view(app).title, "Alpha") == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).selected == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(strcmp(view.actions[view.selected].label, "Zulu") == 0);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	operation = effect.operation;
	const char private_value[] = "SYNTHETIC_PRIVATE_VALUE_NOT_FOR_UI";
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_IMPORT_READ, .operation = operation,
		.status = PLUS_VITA_RESULT_OK, .data = file, .data_size = sizeof(file) - 1 });
	assert(effect.kind == PLUS_VITA_EFFECT_AUTHORIZE);
	assert(effect.credential.length == 64);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK, .entry_count = 0 });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACCOUNT_SAVED, .operation = plus_vita_app_view(app).operation });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	assert(plus_vita_app_view(app).actions[0].id == PLUS_VITA_ACTION_BACK);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = effect.operation, .status = PLUS_VITA_RESULT_MISSING });
	assert(effect.kind == PLUS_VITA_EFFECT_NONE && plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(app).body, "VitaShell") != NULL);
	plus_vita_app_destroy(app);
	const PlusVitaAppResult failures[] = { PLUS_VITA_RESULT_REJECTED, PLUS_VITA_RESULT_NETWORK_ERROR,
		PLUS_VITA_RESULT_CATALOGUE_ERROR, (PlusVitaAppResult)999 };
	for(size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++)
	{
		app = plus_vita_app_create();
		assert(app);
		setup(app);
		effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
		operation = effect.operation;
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
			.operation = operation, .data = file, .data_size = sizeof(file) - 1 });
			plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
			.operation = operation, .status = failures[i], .data = private_value, .data_size = sizeof(private_value) - 1 });
		view = plus_vita_app_view(app);
		if(failures[i] == PLUS_VITA_RESULT_REJECTED) {
			plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACCOUNT_DELETED,
				.operation = view.operation, .restore_blocked = true });
			view = plus_vita_app_view(app);
		}
		assert(view.screen == PLUS_VITA_APP_ERROR && strstr(view.body, private_value) == NULL);
		plus_vita_app_destroy(app);
	}
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	operation = effect.operation;
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .data = file, .data_size = sizeof(file) - 1 });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK, .entries = titles, .entry_count = 2 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	const char malformed[] = "{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}trailing";
	operation = effect.operation;
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .data = malformed, .data_size = sizeof(malformed) - 1 });
	assert(effect.kind == PLUS_VITA_EFFECT_NONE && plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	plus_vita_app_destroy(app);
	puts("app account: retained JSON setup, catalogues, safe failures and stale results passed");
	return 0;
}
