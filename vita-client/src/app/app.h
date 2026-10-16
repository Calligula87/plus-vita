// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_APP_H
#define PLUS_VITA_APP_H
#include <stddef.h>
#include <stdint.h>

typedef struct plus_vita_app_t PlusVitaApp;
typedef enum {
	PLUS_VITA_APP_WELCOME,
	PLUS_VITA_APP_ABOUT,
	PLUS_VITA_APP_PREPARING,
	PLUS_VITA_APP_SETUP,
	PLUS_VITA_APP_EXIT_CONFIRM,
	PLUS_VITA_APP_CLOSED
} PlusVitaAppScreen;
typedef enum {
	PLUS_VITA_ACTION_SETUP,
	PLUS_VITA_ACTION_ABOUT,
	PLUS_VITA_ACTION_EXIT,
	PLUS_VITA_ACTION_BACK
} PlusVitaAppAction;
typedef struct {
	PlusVitaAppAction id;
	const char *label;
	int x, y, width, height;
} PlusVitaAppButton;
typedef struct {
	PlusVitaAppScreen screen;
	const char *title, *subtitle, *body, *hint;
	PlusVitaAppButton actions[3];
	size_t action_count, selected;
} PlusVitaAppView;
typedef enum {
	PLUS_VITA_EVENT_UP, PLUS_VITA_EVENT_DOWN,
	PLUS_VITA_EVENT_ACTIVATE, PLUS_VITA_EVENT_BACK,
	PLUS_VITA_EVENT_TOUCH, PLUS_VITA_EVENT_SETUP_READY
} PlusVitaAppEventKind;
typedef struct {
	PlusVitaAppEventKind kind;
	int x, y;
	uint64_t operation;
} PlusVitaAppEvent;
typedef enum {
	PLUS_VITA_EFFECT_NONE, PLUS_VITA_EFFECT_EXIT, PLUS_VITA_EFFECT_PREPARE_SETUP
} PlusVitaAppEffectKind;
typedef struct { PlusVitaAppEffectKind kind; uint64_t operation; } PlusVitaAppEffect;

PlusVitaApp *plus_vita_app_create(void);
void plus_vita_app_destroy(PlusVitaApp *app);
PlusVitaAppView plus_vita_app_view(const PlusVitaApp *app);
PlusVitaAppEffect plus_vita_app_dispatch(PlusVitaApp *app, PlusVitaAppEvent event);
#endif
