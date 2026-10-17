// SPDX-License-Identifier: AGPL-3.0-only

#include "auth_probe.h"
#include "../app/utf8.h"

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
#define STORES_URL "https://psnow.playstation.com/kamaji/api/pcnow/00_09_000/user/stores"
#define PSNOW_CLIENT_ID "bc6b0777-abb5-40da-92ca-e133cf18e989"
#define KAMAJI_REDIRECT "https://psnow.playstation.com/app/2.2.0/133/5cdcc037d/grc-response.html"
#define KAMAJI_ORIGIN "https://psnow.playstation.com"
#define KAMAJI_REFERER "https://psnow.playstation.com/app/2.2.0/133/5cdcc037d/"
#define KAMAJI_UA "Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) playstation-now/0.0.0 Chrome/83.0.4103.104 Electron/9.0.4 Safari/537.36 gkApollo"
#define GENERIC_UA "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
#define OWNED_AUTH_URL "https://ca.account.sony.com/api/v1/oauth/authorize?response_type=token&scope=kamaji%3Aget_internal_entitlements%20user%3Aaccount.attributes.validate&client_id=dc523cc2-b51b-4190-bff0-3397c06871b3&redirect_uri=https%3A%2F%2Fpsnow.playstation.com%2Fapp%2F2.2.0%2F133%2F5cdcc037d%2Fgrc-response.html&service_entity=urn%3Aservice-entity%3Apsn&prompt=none"
#define OWNED_PAGE_SIZE 300

static void secure_clear(void *value, size_t size)
{
	volatile unsigned char *p = value;
	while(size--)
		*p++ = 0;
}

static bool extract_parameter(const char *url, const char *name,
	char *out, size_t out_size)
{
	out[0] = 0;
	if(!url)
		return false;
	char pattern[64];
	snprintf(pattern, sizeof(pattern), "%s=", name);
	const char *p = strstr(url, pattern);
	while(p && !(p == url || p[-1] == '?' || p[-1] == '&' || p[-1] == '#'))
		p = strstr(p + 1, pattern);
	if(!p)
		return false;
	p += strlen(pattern);
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
	char *country, size_t country_size, char *language, size_t language_size, char online_id[129])
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
	struct json_object *name = NULL;
	if(ok && json_object_object_get_ex(data, "onlineId", &name) && json_object_is_type(name, json_type_string)) {
		const char *text = json_object_get_string(name);
		size_t length = (size_t)json_object_get_string_len(name);
		if(plus_vita_utf8_label(text, length)) { memcpy(online_id, text, length); online_id[length] = 0; }
	}
	if(root)
		json_object_put(root);
	return ok && country[0] && language[0];
}

static bool extract_jsessionid(const char *headers, char *out, size_t out_size)
{
	out[0] = 0;
	if(!headers)
		return false;
	const char *p = strstr(headers, "JSESSIONID=");
	if(!p)
		return false;
	p += strlen("JSESSIONID=");
	size_t length = 0;
	while(*p && *p != ';' && *p != '\r' && *p != '\n'
		&& length + 1 < out_size)
		out[length++] = *p++;
	out[length] = 0;
	return length > 0;
}

static bool parse_store_route(const CCHttpResponse *response,
	char *country, size_t country_size, char *language, size_t language_size,
	char *base_url, size_t base_url_size)
{
	if(!response->data || !response->size)
		return false;
	struct json_object *root = json_tokener_parse(response->data);
	struct json_object *header = NULL, *status = NULL, *data = NULL, *base_obj = NULL;
	bool ok = root
		&& json_object_object_get_ex(root, "header", &header)
		&& json_object_object_get_ex(header, "status_code", &status)
		&& strcmp(json_object_get_string(status), "0x0000") == 0
		&& json_object_object_get_ex(root, "data", &data)
		&& json_object_object_get_ex(data, "base_url", &base_obj);
	const char *base = ok ? json_object_get_string(base_obj) : NULL;
	if(base)
		snprintf(base_url, base_url_size, "%s", base);
	const char *p = base ? strstr(base, "/container/") : NULL;
	if(p)
	{
		p += strlen("/container/");
		const char *slash = strchr(p, '/');
		if(slash && slash > p && (size_t)(slash - p) < country_size)
		{
			memcpy(country, p, (size_t)(slash - p));
			country[slash - p] = 0;
			p = slash + 1;
			slash = strchr(p, '/');
			if(slash && slash > p && (size_t)(slash - p) < language_size)
			{
				memcpy(language, p, (size_t)(slash - p));
				language[slash - p] = 0;
			}
		}
	}
	if(root)
		json_object_put(root);
	return ok && country[0] && language[0];
}

static bool is_alpha_category(const char *name)
{
	static const char *const names[] = {
		"A - B", "C - D", "E - G", "H - L", "M - O", "P - R", "S", "T", "U - Z", NULL
	};
	for(size_t i = 0; names[i]; i++)
		if(strcmp(name, names[i]) == 0)
			return true;
	return false;
}

static const char *catalog_string(struct json_object *item, const char *const *keys)
{
	for(size_t i = 0; keys[i]; i++)
	{
		struct json_object *value = NULL;
		if(json_object_object_get_ex(item, keys[i], &value))
		{
			const char *text = json_object_get_string(value);
			if(text && text[0]) return text;
		}
	}
	return NULL;
}

static const char *infer_catalog_platform(const char *id)
{
	if(!id) return "Unknown";
	if(strstr(id, "CUSA")) return "PS4";
	if(strstr(id, "PPSA")) return "PS5";
	if(strstr(id, "NPUB") || strstr(id, "NPEB") || strstr(id, "NPJB")
		|| strstr(id, "NPHA") || strstr(id, "NPJA") || strstr(id, "NPUA")
		|| strstr(id, "BLES") || strstr(id, "BLUS") || strstr(id, "BCES")
		|| strstr(id, "BCUS") || strstr(id, "BLJM") || strstr(id, "BLAS"))
		return "PS3";
	return "Unknown";
}

static bool append_catalog_entry(PlusVitaAuthProbeResult *result,
	struct json_object *item)
{
	static const char *const name_keys[] = { "name", "title", "displayName", NULL };
	static const char *const id_keys[] = { "productId", "product_id", "id", "titleId", "url", NULL };
	static const char *const platform_keys[] = { "platform", "platformType", "console", NULL };
	if(!item || json_object_get_type(item) != json_type_object) return false;
	PlusVitaCatalogEntry *grown = realloc(result->native_titles,
		(result->native_titles_count + 1) * sizeof(*grown));
	if(!grown) return false;
	result->native_titles = grown;
	PlusVitaCatalogEntry *entry = &grown[result->native_titles_count++];
	memset(entry, 0, sizeof(*entry));
	const char *name = catalog_string(item, name_keys);
	const char *id = catalog_string(item, id_keys);
	const char *platform = catalog_string(item, platform_keys);
	if(id)
	{
		const char *slash = strrchr(id, '/');
		if(slash && slash[1]) id = slash + 1;
		const char *query = strchr(id, '?');
		size_t id_len = query ? (size_t)(query - id) : strlen(id);
		if(id_len >= sizeof(entry->id)) id_len = sizeof(entry->id) - 1;
		memcpy(entry->id, id, id_len);
		entry->id[id_len] = 0;
	}
	snprintf(entry->name, sizeof(entry->name), "%s", name ? name : "Untitled entry");
	if(!entry->id[0]) snprintf(entry->id, sizeof(entry->id), "No identifier");
	snprintf(entry->platform, sizeof(entry->platform), "%s",
		platform ? platform : infer_catalog_platform(entry->id));
	entry->launch_id_valid = strlen(entry->id) >= 8 && strlen(entry->id) < sizeof(entry->id);
	for(size_t i = 0; entry->launch_id_valid && entry->id[i]; i++)
		if(!isalnum((unsigned char)entry->id[i]) && entry->id[i] != '-'
			&& entry->id[i] != '_' && entry->id[i] != '.')
			entry->launch_id_valid = false;
	return true;
}

static bool fetch_native_catalog(const char *base_url, const char *jsession,
	PlusVitaAuthProbeResult *result)
{
	char url[1200];
	snprintf(url, sizeof(url), "%s?size=100", base_url);
	char *cookie = NULL;
	if(cc_http_make_cookie_header(&cookie, "JSESSIONID", jsession)
		!= CHIAKI_ERR_SUCCESS)
		return false;
	const char *root_headers[] = {
		"User-Agent: " KAMAJI_UA, cookie, "Origin: " KAMAJI_ORIGIN,
		"Referer: " KAMAJI_REFERER, "Accept: application/json",
	};
	CCHttpRequest root_request = {
		.url = url, .headers = root_headers, .header_count = 5, .timeout_ms = 30000,
	};
	CCHttpResponse root_response = { 0 };
	bool ok = cc_http_perform(NULL, &root_request, &root_response) == CHIAKI_ERR_SUCCESS
		&& root_response.status_code == 200;
	secure_clear(cookie, strlen(cookie));
	free(cookie);
	struct json_object *root = ok && root_response.data
		? json_tokener_parse(root_response.data) : NULL;
	char category_urls[16][1024] = { { 0 } };
	unsigned int count = 0;
	struct json_object *links = NULL;
	if(root && json_object_object_get_ex(root, "links", &links)
		&& json_object_get_type(links) == json_type_array)
	{
		size_t length = json_object_array_length(links);
		for(size_t i = 0; i < length && count < 16; i++)
		{
			struct json_object *link = json_object_array_get_idx(links, i);
			struct json_object *name_obj = NULL, *url_obj = NULL;
			if(link && json_object_object_get_ex(link, "name", &name_obj)
				&& json_object_object_get_ex(link, "url", &url_obj)
				&& is_alpha_category(json_object_get_string(name_obj)))
				snprintf(category_urls[count++], sizeof(category_urls[0]), "%s",
					json_object_get_string(url_obj));
		}
	}
	if(root) json_object_put(root);
	wipe_response(&root_response);
	secure_clear(url, sizeof(url));
	if(!ok || count == 0)
	{
		secure_clear(category_urls, sizeof(category_urls));
		return false;
	}

	result->native_category_count = count;
	result->native_title_count = 0;
	for(unsigned int i = 0; i < count; i++)
	{
		snprintf(url, sizeof(url), strchr(category_urls[i], '?')
			? "%s&start=0&size=500" : "%s?start=0&size=500", category_urls[i]);
		const char *headers[] = {
			"Content-Type: application/json", "Accept: application/json",
			"User-Agent: " GENERIC_UA,
		};
		CCHttpRequest request = {
			.url = url, .headers = headers, .header_count = 3, .timeout_ms = 30000,
		};
		CCHttpResponse response = { 0 };
		ok = cc_http_perform(NULL, &request, &response) == CHIAKI_ERR_SUCCESS
			&& response.status_code == 200;
		struct json_object *page = ok && response.data
			? json_tokener_parse(response.data) : NULL;
		links = NULL;
		if(page && json_object_object_get_ex(page, "links", &links)
			&& json_object_get_type(links) == json_type_array)
		{
			size_t length = json_object_array_length(links);
			result->native_title_count += (unsigned int)length;
			for(size_t j = 0; j < length; j++)
				if(!append_catalog_entry(result, json_object_array_get_idx(links, j)))
				{
					ok = false;
					break;
				}
		}
		else
			ok = false;
		if(page) json_object_put(page);
		wipe_response(&response);
		secure_clear(url, sizeof(url));
		if(!ok)
			break;
	}
	secure_clear(category_urls, sizeof(category_urls));
	return ok && result->native_title_count > 0
		&& result->native_titles_count == result->native_title_count;
}

static bool entitlement_is_game(struct json_object *entitlement)
{
	struct json_object *game_meta = NULL, *active = NULL, *product = NULL, *feature = NULL;
	if(!entitlement || json_object_get_type(entitlement) != json_type_object
		|| !json_object_object_get_ex(entitlement, "game_meta", &game_meta)
		|| json_object_get_type(game_meta) != json_type_object
		|| !json_object_object_get_ex(entitlement, "active_flag", &active)
		|| !json_object_get_boolean(active)
		|| !json_object_object_get_ex(entitlement, "product_id", &product)
		|| !json_object_object_get_ex(entitlement, "feature_type", &feature)
		|| json_object_get_int(feature) == 0)
		return false;
	const char *product_id = json_object_get_string(product);
	return product_id && strncmp(product_id, "IP", 2) != 0
		&& strncmp(product_id, "SUB", 3) != 0;
}

static void fetch_owned_entitlements(const PlusVitaCredential *credential,
	PlusVitaAuthProbeResult *result)
{
	char *cookie = NULL;
	if(cc_http_make_cookie_header(&cookie, "npsso", credential->value)
		!= CHIAKI_ERR_SUCCESS)
		return;
	const char *headers[] = { cookie, "User-Agent: " GENERIC_UA };
	CCHttpRequest auth_request = {
		.url = OWNED_AUTH_URL, .headers = headers, .header_count = 2,
		.capture_headers = true, .timeout_ms = 30000,
	};
	CCHttpResponse auth_response = { 0 };
	char token[2048] = { 0 };
	bool transport_ok = cc_http_perform(NULL, &auth_request, &auth_response)
		== CHIAKI_ERR_SUCCESS;
	result->owned_authorized = transport_ok && auth_response.status_code == 302
		&& extract_parameter(auth_response.redirect_url, "access_token", token,
			sizeof(token));
	secure_clear(cookie, strlen(cookie));
	free(cookie);
	wipe_response(&auth_response);
	if(!result->owned_authorized)
		goto cleanup;

	char *bearer = NULL;
	if(cc_http_make_bearer_header(&bearer, token) != CHIAKI_ERR_SUCCESS)
		goto cleanup;
	result->owned_complete = true;
	for(unsigned int page = 0; page < 200; page++)
	{
		char url[512];
		snprintf(url, sizeof(url),
			"https://commerce.api.np.km.playstation.net/commerce/api/v1/users/me/internal_entitlements?fields=game_meta&entitlement_type=5&start=%u&size=%u",
			result->owned_entitlement_count, OWNED_PAGE_SIZE);
		const char *page_headers[] = { bearer, "Accept: application/json" };
		CCHttpRequest request = {
			.url = url, .headers = page_headers, .header_count = 2, .timeout_ms = 30000,
		};
		CCHttpResponse response = { 0 };
		bool ok = cc_http_perform(NULL, &request, &response) == CHIAKI_ERR_SUCCESS
			&& response.status_code == 200;
		struct json_object *root = ok && response.data
			? json_tokener_parse(response.data) : NULL;
		struct json_object *entitlements = NULL;
		if(!root || !json_object_object_get_ex(root, "entitlements", &entitlements)
			|| json_object_get_type(entitlements) != json_type_array)
			ok = false;
		unsigned int page_count = ok
			? (unsigned int)json_object_array_length(entitlements) : 0;
		for(unsigned int i = 0; i < page_count; i++)
			if(entitlement_is_game(json_object_array_get_idx(entitlements, i)))
				result->owned_game_count++;
		result->owned_entitlement_count += page_count;
		result->owned_page_count++;
		if(root) json_object_put(root);
		wipe_response(&response);
		secure_clear(url, sizeof(url));
		if(!ok)
		{
			result->owned_complete = false;
			break;
		}
		if(page_count < OWNED_PAGE_SIZE)
			break;
	}
	secure_clear(bearer, strlen(bearer));
	free(bearer);

cleanup:
	secure_clear(token, sizeof(token));
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
		&& extract_parameter(response.redirect_url, "code", code, sizeof(code));
	char error[64] = {0};
	bool login_error = extract_parameter(response.redirect_url, "error", error, sizeof(error))
		&& (strcmp(error, "login_required") == 0 || strcmp(error, "invalid_grant") == 0);
	result->credential_rejected = result->transport_ok && !result->authorized
		&& (response.status_code == 401 || login_error);
	secure_clear(error, sizeof(error));

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
	char jsession[512] = { 0 };
	result->session_transport_ok = cc_http_perform(NULL, &session_request,
		&session_response) == CHIAKI_ERR_SUCCESS;
	result->session_http_status = session_response.status_code;
	result->session_created = result->session_transport_ok
		&& session_response.status_code == 200
		&& parse_session_region(&session_response, result->country,
			sizeof(result->country), result->language, sizeof(result->language), result->online_id)
		&& extract_jsessionid(session_response.headers, jsession, sizeof(jsession));
	wipe_response(&session_response);
	secure_clear(body, sizeof(body));
	if(!result->session_created)
		goto cleanup_session;

	char *session_cookie = NULL;
	if(cc_http_make_cookie_header(&session_cookie, "JSESSIONID", jsession)
		!= CHIAKI_ERR_SUCCESS)
		goto cleanup_session;
	const char *store_headers[] = {
		"User-Agent: " KAMAJI_UA,
		session_cookie,
		"Origin: " KAMAJI_ORIGIN,
		"Referer: " KAMAJI_REFERER,
		"Accept: application/json",
	};
	CCHttpRequest store_request = {
		.url = STORES_URL,
		.headers = store_headers,
		.header_count = 5,
		.timeout_ms = 30000,
	};
	CCHttpResponse store_response = { 0 };
	char store_base_url[1024] = { 0 };
	result->store_transport_ok = cc_http_perform(NULL, &store_request,
		&store_response) == CHIAKI_ERR_SUCCESS;
	result->store_http_status = store_response.status_code;
	result->store_resolved = result->store_transport_ok
		&& store_response.status_code == 200
		&& parse_store_route(&store_response, result->store_country,
			sizeof(result->store_country), result->store_language,
			sizeof(result->store_language), store_base_url, sizeof(store_base_url));
	secure_clear(session_cookie, strlen(session_cookie));
	free(session_cookie);
	wipe_response(&store_response);
	if(result->store_resolved)
		result->native_catalog_complete = fetch_native_catalog(store_base_url,
			jsession, result);
	secure_clear(store_base_url, sizeof(store_base_url));
	if(result->native_catalog_complete)
		fetch_owned_entitlements(credential, result);

cleanup_session:
	secure_clear(jsession, sizeof(jsession));

cleanup:
	secure_clear(code, sizeof(code));
	secure_clear(duid, sizeof(duid));
	secure_clear(url, sizeof(url));
}

void plus_vita_auth_probe_result_fini(PlusVitaAuthProbeResult *result)
{
	if(!result) return;
	free(result->native_titles);
	result->native_titles = NULL;
	result->native_titles_count = 0;
	secure_clear(result->online_id, sizeof(result->online_id));
}
