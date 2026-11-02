// SPDX-License-Identifier: AGPL-3.0-only
#include "app.h"
#include "../src/platform/control_layout.h"
#include "../src/platform/input_vita.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <psp2/kernel/sysmem.h>

static bool fail_rename;
int __real_rename(const char *,const char *);
int __wrap_rename(const char *source,const char *destination)
{ if(fail_rename) { errno=EIO;return -1; }return __real_rename(source,destination); }

static SceCtrlData pad;
int sceKernelGetModel(void) { return SCE_KERNEL_MODEL_VITA; }
int sceCtrlGetControllerPortInfo(SceCtrlPortInfo *info) { *info=(SceCtrlPortInfo){.port={SCE_CTRL_TYPE_PHY}};return 0; }
int sceCtrlPeekBufferPositiveExt2(int port,SceCtrlData *out,int count)
{ (void)port;(void)out;(void)count;assert(!"Unexpected external controller");return -1; }
int sceCtrlPeekBufferPositive(int port,SceCtrlData *out,int count)
{ assert(port==0 && count==1);*out=pad;return 1; }
int sceTouchPeek(uint32_t port,SceTouchData *out,uint32_t count)
{ (void)port;(void)count;memset(out,0,sizeof(*out));return 1; }

static PlusVitaAppEffect choose(PlusVitaApp *app,PlusVitaAppAction action,const char *label)
{
	PlusVitaAppView view=plus_vita_app_view(app);
	for(size_t i=0;i<view.action_count;i++) if(view.actions[i].id==action
		&& (!label || !strcmp(view.actions[i].label,label)))
		return plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_TOUCH,
			.x=view.actions[i].x+10,.y=view.actions[i].y+10});
	assert(!"Expected visible controls action");return (PlusVitaAppEffect){0};
}

static PlusVitaAppEffect play(PlusVitaApp *app)
{
	PlusVitaAppEffect load=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_START});
	const char json[]="{\"npsso\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
	PlusVitaAppEffect auth=plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACCOUNT_LOADED,
		.operation=load.operation,.data=json,.data_size=sizeof(json)-1});
	PlusVitaCatalogEntry game={.name="Test game",.id="FAKECUSA1",.launch_id_valid=true};
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_AUTHORIZED,.operation=auth.operation,
		.entries=&game,.entry_count=1});
	choose(app,PLUS_VITA_ACTION_GAME,NULL);
	return choose(app,PLUS_VITA_ACTION_PLAY,NULL);
}

static void assign_cross(PlusVitaApp *app,const char *destination)
{
	choose(app,PLUS_VITA_ACTION_CONTROL_LAYOUT,NULL);
	choose(app,PLUS_VITA_ACTION_BINDINGS,NULL);
	PlusVitaAppView view=plus_vita_app_view(app);
	assert(strncmp(view.actions[0].label,"Cross: ",7)==0);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_ACTIVATE});
	choose(app,PLUS_VITA_ACTION_BIND_ACTION,destination);
	choose(app,PLUS_VITA_ACTION_BACK,NULL);
}

static void save_controls(PlusVitaApp *app,bool succeeds)
{
	PlusVitaAppEffect save=choose(app,PLUS_VITA_ACTION_SAVE_CONTROLS,NULL);
	assert(save.kind==PLUS_VITA_EFFECT_SAVE_CONTROLS);
	assert(plus_vita_control_layout_save("layout.dat",&save.controls)==succeeds);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_CONTROLS_SAVED,.operation=save.operation,
		.status=succeeds ? PLUS_VITA_RESULT_OK : PLUS_VITA_RESULT_STORAGE_ERROR});
}

static uint32_t restart_cross(void)
{
	PlusVitaControlLayout loaded;
	plus_vita_control_layout_load("layout.dat",&loaded);
	PlusVitaApp *app=plus_vita_app_create();assert(app);
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_CONTROLS_LOADED,.controls=loaded});
	PlusVitaAppEffect session=play(app);
	PlusVitaInputVita input={.layout=session.controls,.layout_ready=true};
	ChiakiControllerState previous;chiaki_controller_state_set_idle(&previous);
	PlusVitaInputSample sample={0};
	assert(plus_vita_input_vita_read(&input,&previous,&sample)==1);
	plus_vita_app_destroy(app);return sample.state.buttons;
}

static void cancel_restore_and_failed_save(void)
{
	PlusVitaApp *app=plus_vita_app_create();assert(app);
	choose(app,PLUS_VITA_ACTION_SETTINGS,NULL);
	assign_cross(app,"Circle");save_controls(app,true);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_MOON);
	assign_cross(app,"Disabled");choose(app,PLUS_VITA_ACTION_CANCEL_CONTROLS,NULL);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_MOON);
	choose(app,PLUS_VITA_ACTION_CONTROL_LAYOUT,NULL);
	choose(app,PLUS_VITA_ACTION_CONTROL_DEFAULTS,NULL);
	choose(app,PLUS_VITA_ACTION_CANCEL_CONTROLS,NULL);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_MOON);
	assign_cross(app,"Disabled");fail_rename=true;save_controls(app,false);fail_rename=false;
	assert(strstr(plus_vita_app_view(app).body,"Save failed"));
	choose(app,PLUS_VITA_ACTION_CANCEL_CONTROLS,NULL);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_MOON);
	choose(app,PLUS_VITA_ACTION_CONTROL_LAYOUT,NULL);
	choose(app,PLUS_VITA_ACTION_CONTROL_DEFAULTS,NULL);save_controls(app,true);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_CROSS);
	FILE *invalid=fopen("layout.dat","wb");assert(invalid);
	assert(fwrite("invalid",1,7,invalid)==7 && fclose(invalid)==0);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_CROSS);
	assert(remove("layout.dat")==0);
	assert(restart_cross()==CHIAKI_CONTROLLER_BUTTON_CROSS);
	plus_vita_app_destroy(app);
}

int main(void)
{
	PlusVitaApp *app=plus_vita_app_create();assert(app);
	choose(app,PLUS_VITA_ACTION_SETTINGS,NULL);
	choose(app,PLUS_VITA_ACTION_CONTROL_LAYOUT,NULL);
	choose(app,PLUS_VITA_ACTION_BINDINGS,NULL);
	choose(app,PLUS_VITA_ACTION_BINDING,"Cross: Cross");
	choose(app,PLUS_VITA_ACTION_BIND_ACTION,"Circle");
	choose(app,PLUS_VITA_ACTION_BACK,NULL);
	PlusVitaAppEffect save=choose(app,PLUS_VITA_ACTION_SAVE_CONTROLS,NULL);
	assert(save.kind==PLUS_VITA_EFFECT_SAVE_CONTROLS);
	assert(plus_vita_control_layout_save("layout.dat",&save.controls));
	plus_vita_app_dispatch(app,(PlusVitaAppEvent){.kind=PLUS_VITA_EVENT_CONTROLS_SAVED,.operation=save.operation});
	choose(app,PLUS_VITA_ACTION_BACK,NULL);
	PlusVitaAppEffect session=play(app);
	assert(session.kind==PLUS_VITA_EFFECT_PLAY);
	PlusVitaInputVita input={.layout=session.controls,.layout_ready=true};
	pad=(SceCtrlData){.buttons=SCE_CTRL_CROSS,.lx=128,.ly=128,.rx=128,.ry=128};
	ChiakiControllerState previous;chiaki_controller_state_set_idle(&previous);
	PlusVitaInputSample sample={0};
	assert(plus_vita_input_vita_read(&input,&previous,&sample)==1);
	assert(sample.state.buttons==CHIAKI_CONTROLLER_BUTTON_MOON);
	PlusVitaControlLayout retained;
	assert(plus_vita_control_layout_load("layout.dat",&retained)==PLUS_VITA_CONTROLS_OK);
	assert(retained.bindings[PLUS_VITA_INPUT_CROSS]==PLUS_VITA_CONTROL_CIRCLE);
	assert(remove("layout.dat")==0);
	plus_vita_app_destroy(app);
	cancel_restore_and_failed_save();
	puts("controls journey: Settings edit, save, next-session mapping and restart persistence passed");
}
