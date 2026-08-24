// SPDX-License-Identifier: AGPL-3.0-only

#include "auth_probe.h"

#include <chiaki/random.h>
#include <curl_http.h>
#include <json_object.h>
#include <json_tokener.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_URL_PREFIX "https://ca.account.sony.com/api/v1/oauth/authorize?smcid=pc%3Apsnow&applicationId=psnow&response_type=code&scope=kamaji%3Acommerce_native%20kamaji%3Acommerce_container%20kamaji%3Alists%20kamaji%3As2s.subscriptionsPremium.get&client_id=bc6b0777-abb5-40da-92ca-e133cf18e989&redirect_uri=https%3A%2F%2Fpsnow.playstation.com%2Fapp%2F2.2.0%2F133%2F5cdcc037d%2Fgrc-response.html&service_entity=urn%3Aservice-entity%3Apsn&prompt=none&renderMode=mobilePortrait&hidePageElements=forgotPasswordLink&displayFooter=none&disableLinks=qriocityLink&mid=PSNOW&duid="
#define AUTH_URL_SUFFIX "&layout_type=popup&service_logo=ps&tp_psn=true&noEVBlock=true"
#define SESSION_URL "https://psnow.playstation.com/kamaji/api/pcnow/00_09_000/user/session"
#define PSNOW_CLIENT_ID "bc6b0777-abb5-40da-92ca-e133cf18e989"
#define KAMAJI_REDIRECT "https://psnow.playstation.com/app/2.2.0/133/5cdcc037d/grc-response.html"
#define KAMAJI_ORIGIN "https://psnow.playstation.com"
#define KAMAJI_REFERER "https://psnow.playstation.com/app/2.2.0/133/5cdcc037d/"
#define KAMAJI_UA "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) playstation-now/0.0.0 Chrome/83.0.4103.104 Electron/9.0.4 Safari/537.36 gkApollo"

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

static bool extract_code(const char *url, char *out, size_t out_size)
{
	out[0] = 0;
	if(!url)
		return false;
	const char *p = strstr(url, "code=");
	while(p && !(p == url || p[-1] == '?' || p[-1] == '&' || p[-1] == '#'))
		p = strstr(p + 1, "code=");
	if(!p)
		return false;
	p += 5;
	size_t length = 0;
	while(*p && *p != '&' && length + 1 < out_size)
		out[length++] = *p++;
	out[length] = 0;
	return length > 0;
}

static void wipe_response(CCHttpResponse *response)
{
	if(response->redirect_url)
		secure_clear(response->redirect_url, strlen(response->redirect_url));
	if(response->data)
		secure_clear(response->data, response->size);
	if(response->headers)
		secure_clear(response->headers, response->headers_size);
	cc_http_response_fini(response);
}

static bool parse_session_region(const CCHttpResponse *response,
	char *country, size_t country_size, char *language, size_t language_size)
{
	if(!response->data || !response->size)
		return false;
	struct json_object *root = json_tokener_parse(response->data);
	struct json_object *header = NULL, *status = NULL, *data = NULL;
	struct json_object *country_obj = NULL, *language_obj = NULL;
	bool ok = root
		&& json_object_object_get_ex(root, "header", &header)
		&& json_object_object_get_ex(header, "status_code", &status)
		&& strcmp(json_object_get_string(status), "0x0000") == 0
		&& json_object_object_get_ex(root, "data", &data);
	if(ok && json_object_object_get_ex(data, "country", &country_obj))
		snprintf(country, country_size, "%s", json_object_get_string(country_obj));
	if(ok && json_object_object_get_ex(data, "language", &language_obj))
		snprintf(language, language_size, "%s", json_object_get_string(language_obj));
	if(root)
		json_object_put(root);
	return ok && country[0] && language[0];
}

void plus_vita_auth_probe(const PlusVitaCredential *credential,
	PlusVitaAuthProbeResult *result)
{
	memset(result, 0, sizeof(*result));
	if(!credential || !credential->length)
		return;
	result->format_valid = credential->length == 64;
	for(size_t i = 0; result->format_valid && i < credential->length; i++)
		result->format_valid = isalnum((unsigned char)credential->value[i]) != 0;
	if(!result->format_valid)
		return;

	uint8_t random_bytes[16];
	char duid[49] = "0000000700410080";
	if(chiaki_random_bytes_crypt(random_bytes, sizeof(random_bytes)) != CHIAKI_ERR_SUCCESS)
		return;
	for(size_t i = 0; i < sizeof(random_bytes); i++)
		snprintf(duid + 16 + i * 2, 3, "%02x", random_bytes[i]);
	secure_clear(random_bytes, sizeof(random_bytes));

	char url[2048];
	snprintf(url, sizeof(url), "%s%s%s", AUTH_URL_PREFIX, duid, AUTH_URL_SUFFIX);
	char *cookie = NULL;
	if(cc_http_make_cookie_header(&cookie, "npsso", credential->value)
		!= CHIAKI_ERR_SUCCESS)
	{
		secure_clear(duid, sizeof(duid));
		secure_clear(url, sizeof(url));
		return;
	}

	const char *headers[] = {
		"User-Agent: " KAMAJI_UA,
		cookie,
	};
	CCHttpRequest request = {
		.url = url,
		.headers = headers,
		.header_count = 2,
		.capture_headers = true,
		.timeout_ms = 30000,
	};
	CCHttpResponse response = { 0 };
	result->attempted = true;
	result->transport_ok = cc_http_perform(NULL, &request, &response)
		== CHIAKI_ERR_SUCCESS;
	result->http_status = response.status_code;
	char code[1024] = { 0 };
	result->authorized = result->transport_ok && response.status_code == 302
		&& extract_code(response.redirect_url, code, sizeof(code));

	secure_clear(cookie, strlen(cookie));
	free(cookie);
	wipe_response(&response);
	if(!result->authorized)
		goto cleanup;

	char body[1536];
	snprintf(body, sizeof(body), "code=%s&client_id=%s&duid=%s",
		code, PSNOW_CLIENT_ID, duid);
	const char *session_headers[] = {
		"Content-Type: text/plain;charset=UTF-8",
		"User-Agent: " KAMAJI_UA,
		"X-Alt-Referer: " KAMAJI_REDIRECT,
		"Origin: " KAMAJI_ORIGIN,
		"Referer: " KAMAJI_REFERER,
		"Accept: */*",
	};
	CCHttpRequest session_request = {
		.method = "POST",
		.url = SESSION_URL,
		.headers = session_headers,
		.header_count = 6,
		.body = body,
		.capture_headers = true,
		.timeout_ms = 30000,
	};
	CCHttpResponse session_response = { 0 };
	result->session_transport_ok = cc_http_perform(NULL, &session_request,
		&session_response) == CHIAKI_ERR_SUCCESS;
	result->session_http_status = session_response.status_code;
	result->session_created = result->session_transport_ok
		&& session_response.status_code == 200
		&& parse_session_region(&session_response, result->country,
			sizeof(result->country), result->language, sizeof(result->language));
	wipe_response(&session_response);
	secure_clear(body, sizeof(body));

cleanup:
	secure_clear(code, sizeof(code));
	secure_clear(duid, sizeof(duid));
	secure_clear(url, sizeof(url));
}
