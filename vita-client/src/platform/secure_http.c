// SPDX-License-Identifier: AGPL-3.0-only

#include "secure_http.h"
#include "network.h"

static bool initialized;
static bool network_owned;

#ifdef __vita__
#include <psp2/libssl.h>
#include <psp2/net/http.h>
#include <psp2/sysmodule.h>

#define PLUS_VITA_HTTP_POOL_SIZE (1024 * 1024)
#define PLUS_VITA_SSL_POOL_SIZE (1024 * 1024)

static bool module_owned;
static bool http_initialized;
#endif

ChiakiErrorCode plus_vita_secure_http_init(void)
{
	if(initialized)
		return CHIAKI_ERR_SUCCESS;

	network_owned = !plus_vita_network_is_initialized();
	if(plus_vita_network_init() != CHIAKI_ERR_SUCCESS)
		return CHIAKI_ERR_NETWORK;

#ifdef __vita__
	if(sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_SYSMODULE_LOADED)
	{
		if(sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS) < 0)
			goto fail;
		module_owned = true;
	}

	if(sceHttpInit(PLUS_VITA_HTTP_POOL_SIZE) < 0)
		goto fail;
	http_initialized = true;

	if(sceSslInit(PLUS_VITA_SSL_POOL_SIZE) < 0)
		goto fail;
#endif

	initialized = true;
	return CHIAKI_ERR_SUCCESS;

#ifdef __vita__
fail:
	if(http_initialized)
	{
		sceHttpTerm();
		http_initialized = false;
	}
	if(module_owned)
	{
		sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
		module_owned = false;
	}
	if(network_owned)
		plus_vita_network_fini();
	network_owned = false;
	return CHIAKI_ERR_NETWORK;
#endif
}

void plus_vita_secure_http_fini(void)
{
	if(!initialized)
		return;

#ifdef __vita__
	sceSslTerm();
	sceHttpTerm();
	http_initialized = false;
	if(module_owned)
	{
		sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTPS);
		module_owned = false;
	}
#endif

	initialized = false;
	if(network_owned)
		plus_vita_network_fini();
	network_owned = false;
}

bool plus_vita_secure_http_is_initialized(void)
{
	return initialized;
}
