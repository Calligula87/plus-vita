// SPDX-License-Identifier: AGPL-3.0-only

#include "secure_http.h"
#include "network.h"

static bool initialized;
static bool network_owned;

#ifdef __vita__
#include <psp2/libssl.h>
#include <psp2/net/http.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	result->http_status = 0;
	result->body_size = 0;
	result->content_length_result = 0;
	result->content_length = 0;
	result->status_result = 0;
	result->read_result = 0;
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

ChiakiErrorCode plus_vita_secure_http_probe_trusted(
	const char *url, const char *ca_path, PlusVitaTlsProbeResult *result)
{
	static const char expected_body[] = "plus-vita trusted probe\n";
	if(!initialized || !url || !ca_path || !result)
		return CHIAKI_ERR_UNINITIALIZED;

	FILE *file = fopen(ca_path, "rb");
	if(!file)
		return CHIAKI_ERR_INVALID_DATA;
	fseek(file, 0, SEEK_END);
	long ca_size = ftell(file);
	rewind(file);
	char *ca_bytes = ca_size > 0 ? malloc((size_t)ca_size) : NULL;
	if(!ca_bytes || fread(ca_bytes, 1, (size_t)ca_size, file) != (size_t)ca_size)
	{
		fclose(file);
		free(ca_bytes);
		return CHIAKI_ERR_INVALID_DATA;
	}
	fclose(file);

	SceHttpsData ca = { .ptr = ca_bytes, .size = (unsigned int)ca_size };
	const SceHttpsData *ca_list[] = { &ca };
	if(sceHttpsLoadCert(1, ca_list, NULL, NULL) < 0)
	{
		free(ca_bytes);
		return CHIAKI_ERR_INVALID_DATA;
	}

	result->send_result = 0;
	result->ssl_error = 0;
	result->ssl_detail = 0;
	result->http_status = 0;
	result->body_size = 0;
	result->content_length_result = 0;
	result->content_length = 0;
	result->status_result = 0;
	result->read_result = 0;
	int template_id = sceHttpCreateTemplate(
		"plus-vita-trusted-probe/1", SCE_HTTP_VERSION_1_1, 1);
	int connection_id = template_id >= 0
		? sceHttpCreateConnectionWithURL(template_id, url, 0) : -1;
	int request_id = connection_id >= 0 ? sceHttpCreateRequestWithURL(
		connection_id, SCE_HTTP_METHOD_GET, url, 0) : -1;
	char body[sizeof(expected_body)] = { 0 };
	if(request_id >= 0)
	{
		sceHttpSetConnectTimeOut(request_id, 5000000);
		sceHttpSetSendTimeOut(request_id, 5000000);
		sceHttpSetRecvTimeOut(request_id, 5000000);
		result->send_result = sceHttpSendRequest(request_id, NULL, 0);
		if(result->send_result >= 0)
		{
			result->content_length_result = sceHttpGetResponseContentLength(
				request_id, &result->content_length);
			result->status_result = sceHttpGetStatusCode(
				request_id, &result->http_status);
			while(result->body_size < (int)sizeof(expected_body) - 1)
			{
				int chunk = sceHttpReadData(request_id,
					body + result->body_size,
					(unsigned int)(sizeof(expected_body) - 1 - result->body_size));
				result->read_result = chunk;
				if(chunk <= 0)
					break;
				result->body_size += chunk;
			}
		}
		else
			sceHttpsGetSslError(request_id, &result->ssl_error, &result->ssl_detail);
	}
	bool success = result->send_result >= 0 && result->http_status == 200
		&& result->status_result >= 0
		&& result->body_size == (int)sizeof(expected_body) - 1
		&& memcmp(body, expected_body, sizeof(expected_body) - 1) == 0;
	if(request_id >= 0)
	{
		sceHttpAbortRequest(request_id);
		sceHttpDeleteRequest(request_id);
	}
	if(connection_id >= 0)
		sceHttpDeleteConnection(connection_id);
	if(template_id >= 0)
		sceHttpDeleteTemplate(template_id);
	sceHttpsUnloadCert();
	free(ca_bytes);
	return success ? CHIAKI_ERR_SUCCESS : CHIAKI_ERR_INVALID_RESPONSE;
}

ChiakiErrorCode plus_vita_secure_http_probe_public(
	const char *url, PlusVitaTlsProbeResult *result)
{
	if(!initialized || !url || !result)
		return CHIAKI_ERR_UNINITIALIZED;
	memset(result, 0, sizeof(*result));

	int template_id = sceHttpCreateTemplate(
		"plus-vita-public-trust-probe/1", SCE_HTTP_VERSION_1_0, 1);
	if(template_id < 0)
		return CHIAKI_ERR_NETWORK;
	sceHttpSetResolveTimeOut(template_id, 10000000);
	sceHttpSetConnectTimeOut(template_id, 10000000);
	sceHttpSetSendTimeOut(template_id, 10000000);
	sceHttpSetRecvTimeOut(template_id, 10000000);

	int connection_id = sceHttpCreateConnectionWithURL(template_id, url, 0);
	int request_id = connection_id >= 0 ? sceHttpCreateRequestWithURL(
		connection_id, SCE_HTTP_METHOD_GET, url, 0) : -1;
	char body[256] = { 0 };
	if(request_id >= 0)
	{
		result->send_result = sceHttpSendRequest(request_id, NULL, 0);
		if(result->send_result >= 0)
		{
			result->read_result = sceHttpReadData(request_id, body, sizeof(body));
			if(result->read_result > 0)
				result->body_size = result->read_result;
			result->status_result = sceHttpGetStatusCode(
				request_id, &result->http_status);
		}
		else
			sceHttpsGetSslError(request_id, &result->ssl_error, &result->ssl_detail);
	}
	bool success = result->send_result >= 0 && result->read_result > 0
		&& result->status_result >= 0 && result->http_status == 200;
	if(request_id >= 0)
	{
		sceHttpAbortRequest(request_id);
		sceHttpDeleteRequest(request_id);
	}
	if(connection_id >= 0)
		sceHttpDeleteConnection(connection_id);
	sceHttpDeleteTemplate(template_id);
	return success ? CHIAKI_ERR_SUCCESS : CHIAKI_ERR_INVALID_RESPONSE;
}
#endif
