// SPDX-License-Identifier: AGPL-3.0-only

#include "curl_http.h"
#include "secure_http.h"
#include "network.h"
#include "request_cancel.h"

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>

#define PLUS_VITA_HTTP_MAX_BODY (4U * 1024U * 1024U)
#define PLUS_VITA_HTTP_MAX_HEADERS (64U * 1024U)
#define PLUS_VITA_HTTP_DEFAULT_TIMEOUT_MS 30000L

#ifdef __vita__
static bool test_network_blocked;
static unsigned int test_network_attempts;
static const char *test_last_stage = "none";
static int test_last_native_result;
static int test_last_error;
static int test_last_status;
static int test_last_ssl_error;
static unsigned int test_last_ssl_detail;

void plus_vita_http_test_block_network(bool blocked)
{
	test_network_blocked = blocked;
	test_network_attempts = 0;
	test_last_stage = "none";
	test_last_native_result = 0;
	test_last_error = 0;
	test_last_status = 0;
	test_last_ssl_error = 0;
	test_last_ssl_detail = 0;
}

const char *plus_vita_http_test_last_stage(void) { return test_last_stage; }
int plus_vita_http_test_last_native_result(void) { return test_last_native_result; }
int plus_vita_http_test_last_error(void) { return test_last_error; }
int plus_vita_http_test_last_status(void) { return test_last_status; }
int plus_vita_http_test_last_ssl_error(void) { return test_last_ssl_error; }
unsigned int plus_vita_http_test_last_ssl_detail(void) { return test_last_ssl_detail; }

unsigned int plus_vita_http_test_network_attempts(void)
{
	return test_network_attempts;
}

static __attribute__((unused)) ChiakiErrorCode append_bytes(char **data, size_t *size, const void *chunk,
	size_t chunk_size, size_t limit)
{
	if(*size > limit || chunk_size > limit - *size)
		return CHIAKI_ERR_OVERFLOW;
	char *next = realloc(*data, *size + chunk_size + 1);
	if(!next)
		return CHIAKI_ERR_MEMORY;
	memcpy(next + *size, chunk, chunk_size);
	*size += chunk_size;
	next[*size] = 0;
	*data = next;
	return CHIAKI_ERR_SUCCESS;
}
#endif

void cc_http_response_fini(CCHttpResponse *response)
{
	if(!response)
		return;
	free(response->data);
	free(response->headers);
	free(response->redirect_url);
	memset(response, 0, sizeof(*response));
}

ChiakiErrorCode cc_http_make_bearer_header(char **out, const char *token)
{
	if(!out || !token)
		return CHIAKI_ERR_INVALID_DATA;
	static const char prefix[] = "Authorization: Bearer ";
	size_t size = sizeof(prefix) + strlen(token);
	*out = malloc(size);
	if(!*out)
		return CHIAKI_ERR_MEMORY;
	strcpy(*out, prefix);
	strcat(*out, token);
	return CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode cc_http_make_cookie_header(char **out, const char *name,
	const char *value)
{
	if(!out || !name || !value)
		return CHIAKI_ERR_INVALID_DATA;
	size_t size = sizeof("Cookie: =") + strlen(name) + strlen(value);
	*out = malloc(size);
	if(!*out)
		return CHIAKI_ERR_MEMORY;
	strcpy(*out, "Cookie: ");
	strcat(*out, name);
	strcat(*out, "=");
	strcat(*out, value);
	return CHIAKI_ERR_SUCCESS;
}

#ifdef __vita__
#include <psp2/net/http.h>

static __attribute__((unused)) int request_method(const char *method)
{
	if(!method || strcmp(method, "GET") == 0)
		return SCE_HTTP_METHOD_GET;
	if(strcmp(method, "POST") == 0)
		return SCE_HTTP_METHOD_POST;
	return -1;
}

static __attribute__((unused)) unsigned int timeout_us(long timeout_ms)
{
	long value = timeout_ms > 0 ? timeout_ms : PLUS_VITA_HTTP_DEFAULT_TIMEOUT_MS;
	if(value > 2147483L)
		value = 2147483L;
	return (unsigned int)value * 1000U;
}

static __attribute__((unused)) ChiakiErrorCode add_request_header(int request_id, const char *header)
{
	const char *colon = header ? strchr(header, ':') : NULL;
	if(!colon || colon == header)
		return CHIAKI_ERR_INVALID_DATA;
	size_t name_size = (size_t)(colon - header);
	char *name = malloc(name_size + 1);
	if(!name)
		return CHIAKI_ERR_MEMORY;
	memcpy(name, header, name_size);
	name[name_size] = 0;
	const char *value = colon + 1;
	while(*value == ' ' || *value == '\t')
		value++;
	int result = sceHttpAddRequestHeader(
		request_id, name, value, SCE_HTTP_HEADER_ADD);
	free(name);
	return result < 0 ? CHIAKI_ERR_INVALID_DATA : CHIAKI_ERR_SUCCESS;
}

ChiakiErrorCode cc_http_perform(ChiakiLog *log, const CCHttpRequest *request,
	CCHttpResponse *response)
{
	extern ChiakiErrorCode plus_vita_curl_http_perform(ChiakiLog *,
		const CCHttpRequest *, CCHttpResponse *);
	if(!request || !request->url || !response)
		return CHIAKI_ERR_INVALID_DATA;
	memset(response, 0, sizeof(*response));
	test_network_attempts++;
	if(test_network_blocked || plus_vita_request_cancelled())
		return CHIAKI_ERR_CANCELED;

	bool network_owned = !plus_vita_network_is_initialized();
	if(plus_vita_network_init() != CHIAKI_ERR_SUCCESS)
	{
		test_last_stage = "curl-net-init";
		test_last_error = CHIAKI_ERR_NETWORK;
		return CHIAKI_ERR_NETWORK;
	}
	setenv("CHIAKI_CA_BUNDLE", "app0:cacert.pem", 1);
	CURLcode global_result = curl_global_init(CURL_GLOBAL_DEFAULT);
	if(global_result != CURLE_OK)
	{
		test_last_stage = "curl-init";
		test_last_native_result = global_result;
		test_last_error = CHIAKI_ERR_NETWORK;
		if(network_owned) plus_vita_network_fini();
		return CHIAKI_ERR_NETWORK;
	}

	ChiakiErrorCode error = plus_vita_curl_http_perform(log, request, response);
	test_last_stage = error == CHIAKI_ERR_SUCCESS ? "curl-complete" : "curl-perform";
	test_last_error = error;
	test_last_status = (int)response->status_code;
	curl_global_cleanup();
	if(network_owned) plus_vita_network_fini();
	return error;

#if 0
	(void)log;
	if(!request || !request->url || !response)
		return CHIAKI_ERR_INVALID_DATA;
	memset(response, 0, sizeof(*response));
	test_network_attempts++;
	if(test_network_blocked)
	{
		return CHIAKI_ERR_CANCELED;
	}
	int method = request_method(request->method);
	if(method < 0)
		return CHIAKI_ERR_INVALID_DATA;
	ChiakiErrorCode error = plus_vita_secure_http_init();
	if(error != CHIAKI_ERR_SUCCESS)
	{
		test_last_stage = "secure-init";
		test_last_error = error;
		return error;
	}

	int template_id = -1;
	int connection_id = -1;
	int request_id = -1;
	/* Sony's legacy pcnow edge currently fails its TLS handshake through the
	 * Vita HTTP/1.1 template even though the same system trust succeeds on
	 * other public hosts.  The native public probe already proves HTTP/1.0 is
	 * the most compatible template mode on this platform. */
	template_id = sceHttpCreateTemplate("plus-vita/0.1", SCE_HTTP_VERSION_1_0, 1);
	if(template_id < 0)
	{
		test_last_stage = "template";
		test_last_native_result = template_id;
		error = CHIAKI_ERR_NETWORK;
		goto cleanup;
	}
	unsigned int timeout = timeout_us(request->timeout_ms);
	sceHttpSetResolveTimeOut(template_id, timeout);
	sceHttpSetConnectTimeOut(template_id, timeout);
	sceHttpSetSendTimeOut(template_id, timeout);
	sceHttpSetRecvTimeOut(template_id, timeout);
	sceHttpSetAutoRedirect(template_id, request->follow_redirects ? 1 : 0);

	connection_id = sceHttpCreateConnectionWithURL(template_id, request->url, 0);
	if(connection_id < 0)
	{
		test_last_stage = "connection";
		test_last_native_result = connection_id;
		error = CHIAKI_ERR_NETWORK;
		goto cleanup;
	}
	size_t body_size = request->body ? request->body_len : 0;
	if(request->body && body_size == 0)
		body_size = strlen(request->body);
	if(body_size > UINT32_MAX)
	{
		error = CHIAKI_ERR_OVERFLOW;
		goto cleanup;
	}
	request_id = sceHttpCreateRequestWithURL(
		connection_id, method, request->url, body_size);
	if(request_id < 0)
	{
		test_last_stage = "request";
		test_last_native_result = request_id;
		error = CHIAKI_ERR_NETWORK;
		goto cleanup;
	}
	for(size_t i = 0; i < request->header_count; i++)
	{
		error = add_request_header(request_id, request->headers[i]);
		if(error != CHIAKI_ERR_SUCCESS)
		{
			test_last_stage = "header";
			goto cleanup;
		}
	}
	int send_result = sceHttpSendRequest(request_id, request->body,
		(unsigned int)body_size);
	if(send_result < 0)
	{
		test_last_stage = "send";
		test_last_native_result = send_result;
		sceHttpsGetSslError(request_id, &test_last_ssl_error,
			&test_last_ssl_detail);
		error = send_result == (int)SCE_HTTP_ERROR_TIMEOUT
			? CHIAKI_ERR_TIMEOUT : CHIAKI_ERR_NETWORK;
		goto cleanup;
	}

	char chunk[4096];
	for(;;)
	{
		int read_size = sceHttpReadData(request_id, chunk, sizeof(chunk));
		if(read_size < 0)
		{
			test_last_stage = "read";
			test_last_native_result = read_size;
			error = read_size == (int)SCE_HTTP_ERROR_TIMEOUT
				? CHIAKI_ERR_TIMEOUT : CHIAKI_ERR_NETWORK;
			goto cleanup;
		}
		if(read_size == 0)
			break;
		error = append_bytes(&response->data, &response->size, chunk,
			(size_t)read_size, PLUS_VITA_HTTP_MAX_BODY);
		if(error != CHIAKI_ERR_SUCCESS)
			goto cleanup;
	}
	int status = 0;
	int status_result = sceHttpGetStatusCode(request_id, &status);
	if(status_result < 0)
	{
		test_last_stage = "status";
		test_last_native_result = status_result;
		error = CHIAKI_ERR_INVALID_RESPONSE;
		goto cleanup;
	}
	response->status_code = status;
	test_last_status = status;
	test_last_stage = "complete";
	char *headers = NULL;
	unsigned int headers_size = 0;
	if(sceHttpGetAllResponseHeaders(request_id, &headers, &headers_size) >= 0
		&& headers && headers_size <= PLUS_VITA_HTTP_MAX_HEADERS)
	{
		const char *location = NULL;
		unsigned int location_size = 0;
		if(sceHttpParseResponseHeader(headers, headers_size, "Location",
			&location, &location_size) >= 0 && location && location_size)
		{
			response->redirect_url = malloc((size_t)location_size + 1);
			if(!response->redirect_url)
			{
				error = CHIAKI_ERR_MEMORY;
				goto cleanup;
			}
			memcpy(response->redirect_url, location, location_size);
			response->redirect_url[location_size] = 0;
		}
		if(request->capture_headers)
			error = append_bytes(&response->headers, &response->headers_size,
				headers, headers_size, PLUS_VITA_HTTP_MAX_HEADERS);
	}
	else if(request->capture_headers)
		error = CHIAKI_ERR_INVALID_RESPONSE;

cleanup:
	if(request_id >= 0)
	{
		sceHttpAbortRequest(request_id);
		sceHttpDeleteRequest(request_id);
	}
	if(connection_id >= 0)
		sceHttpDeleteConnection(connection_id);
	if(template_id >= 0)
		sceHttpDeleteTemplate(template_id);
	plus_vita_secure_http_fini();
	test_last_error = error;
	if(error != CHIAKI_ERR_SUCCESS)
		cc_http_response_fini(response);
	return error;
#endif
}
#else
ChiakiErrorCode cc_http_perform(ChiakiLog *log, const CCHttpRequest *request,
	CCHttpResponse *response)
{
	(void)log;
	if(!request || !request->url || !response)
		return CHIAKI_ERR_INVALID_DATA;
	memset(response, 0, sizeof(*response));
	return CHIAKI_ERR_UNINITIALIZED;
}
#endif
