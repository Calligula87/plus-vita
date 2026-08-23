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

#ifdef __vita__
ChiakiErrorCode plus_vita_secure_http_probe_untrusted(
	const char *url, PlusVitaTlsProbeResult *result)
{
	if(!initialized || !url || !result)
		return CHIAKI_ERR_UNINITIALIZED;

	result->send_result = 0;
	result->ssl_error = 0;
	result->ssl_detail = 0;
	int template_id = sceHttpCreateTemplate(
		"plus-vita-local-cert-probe/1", SCE_HTTP_VERSION_1_1, 0);
	if(template_id < 0)
		return CHIAKI_ERR_NETWORK;
	sceHttpSetConnectTimeOut(template_id, 5000000);
	sceHttpSetSendTimeOut(template_id, 5000000);
	sceHttpSetRecvTimeOut(template_id, 5000000);

	int connection_id = sceHttpCreateConnectionWithURL(template_id, url, 0);
	if(connection_id < 0)
	{
		sceHttpDeleteTemplate(template_id);
		return CHIAKI_ERR_NETWORK;
	}
	int request_id = sceHttpCreateRequestWithURL(
		connection_id, SCE_HTTP_METHOD_GET, url, 0);
	if(request_id < 0)
	{
		sceHttpDeleteConnection(connection_id);
		sceHttpDeleteTemplate(template_id);
		return CHIAKI_ERR_NETWORK;
	}

	result->send_result = sceHttpSendRequest(request_id, NULL, 0);
	int ssl_result = sceHttpsGetSslError(
		request_id, &result->ssl_error, &result->ssl_detail);
	bool rejected_unknown_ca = result->send_result < 0 && ssl_result >= 0
		&& (result->ssl_detail & SCE_HTTPS_ERROR_SSL_UNKNOWN_CA) != 0;

	sceHttpAbortRequest(request_id);
	sceHttpDeleteRequest(request_id);
	sceHttpDeleteConnection(connection_id);
	sceHttpDeleteTemplate(template_id);
	return rejected_unknown_ca ? CHIAKI_ERR_SUCCESS : CHIAKI_ERR_INVALID_RESPONSE;
}
#endif
