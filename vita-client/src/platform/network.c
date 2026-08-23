// SPDX-License-Identifier: AGPL-3.0-only

#include "network.h"

static bool initialized;

#ifdef __vita__
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>

#define PLUS_VITA_NET_MEMORY_SIZE (1024 * 1024)

static unsigned char net_memory[PLUS_VITA_NET_MEMORY_SIZE] __attribute__((aligned(64)));
static bool module_owned;
#endif

ChiakiErrorCode plus_vita_network_init(void)
{
	if(initialized)
		return CHIAKI_ERR_SUCCESS;

#ifdef __vita__
	if(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) != SCE_SYSMODULE_LOADED)
	{
		if(sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0)
			return CHIAKI_ERR_NETWORK;
		module_owned = true;
	}

	SceNetInitParam params = {
		.memory = net_memory,
		.size = sizeof(net_memory),
		.flags = 0,
	};
	if(sceNetInit(&params) < 0)
	{
		if(module_owned)
		{
			sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
			module_owned = false;
		}
		return CHIAKI_ERR_NETWORK;
	}
#endif

	initialized = true;
	return CHIAKI_ERR_SUCCESS;
}

void plus_vita_network_fini(void)
{
	if(!initialized)
		return;

#ifdef __vita__
	sceNetTerm();
	if(module_owned)
	{
		sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
		module_owned = false;
	}
#endif

	initialized = false;
}

bool plus_vita_network_is_initialized(void)
{
	return initialized;
}
