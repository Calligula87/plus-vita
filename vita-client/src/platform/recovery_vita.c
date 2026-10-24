// SPDX-License-Identifier: AGPL-3.0-only
#include "recovery_vita.h"
#include <psp2/power.h>
#include <psp2/kernel/threadmgr/callback.h>
#include <psp2/net/netctl.h>
#include <stddef.h>

static int callback_id = -1;
static bool interrupted;

static int power_callback(int id, int count, int flags, void *user)
{
	(void)id; (void)count; (void)user;
	if(flags & (SCE_POWER_CB_SYSTEM_SUSPEND | SCE_POWER_CB_SYSTEM_RESUMING
		| SCE_POWER_CB_SYSTEM_RESUME | SCE_POWER_CB_APP_RESUME)) interrupted = true;
	return 0;
}

bool plus_vita_recovery_init(void)
{
	interrupted = false;
	callback_id = sceKernelCreateCallback("Plus Vita power", 0, power_callback, NULL);
	if(callback_id < 0) return false;
	if(scePowerRegisterCallback(callback_id) < 0) {
		sceKernelDeleteCallback(callback_id); callback_id = -1; return false;
	}
	return true;
}

void plus_vita_recovery_fini(void)
{
	if(callback_id < 0) return;
	scePowerUnregisterCallback(callback_id);
	sceKernelDeleteCallback(callback_id);
	callback_id = -1;
}

PlusVitaRecovery plus_vita_recovery_poll(void)
{
	if(callback_id < 0) return PLUS_VITA_RECOVERY_NONE;
	sceKernelCheckCallback();
	if(interrupted || scePowerIsSuspendRequired()) return PLUS_VITA_RECOVERY_SLEEP;
	int state = 0;
	if(sceNetCtlInetGetState(&state) < 0 || state != SCE_NETCTL_STATE_CONNECTED)
		return PLUS_VITA_RECOVERY_WIFI;
	return PLUS_VITA_RECOVERY_NONE;
}

void plus_vita_recovery_acknowledge(void)
{
	/* Dispatch pending resume notifications before accepting explicit new work. */
	if(callback_id >= 0) sceKernelCheckCallback();
	interrupted = false;
}
