// SPDX-License-Identifier: AGPL-3.0-only
#include "account_vita.h"
#include "../platform/request_cancel.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <json_object.h>

#define IMPORT_FILE "ux0:data/plus-vita-auth.json"
#define ACCOUNT_DIR "ux0:data/plus-vita"
#define ACCOUNT_FILE ACCOUNT_DIR "/account.json"
#define ACCOUNT_TEMP ACCOUNT_DIR "/account.tmp"
#define ACCOUNT_BLOCK ACCOUNT_DIR "/account.blocked"
struct plus_vita_account_t {
	pthread_mutex_t mutex;
	pthread_t thread;
	bool running, complete, joined;
	PlusVitaAppEffect job;
	PlusVitaAppEvent result;
	PlusVitaAuthProbeResult catalogue;
	char file[4097];
};

static void clear_bytes(void *memory, size_t size)
{
	volatile unsigned char *p = memory;
	while(size--) *p++ = 0;
}

static void read_import(PlusVitaAccount *account)
{
	errno = 0;
	FILE *file = fopen(IMPORT_FILE, "rb");
	if(!file)
	{
		account->result.status = errno == ENOENT ? PLUS_VITA_RESULT_MISSING : PLUS_VITA_RESULT_STORAGE_ERROR;
		return;
	}
	size_t size = fread(account->file, 1, sizeof(account->file), file);
	bool failed = ferror(file) != 0;
	if(fclose(file) != 0) failed = true;
	/* The user-owned browser response stays in place for subsequent launches. */
	account->result.status = failed ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_OK;
	account->result.data = account->file;
	account->result.data_size = size;
}

static void authorize(PlusVitaAccount *account)
{
	plus_vita_auth_probe(&account->job.credential, &account->catalogue);
	PlusVitaAuthProbeResult *result = &account->catalogue;
	if(!result->transport_ok) account->result.status = PLUS_VITA_RESULT_NETWORK_ERROR;
	else if(!result->authorized) account->result.status = result->credential_rejected
		? PLUS_VITA_RESULT_REJECTED : PLUS_VITA_RESULT_CATALOGUE_ERROR;
	else if(!result->session_transport_ok) account->result.status = PLUS_VITA_RESULT_NETWORK_ERROR;
	else if(!result->session_created) account->result.status = PLUS_VITA_RESULT_CATALOGUE_ERROR;
	else if(!result->store_transport_ok) account->result.status = PLUS_VITA_RESULT_NETWORK_ERROR;
	else if(!result->store_resolved || !result->native_catalog_complete) account->result.status = PLUS_VITA_RESULT_CATALOGUE_ERROR;
	else account->result.status = PLUS_VITA_RESULT_OK;
	account->result.entries = result->native_titles;
	account->result.entry_count = result->native_titles_count;
}

static void load_account(PlusVitaAccount *account)
{
	struct stat status;
	if(stat(ACCOUNT_BLOCK, &status) == 0) { account->result.status = PLUS_VITA_RESULT_MISSING; account->result.restore_blocked = true; return; }
	if(errno != ENOENT) { account->result.status = PLUS_VITA_RESULT_STORAGE_ERROR; return; }
	FILE *file = fopen(ACCOUNT_FILE, "rb");
	if(!file) {
		if(errno == ENOENT) { account->result.retained_json = true; read_import(account); }
		else account->result.status = PLUS_VITA_RESULT_STORAGE_ERROR;
		return;
	}
	size_t size = fread(account->file, 1, sizeof(account->file), file);
	bool failed = ferror(file) != 0;
	if(fclose(file) != 0) failed = true;
	account->result.status = failed ? PLUS_VITA_RESULT_STORAGE_ERROR : PLUS_VITA_RESULT_OK;
	account->result.data = account->file;
	account->result.data_size = size;
}

static bool account_directory(void)
{
	return mkdir(ACCOUNT_DIR, 0700) == 0 || errno == EEXIST;
}

static void save_account(PlusVitaAccount *account)
{
	account->result.status = PLUS_VITA_RESULT_STORAGE_ERROR;
	if(!account_directory()) return;
	struct json_object *root = json_object_new_object();
	if(!root) return;
	struct json_object *token = json_object_new_string_len(account->job.credential.value, (int)account->job.credential.length);
	struct json_object *label = json_object_new_string(account->job.label);
	if(!token || !label) { json_object_put(token); json_object_put(label); json_object_put(root); return; }
	if(json_object_object_add(root, "npsso", token) < 0) {
		json_object_put(token); json_object_put(label); json_object_put(root); return;
	}
	if(json_object_object_add(root, "label", label) < 0) {
		json_object_put(label); json_object_put(root); return;
	}
	const char *data = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
	FILE *file = fopen(ACCOUNT_TEMP, "wb");
	bool success = false;
	if(file)
	{
		success = data && fwrite(data, 1, strlen(data), file) == strlen(data);
		if(fclose(file) != 0) success = false;
	}
	if(success) success = rename(ACCOUNT_TEMP, ACCOUNT_FILE) == 0;
	if(success) success = remove(ACCOUNT_BLOCK) == 0 || errno == ENOENT;
	if(!success) remove(ACCOUNT_TEMP);
	json_object_put(root);
	if(success) account->result.status = PLUS_VITA_RESULT_OK;
}

static void delete_account(PlusVitaAccount *account)
{
	account->result.status = PLUS_VITA_RESULT_STORAGE_ERROR;
	if(!account_directory()) return;
	/* A persistent tombstone prevents a failed unlink or old file from silently
	 * restoring an account the user has explicitly disabled/forgotten. */
	FILE *block = fopen(ACCOUNT_BLOCK, "wb");
	if(!block) return;
	if(fclose(block) != 0) return;
	account->result.restore_blocked = true;
	bool success = remove(ACCOUNT_FILE) == 0 || errno == ENOENT;
	if(remove(ACCOUNT_TEMP) != 0 && errno != ENOENT) success = false;
	account->result.status = success ? PLUS_VITA_RESULT_OK : PLUS_VITA_RESULT_STORAGE_ERROR;
}

static void *worker(void *context)
{
	PlusVitaAccount *account = context;
	account->result.operation = account->job.operation;
	if(account->job.kind == PLUS_VITA_EFFECT_READ_IMPORT)
	{
		account->result.kind = PLUS_VITA_EVENT_IMPORT_READ;
		read_import(account);
	}
	else if(account->job.kind == PLUS_VITA_EFFECT_LOAD_ACCOUNT)
	{
		account->result.kind = PLUS_VITA_EVENT_ACCOUNT_LOADED;
		load_account(account);
	}
	else if(account->job.kind == PLUS_VITA_EFFECT_SAVE_ACCOUNT)
	{
		account->result.kind = PLUS_VITA_EVENT_ACCOUNT_SAVED;
		save_account(account);
	}
	else if(account->job.kind == PLUS_VITA_EFFECT_DELETE_ACCOUNT)
	{
		account->result.kind = PLUS_VITA_EVENT_ACCOUNT_DELETED;
		delete_account(account);
	}
	else if(account->job.kind == PLUS_VITA_EFFECT_ENABLE_AUTO)
	{
		account->result.kind = PLUS_VITA_EVENT_AUTO_ENABLED;
		account->result.status = remove(ACCOUNT_BLOCK) == 0 || errno == ENOENT
			? PLUS_VITA_RESULT_OK : PLUS_VITA_RESULT_STORAGE_ERROR;
	}
	else
	{
		account->result.kind = PLUS_VITA_EVENT_AUTHORIZED;
		authorize(account);
	}
	clear_bytes(&account->job, sizeof(account->job));
	pthread_mutex_lock(&account->mutex);
	account->complete = true;
	pthread_mutex_unlock(&account->mutex);
	return NULL;
}

PlusVitaAccount *plus_vita_account_create(void)
{
	PlusVitaAccount *account = calloc(1, sizeof(*account));
	if(!account) return NULL;
	if(pthread_mutex_init(&account->mutex, NULL) != 0) { free(account); return NULL; }
	return account;
}

bool plus_vita_account_start(PlusVitaAccount *account, const PlusVitaAppEffect *effect)
{
	if(account->running) return false;
	if(effect->kind != PLUS_VITA_EFFECT_READ_IMPORT
		&& effect->kind != PLUS_VITA_EFFECT_AUTHORIZE && effect->kind != PLUS_VITA_EFFECT_LOAD_ACCOUNT
		&& effect->kind != PLUS_VITA_EFFECT_SAVE_ACCOUNT && effect->kind != PLUS_VITA_EFFECT_DELETE_ACCOUNT
		&& effect->kind != PLUS_VITA_EFFECT_ENABLE_AUTO) return false;
	account->job = *effect;
	account->complete = account->joined = false;
	memset(&account->result, 0, sizeof(account->result));
	if(effect->kind == PLUS_VITA_EFFECT_AUTHORIZE) plus_vita_request_cancel_set(false);
	pthread_attr_t attr;
	bool attr_ready = pthread_attr_init(&attr) == 0;
	int result = attr_ready ? pthread_attr_setstacksize(&attr, 256 * 1024) : -1;
	if(result == 0) result = pthread_create(&account->thread, &attr, worker, account);
	if(attr_ready) pthread_attr_destroy(&attr);
	if(result != 0) { clear_bytes(&account->job, sizeof(account->job)); return false; }
	account->running = true;
	return true;
}

bool plus_vita_account_idle(const PlusVitaAccount *account) { return !account->running; }

bool plus_vita_account_poll(PlusVitaAccount *account, PlusVitaAppEvent *event)
{
	if(!account->running || account->joined) return false;
	pthread_mutex_lock(&account->mutex);
	bool complete = account->complete;
	pthread_mutex_unlock(&account->mutex);
	if(!complete) return false;
	pthread_join(account->thread, NULL);
	account->joined = true;
	*event = account->result;
	return true;
}

void plus_vita_account_release_result(PlusVitaAccount *account)
{
	if(!account->joined) return;
	plus_vita_auth_probe_result_fini(&account->catalogue);
	memset(&account->catalogue, 0, sizeof(account->catalogue));
	clear_bytes(account->file, sizeof(account->file));
	memset(&account->result, 0, sizeof(account->result));
	account->running = false;
}

void plus_vita_account_cancel(PlusVitaAccount *account)
{
	(void)account;
	plus_vita_request_cancel_set(true);
}

bool plus_vita_account_close(PlusVitaAccount *account)
{
	if(!account) return true;
	plus_vita_account_cancel(account);
	if(account->running)
	{
		PlusVitaAppEvent ignored;
		if(!account->joined && !plus_vita_account_poll(account, &ignored)) return false;
		plus_vita_account_release_result(account);
	}
	pthread_mutex_destroy(&account->mutex);
	clear_bytes(account, sizeof(*account));
	free(account);
	return true;
}
