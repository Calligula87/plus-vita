// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_APP_H
#define PLUS_VITA_APP_H
#include <stddef.h>
#include <stdint.h>
#include "../platform/auth_probe.h"

typedef struct plus_vita_app_t PlusVitaApp;
typedef enum {
	PLUS_VITA_APP_WELCOME,
	PLUS_VITA_APP_ABOUT,
	PLUS_VITA_APP_PREPARING,
	PLUS_VITA_APP_SETUP_GUIDE,
	PLUS_VITA_APP_SETUP,
	PLUS_VITA_APP_AUTHORIZING,
	PLUS_VITA_APP_CATALOGUE,
	PLUS_VITA_APP_ERROR,
	PLUS_VITA_APP_EXIT_CONFIRM,
	PLUS_VITA_APP_CLOSED, PLUS_VITA_APP_ACCOUNT, PLUS_VITA_APP_FORGET_CONFIRM
} PlusVitaAppScreen;
typedef enum {
	PLUS_VITA_ACTION_SETUP,
	PLUS_VITA_ACTION_CATALOGUE,
	PLUS_VITA_ACTION_ABOUT,
	PLUS_VITA_ACTION_EXIT,
	PLUS_VITA_ACTION_BACK,
	PLUS_VITA_ACTION_IMPORT,
	PLUS_VITA_ACTION_GAME, PLUS_VITA_ACTION_CONTINUE,
	PLUS_VITA_ACTION_REMEMBER, PLUS_VITA_ACTION_ACCOUNT, PLUS_VITA_ACTION_RESTORE,
	PLUS_VITA_ACTION_RETRY, PLUS_VITA_ACTION_LABEL, PLUS_VITA_ACTION_FORGET, PLUS_VITA_ACTION_CONFIRM_FORGET
} PlusVitaAppAction;
typedef struct {
	PlusVitaAppAction id;
	const char *label;
	int x, y, width, height;
} PlusVitaAppButton;
typedef struct {
	PlusVitaAppScreen screen;
	const char *title, *subtitle, *body, *hint;
	PlusVitaAppButton actions[4];
	size_t action_count, selected;
	uint64_t operation;
} PlusVitaAppView;
typedef enum {
	PLUS_VITA_EVENT_PAD,
	PLUS_VITA_EVENT_UP, PLUS_VITA_EVENT_DOWN,
	PLUS_VITA_EVENT_ACTIVATE, PLUS_VITA_EVENT_BACK,
	PLUS_VITA_EVENT_TOUCH, PLUS_VITA_EVENT_SETUP_READY,
	PLUS_VITA_EVENT_IMPORT_READ,
	PLUS_VITA_EVENT_AUTHORIZED, PLUS_VITA_EVENT_START, PLUS_VITA_EVENT_ACCOUNT_LOADED,
	PLUS_VITA_EVENT_ACCOUNT_SAVED, PLUS_VITA_EVENT_ACCOUNT_DELETED, PLUS_VITA_EVENT_LABEL_ENTERED,
	PLUS_VITA_EVENT_PAD_RESET, PLUS_VITA_EVENT_AUTO_ENABLED
} PlusVitaAppEventKind;
typedef enum {
	PLUS_VITA_RESULT_OK, PLUS_VITA_RESULT_STORAGE_ERROR, PLUS_VITA_RESULT_MISSING,
	PLUS_VITA_RESULT_REJECTED, PLUS_VITA_RESULT_NETWORK_ERROR, PLUS_VITA_RESULT_CATALOGUE_ERROR,
	PLUS_VITA_RESULT_CANCELLED, PLUS_VITA_RESULT_BUSY
} PlusVitaAppResult;

/* Vita button bits; the adapter verifies these against the SDK. Other bits,
 * including the D-pad, are deliberately ignored by menu navigation. */
enum { PLUS_VITA_MENU_CIRCLE = 0x2000, PLUS_VITA_MENU_CROSS = 0x4000 };
typedef struct {
	uint32_t buttons;
	unsigned char left_y;
	uint64_t time_us;
} PlusVitaAppPad;
typedef struct {
	PlusVitaAppEventKind kind;
	PlusVitaAppPad pad;
	int x, y;
	uint64_t operation;
	PlusVitaAppResult status;
	const char *data;
	size_t data_size;
	const PlusVitaCatalogEntry *entries;
	size_t entry_count;
	bool restore_blocked, retained_json;
	char account_name[129];
} PlusVitaAppEvent;
typedef enum {
	PLUS_VITA_EFFECT_NONE, PLUS_VITA_EFFECT_EXIT, PLUS_VITA_EFFECT_PREPARE_SETUP,
	PLUS_VITA_EFFECT_READ_IMPORT,
	PLUS_VITA_EFFECT_AUTHORIZE, PLUS_VITA_EFFECT_LOAD_ACCOUNT, PLUS_VITA_EFFECT_SAVE_ACCOUNT,
	PLUS_VITA_EFFECT_DELETE_ACCOUNT, PLUS_VITA_EFFECT_EDIT_LABEL, PLUS_VITA_EFFECT_ENABLE_AUTO
} PlusVitaAppEffectKind;
/* Authorization effects contain sensitive material for the Sony adapter only.
 * Never log or render an effect/event; clear its credential after consumption. */
typedef struct {
	PlusVitaAppEffectKind kind;
	uint64_t operation;
	PlusVitaCredential credential;
	char label[129];
	bool forget;
} PlusVitaAppEffect;

PlusVitaApp *plus_vita_app_create(void);
void plus_vita_app_destroy(PlusVitaApp *app);
PlusVitaAppView plus_vita_app_view(const PlusVitaApp *app);
PlusVitaAppEffect plus_vita_app_dispatch(PlusVitaApp *app, PlusVitaAppEvent event);
#endif
