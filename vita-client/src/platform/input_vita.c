// SPDX-License-Identifier: AGPL-3.0-only
#include "input_vita.h"

static int16_t probe_vita_axis(uint8_t value)
{
	/* Vita sticks are unsigned 0..255; Chiaki expects signed centered axes.
	 * Ignore the small hardware-center wobble to avoid redundant feedback. */
	if(value >= 120 && value <= 135) return 0;
	return (int16_t)((int)value * 257 - 32768);
}

static void map_pad_axes(const SceCtrlData *pad,ChiakiControllerState *state)
{
	state->left_x=probe_vita_axis(pad->lx);state->left_y=probe_vita_axis(pad->ly);
	state->right_x=probe_vita_axis(pad->rx);state->right_y=probe_vita_axis(pad->ry);
}

void plus_vita_input_map_pad(const SceCtrlData *pad,ChiakiControllerState *state)
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
	map_pad_axes(pad,state);
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

static void map_front_contacts(const SceTouchData *front,
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
}

void plus_vita_input_map_front(const SceTouchData *front,
	const SceTouchPanelInfo *panel,const ChiakiControllerState *previous,ChiakiControllerState *state)
{
	map_front_contacts(front,panel,previous,state);
	if(front->reportNum>=2) state->buttons|=CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
}

unsigned int plus_vita_input_touch_count(const ChiakiControllerState *state)
{
	unsigned int count = 0;
	for(unsigned int i = 0; i < CHIAKI_CONTROLLER_TOUCHES_MAX; i++)
		if(state->touches[i].id >= 0) count++;
	return count;
}

static void apply_layout(const PlusVitaControlLayout *layout,const uint8_t *levels,ChiakiControllerState *state)
{
	static const uint32_t buttons[PLUS_VITA_CONTROL_COUNT]={0,
		CHIAKI_CONTROLLER_BUTTON_CROSS,CHIAKI_CONTROLLER_BUTTON_MOON,
		CHIAKI_CONTROLLER_BUTTON_BOX,CHIAKI_CONTROLLER_BUTTON_PYRAMID,
		CHIAKI_CONTROLLER_BUTTON_DPAD_UP,CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN,
		CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT,CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT,
		CHIAKI_CONTROLLER_BUTTON_L1,CHIAKI_CONTROLLER_BUTTON_R1,0,0,
		CHIAKI_CONTROLLER_BUTTON_L3,CHIAKI_CONTROLLER_BUTTON_R3,
		CHIAKI_CONTROLLER_BUTTON_OPTIONS,CHIAKI_CONTROLLER_BUTTON_SHARE,CHIAKI_CONTROLLER_BUTTON_TOUCHPAD};
	state->buttons=0;state->l2_state=state->r2_state=0;
	for(unsigned i=0;i<PLUS_VITA_INPUT_COUNT;i++) {
		unsigned action=layout->bindings[i];
		if(action==PLUS_VITA_CONTROL_L2) { if(levels[i]>state->l2_state) state->l2_state=levels[i]; }
		else if(action==PLUS_VITA_CONTROL_R2) { if(levels[i]>state->r2_state) state->r2_state=levels[i]; }
		/* Half pressure is a digital press; below half releases it. */
		else if(levels[i]>=128 && action<PLUS_VITA_CONTROL_COUNT) state->buttons|=buttons[action];
	}
}

int plus_vita_input_vita_read(void *user, const ChiakiControllerState *previous,
	PlusVitaInputSample *sample)
{
	PlusVitaInputVita *input = user;
	SceCtrlData pad = { 0 };
	int available = plus_vita_controller_read(&input->controller,&pad);
	if(available <= 0) return available < 0 ? -1 : 0;
	const uint32_t chord = SCE_CTRL_START | SCE_CTRL_SELECT;
	sample->stop_chord = (pad.buttons & chord) == chord;
	if(sample->stop_chord) pad.buttons &= ~chord;
	sample->native_timestamp = pad.timeStamp;
	if(!input->controller.ready) { chiaki_controller_state_set_idle(&sample->state);return 1; }
	chiaki_controller_state_set_idle(&sample->state);
	map_pad_axes(&pad,&sample->state);
	PlusVitaControlLayout defaults;
	plus_vita_control_layout_defaults(&defaults);
	const PlusVitaControlLayout *layout=input->layout_ready && plus_vita_control_layout_valid(&input->layout) ? &input->layout : &defaults;
	uint8_t levels[PLUS_VITA_INPUT_COUNT]={0};
	static const uint32_t pad_buttons[]={SCE_CTRL_CROSS,SCE_CTRL_CIRCLE,SCE_CTRL_SQUARE,SCE_CTRL_TRIANGLE,
		SCE_CTRL_UP,SCE_CTRL_DOWN,SCE_CTRL_LEFT,SCE_CTRL_RIGHT,SCE_CTRL_LTRIGGER,SCE_CTRL_RTRIGGER,SCE_CTRL_START,SCE_CTRL_SELECT};
	for(unsigned i=0;i<sizeof(pad_buttons)/sizeof(*pad_buttons);i++) if(pad.buttons&pad_buttons[i]) levels[i]=255;
	if(input->controller.external) {
		levels[PLUS_VITA_INPUT_L1]=(pad.buttons&SCE_CTRL_L1) ? 255 : 0;
		levels[PLUS_VITA_INPUT_R1]=(pad.buttons&SCE_CTRL_R1) ? 255 : 0;
		levels[PLUS_VITA_INPUT_L2]=pad.lt ? pad.lt : (pad.buttons&SCE_CTRL_L2) ? 255 : 0;
		levels[PLUS_VITA_INPUT_R2]=pad.rt ? pad.rt : (pad.buttons&SCE_CTRL_R2) ? 255 : 0;
		levels[PLUS_VITA_INPUT_L3]=(pad.buttons&SCE_CTRL_L3) ? 255 : 0;
		levels[PLUS_VITA_INPUT_R3]=(pad.buttons&SCE_CTRL_R3) ? 255 : 0;
	}
	if(input->rear_ready && !input->controller.external)
	{
		SceTouchData touch = { 0 };
		if(sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1) > 0) {
			unsigned count=touch.reportNum>SCE_TOUCH_MAX_REPORT ? SCE_TOUCH_MAX_REPORT : touch.reportNum;
			for(unsigned i=0;i<count;i++) {
				unsigned quadrant=(touch.report[i].y<input->rear_mid_y ? 0 : 2)+(touch.report[i].x<input->rear_mid_x ? 0 : 1);
				levels[PLUS_VITA_INPUT_REAR_TL+quadrant]=255;
			}
		}
	}
	if(input->front_ready && !input->controller.external)
	{
		SceTouchData touch = { 0 };
		if(sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0) {
			if(layout->front_motion) map_front_contacts(&touch,&input->front_info,previous,&sample->state);
			if(touch.reportNum>=2) levels[PLUS_VITA_INPUT_FRONT_CLICK]=255;
		}
	}
	apply_layout(layout,levels,&sample->state);
	return 1;
}

