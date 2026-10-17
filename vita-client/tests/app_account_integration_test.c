// SPDX-License-Identifier: AGPL-3.0-only
/* Exercise the real app, worker, files, Sony response parsing and cancellation.
 * Only the external HTTP transport is replaced; no real network is possible. */
#include "app.h"
#include "account_vita.h"
#include "request_cancel.h"
#include "curl_http.h"
#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static atomic_bool block_server, server_entered;
static atomic_uint requests;
static atomic_int auth_status;
static void pause_poll(void) { const struct timespec delay = { .tv_nsec = 1000000 }; nanosleep(&delay, NULL); }

static void body(CCHttpResponse *response, const char *value)
{
	response->data = strdup(value);
	assert(response->data);
	response->size = strlen(value);
}

ChiakiErrorCode __wrap_cc_http_perform(ChiakiLog *log, const CCHttpRequest *request, CCHttpResponse *response)
{
	(void)log;
	atomic_fetch_add(&requests, 1);
	memset(response, 0, sizeof(*response));
	atomic_store(&server_entered, true);
	while(atomic_load(&block_server))
	{
		if(plus_vita_request_progress(NULL, 0, 0, 0, 0)) return CHIAKI_ERR_CANCELED;
		pause_poll();
	}
	if(strstr(request->url, "response_type=code"))
	{
		int status = atomic_load(&auth_status);
		if(status) {
			response->status_code = status;
			if(status == 302) response->redirect_url = strdup("https://example.invalid/callback?error=login_required");
			return CHIAKI_ERR_SUCCESS;
		}
		response->status_code = 302;
		response->redirect_url = strdup("https://example.invalid/callback?code=synthetic-code");
	}
	else if(strstr(request->url, "/user/session"))
	{
		response->status_code = 200;
		response->headers = strdup("Set-Cookie: JSESSIONID=synthetic-session; Path=/\r\n");
		response->headers_size = strlen(response->headers);
		body(response, "{\"header\":{\"status_code\":\"0x0000\"},\"data\":{\"country\":\"CH\",\"language\":\"en\"}}");
	}
	else if(strstr(request->url, "/user/stores"))
	{
		response->status_code = 200;
		body(response, "{\"header\":{\"status_code\":\"0x0000\"},\"data\":{\"base_url\":\"https://example.invalid/container/CH/en/0/ROOT\"}}");
	}
	else if(strstr(request->url, "/container/CH/en/0/ROOT"))
	{
		response->status_code = 200;
		body(response, "{\"links\":[{\"name\":\"A - B\",\"url\":\"https://example.invalid/category/ab\"}]}");
	}
	else if(strstr(request->url, "/category/ab"))
	{
		response->status_code = 200;
		body(response, "{\"links\":[{\"name\":\"Zulu\",\"productId\":\"FAKECUSA00002\"},{\"name\":\"Alpha\",\"productId\":\"FAKECUSA00001\"}]}");
	}
	else if(strstr(request->url, "response_type=token")) response->status_code = 403;
	else assert(!"Unexpected HTTP route; test transport never contacts a server");
	return CHIAKI_ERR_SUCCESS;
}

static void import_file(void)
{
	if(mkdir("ux0:data", 0700) != 0) assert(errno == EEXIST);
	FILE *file = fopen("ux0:data/plus-vita-auth.json", "wb");
	assert(file);
	assert(fputs("{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}", file) >= 0);
	assert(fclose(file) == 0);
}

static void finish(PlusVitaApp *app, PlusVitaAccount *account, PlusVitaAppEffect pending);

static PlusVitaAppEffect open_import(PlusVitaApp *app, PlusVitaAccount *account)
{
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE })); /* Runtime only. */
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_UP });
	return plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
}

static void drive(PlusVitaApp *app, PlusVitaAccount *account, PlusVitaAppEffect *pending)
{
	if(pending->kind != PLUS_VITA_EFFECT_NONE)
	{
		assert(plus_vita_account_start(account, pending));
		plus_vita_credential_clear(&pending->credential);
		pending->kind = PLUS_VITA_EFFECT_NONE;
	}
	PlusVitaAppEvent event;
	if(plus_vita_account_poll(account, &event))
	{
		*pending = plus_vita_app_dispatch(app, event);
		plus_vita_account_release_result(account);
	}
	pause_poll();
}

static PlusVitaAppEffect choose(PlusVitaApp *app, PlusVitaAppAction action)
{
	if(action == PLUS_VITA_ACTION_BACK) return plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	PlusVitaAppView view = plus_vita_app_view(app);
	for(size_t i = 0; i < view.action_count; i++)
		if(view.actions[i].id == action) return plus_vita_app_dispatch(app, (PlusVitaAppEvent){
			.kind = PLUS_VITA_EVENT_TOUCH, .x = view.actions[i].x + 10, .y = view.actions[i].y + 10 });
	fprintf(stderr, "Missing action %d on screen %d\n", action, view.screen);
	fprintf(stderr, "Load-error category: %d; save-error category: %d\n",
		view.body && strstr(view.body, "could not be read") != NULL,
		view.body && strstr(view.body, "could not be saved") != NULL);
	assert(!"Expected visible action");
	return (PlusVitaAppEffect){0};
}

static void finish(PlusVitaApp *app, PlusVitaAccount *account, PlusVitaAppEffect pending)
{
	for(unsigned i = 0; i < 5000 && plus_vita_app_view(app).screen == PLUS_VITA_APP_AUTHORIZING; i++) drive(app, account, &pending);
	assert(plus_vita_app_view(app).screen != PLUS_VITA_APP_AUTHORIZING);
}

static void remembered_restart(void)
{
	import_file();
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	PlusVitaAppEffect pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = pending.operation });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	for(unsigned i = 0; i < 5000 && plus_vita_app_view(app).screen == PLUS_VITA_APP_AUTHORIZING; i++) drive(app, account, &pending);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	assert(access("ux0:data/plus-vita/account.json", F_OK) == 0);
	assert(access("ux0:data/plus-vita-auth.json", F_OK) == 0);
	assert(remove("ux0:data/plus-vita-auth.json") == 0); /* Also cover cache-only restart. */
	assert(plus_vita_account_close(account));
	plus_vita_app_destroy(app);
	app = plus_vita_app_create(); account = plus_vita_account_create();
	pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START });
	for(unsigned i = 0; i < 5000 && plus_vita_app_view(app).screen == PLUS_VITA_APP_AUTHORIZING; i++) drive(app, account, &pending);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	choose(app, PLUS_VITA_ACTION_BACK);
	choose(app, PLUS_VITA_ACTION_ACCOUNT);
	pending = choose(app, PLUS_VITA_ACTION_LABEL);
	assert(pending.kind == PLUS_VITA_EFFECT_EDIT_LABEL);
	const char label[] = "Müller 日本";
	pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_LABEL_ENTERED,
		.operation = pending.operation, .data = label, .data_size = sizeof(label) - 1 });
	finish(app, account, pending);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ACCOUNT);
	assert(strstr(plus_vita_app_view(app).body, label));
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
	choose(app, PLUS_VITA_ACTION_BACK);
	assert(choose(app, PLUS_VITA_ACTION_CATALOGUE).kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	choose(app, PLUS_VITA_ACTION_BACK);
	choose(app, PLUS_VITA_ACTION_ACCOUNT);
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(plus_vita_account_close(account));
	plus_vita_app_destroy(app);
	/* A service outage must retain saved sign-in, and Retry needs no JSON. */
	atomic_store(&auth_status, 503);
	app = plus_vita_app_create(); account = plus_vita_account_create();
	finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(access("ux0:data/plus-vita/account.json", F_OK) == 0);
	atomic_store(&auth_status, 0);
	finish(app, account, choose(app, PLUS_VITA_ACTION_RETRY));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_ACCOUNT);
	assert(strstr(plus_vita_app_view(app).body, label));
	import_file(); /* Forget must remove this stale transfer too. */
	choose(app, PLUS_VITA_ACTION_FORGET);
	finish(app, account, choose(app, PLUS_VITA_ACTION_CONFIRM_FORGET));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
	assert(access("ux0:data/plus-vita-auth.json", F_OK) == 0);
	assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
	app = plus_vita_app_create(); account = plus_vita_account_create();
	finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita-auth.json") == 0);
	assert(remove("ux0:data/plus-vita/account.blocked") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
}

static void saved_error_paths(void)
{
	const int responses[] = {401, 403, 429, 503, 302};
	for(unsigned i = 0; i < sizeof(responses) / sizeof(responses[0]); i++)
	{
		if(mkdir("ux0:data/plus-vita", 0700) != 0) assert(errno == EEXIST);
		FILE *saved = fopen("ux0:data/plus-vita/account.json", "wb");
		assert(saved && fputs("{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"label\":\"synthetic\"}", saved) >= 0);
		assert(fclose(saved) == 0);
		atomic_store(&auth_status, responses[i]);
		PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *account = plus_vita_account_create();
		finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
		if(responses[i] == 401 || responses[i] == 302) {
			assert(strstr(plus_vita_app_view(app).body, "expired"));
			assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
		} else {
			assert(access("ux0:data/plus-vita/account.json", F_OK) == 0);
			atomic_store(&auth_status, 0);
			finish(app, account, choose(app, PLUS_VITA_ACTION_RETRY));
			assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
			choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_ACCOUNT);
			choose(app, PLUS_VITA_ACTION_FORGET);
			finish(app, account, choose(app, PLUS_VITA_ACTION_CONFIRM_FORGET));
		}
		assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
		assert(remove("ux0:data/plus-vita/account.blocked") == 0);
		assert(rmdir("ux0:data/plus-vita") == 0);
	}
	atomic_store(&auth_status, 0);
	/* Corrupt storage is not an authorized account and can be forgotten. */
	assert(mkdir("ux0:data/plus-vita", 0700) == 0);
	FILE *saved = fopen("ux0:data/plus-vita/account.json", "wb");
	assert(saved && fputs("not-json", saved) >= 0 && fclose(saved) == 0);
	PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *account = plus_vita_account_create();
	finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	choose(app, PLUS_VITA_ACTION_FORGET);
	finish(app, account, choose(app, PLUS_VITA_ACTION_CONFIRM_FORGET));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita/account.blocked") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
}

static void failed_removal(void)
{
	/* A failed deletion must be visible, and a fresh launch must honor the
	 * forget tombstone even while the stubborn old path still exists. */
	assert(mkdir("ux0:data/plus-vita", 0700) == 0);
	assert(mkdir("ux0:data/plus-vita/account.json", 0700) == 0);
	FILE *child = fopen("ux0:data/plus-vita/account.json/obstruction", "wb");
	assert(child && fclose(child) == 0);
	PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *account = plus_vita_account_create();
	finish(app, account, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	choose(app, PLUS_VITA_ACTION_FORGET);
	finish(app, account, choose(app, PLUS_VITA_ACTION_CONFIRM_FORGET));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(app).body, "not complete"));
	PlusVitaApp *new_app = plus_vita_app_create(); PlusVitaAccount *new_worker = plus_vita_account_create();
	finish(new_app, new_worker, plus_vita_app_dispatch(new_app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(new_app).screen == PLUS_VITA_APP_WELCOME);
	assert(plus_vita_account_close(new_worker)); plus_vita_app_destroy(new_app);
	assert(remove("ux0:data/plus-vita/account.json/obstruction") == 0);
	assert(rmdir("ux0:data/plus-vita/account.json") == 0);
	finish(app, account, choose(app, PLUS_VITA_ACTION_RETRY));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita/account.blocked") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
}

static void failed_save_disable_and_cancelled_restore(void)
{
	import_file();
	PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *account = plus_vita_account_create();
	finish(app, account, open_import(app, account));
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_ACCOUNT);
	/* Block the atomic temporary write with a directory, without mocking storage. */
	assert(mkdir("ux0:data/plus-vita/account.tmp", 0700) == 0);
	FILE *temporary_child = fopen("ux0:data/plus-vita/account.tmp/obstruction", "wb");
	assert(temporary_child && fclose(temporary_child) == 0);
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(app).body, "could not be saved"));
	assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
	/* The failed write still permits runtime catalogue use. */
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_CATALOGUE);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	assert(remove("ux0:data/plus-vita/account.tmp/obstruction") == 0);
	assert(rmdir("ux0:data/plus-vita/account.tmp") == 0);
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_ACCOUNT);
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(remove("ux0:data/plus-vita/account.json") == 0);
	assert(mkdir("ux0:data/plus-vita/account.json", 0700) == 0);
	FILE *child = fopen("ux0:data/plus-vita/account.json/obstruction", "wb");
	assert(child && fclose(child) == 0);
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(app).actions[0].label, "Retry removing"));
	assert(remove("ux0:data/plus-vita/account.json/obstruction") == 0);
	assert(rmdir("ux0:data/plus-vita/account.json") == 0);
	finish(app, account, choose(app, PLUS_VITA_ACTION_RETRY));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ACCOUNT);
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_CATALOGUE);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	choose(app, PLUS_VITA_ACTION_BACK); choose(app, PLUS_VITA_ACTION_ACCOUNT);
	finish(app, account, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(plus_vita_account_close(account)); plus_vita_app_destroy(app);
	/* Let restored authorization finish externally, but cancel before its result
	 * reaches the app. Forget must remain final even after that successful result. */
	app = plus_vita_app_create(); account = plus_vita_account_create();
	PlusVitaAppEffect pending = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START });
	for(unsigned i = 0; i < 5000 && pending.kind != PLUS_VITA_EFFECT_AUTHORIZE; i++) drive(app, account, &pending);
	assert(pending.kind == PLUS_VITA_EFFECT_AUTHORIZE);
	assert(plus_vita_account_start(account, &pending));
	plus_vita_credential_clear(&pending.credential);
	PlusVitaAppEvent late; bool done = false;
	for(unsigned i = 0; i < 5000 && !done; i++) { done = plus_vita_account_poll(account, &late); if(!done) pause_poll(); }
	assert(done && late.status == PLUS_VITA_RESULT_OK);
	choose(app, PLUS_VITA_ACTION_BACK); plus_vita_account_cancel(account);
	choose(app, PLUS_VITA_ACTION_ACCOUNT); choose(app, PLUS_VITA_ACTION_FORGET);
	PlusVitaAccount *storage = plus_vita_account_create();
	finish(app, storage, choose(app, PLUS_VITA_ACTION_CONFIRM_FORGET));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(plus_vita_app_dispatch(app, late).kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_SETUP);
	assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
	plus_vita_account_release_result(account);
	assert(plus_vita_account_close(account)); assert(plus_vita_account_close(storage)); plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita-auth.json") == 0);
	assert(remove("ux0:data/plus-vita/account.blocked") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
}

static void retained_json_launch(void)
{
	import_file();
	struct stat original, current;
	assert(stat("ux0:data/plus-vita-auth.json", &original) == 0);
	for(unsigned launch = 0; launch < 2; launch++) {
		PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *worker = plus_vita_account_create();
		finish(app, worker, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
		assert(stat("ux0:data/plus-vita-auth.json", &current) == 0 && current.st_size == original.st_size);
		/* Launch uses the original JSON without requiring an app-managed copy. */
		assert(access("ux0:data/plus-vita/account.json", F_OK) != 0);
		assert(plus_vita_account_close(worker)); plus_vita_app_destroy(app);
	}
	/* An explicit Off choice survives exit before importing/authorizing. */
	PlusVitaApp *app = plus_vita_app_create(); PlusVitaAccount *worker = plus_vita_account_create();
	PlusVitaAppEffect prepare = choose(app, PLUS_VITA_ACTION_SETUP);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = prepare.operation });
	choose(app, PLUS_VITA_ACTION_CONTINUE);
	finish(app, worker, choose(app, PLUS_VITA_ACTION_REMEMBER));
	assert(strstr(plus_vita_app_view(app).actions[1].label, "Off"));
	assert(plus_vita_account_close(worker)); plus_vita_app_destroy(app);
	app = plus_vita_app_create(); worker = plus_vita_account_create();
	finish(app, worker, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	prepare = choose(app, PLUS_VITA_ACTION_SETUP);
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = prepare.operation });
	choose(app, PLUS_VITA_ACTION_CONTINUE);
	finish(app, worker, choose(app, PLUS_VITA_ACTION_REMEMBER)); /* On, without importing. */
	assert(plus_vita_account_close(worker)); plus_vita_app_destroy(app);
	app = plus_vita_app_create(); worker = plus_vita_account_create();
	finish(app, worker, plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_START }));
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
	assert(plus_vita_account_close(worker)); plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita-auth.json") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
	assert(rmdir("ux0:data") == 0);
}

int main(void)
{
	retained_json_launch();
	atomic_store(&server_entered, false);
	/* Cancel before delivering the read result: the source must stay intact. */
	import_file();
	PlusVitaApp *cancelled_app = plus_vita_app_create();
	PlusVitaAccount *cancelled_account = plus_vita_account_create();
	PlusVitaAppEffect read = open_import(cancelled_app, cancelled_account);
	assert(plus_vita_account_start(cancelled_account, &read));
	plus_vita_app_dispatch(cancelled_app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	plus_vita_account_cancel(cancelled_account);
	PlusVitaAppEvent consumed;
	bool read_done = false;
	for(unsigned i = 0; i < 5000 && !read_done; i++) {
		read_done = plus_vita_account_poll(cancelled_account, &consumed);
		if(!read_done) pause_poll();
	}
	assert(read_done);
	assert(access("ux0:data/plus-vita-auth.json", F_OK) == 0);
	assert(plus_vita_app_dispatch(cancelled_app, consumed).kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(cancelled_app).screen == PLUS_VITA_APP_WELCOME);
	assert(!atomic_load(&server_entered));
	plus_vita_account_release_result(cancelled_account);
	assert(plus_vita_account_close(cancelled_account));
	plus_vita_app_destroy(cancelled_app);

	import_file();
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	assert(app && account);
	PlusVitaAppEffect pending = open_import(app, account);
	for(unsigned i = 0; i < 5000 && plus_vita_app_view(app).screen == PLUS_VITA_APP_AUTHORIZING; i++) drive(app, account, &pending);
	PlusVitaAppView view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_CATALOGUE);
	assert(view.action_count == 2 && strcmp(view.actions[0].label, "Alpha") == 0);
	assert(access("ux0:data/plus-vita-auth.json", F_OK) == 0);
	unsigned initial_requests = atomic_load(&requests);
	/* Navigation retains the runtime account without rereading the source. */
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	for(unsigned repeat = 0; repeat < 3; repeat++)
	{
		assert(strcmp(plus_vita_app_view(app).actions[plus_vita_app_view(app).selected].label, "Zulu") == 0);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		view = plus_vita_app_view(app);
		assert(view.screen == PLUS_VITA_APP_WELCOME);
		assert(strcmp(view.actions[0].label, "Open catalogue") == 0);
		/* About and cancelled exit are navigation, not sign-out. */
		choose(app, PLUS_VITA_ACTION_ABOUT);
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ABOUT);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_EXIT_CONFIRM);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
		/* Touch the catalogue action regardless of restored home focus. */
		PlusVitaAppEffect reopened = choose(app, PLUS_VITA_ACTION_CATALOGUE);
		assert(reopened.kind == PLUS_VITA_EFFECT_NONE);
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
		assert(atomic_load(&requests) == initial_requests);
		assert(access("ux0:data/plus-vita-auth.json", F_OK) == 0);
	}
	assert(plus_vita_account_close(account));
	plus_vita_app_destroy(app);

	import_file();
	atomic_store(&block_server, true);
	atomic_store(&server_entered, false);
	app = plus_vita_app_create();
	account = plus_vita_account_create();
	assert(app && account);
	pending = open_import(app, account);
	for(unsigned i = 0; i < 5000 && !atomic_load(&server_entered); i++) drive(app, account, &pending);
	assert(atomic_load(&server_entered));
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	plus_vita_account_cancel(account);
	PlusVitaAppEvent late;
	bool completed = false;
	for(unsigned i = 0; i < 5000 && !completed; i++) { completed = plus_vita_account_poll(account, &late); if(!completed) pause_poll(); }
	assert(completed);
	plus_vita_app_dispatch(app, late);
	plus_vita_account_release_result(account);
	assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_WELCOME);
	assert(plus_vita_account_close(account));
	plus_vita_app_destroy(app);
	assert(remove("ux0:data/plus-vita-auth.json") == 0);
	assert(remove("ux0:data/plus-vita/account.blocked") == 0);
	assert(rmdir("ux0:data/plus-vita") == 0);
	assert(rmdir("ux0:data") == 0);
	atomic_store(&block_server, false);
	remembered_restart();
	saved_error_paths();
	failed_removal();
	failed_save_disable_and_cancelled_restore();
	assert(rmdir("ux0:data") == 0);
	puts("app account integration: retained JSON, restore, storage recovery and stale results passed");
	return 0;
}
