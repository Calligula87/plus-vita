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
	/* The externally observable request may occur only after file consumption. */
	assert(access("ux0:data/plus-vita-auth.json", F_OK) != 0 && errno == ENOENT);
	memset(response, 0, sizeof(*response));
	atomic_store(&server_entered, true);
	while(atomic_load(&block_server))
	{
		if(plus_vita_request_progress(NULL, 0, 0, 0, 0)) return CHIAKI_ERR_CANCELED;
		pause_poll();
	}
	if(strstr(request->url, "response_type=code"))
	{
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

static PlusVitaAppEffect open_import(PlusVitaApp *app)
{
	PlusVitaAppEffect effect = plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_SETUP_READY, .operation = effect.operation });
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
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

int main(void)
{
	/* Cancel before delivering the read result: consumption must still finish. */
	import_file();
	PlusVitaApp *cancelled_app = plus_vita_app_create();
	PlusVitaAccount *cancelled_account = plus_vita_account_create();
	PlusVitaAppEffect read = open_import(cancelled_app);
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
	assert(access("ux0:data/plus-vita-auth.json", F_OK) != 0 && errno == ENOENT);
	assert(plus_vita_app_dispatch(cancelled_app, consumed).kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(cancelled_app).screen == PLUS_VITA_APP_WELCOME);
	assert(!atomic_load(&server_entered));
	plus_vita_account_release_result(cancelled_account);
	assert(plus_vita_account_close(cancelled_account));
	plus_vita_app_destroy(cancelled_app);

	/* A nonempty directory at the import path cannot be consumed/removed.
	 * Even after cancellation the user must see the cleanup failure. */
	assert(mkdir("ux0:data/plus-vita-auth.json", 0700) == 0);
	FILE *obstruction = fopen("ux0:data/plus-vita-auth.json/keep", "wb");
	assert(obstruction && fclose(obstruction) == 0);
	cancelled_app = plus_vita_app_create();
	cancelled_account = plus_vita_account_create();
	read = open_import(cancelled_app);
	assert(plus_vita_account_start(cancelled_account, &read));
	plus_vita_app_dispatch(cancelled_app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
	read_done = false;
	for(unsigned i = 0; i < 5000 && !read_done; i++) {
		read_done = plus_vita_account_poll(cancelled_account, &consumed);
		if(!read_done) pause_poll();
	}
	assert(read_done && consumed.status == PLUS_VITA_RESULT_CLEANUP_ERROR);
	assert(plus_vita_app_dispatch(cancelled_app, consumed).kind == PLUS_VITA_EFFECT_NONE);
	assert(plus_vita_app_view(cancelled_app).screen == PLUS_VITA_APP_ERROR);
	assert(strstr(plus_vita_app_view(cancelled_app).body, "Remove it with VitaShell"));
	assert(!atomic_load(&server_entered));
	plus_vita_account_release_result(cancelled_account);
	assert(plus_vita_account_close(cancelled_account));
	plus_vita_app_destroy(cancelled_app);
	assert(remove("ux0:data/plus-vita-auth.json/keep") == 0);
	assert(rmdir("ux0:data/plus-vita-auth.json") == 0);

	import_file();
	PlusVitaApp *app = plus_vita_app_create();
	PlusVitaAccount *account = plus_vita_account_create();
	assert(app && account);
	PlusVitaAppEffect pending = open_import(app);
	for(unsigned i = 0; i < 5000 && plus_vita_app_view(app).screen == PLUS_VITA_APP_AUTHORIZING; i++) drive(app, account, &pending);
	PlusVitaAppView view = plus_vita_app_view(app);
	assert(view.screen == PLUS_VITA_APP_CATALOGUE);
	assert(view.action_count == 2 && strcmp(view.actions[0].label, "Alpha") == 0);
	assert(access("ux0:data/plus-vita-auth.json", F_OK) != 0);
	unsigned initial_requests = atomic_load(&requests);
	/* Source file has been consumed. Navigation must retain the runtime account. */
	plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
	for(unsigned repeat = 0; repeat < 3; repeat++)
	{
		assert(strcmp(plus_vita_app_view(app).actions[plus_vita_app_view(app).selected].label, "Zulu") == 0);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		view = plus_vita_app_view(app);
		assert(view.screen == PLUS_VITA_APP_WELCOME);
		assert(strcmp(view.actions[0].label, "Open catalogue") == 0);
		/* About and cancelled exit are navigation, not sign-out. */
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_DOWN });
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_ABOUT);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_BACK });
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_EXIT_CONFIRM);
		plus_vita_app_dispatch(app, (PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_ACTIVATE });
		/* Touch the catalogue action regardless of restored home focus. */
		PlusVitaAppEffect reopened = plus_vita_app_dispatch(app,
			(PlusVitaAppEvent){ .kind = PLUS_VITA_EVENT_TOUCH, .x = 120, .y = 340 });
		assert(reopened.kind == PLUS_VITA_EFFECT_NONE);
		assert(plus_vita_app_view(app).screen == PLUS_VITA_APP_CATALOGUE);
		assert(atomic_load(&requests) == initial_requests);
		assert(access("ux0:data/plus-vita-auth.json", F_OK) != 0);
	}
	assert(plus_vita_account_close(account));
	plus_vita_app_destroy(app);

	import_file();
	atomic_store(&block_server, true);
	atomic_store(&server_entered, false);
	app = plus_vita_app_create();
	account = plus_vita_account_create();
	assert(app && account);
	pending = open_import(app);
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
	assert(rmdir("ux0:data") == 0);
	puts("app account integration: real import/worker/parsers, no retained file and cancelled blocked transport passed");
	return 0;
}
