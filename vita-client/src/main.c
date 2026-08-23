// SPDX-License-Identifier: AGPL-3.0-only

#include <chiaki/base64.h>
#include <chiaki/common.h>
#include <chiaki/seqnum.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <takion.pb.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

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

int main(void)
{
	printf("plus-vita native libchiaki smoke test\n");
	test_base64();
	test_protocol_primitives();
	test_takion_protobuf();
	printf("RESULT %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures,
		failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
