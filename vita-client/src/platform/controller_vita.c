// SPDX-License-Identifier: AGPL-3.0-only
#include "controller_vita.h"
#include <psp2/kernel/sysmem.h>

static bool supported(unsigned type)
{ return type==SCE_CTRL_TYPE_DS3 || type==SCE_CTRL_TYPE_DS4; }

static int disconnected(PlusVitaController *controller,SceCtrlData *pad)
{
	controller->ready=false;controller->neutral_required=true;
	controller->device=PLUS_VITA_DEVICE_DISCONNECTED;controller->port=-1;
	*pad=(SceCtrlData){.lx=128,.ly=128,.rx=128,.ry=128};
	return 1;
}

static bool neutral(const SceCtrlData *pad)
{
	return !(pad->buttons&0xffff) && !pad->lt && !pad->rt
		&& pad->lx>=120 && pad->lx<=135 && pad->ly>=120 && pad->ly<=135
		&& pad->rx>=120 && pad->rx<=135 && pad->ry>=120 && pad->ry<=135;
}

int plus_vita_controller_read(PlusVitaController *controller,SceCtrlData *pad)
{
	if(!controller->initialized) {
		controller->initialized=true;controller->port=-1;
		controller->tv=sceKernelGetModel()==SCE_KERNEL_MODEL_VITATV;
	}
	SceCtrlPortInfo ports={0};
	int info=sceCtrlGetControllerPortInfo(&ports);
	int selected=-1;
	if(info>=0) {
		if(controller->external && controller->port>=0 && controller->port<(int)sizeof(ports.port)
			&& supported(ports.port[controller->port])) selected=controller->port;
		else for(unsigned i=0;i<sizeof(ports.port);i++) if(supported(ports.port[i])) { selected=(int)i;break; }
	} else if(controller->external) selected=controller->port;
	if(selected<0) {
		if(controller->tv || controller->external || (info>=0 && ports.port[0]==SCE_CTRL_TYPE_VIRT))
			return disconnected(controller,pad);
		int count=sceCtrlPeekBufferPositive(0,pad,1);
		controller->device=PLUS_VITA_DEVICE_VITA;controller->ready=count>0;
		return count<0 ? -1 : count;
	}
	PlusVitaControlDevice device=info>=0
		? (ports.port[selected]==SCE_CTRL_TYPE_DS4 ? PLUS_VITA_DEVICE_DS4 : PLUS_VITA_DEVICE_DS3)
		: controller->device;
	if(selected!=controller->port || device!=controller->device) controller->neutral_required=true;
	controller->port=selected;controller->external=true;controller->device=device;
	int count=sceCtrlPeekBufferPositiveExt2(selected,pad,1);
	if(count==(int)SCE_CTRL_ERROR_NO_DEVICE) return disconnected(controller,pad);
	if(count<=0) { controller->ready=false;return count<0 ? -1 : 0; }
	if(pad->buttons&SCE_CTRL_INTERCEPTED) controller->neutral_required=true;
	else if(controller->neutral_required && neutral(pad)) controller->neutral_required=false;
	controller->ready=!controller->neutral_required;
	return 1;
}
