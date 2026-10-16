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
}

int main(void)
{
	/* Synthetic test credential; never use a real account as a fixture. */
	const char file[] = "{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
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
	assert(effect.kind == PLUS_VITA_EFFECT_REMOVE_IMPORT);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_IMPORT_REMOVED, .operation = operation,
		.status = PLUS_VITA_RESULT_OK });
	assert(effect.kind == PLUS_VITA_EFFECT_AUTHORIZE);
	assert(effect.credential.length == 64);
	PlusVitaCatalogEntry titles[] = { { .name = "Zulu", .id = "z" }, { .name = "Alpha", .id = "a" } };
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK,
		.entries = titles, .entry_count = 2 });
	PlusVitaAppView view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_CATALOGUE);
	assert(view.action_count == 2 && strcmp(view.actions[0].label, "Alpha") == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(strcmp(view.actions[view.selected].label, "Zulu") == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH, .x = 120, .y = 340 });
	assert(plus_vita_app_view(app).selected == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	view = plus_vita_app_view(app);
	assert(strcmp(view.actions[view.selected].label, "Zulu") == 0);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	assert(effect.kind == PLUS_VITA_EFFECT_ENTER_TOKEN);
	operation = effect.operation;
	const char manual[] = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_TOKEN_ENTERED, .operation = operation,
		.status = PLUS_VITA_RESULT_OK, .data = manual, .data_size = sizeof(manual) - 1 });
	assert(effect.kind == PLUS_VITA_EFFECT_REMOVE_IMPORT);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){
		.kind = PLUS_VITA_EVENT_IMPORT_REMOVED, .operation = operation, .status = PLUS_VITA_RESULT_OK });
	assert(effect.kind == PLUS_VITA_EFFECT_AUTHORIZE);
	assert(effect.credential.length == 64 && memcmp(effect.credential.value, manual, 64) == 0);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK, .entry_count = 0 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	assert(plus_vita_app_view(app).actions[0].id == PLUS_VITA_ACTION_BACK);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	operation = effect.operation;
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .status = PLUS_VITA_RESULT_OK, .data = file, .data_size = sizeof(file) - 1 });
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_REMOVED,
		.operation = operation, .status = PLUS_VITA_RESULT_STORAGE_ERROR });
	assert(effect.kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(app).body, "remove") != NULL);
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
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_REMOVED, .operation = operation });
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
			.operation = operation, .status = failures[i], .data = manual, .data_size = 64 });
		view = plus_vita_app_view(app);
		assert(view.screen == PLUS_VITA_APP_ERROR && strstr(view.body, manual) == NULL);
		plus_vita_app_destroy(app);
	}
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	operation = effect.operation;
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .data = file, .data_size = sizeof(file) - 1 });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_REMOVED, .operation = operation });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_AUTHORIZED,
		.operation = operation, .status = PLUS_VITA_RESULT_OK, .entries = titles, .entry_count = 2 });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	plus_vita_app_destroy(app);
	app = plus_vita_app_create();
	assert(app);
	setup(app);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOKEN_ENTERED,
		.operation = effect.operation, .status = PLUS_VITA_RESULT_CANCELLED });
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	const char malformed[] = "{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}trailing";
	operation = effect.operation;
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_READ,
		.operation = operation, .data = malformed, .data_size = sizeof(malformed) - 1 });
	assert(effect.kind == PLUS_VITA_EFFECT_REMOVE_IMPORT);
	effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_IMPORT_REMOVED, .operation = operation });
	assert(effect.kind == PLUS_VITA_EFFECT_NONE && plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	plus_vita_app_destroy(app);
	puts("app account: import routes, cleanup, catalogues, safe failures, keyboard cancel and stale results passed");
	return 0;
}
