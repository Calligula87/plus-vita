// SPDX-License-Identifier: AGPL-3.0-only
#include "control_layout.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

void plus_vita_control_layout_defaults(PlusVitaControlLayout *layout)
{
	*layout=(PlusVitaControlLayout){.bindings={
		PLUS_VITA_CONTROL_CROSS,PLUS_VITA_CONTROL_CIRCLE,PLUS_VITA_CONTROL_SQUARE,PLUS_VITA_CONTROL_TRIANGLE,
		PLUS_VITA_CONTROL_UP,PLUS_VITA_CONTROL_DOWN,PLUS_VITA_CONTROL_LEFT,PLUS_VITA_CONTROL_RIGHT,
		PLUS_VITA_CONTROL_L1,PLUS_VITA_CONTROL_R1,PLUS_VITA_CONTROL_OPTIONS,PLUS_VITA_CONTROL_SHARE,
		PLUS_VITA_CONTROL_L2,PLUS_VITA_CONTROL_R2,PLUS_VITA_CONTROL_L3,PLUS_VITA_CONTROL_R3,
		PLUS_VITA_CONTROL_L2,PLUS_VITA_CONTROL_R2,PLUS_VITA_CONTROL_L3,PLUS_VITA_CONTROL_R3,
		PLUS_VITA_CONTROL_TOUCHPAD},.front_motion=true};
}
bool plus_vita_control_layout_valid(const PlusVitaControlLayout *layout)
{
	if(!layout) return false;
	for(unsigned i=0;i<PLUS_VITA_INPUT_COUNT;i++) if(layout->bindings[i]>=PLUS_VITA_CONTROL_COUNT) return false;
	return true;
}
const char *plus_vita_control_action_name(unsigned action)
{
	static const char *const names[]={"Disabled","Cross","Circle","Square","Triangle",
		"D-pad Up","D-pad Down","D-pad Left","D-pad Right","L1","R1","L2","R2","L3","R3",
		"Options","Share","Touchpad click"};
	return action<PLUS_VITA_CONTROL_COUNT ? names[action] : "Invalid";
}
const char *plus_vita_control_input_name(unsigned input,PlusVitaControlDevice device)
{
	static const char *const names[]={"Cross","Circle","Square","Triangle","D-pad Up","D-pad Down",
		"D-pad Left","D-pad Right","L1","R1","START","SELECT","L2","R2","L3","R3",
		"Rear top left","Rear top right","Rear bottom left","Rear bottom right","Front two-finger click"};
	if(device==PLUS_VITA_DEVICE_DS4 && input==PLUS_VITA_INPUT_START) return "OPTIONS";
	if(device==PLUS_VITA_DEVICE_DS4 && input==PLUS_VITA_INPUT_SELECT) return "SHARE";
	return input<PLUS_VITA_INPUT_COUNT ? names[input] : "Invalid";
}
bool plus_vita_control_input_available(unsigned input,PlusVitaControlDevice device)
{
	if(input>=PLUS_VITA_INPUT_COUNT || device==PLUS_VITA_DEVICE_DISCONNECTED) return false;
	return device==PLUS_VITA_DEVICE_VITA
		? input<PLUS_VITA_INPUT_L2 || input>=PLUS_VITA_INPUT_REAR_TL
		: input<PLUS_VITA_INPUT_REAR_TL;
}

/* Fixed byte encoding, not a compiler struct dump. The magic versions the
 * complete record; unknown or partial layouts are never partly applied. */
PlusVitaControlsResult plus_vita_control_layout_load(const char *path,PlusVitaControlLayout *layout)
{
	plus_vita_control_layout_defaults(layout);
	FILE *file=fopen(path,"rb");
	if(!file) return errno==ENOENT ? PLUS_VITA_CONTROLS_MISSING : PLUS_VITA_CONTROLS_STORAGE_ERROR;
	unsigned char record[9+PLUS_VITA_INPUT_COUNT];
	size_t size=fread(record,1,sizeof(record),file);
	int extra=fgetc(file);
	bool failed=ferror(file)!=0;
	if(fclose(file)) failed=true;
	if(failed) return PLUS_VITA_CONTROLS_STORAGE_ERROR;
	if(size!=sizeof(record) || extra!=EOF || memcmp(record,"PVCTRL1",8) || record[8]>1)
		return PLUS_VITA_CONTROLS_INVALID;
	PlusVitaControlLayout loaded={.front_motion=record[8]!=0};
	memcpy(loaded.bindings,record+9,PLUS_VITA_INPUT_COUNT);
	if(!plus_vita_control_layout_valid(&loaded)) return PLUS_VITA_CONTROLS_INVALID;
	*layout=loaded;return PLUS_VITA_CONTROLS_OK;
}
bool plus_vita_control_layout_save(const char *path,const PlusVitaControlLayout *layout)
{
	if(!path || !plus_vita_control_layout_valid(layout)) return false;
	char temporary[1024];
	int length=snprintf(temporary,sizeof(temporary),"%s.tmp",path);
	if(length<0 || (size_t)length>=sizeof(temporary)) return false;
	unsigned char record[9+PLUS_VITA_INPUT_COUNT]="PVCTRL1";
	record[8]=layout->front_motion ? 1 : 0;
	memcpy(record+9,layout->bindings,PLUS_VITA_INPUT_COUNT);
	FILE *file=fopen(temporary,"wb");if(!file) return false;
	bool success=fwrite(record,1,sizeof(record),file)==sizeof(record);
	if(fflush(file)) success=false;
	if(fclose(file)) success=false;
	if(success) success=rename(temporary,path)==0;
	if(!success) remove(temporary);
	return success;
}
