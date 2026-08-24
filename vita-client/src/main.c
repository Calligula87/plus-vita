// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/base64.h>
#include <chiaki/common.h>
#include <chiaki/cloudcatalog.h>
#include <chiaki/log.h>
#include <chiaki/random.h>
#include <chiaki/seqnum.h>
#include <chiaki/sock.h>
#include <chiaki/thread.h>
#include <chiaki/time.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <takion.pb.h>
#include <curl_http.h>
#include <curl/curl.h>
#include "cloudcatalog_internal.h"
#include "platform/network.h"
#include "platform/secure_http.h"
#ifdef __vita__
#include "platform/catalog_fetch_guard.h"
#include "platform/auth_probe.h"
#include "platform/credential.h"
#include "platform/credential_file.h"
#include "platform/credential_ime.h"
#include "platform/vita_cc_http_test.h"
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#ifdef __vita__
#include <debugScreen.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#define printf psvDebugScreenPrintf
#endif

static int failures;
static int checks_run;

typedef struct log_capture_t
{
	unsigned int calls;
	ChiakiLogLevel level;
	char message[64];
} LogCapture;

typedef struct thread_probe_t
{
	ChiakiMutex *mutex;
	int *value;
} ThreadProbe;

static int thread_result_marker;

static void *thread_probe_run(void *user)
{
	ThreadProbe *probe = user;
	if(chiaki_mutex_lock(probe->mutex) != CHIAKI_ERR_SUCCESS)
		return NULL;
	*probe->value = 42;
	if(chiaki_mutex_unlock(probe->mutex) != CHIAKI_ERR_SUCCESS)
		return NULL;
	return &thread_result_marker;
}

static void capture_log(ChiakiLogLevel level, const char *message, void *user)
{
	LogCapture *capture = user;
	capture->calls++;
	capture->level = level;
	snprintf(capture->message, sizeof(capture->message), "%s", message);
}

static void check(int condition, const char *name)
{
	checks_run++;
	if(condition)
		printf("PASS %s\n", name);
	else
	{
		printf("FAIL %s\n", name);
		failures++;
	}
}

static void test_base64(void)
{
	static const uint8_t plain[] = { 'V', 'i', 't', 'a' };
	char encoded[16] = { 0 };
	uint8_t decoded[8] = { 0 };
	size_t decoded_size = sizeof(decoded);

	check(chiaki_base64_encode(plain, sizeof(plain), encoded, sizeof(encoded))
		== CHIAKI_ERR_SUCCESS, "base64 encode result");
	check(strcmp(encoded, "Vml0YQ==") == 0, "base64 encoded value");
	check(chiaki_base64_decode(encoded, strlen(encoded), decoded, &decoded_size)
		== CHIAKI_ERR_SUCCESS, "base64 decode result");
	check(decoded_size == sizeof(plain), "base64 decoded size");
	check(memcmp(decoded, plain, sizeof(plain)) == 0, "base64 round trip");
}

static void test_runtime_credential_lifecycle(void)
{
	static const char fake[] = "not-a-real-test-credential";
	PlusVitaCredential credential;
	plus_vita_credential_init(&credential);
	check(plus_vita_credential_is_clear(&credential),
		"credential initialized clear");
	check(plus_vita_credential_set(&credential, fake, sizeof(fake) - 1),
		"credential bounded runtime set");
	check(credential.length == sizeof(fake) - 1
		&& strcmp(credential.value, fake) == 0,
		"credential runtime value");
	char redacted[48] = { 0 };
	plus_vita_credential_redacted(&credential, redacted, sizeof(redacted));
	check(strstr(redacted, fake) == NULL && strstr(redacted, "credential:") != NULL,
		"credential redacted reporting");
	check(!plus_vita_credential_set(&credential, "has space", 9),
		"credential rejects whitespace");
	plus_vita_credential_clear(&credential);
	check(plus_vita_credential_is_clear(&credential),
		"credential compiler-resistant wipe");
}

static void test_protocol_primitives(void)
{
	check(chiaki_seq_num_16_lt(UINT16_MAX, 0), "16-bit sequence wrap");
	check(chiaki_seq_num_32_gt(0, UINT32_MAX), "32-bit sequence wrap");
	check(chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_PSNOW), "PS Now is cloud");
	check(chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_PSCLOUD), "PS Cloud is cloud");
	check(!chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_REMOTE_PLAY),
		"Remote Play is not cloud");
}

static void test_cloudcatalog_locale_module(void)
{
	char language[8] = { 0 };
	chiaki_cloud_gaikai_language("DE-de", language, sizeof(language));
	check(strcmp(language, "de") == 0, "cloud locale Gaikai language");
	chiaki_cloud_gaikai_language(NULL, language, sizeof(language));
	check(strcmp(language, "en") == 0, "cloud locale default language");
	size_t count = chiaki_cloud_supported_locale_count();
	check(count >= 10, "cloud supported locale count");
	check(strcmp(chiaki_cloud_supported_locale(0), "en-US") == 0,
		"cloud supported locale first");
	check(strcmp(chiaki_cloud_supported_locale(count), "") == 0,
		"cloud supported locale bounds");
}

static void test_cloudcatalog_fetch_parsers(void)
{
	CURL *curl = curl_easy_init();
	char *escaped = curl ? curl_easy_escape(curl, "scope one:/?+", 0) : NULL;
	check(escaped && strcmp(escaped, "scope%20one%3A%2F%3F%2B") == 0,
		"cloud fetch URL escaping");
	curl_free(escaped);
	curl_easy_cleanup(curl);
	char country[8] = { 0 };
	char language[8] = { 0 };
	check(cc_parse_container_store_locale(
		"https://store.example/container/FI/fi/19/PPSA00001_00",
		country, sizeof(country), language, sizeof(language))
		&& strcmp(country, "FI") == 0 && strcmp(language, "fi") == 0,
		"cloud fetch store locale parse");
	check(!cc_parse_container_store_locale("https://store.example/not-container",
		country, sizeof(country), language, sizeof(language))
		&& country[0] == 0 && language[0] == 0,
		"cloud fetch store locale rejection");
}

static void test_cloudcatalog_merge_module(void)
{
	static const char browse_json[] =
		"[{\"productId\":\"PPSA-ZEBRA_00\",\"name\":\"Zebra\","
		"\"device\":[\"PS5\"],\"streamingSupported\":true,\"conceptId\":1},"
		"{\"productId\":\"PPSA-APPLE_00\",\"name\":\"Apple\","
		"\"device\":[\"PS5\"],\"streamingSupported\":true,\"conceptId\":2}]";
	static const char owned_json[] =
		"[{\"serviceType\":\"pscloud\",\"id\":\"PPSA-ZEBRA_00\","
		"\"product_id\":\"PPSA-ZEBRA_00\",\"conceptId\":1,"
		"\"feature_type\":3,\"game_meta\":{\"name\":\"Zebra\"}}]";
	struct json_object *browse = json_tokener_parse(browse_json);
	struct json_object *owned = json_tokener_parse(owned_json);
	check(browse && owned, "cloud catalog fixture parse");
	if(!browse || !owned)
	{
		if(browse) json_object_put(browse);
		if(owned) json_object_put(owned);
		return;
	}
	CCAssembleInput input = {
		.imagic_browse = browse,
		.owned_cross_ref = owned,
		.settled_locale = "en-US",
	};
	LogCapture capture = { 0 };
	ChiakiLog log;
	chiaki_log_init(&log, 0, capture_log, &capture);
	struct json_object *envelope = cc_assemble_unified_catalog(&log, &input);
	check(envelope != NULL, "cloud catalog fixture assembly");
	struct json_object *games = envelope ? cc_json_arr(envelope, "games") : NULL;
	check(games && json_object_array_length(games) == 2,
		"cloud catalog fixture total");
	check(envelope && cc_json_int(envelope, "schemaVersion")
		== CHIAKI_CLOUDCATALOG_SCHEMA_VERSION,
		"cloud catalog schema version");
	struct json_object *first = games ? json_object_array_get_idx(games, 0) : NULL;
	struct json_object *second = games ? json_object_array_get_idx(games, 1) : NULL;
	check(first && cc_json_bool(first, "isOwned")
		&& strcmp(cc_json_str(first, "name"), "Zebra") == 0,
		"cloud catalog owned-first order");
	check(second && strcmp(cc_json_str(second, "name"), "Apple") == 0
		&& strcmp(cc_json_str(second, "category"), "purchaseable") == 0,
		"cloud catalog purchaseable route");
	if(envelope) json_object_put(envelope);
	json_object_put(browse);
	json_object_put(owned);
}

#ifdef __vita__
static void test_cloudcatalog_cache_module(void)
{
	static const char cache_dir[] = "ux0:data/plus-vita-cache-test";
	static const char cache_key[] = "fixture:v8";
	LogCapture capture = { 0 };
	ChiakiLog log;
	chiaki_log_init(&log, 0, capture_log, &capture);
	cc_cache_remove(cache_dir, cache_key);
	check(cc_cache_ensure_dir(cache_dir) == CHIAKI_ERR_SUCCESS,
		"cloud cache directory");
	struct json_object *fixture = json_tokener_parse(
		"{\"schemaVersion\":8,\"marker\":\"vita-cache\"}");
	check(fixture != NULL, "cloud cache fixture parse");
	check(fixture && cc_cache_write(&log, cache_dir, cache_key, fixture)
		== CHIAKI_ERR_SUCCESS, "cloud cache write");
	struct json_object *loaded = cc_cache_read(
		&log, cache_dir, cache_key, 60L * 60L * 1000L);
	check(loaded != NULL, "cloud cache read");
	check(loaded && cc_json_int(loaded, "schemaVersion") == 8
		&& strcmp(cc_json_str(loaded, "marker"), "vita-cache") == 0,
		"cloud cache content");
	if(loaded) json_object_put(loaded);
	if(fixture) json_object_put(fixture);
	cc_cache_remove(cache_dir, cache_key);
	loaded = cc_cache_read(&log, cache_dir, cache_key, 60L * 60L * 1000L);
	check(loaded == NULL, "cloud cache removal");
	if(loaded) json_object_put(loaded);
}

static void test_cloudcatalog_unified_cache_hit(void)
{
	static const char cache_dir[] = "ux0:data/plus-vita-unified-test";
	LogCapture capture = { 0 };
	ChiakiLog log;
	chiaki_log_init(&log, 0, capture_log, &capture);
	chiaki_cloudcatalog_invalidate_cache(cache_dir);
	struct json_object *cached = json_tokener_parse(
		"{\"schemaVersion\":8,\"total\":1,\"nativeMode\":false,"
		"\"fallbackRegion\":\"US\",\"resolvedStoreLang\":\"\","
		"\"settledLocale\":\"en-US\",\"warning\":\"\",\"games\":[{"
		"\"productId\":\"PPSA-OFFLINE_00\",\"name\":\"Offline Fixture\","
		"\"category\":\"purchaseable\",\"serviceType\":\"pscloud\","
		"\"platform\":\"ps5\",\"isOwned\":false}]}" );
	check(cached != NULL, "unified cache fixture parse");
	check(cached && cc_cache_write(&log, cache_dir, "unified_catalog_v3", cached)
		== CHIAKI_ERR_SUCCESS, "unified cache seed");
	if(cached) json_object_put(cached);
	plus_vita_catalog_fetch_guard_reset();
	plus_vita_http_test_block_network(true);
	ChiakiCloudCatalogConfig config = {
		.locale = "en-US",
		.cache_dir = cache_dir,
	};
	ChiakiCloudCatalogResult result = { 0 };
	ChiakiErrorCode error = chiaki_cloudcatalog_fetch_unified(&config, &result, &log);
	check(error == CHIAKI_ERR_SUCCESS && result.json,
		"unified public API cache hit");
	check(plus_vita_http_test_network_attempts() == 0,
		"unified cache hit zero HTTP");
	check(plus_vita_catalog_fetch_guard_attempts() == 0,
		"unified cache hit zero fetch");
	struct json_object *envelope = result.json ? json_tokener_parse(result.json) : NULL;
	struct json_object *games = envelope ? cc_json_arr(envelope, "games") : NULL;
	check(envelope && cc_json_int(envelope, "schemaVersion") == 8
		&& games && json_object_array_length(games) == 1,
		"unified cache contract");
	struct json_object *game = games ? json_object_array_get_idx(games, 0) : NULL;
	check(game && strcmp(cc_json_str(game, "productId"),
		"PPSA-OFFLINE_00") == 0, "unified cache game content");
	if(envelope) json_object_put(envelope);
	chiaki_cloudcatalog_result_fini(&result);
	chiaki_cloudcatalog_invalidate_cache(cache_dir);
	plus_vita_http_test_block_network(false);
	cached = cc_cache_read(&log, cache_dir, "unified_catalog_v3", CC_CACHE_TTL_MS);
	check(cached == NULL, "unified cache invalidation");
	if(cached) json_object_put(cached);
}

#ifdef PLUS_VITA_PUBLIC_APOLLO_TEST
static size_t public_apollo_game_count;
static bool public_apollo_complete;

static void test_public_apollo_catalog(void)
{
	LogCapture capture = { 0 };
	ChiakiLog log;
	chiaki_log_init(&log, 0, capture_log, &capture);
	plus_vita_http_test_block_network(false);
	bool complete = false;
	struct json_object *games = cc_fetch_apollo_fallback(&log, "CH", &complete);
	size_t count = games ? json_object_array_length(games) : 0;
	public_apollo_complete = complete;
	public_apollo_game_count = count;
	printf("INFO public Apollo CH/EU complete=%d games=%u GETs=%u\n", complete,
		(unsigned int)count, plus_vita_http_test_network_attempts());
	check(games != NULL, "public Apollo response");
	check(complete, "public Apollo traversal complete");
	check(count > 0, "public Apollo game count");
	check(plus_vita_http_test_network_attempts() > 0,
		"public Apollo used native HTTP");
	if(games) json_object_put(games);
}
#endif
#endif

static void test_takion_protobuf(void)
{
	uint8_t wire[64] = { 0 };
	tkproto_TakionMessage message = tkproto_TakionMessage_init_zero;
	tkproto_TakionMessage decoded = tkproto_TakionMessage_init_zero;

	message.type = tkproto_TakionMessage_PayloadType_TAKIONPROTOCOLREQUESTACK;
	message.has_takion_protocol_request_ack = true;
	message.takion_protocol_request_ack.has_takion_protocol_version = true;
	message.takion_protocol_request_ack.takion_protocol_version = 12;

	pb_ostream_t output = pb_ostream_from_buffer(wire, sizeof(wire));
	check(pb_encode(&output, tkproto_TakionMessage_fields, &message),
		"Takion protobuf encode");
	check(output.bytes_written > 0 && output.bytes_written <= sizeof(wire),
		"Takion protobuf wire size");

	pb_istream_t input = pb_istream_from_buffer(wire, output.bytes_written);
	check(pb_decode(&input, tkproto_TakionMessage_fields, &decoded),
		"Takion protobuf decode");
	check(decoded.type == tkproto_TakionMessage_PayloadType_TAKIONPROTOCOLREQUESTACK,
		"Takion payload type");
	check(decoded.has_takion_protocol_request_ack
		&& decoded.takion_protocol_request_ack.has_takion_protocol_version
		&& decoded.takion_protocol_request_ack.takion_protocol_version == 12,
		"Takion protocol version");
}

static void test_platform_primitives(void)
{
	LogCapture capture = { 0 };
	ChiakiLog log;
	chiaki_log_init(&log, CHIAKI_LOG_INFO | CHIAKI_LOG_ERROR, capture_log, &capture);
	CHIAKI_LOGD(&log, "hidden");
	CHIAKI_LOGI(&log, "vita platform %d", 1);
	check(capture.calls == 1, "log level mask");
	check(capture.level == CHIAKI_LOG_INFO
		&& strcmp(capture.message, "vita platform 1") == 0, "log formatting");

	uint64_t first = chiaki_time_now_monotonic_us();
	uint64_t second = chiaki_time_now_monotonic_us();
	check(first > 0 && second >= first, "monotonic clock");

	uint8_t entropy[80] = { 0 };
	check(chiaki_random_bytes_crypt(entropy, sizeof(entropy)) == CHIAKI_ERR_SUCCESS,
		"secure entropy result");
	unsigned int entropy_or = 0;
	for(size_t i = 0; i < sizeof(entropy); i++)
		entropy_or |= entropy[i];
	check(entropy_or != 0, "secure entropy content");

	ChiakiMutex mutex;
	check(chiaki_mutex_init(&mutex, false) == CHIAKI_ERR_SUCCESS, "mutex init");
	check(chiaki_mutex_lock(&mutex) == CHIAKI_ERR_SUCCESS, "mutex lock");
	check(chiaki_mutex_trylock(&mutex) == CHIAKI_ERR_MUTEX_LOCKED, "mutex contention");
	check(chiaki_mutex_unlock(&mutex) == CHIAKI_ERR_SUCCESS, "mutex unlock");

	int protected_value = 0;
	ThreadProbe probe = { .mutex = &mutex, .value = &protected_value };
	ChiakiThread thread;
	void *thread_result = NULL;
	check(chiaki_thread_create(&thread, thread_probe_run, &probe) == CHIAKI_ERR_SUCCESS,
		"thread create");
	check(chiaki_thread_join(&thread, &thread_result) == CHIAKI_ERR_SUCCESS,
		"thread join");
	check(protected_value == 42 && thread_result == &thread_result_marker,
		"thread synchronized result");

	check(chiaki_mutex_fini(&mutex) == CHIAKI_ERR_SUCCESS, "mutex fini");
}

static void test_http_request_helpers(void)
{
	char *bearer = NULL;
	char *cookie = NULL;
	check(cc_http_make_bearer_header(&bearer, "test-token") == CHIAKI_ERR_SUCCESS,
		"HTTP bearer header allocation");
	check(bearer && strcmp(bearer, "Authorization: Bearer test-token") == 0,
		"HTTP bearer header value");
	check(cc_http_make_cookie_header(&cookie, "test", "value") == CHIAKI_ERR_SUCCESS,
		"HTTP cookie header allocation");
	check(cookie && strcmp(cookie, "Cookie: test=value") == 0,
		"HTTP cookie header value");
	free(bearer);
	free(cookie);

	CCHttpResponse response = {
		.data = malloc(2), .headers = malloc(2), .redirect_url = malloc(2),
		.size = 1, .headers_size = 1, .status_code = 200,
	};
	cc_http_response_fini(&response);
	check(!response.data && !response.headers && !response.redirect_url
		&& response.size == 0 && response.status_code == 0,
		"HTTP response cleanup");
	CCHttpRequest invalid_request = { 0 };
	check(cc_http_perform(NULL, &invalid_request, &response) == CHIAKI_ERR_INVALID_DATA,
		"HTTP request validation");
}

static int run_loopback_probe(void)
{
	static const char payload[] = "vita-loopback";
	struct addrinfo hints = { 0 };
	struct addrinfo *resolved = NULL;
	struct sockaddr_in address = { 0 };
	socklen_t address_size = sizeof(address);
	chiaki_socket_t server = CHIAKI_INVALID_SOCKET;
	chiaki_socket_t client = CHIAKI_INVALID_SOCKET;
	chiaki_socket_t peer = CHIAKI_INVALID_SOCKET;
	char received[sizeof(payload)] = { 0 };
	int result = -1;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST;
	if(getaddrinfo("127.0.0.1", NULL, &hints, &resolved) != 0 || !resolved)
		goto cleanup;

	server = socket(AF_INET, SOCK_STREAM, 0);
	if(CHIAKI_SOCKET_IS_INVALID(server))
		goto cleanup;
	int reuse = 1;
	if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
		goto cleanup;
	memcpy(&address, resolved->ai_addr, sizeof(address));
	address.sin_port = 0;
	if(bind(server, (struct sockaddr *)&address, sizeof(address)) < 0
		|| getsockname(server, (struct sockaddr *)&address, &address_size) < 0
		|| listen(server, 1) < 0)
		goto cleanup;

	client = socket(AF_INET, SOCK_STREAM, 0);
	if(CHIAKI_SOCKET_IS_INVALID(client)
		|| connect(client, (struct sockaddr *)&address, sizeof(address)) < 0)
		goto cleanup;
	peer = accept(server, NULL, NULL);
	if(CHIAKI_SOCKET_IS_INVALID(peer))
		goto cleanup;
	if(chiaki_socket_set_nonblock(client, true) != CHIAKI_ERR_SUCCESS
		|| chiaki_socket_set_nonblock(client, false) != CHIAKI_ERR_SUCCESS)
		goto cleanup;
	if(send(client, payload, sizeof(payload), 0) != (ssize_t)sizeof(payload)
		|| recv(peer, received, sizeof(received), 0) != (ssize_t)sizeof(received)
		|| memcmp(received, payload, sizeof(payload)) != 0)
		goto cleanup;
	result = 0;

cleanup:
	if(!CHIAKI_SOCKET_IS_INVALID(peer))
		CHIAKI_SOCKET_CLOSE(peer);
	if(!CHIAKI_SOCKET_IS_INVALID(client))
		CHIAKI_SOCKET_CLOSE(client);
	if(!CHIAKI_SOCKET_IS_INVALID(server))
		CHIAKI_SOCKET_CLOSE(server);
	if(resolved)
		freeaddrinfo(resolved);
	return result;
}

static void test_network_primitives(void)
{
	check(plus_vita_network_init() == CHIAKI_ERR_SUCCESS, "network initialization");
	check(plus_vita_network_is_initialized(), "network initialized state");
	if(plus_vita_network_is_initialized())
		check(run_loopback_probe() == 0, "numeric resolution and loopback exchange");
	plus_vita_network_fini();
	check(!plus_vita_network_is_initialized(), "network shutdown state");
}

static void test_secure_http_lifecycle(void)
{
	check(plus_vita_secure_http_init() == CHIAKI_ERR_SUCCESS,
		"secure HTTP initialization");
	check(plus_vita_secure_http_is_initialized(), "secure HTTP initialized state");
	check(plus_vita_network_is_initialized(), "secure HTTP network dependency");
	plus_vita_secure_http_fini();
	check(!plus_vita_secure_http_is_initialized(), "secure HTTP shutdown state");
	check(!plus_vita_network_is_initialized(), "secure HTTP releases owned network");

	check(plus_vita_network_init() == CHIAKI_ERR_SUCCESS,
		"shared network initialization");
	check(plus_vita_secure_http_init() == CHIAKI_ERR_SUCCESS,
		"secure HTTP with shared network");
	plus_vita_secure_http_fini();
	check(plus_vita_network_is_initialized(), "secure HTTP preserves shared network");
	plus_vita_network_fini();
}

#if defined(__vita__) && defined(PLUS_VITA_TLS_PROBE_URL)
static void test_untrusted_local_certificate(void)
{
	static const char probe_url[] = PLUS_VITA_TLS_PROBE_URL;
	PlusVitaTlsProbeResult result = { 0 };
	check(plus_vita_secure_http_init() == CHIAKI_ERR_SUCCESS,
		"local TLS probe initialization");
	ChiakiErrorCode probe_result = plus_vita_secure_http_probe_untrusted(
		probe_url, &result);
	printf("INFO TLS send=0x%08x ssl=0x%08x detail=0x%08x\n",
		(unsigned int)result.send_result, (unsigned int)result.ssl_error,
		result.ssl_detail);
	check(probe_result == CHIAKI_ERR_SUCCESS,
		"untrusted local certificate rejected");
	plus_vita_secure_http_fini();
}
#endif

#if defined(__vita__) && defined(PLUS_VITA_TLS_TRUSTED_PROBE)
static void test_trusted_local_certificate(void)
{
	PlusVitaTlsProbeResult result = { 0 };
	check(plus_vita_secure_http_init() == CHIAKI_ERR_SUCCESS,
		"trusted TLS probe initialization");
	ChiakiErrorCode probe_result = plus_vita_secure_http_probe_trusted(
		PLUS_VITA_TLS_PROBE_URL, "app0:local_probe_ca.der", &result);
	printf("INFO trusted TLS send=0x%08x ssl=0x%08x detail=0x%08x\n",
		(unsigned int)result.send_result, (unsigned int)result.ssl_error,
		result.ssl_detail);
	printf("INFO trusted HTTP status=%d body=%d\n",
		result.http_status, result.body_size);
	printf("INFO HTTP lenr=0x%08x len=%llu statusr=0x%08x read=%d\n",
		(unsigned int)result.content_length_result, result.content_length,
		(unsigned int)result.status_result, result.read_result);
	check(probe_result == CHIAKI_ERR_SUCCESS,
		"trusted local HTTPS response");
	plus_vita_secure_http_fini();
}
#endif

#if defined(__vita__) && defined(PLUS_VITA_PUBLIC_HTTPS_URL)
static void test_public_https(void)
{
	static const char *const headers[] = { "Accept: text/html" };
	CCHttpRequest request = {
		.method = "GET",
		.url = PLUS_VITA_PUBLIC_HTTPS_URL,
		.headers = headers,
		.header_count = 1,
		.follow_redirects = true,
		.capture_headers = true,
		.timeout_ms = 10000,
	};
	CCHttpResponse response = { 0 };
	ChiakiErrorCode result = cc_http_perform(NULL, &request, &response);
	printf("INFO compat GET result=%d status=%ld body=%u headers=%u\n",
		result, response.status_code, (unsigned int)response.size,
		(unsigned int)response.headers_size);
	check(result == CHIAKI_ERR_SUCCESS, "compat HTTP GET transport");
	check(response.status_code == 200, "compat HTTP GET status");
	check(response.data && response.size > 0, "compat HTTP GET body");
	check(response.headers && response.headers_size > 0,
		"compat HTTP GET response headers");
	cc_http_response_fini(&response);
}
#endif

#if defined(__vita__) && defined(PLUS_VITA_POST_ECHO_URL)
static void test_post_echo(void)
{
	static const char body[] = "plus-vita-post=takion-12";
	static const char *const headers[] = {
		"Content-Type: application/x-www-form-urlencoded",
		"X-Plus-Vita-Test: compat-post",
	};
	CCHttpRequest request = {
		.method = "POST",
		.url = PLUS_VITA_POST_ECHO_URL,
		.headers = headers,
		.header_count = 2,
		.body = body,
		.body_len = sizeof(body) - 1,
		.capture_headers = true,
		.timeout_ms = 10000,
	};
	CCHttpResponse response = { 0 };
	ChiakiErrorCode result = cc_http_perform(NULL, &request, &response);
	printf("INFO compat POST result=%d status=%ld body=%u headers=%u\n",
		result, response.status_code, (unsigned int)response.size,
		(unsigned int)response.headers_size);
	check(result == CHIAKI_ERR_SUCCESS, "compat HTTP POST transport");
	check(response.status_code == 200, "compat HTTP POST status");
	check(response.data && response.size == sizeof(body) - 1
		&& memcmp(response.data, body, sizeof(body) - 1) == 0,
		"compat HTTP POST body echo");
	check(response.headers && strstr(response.headers, "X-Plus-Vita-Echo: ok"),
		"compat HTTP POST response header");
	cc_http_response_fini(&response);
}
#endif

int main(void)
{
#ifdef __vita__
	PlusVitaCredentialImeResult ime_result;
	PlusVitaCredentialFileResult file_result;
	PlusVitaCredential runtime_credential;
	PlusVitaAuthProbeResult auth_result;
	memset(&ime_result, 0, sizeof(ime_result));
	plus_vita_credential_file_read(&runtime_credential, &file_result);
	if(!file_result.parsed)
		plus_vita_credential_ime_read(&runtime_credential, &ime_result);
	else
	{
		ime_result.initialized = true;
		ime_result.accepted = true;
		ime_result.validated = true;
		ime_result.input_wiped = file_result.buffer_wiped;
		ime_result.entered_length = runtime_credential.length;
	}
	plus_vita_auth_probe(&runtime_credential, &auth_result);
	plus_vita_credential_clear(&runtime_credential);
	ime_result.credential_wiped = plus_vita_credential_is_clear(&runtime_credential);
	psvDebugScreenInit();
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
#endif
	printf("plus-vita native libchiaki smoke test\n");
#ifdef __vita__
	check(ime_result.initialized, "credential dialog initialization");
	check(ime_result.accepted, "credential runtime input accepted");
	check(ime_result.validated && ime_result.entered_length > 0,
		"credential bounded validation");
	check(ime_result.credential_wiped, "credential runtime wipe");
	check(ime_result.input_wiped, "credential IME buffer wipe");
	check(!file_result.found || (file_result.parsed && file_result.deleted
		&& file_result.buffer_wiped), "one-time credential file consumed");
	check(auth_result.format_valid, "NPSSO 64-character format");
	check(auth_result.attempted, "runtime-only authorization attempted");
	check(auth_result.transport_ok, "authorization TLS transport");
	check(auth_result.authorized, "NPSSO authorization accepted");
	check(auth_result.session_transport_ok, "PS Now session TLS transport");
	check(auth_result.session_created, "PS Now session region accepted");
	check(auth_result.store_transport_ok, "PS Now store TLS transport");
	check(auth_result.store_resolved, "PS Now native store route resolved");
	check(auth_result.native_catalog_complete, "PS Now native catalog traversal");
	check(auth_result.owned_authorized, "owned entitlement authorization");
	check(auth_result.owned_complete, "owned entitlement traversal");
#endif
	test_base64();
	test_runtime_credential_lifecycle();
	test_protocol_primitives();
	test_cloudcatalog_locale_module();
	test_cloudcatalog_fetch_parsers();
	test_cloudcatalog_merge_module();
#ifdef __vita__
	test_cloudcatalog_cache_module();
	test_cloudcatalog_unified_cache_hit();
#ifdef PLUS_VITA_PUBLIC_APOLLO_TEST
	test_public_apollo_catalog();
#endif
#endif
	test_takion_protobuf();
	test_platform_primitives();
	test_http_request_helpers();
	test_network_primitives();
	test_secure_http_lifecycle();
#if defined(__vita__) && defined(PLUS_VITA_TLS_PROBE_URL)
	test_untrusted_local_certificate();
#endif
#if defined(__vita__) && defined(PLUS_VITA_TLS_TRUSTED_PROBE)
	test_trusted_local_certificate();
#endif
#if defined(__vita__) && defined(PLUS_VITA_PUBLIC_HTTPS_URL)
	test_public_https();
#endif
#if defined(__vita__) && defined(PLUS_VITA_POST_ECHO_URL)
	test_post_echo();
#endif
#ifdef __vita__
	printf("\033[2J\033[H");
	printf("plus-vita hardware test complete\n\n");
	printf("RESULT %s\n", failures ? "FAIL" : "PASS");
	printf("Checks: %d   Failures: %d\n\n", checks_run, failures);
	printf("Unified catalog cache-hit test included.\n");
	printf("Runtime-only credential lifecycle included.\n");
	printf("Visible Vita credential entry (runtime-only) included.\n");
	printf("One-time credential file: %s\n",
		file_result.found ? (file_result.parsed && file_result.deleted ? "consumed" : "failed") : "not present");
	printf("Runtime-only NPSSO authorization: %s (HTTP %ld)\n",
		auth_result.authorized ? "accepted" : "rejected", auth_result.http_status);
	printf("PS Now session: %s (HTTP %ld) Region: %s/%s\n",
		auth_result.session_created ? "created" : "failed",
		auth_result.session_http_status,
		auth_result.country[0] ? auth_result.country : "--",
		auth_result.language[0] ? auth_result.language : "--");
	printf("Native store: %s (HTTP %ld) Route: %s/%s\n",
		auth_result.store_resolved ? "resolved" : "failed",
		auth_result.store_http_status,
		auth_result.store_country[0] ? auth_result.store_country : "--",
		auth_result.store_language[0] ? auth_result.store_language : "--");
	printf("Native catalog: %s Categories: %u Titles: %u\n",
		auth_result.native_catalog_complete ? "complete" : "failed",
		auth_result.native_category_count, auth_result.native_title_count);
	printf("Owned: %s Pages: %u Entitlements: %u Games: %u\n",
		auth_result.owned_complete ? "complete" : "failed",
		auth_result.owned_page_count, auth_result.owned_entitlement_count,
		auth_result.owned_game_count);
	printf("NPSSO entered length: %u (expected 64)\n",
		(unsigned int)ime_result.entered_length);
#ifdef PLUS_VITA_PUBLIC_APOLLO_TEST
	printf("Public Apollo region: CH/EU\n");
	printf("Apollo complete: %s   Titles: %u\n",
		public_apollo_complete ? "yes" : "no",
		(unsigned int)public_apollo_game_count);
#endif
	printf("HTTP attempts: %u   Fetch attempts: %u\n",
		plus_vita_http_test_network_attempts(),
		plus_vita_catalog_fetch_guard_attempts());
#ifdef PLUS_VITA_PUBLIC_APOLLO_TEST
	printf("Last HTTP: %s native=0x%08X err=%d status=%d\n",
		plus_vita_http_test_last_stage(),
		(unsigned int)plus_vita_http_test_last_native_result(),
		plus_vita_http_test_last_error(),
		plus_vita_http_test_last_status());
	printf("TLS error=0x%08X detail=0x%08X\n",
		(unsigned int)plus_vita_http_test_last_ssl_error(),
		plus_vita_http_test_last_ssl_detail());
#endif
	printf("\nPress X to close. Photograph this screen for the test report.\n");
	SceCtrlData pad = { 0 };
	do
	{
		sceKernelDelayThread(20000);
		sceCtrlPeekBufferPositive(0, &pad, 1);
	} while(!(pad.buttons & SCE_CTRL_CROSS));
	psvDebugScreenFinish();
#else
	printf("RESULT %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures,
		failures == 1 ? "" : "s");
#endif
	return failures ? 1 : 0;
}
