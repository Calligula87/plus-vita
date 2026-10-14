// SPDX-License-Identifier: AGPL-3.0-only
#include "input_vita.h"

static int16_t probe_vita_axis(uint8_t value)
{
	/* Vita sticks are unsigned 0..255; Chiaki expects signed centered axes.
	 * Ignore the small hardware-center wobble to avoid redundant feedback. */
	if(value >= 120 && value <= 135) return 0;
	return (int16_t)((int)value * 257 - 32768);
}

void plus_vita_input_map_pad(const SceCtrlData *pad,
	ChiakiControllerState *state)
{
	chiaki_controller_state_set_idle(state);
	uint32_t buttons = pad->buttons;
	if(buttons & SCE_CTRL_CROSS) state->buttons |= CHIAKI_CONTROLLER_BUTTON_CROSS;
	if(buttons & SCE_CTRL_CIRCLE) state->buttons |= CHIAKI_CONTROLLER_BUTTON_MOON;
	if(buttons & SCE_CTRL_SQUARE) state->buttons |= CHIAKI_CONTROLLER_BUTTON_BOX;
	if(buttons & SCE_CTRL_TRIANGLE) state->buttons |= CHIAKI_CONTROLLER_BUTTON_PYRAMID;
	if(buttons & SCE_CTRL_LEFT) state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT;
	if(buttons & SCE_CTRL_RIGHT) state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT;
	if(buttons & SCE_CTRL_UP) state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_UP;
	if(buttons & SCE_CTRL_DOWN) state->buttons |= CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN;
	if(buttons & SCE_CTRL_LTRIGGER) state->buttons |= CHIAKI_CONTROLLER_BUTTON_L1;
	if(buttons & SCE_CTRL_RTRIGGER) state->buttons |= CHIAKI_CONTROLLER_BUTTON_R1;
	if(buttons & SCE_CTRL_START) state->buttons |= CHIAKI_CONTROLLER_BUTTON_OPTIONS;
	if(buttons & SCE_CTRL_SELECT) state->buttons |= CHIAKI_CONTROLLER_BUTTON_SHARE;
	state->left_x = probe_vita_axis(pad->lx);
	state->left_y = probe_vita_axis(pad->ly);
	state->right_x = probe_vita_axis(pad->rx);
	state->right_y = probe_vita_axis(pad->ry);
}

void plus_vita_input_map_rear(const SceTouchData *touch,
	int rear_mid_x, int rear_mid_y, ChiakiControllerState *state)
{
	/* Rear top corners are L2/R2; lower corners are stick clicks L3/R3.
	 * Multiple contacts may hold different controls at the same time. */
	unsigned int count = touch->reportNum;
	if(count > SCE_TOUCH_MAX_REPORT) count = SCE_TOUCH_MAX_REPORT;
	for(unsigned int i = 0; i < count; i++)
	{
		bool left = touch->report[i].x < rear_mid_x;
		bool upper = touch->report[i].y < rear_mid_y;
		if(upper && left)
			state->l2_state = 255;
		else if(upper)
			state->r2_state = 255;
		else if(left)
			state->buttons |= CHIAKI_CONTROLLER_BUTTON_L3;
		else
			state->buttons |= CHIAKI_CONTROLLER_BUTTON_R3;
	}
}

static uint16_t probe_vita_touch_coord(int value, int minimum, int maximum,
	int output_maximum)
{
	if(maximum <= minimum || value <= minimum) return 0;
	if(value >= maximum) return (uint16_t)output_maximum;
	return (uint16_t)(((value - minimum) * output_maximum)
		/ (maximum - minimum));
}

void plus_vita_input_map_front(const SceTouchData *front,
	const SceTouchPanelInfo *panel, const ChiakiControllerState *previous,
	ChiakiControllerState *state)
{
	/* Preserve each Vita contact's slot so Chiaki can send its movement and
	 * release with the same touch ID. The protocol supports two contacts. */
	unsigned int count = front->reportNum;
	if(count > SCE_TOUCH_MAX_REPORT) count = SCE_TOUCH_MAX_REPORT;
	bool used[SCE_TOUCH_MAX_REPORT] = { 0 };
	for(unsigned int slot = 0; slot < CHIAKI_CONTROLLER_TOUCHES_MAX; slot++)
	{
		if(previous->touches[slot].id < 0) continue;
		for(unsigned int i = 0; i < count; i++)
		{
			int8_t id = (int8_t)(front->report[i].id & 0x7f);
			if(used[i] || id != previous->touches[slot].id) continue;
			state->touches[slot].id = id;
			state->touches[slot].x = probe_vita_touch_coord(front->report[i].x,
				panel->minAaX, panel->maxAaX, 1920);
			state->touches[slot].y = probe_vita_touch_coord(front->report[i].y,
				panel->minAaY, panel->maxAaY, 942);
			used[i] = true;
			break;
		}
	}
	for(unsigned int i = 0; i < count; i++)
	{
		if(used[i]) continue;
		for(unsigned int slot = 0; slot < CHIAKI_CONTROLLER_TOUCHES_MAX; slot++)
		{
			if(state->touches[slot].id >= 0) continue;
			state->touches[slot].id = (int8_t)(front->report[i].id & 0x7f);
			state->touches[slot].x = probe_vita_touch_coord(front->report[i].x,
				panel->minAaX, panel->maxAaX, 1920);
			state->touches[slot].y = probe_vita_touch_coord(front->report[i].y,
				panel->minAaY, panel->maxAaY, 942);
			break;
		}
	}
	if(count >= 2)
		state->buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
}

unsigned int plus_vita_input_touch_count(const ChiakiControllerState *state)
{
	unsigned int count = 0;
	for(unsigned int i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
		if(state->touches[i].id >= 0) count++;
	return count;
}

int plus_vita_input_vita_read(void *user, const ChiakiControllerState *previous,
	PlusVitaInputSample *sample)
{
	const PlusVitaInputVita *input = user;
	SceCtrlData pad = { 0 };
	int available = sceCtrlPeekBufferPositive(0, &pad, 1);
	if(available <= 0) return available < 0 ? -1 : 0;
	const uint32_t chord = SCE_CTRL_START | SCE_CTRL_SELECT;
	sample->stop_chord = (pad.buttons & chord) == chord;
	if(sample->stop_chord) pad.buttons &= ~chord;
	sample->native_timestamp = pad.timeStamp;
	plus_vita_input_map_pad(&pad, &sample->state);
	if(input->rear_ready)
	{
		SceTouchData touch = { 0 };
		if(sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1) > 0)
			plus_vita_input_map_rear(&touch, input->rear_mid_x, input->rear_mid_y, &sample->state);
	}
	if(input->front_ready)
	{
		SceTouchData touch = { 0 };
		if(sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0)
			plus_vita_input_map_front(&touch, &input->front_info, previous, &sample->state);
	}
	return 1;
}

