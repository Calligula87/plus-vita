// SPDX-License-Identifier: AGPL-3.0-only
#ifndef PLUS_VITA_CONTROL_LAYOUT_H
#define PLUS_VITA_CONTROL_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>

#define PLUS_VITA_CONTROLS_PATH "ux0:data/plus-vita-controls.dat"
typedef enum {
	PLUS_VITA_INPUT_CROSS, PLUS_VITA_INPUT_CIRCLE, PLUS_VITA_INPUT_SQUARE, PLUS_VITA_INPUT_TRIANGLE,
	PLUS_VITA_INPUT_UP, PLUS_VITA_INPUT_DOWN, PLUS_VITA_INPUT_LEFT, PLUS_VITA_INPUT_RIGHT,
	PLUS_VITA_INPUT_L1, PLUS_VITA_INPUT_R1, PLUS_VITA_INPUT_START, PLUS_VITA_INPUT_SELECT,
	PLUS_VITA_INPUT_L2, PLUS_VITA_INPUT_R2, PLUS_VITA_INPUT_L3, PLUS_VITA_INPUT_R3,
	PLUS_VITA_INPUT_REAR_TL, PLUS_VITA_INPUT_REAR_TR, PLUS_VITA_INPUT_REAR_BL, PLUS_VITA_INPUT_REAR_BR,
	PLUS_VITA_INPUT_FRONT_CLICK, PLUS_VITA_INPUT_COUNT
} PlusVitaControlInput;
typedef enum {
	PLUS_VITA_CONTROL_DISABLED, PLUS_VITA_CONTROL_CROSS, PLUS_VITA_CONTROL_CIRCLE,
	PLUS_VITA_CONTROL_SQUARE, PLUS_VITA_CONTROL_TRIANGLE, PLUS_VITA_CONTROL_UP,
	PLUS_VITA_CONTROL_DOWN, PLUS_VITA_CONTROL_LEFT, PLUS_VITA_CONTROL_RIGHT,
	PLUS_VITA_CONTROL_L1, PLUS_VITA_CONTROL_R1, PLUS_VITA_CONTROL_L2, PLUS_VITA_CONTROL_R2,
	PLUS_VITA_CONTROL_L3, PLUS_VITA_CONTROL_R3, PLUS_VITA_CONTROL_OPTIONS, PLUS_VITA_CONTROL_SHARE,
	PLUS_VITA_CONTROL_TOUCHPAD, PLUS_VITA_CONTROL_COUNT
} PlusVitaControlAction;
typedef enum { PLUS_VITA_DEVICE_VITA, PLUS_VITA_DEVICE_DS3, PLUS_VITA_DEVICE_DS4,
	PLUS_VITA_DEVICE_DISCONNECTED } PlusVitaControlDevice;
typedef struct {
	uint8_t bindings[PLUS_VITA_INPUT_COUNT];
	bool front_motion;
} PlusVitaControlLayout;
typedef enum { PLUS_VITA_CONTROLS_OK, PLUS_VITA_CONTROLS_MISSING,
	PLUS_VITA_CONTROLS_INVALID, PLUS_VITA_CONTROLS_STORAGE_ERROR } PlusVitaControlsResult;

void plus_vita_control_layout_defaults(PlusVitaControlLayout *layout);
bool plus_vita_control_layout_valid(const PlusVitaControlLayout *layout);
const char *plus_vita_control_action_name(unsigned action);
const char *plus_vita_control_input_name(unsigned input,PlusVitaControlDevice device);
bool plus_vita_control_input_available(unsigned input,PlusVitaControlDevice device);
PlusVitaControlsResult plus_vita_control_layout_load(const char *path,PlusVitaControlLayout *layout);
bool plus_vita_control_layout_save(const char *path,const PlusVitaControlLayout *layout);
#endif
