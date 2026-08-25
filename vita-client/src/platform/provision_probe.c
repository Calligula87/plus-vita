// SPDX-License-Identifier: AGPL-3.0-only

#include "provision_probe.h"

#include <chiaki/random.h>
#include <curl_http.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUTHORIZE_CHECK_URL "https://ca.account.sony.com/api/authz/v3/oauth/authorizeCheck"
#define PSNOW_CLIENT_ID "bc6b0777-abb5-40da-92ca-e133cf18e989"
#define PSNOW_SCOPE "kamaji:commerce_native kamaji:commerce_container kamaji:lists kamaji:s2s.subscriptionsPremium.get"
#define PSNOW_REDIRECT "https://psnow.playstation.com/app/2.2.0/133/5cdcc037d/grc-response.html"
#define PSNOW_USER_AGENT "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) playstation-now/0.0.0 Chrome/83.0.4103.104 Electron/9.0.4 Safari/537.36 gkApollo"

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

void plus_vita_provision_authorize(const PlusVitaCredential *credential,
	PlusVitaProvisionProbeResult *result)
{
	memset(result, 0, sizeof(*result));
	if(!credential || credential->length != 64)
		return;

	uint8_t random_bytes[16];
	char duid[49] = "0000000700410080";
	if(chiaki_random_bytes_crypt(random_bytes, sizeof(random_bytes))
		!= CHIAKI_ERR_SUCCESS)
		return;
	for(size_t i = 0; i < sizeof(random_bytes); i++)
		snprintf(duid + 16 + i * 2, 3, "%02x", random_bytes[i]);
	secure_clear(random_bytes, sizeof(random_bytes));

	char body[768];
	snprintf(body, sizeof(body),
		"{\"client_id\":\"%s\",\"scope\":\"%s\",\"redirect_uri\":\"%s\","
		"\"response_type\":\"code\",\"service_entity\":\"urn:service-entity:psn\",\"duid\":\"%s\"}",
		PSNOW_CLIENT_ID, PSNOW_SCOPE, PSNOW_REDIRECT, duid);
	char *cookie = NULL;
	if(cc_http_make_cookie_header(&cookie, "npsso", credential->value)
		!= CHIAKI_ERR_SUCCESS)
		goto cleanup;
	const char *headers[] = {
		"Content-Type: application/json; charset=UTF-8",
		"User-Agent: " PSNOW_USER_AGENT,
		cookie,
	};
	CCHttpRequest request = {
		.method = "POST", .url = AUTHORIZE_CHECK_URL,
		.headers = headers, .header_count = 3, .body = body,
		.timeout_ms = 30000,
	};
	CCHttpResponse response = { 0 };
	result->attempted = true;
	result->transport_ok = cc_http_perform(NULL, &request, &response)
		== CHIAKI_ERR_SUCCESS;
	result->http_status = response.status_code;
	result->authorized = result->transport_ok && response.status_code >= 200
		&& response.status_code < 300;
	cc_http_response_fini(&response);
	secure_clear(cookie, strlen(cookie));
	free(cookie);

cleanup:
	secure_clear(body, sizeof(body));
	secure_clear(duid, sizeof(duid));
}
