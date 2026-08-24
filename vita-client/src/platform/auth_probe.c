// SPDX-License-Identifier: AGPL-3.0-only

#include "auth_probe.h"

#include <chiaki/random.h>
#include <curl_http.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define AUTH_URL_PREFIX "https://ca.account.sony.com/api/v1/oauth/authorize?smcid=pc%3Apsnow&applicationId=psnow&response_type=code&scope=kamaji%3Acommerce_native%20kamaji%3Acommerce_container%20kamaji%3Alists%20kamaji%3As2s.subscriptionsPremium.get&client_id=bc6b0777-abb5-40da-92ca-e133cf18e989&redirect_uri=https%3A%2F%2Fpsnow.playstation.com%2Fapp%2F2.2.0%2F133%2F5cdcc037d%2Fgrc-response.html&service_entity=urn%3Aservice-entity%3Apsn&prompt=none&renderMode=mobilePortrait&hidePageElements=forgotPasswordLink&displayFooter=none&disableLinks=qriocityLink&mid=PSNOW&duid="
#define AUTH_URL_SUFFIX "&layout_type=popup&service_logo=ps&tp_psn=true&noEVBlock=true"

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

static bool has_code_parameter(const char *url)
{
	if(!url)
		return false;
	const char *p = strstr(url, "code=");
	return p && (p == url || p[-1] == '?' || p[-1] == '&' || p[-1] == '#')
		&& p[5] != 0;
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
	secure_clear(duid, sizeof(duid));
	char *cookie = NULL;
	if(cc_http_make_cookie_header(&cookie, "npsso", credential->value)
		!= CHIAKI_ERR_SUCCESS)
		return;

	const char *headers[] = {
		"User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) playstation-now/0.0.0 Chrome/83.0.4103.104 Electron/9.0.4 Safari/537.36 gkApollo",
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
	result->authorized = result->transport_ok && response.status_code == 302
		&& has_code_parameter(response.redirect_url);

	secure_clear(cookie, strlen(cookie));
	free(cookie);
	if(response.redirect_url)
		secure_clear(response.redirect_url, strlen(response.redirect_url));
	if(response.data)
		secure_clear(response.data, response.size);
	if(response.headers)
		secure_clear(response.headers, response.headers_size);
	cc_http_response_fini(&response);
}
