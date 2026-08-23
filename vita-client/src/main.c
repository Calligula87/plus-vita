// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/base64.h>
#include <chiaki/common.h>
#include <chiaki/log.h>
#include <chiaki/random.h>
#include <chiaki/seqnum.h>
#include <chiaki/thread.h>
#include <chiaki/time.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <takion.pb.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

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

static void test_protocol_primitives(void)
{
	check(chiaki_seq_num_16_lt(UINT16_MAX, 0), "16-bit sequence wrap");
	check(chiaki_seq_num_32_gt(0, UINT32_MAX), "32-bit sequence wrap");
	check(chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_PSNOW), "PS Now is cloud");
	check(chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_PSCLOUD), "PS Cloud is cloud");
	check(!chiaki_service_type_is_cloud(CHIAKI_SERVICE_TYPE_REMOTE_PLAY),
		"Remote Play is not cloud");
}

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

int main(void)
{
	printf("plus-vita native libchiaki smoke test\n");
	test_base64();
	test_protocol_primitives();
	test_takion_protobuf();
	test_platform_primitives();
	printf("RESULT %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures,
		failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
