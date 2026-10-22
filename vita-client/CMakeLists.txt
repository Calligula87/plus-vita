cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
  else()
    message(FATAL_ERROR "Set VITASDK to the VitaSDK installation directory")
  endif()
endif()

project(plus_vita_smoke C)

set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
set(OPUS_FIXED_POINT ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../external/opus-1.6.1" opus)

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

option(PLUS_VITA_DIAGNOSTIC_APP "Launch the historical diagnostic/test interface" OFF)
if(PLUS_VITA_DIAGNOSTIC_APP)
  set(VITA_APP_NAME "plus-vita smoke")
  set(PLUS_VITA_ENTRY_SOURCE src/main.c src/app/utf8.c)
else()
  set(VITA_APP_NAME "Plus Vita")
  set(PLUS_VITA_ENTRY_SOURCE src/app/main.c src/app/app.c src/app/utf8.c src/app/text.c src/app/font_vita.c src/app/render.c src/app/ui_flow.c src/app/ui_vita.c src/app/account_vita.c src/app/covers.c src/platform/cover_image.c src/platform/cover_download.c src/platform/cover_resolve.c)
endif()
set(VITA_TITLEID "PLVT00001")
set(VITA_VERSION "00.01")
set(PLUS_VITA_TLS_PROBE_URL "" CACHE STRING
  "Optional controlled HTTPS URL for the physical certificate-rejection test")
set(PLUS_VITA_TLS_TRUSTED_CA_FILE "" CACHE FILEPATH
  "Optional DER test CA packaged for the controlled trusted-HTTPS test")
set(PLUS_VITA_PUBLIC_HTTPS_URL "" CACHE STRING
  "Optional credential-free public HTTPS URL for a system-trust test")
set(PLUS_VITA_POST_ECHO_URL "" CACHE STRING
  "Optional controlled LAN URL for a physical POST echo test")
option(PLUS_VITA_PUBLIC_APOLLO_TEST
  "Enable the explicitly authorized credential-free public Apollo GET test" OFF)
option(PLUS_VITA_INPUT_WORKER "Evaluate independent 4ms input sampling" OFF)
option(PLUS_VITA_DEFERRED_LOG "Evaluate deferred streaming diagnostics" OFF)
if(PLUS_VITA_INPUT_WORKER AND PLUS_VITA_DEFERRED_LOG)
  # C and L have separate Vita evidence. Their combination is a new identified
  # test profile; component acceptance does not imply combined acceptance.
  message(STATUS "Experimental combined input-worker/deferred-log profile")
endif()

set(PYLUX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/pylux")
set(NANOPB_ROOT "${PYLUX_ROOT}/third-party/nanopb")
set(JSONC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/json-c")
set(VITASDK_COMMON "$ENV{VITASDK}/share/gcc-arm-vita-eabi/samples/common")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/video_fec_backend.cmake")
if(NOT EXISTS "${PYLUX_ROOT}/lib/src/base64.c")
  message(FATAL_ERROR "Pylux submodule is missing; run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${NANOPB_ROOT}/pb_encode.c")
  message(FATAL_ERROR "Pylux nanopb submodule is missing; run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${JSONC_ROOT}/json_object.c")
  message(FATAL_ERROR "json-c submodule is missing; run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
set(DISABLE_THREAD_LOCAL_STORAGE ON CACHE BOOL "" FORCE)
set(DISABLE_EXTRA_LIBS ON CACHE BOOL "" FORCE)
set(DISABLE_STATIC_FPIC ON CACHE BOOL "" FORCE)
# VitaSDK/newlib exposes these, but json-c only probes several of them when
# CMake's target platform is classified as UNIX. The Vita toolchain is Generic.
set(HAVE_SNPRINTF 1 CACHE INTERNAL "VitaSDK provides snprintf" FORCE)
set(HAVE_VASPRINTF 1 CACHE INTERNAL "VitaSDK provides vasprintf" FORCE)
set(HAVE_DECL_INFINITY 1 CACHE INTERNAL "VitaSDK provides INFINITY" FORCE)
set(HAVE_DECL_ISINF 1 CACHE INTERNAL "VitaSDK provides isinf" FORCE)
set(HAVE_DECL_ISNAN 1 CACHE INTERNAL "VitaSDK provides isnan" FORCE)
set(HAVE_DECL_NAN 1 CACHE INTERNAL "VitaSDK provides NAN" FORCE)
add_subdirectory("${JSONC_ROOT}" "${CMAKE_BINARY_DIR}/json-c" EXCLUDE_FROM_ALL)

# Generate a local, build-only Takion copy with complete error propagation.
# The upstream function can clean up a failed stop-pipe/read/thread operation
# while accidentally returning success, which makes the caller close an
# object that was never fully initialized. Keep every worker on Chiaki's
# pthread abstraction so creation and cleanup use one consistent thread API.
set(PLUS_VITA_TAKION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/takion_vita.c")
file(READ "${PYLUX_ROOT}/lib/src/takion.c" PLUS_VITA_TAKION_TEXT)
set(PLUS_VITA_TAKION_STOP_PIPE_OLD [=[
		CHIAKI_LOGE(takion->log, "Takion failed to create stop pipe");
		goto error_seq_num_local_mutex;
]=])
set(PLUS_VITA_TAKION_STOP_PIPE_NEW [=[
		CHIAKI_LOGE(takion->log, "Takion failed to create stop pipe: %s (%d)", chiaki_error_string(err), (int)err);
		ret = err;
		goto error_seq_num_local_mutex;
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_STOP_PIPE_OLD}" PLUS_VITA_TAKION_STOP_PIPE_POS)
if(PLUS_VITA_TAKION_STOP_PIPE_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion stop-pipe error patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_STOP_PIPE_OLD}" "${PLUS_VITA_TAKION_STOP_PIPE_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")

set(PLUS_VITA_TAKION_EXTRA_READ_OLD [=[
			CHIAKI_LOGE(takion->log, "Takion had problem reading extra messages from socket using PSN Connection with error: " CHIAKI_SOCKET_ERROR_FMT, CHIAKI_SOCKET_ERROR_VALUE);
			goto error_sock;
]=])
set(PLUS_VITA_TAKION_EXTRA_READ_NEW [=[
			CHIAKI_LOGE(takion->log, "Takion had problem reading extra messages from socket using PSN Connection: %s (%d)", chiaki_error_string(err), (int)err);
			ret = err;
			goto error_sock;
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_EXTRA_READ_OLD}" PLUS_VITA_TAKION_EXTRA_READ_POS)
if(PLUS_VITA_TAKION_EXTRA_READ_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion socket-read error patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_EXTRA_READ_OLD}" "${PLUS_VITA_TAKION_EXTRA_READ_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")

set(PLUS_VITA_TAKION_CONNECT_RESULT_OLD [=[
		if(r != CHIAKI_ERR_SUCCESS)
		{
			ret = err;
			goto error_sock;
		}
]=])
set(PLUS_VITA_TAKION_CONNECT_RESULT_NEW [=[
		if(r != 0)
		{
			CHIAKI_LOGE(takion->log, "Takion connect returned an unexpected result: %d", r);
			ret = CHIAKI_ERR_NETWORK;
			goto error_sock;
		}
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_CONNECT_RESULT_OLD}" PLUS_VITA_TAKION_CONNECT_RESULT_POS)
if(PLUS_VITA_TAKION_CONNECT_RESULT_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion connect-result patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_CONNECT_RESULT_OLD}" "${PLUS_VITA_TAKION_CONNECT_RESULT_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")

set(PLUS_VITA_TAKION_OLD [=[
	err = chiaki_thread_create(&takion->thread, takion_thread_func, takion);

	chiaki_thread_set_name(&takion->thread, "Chiaki Takion");
]=])
set(PLUS_VITA_TAKION_NEW [=[
	err = chiaki_thread_create(&takion->thread, takion_thread_func, takion);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		CHIAKI_LOGE(takion->log, "Takion failed to create worker thread: %s (%d)", chiaki_error_string(err), (int)err);
		ret = err;
		goto error_sock;
	}

	chiaki_thread_set_name(&takion->thread, "Chiaki Takion");
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_OLD}" PLUS_VITA_TAKION_PATCH_POS)
if(PLUS_VITA_TAKION_PATCH_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion thread-create patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_OLD}" "${PLUS_VITA_TAKION_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")

string(REPLACE "#include <chiaki/takion.h>"
  "#include <chiaki/takion.h>\n#include \"socket_vita.h\""
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")
set(PLUS_VITA_TAKION_SOCKET_OLD [=[
		takion->sock = socket(info->sa->sa_family, SOCK_DGRAM, IPPROTO_UDP);
]=])
set(PLUS_VITA_TAKION_SOCKET_NEW [=[
		takion->sock = plus_vita_socket_open("plus-vita-takion", info->sa->sa_family, SOCK_DGRAM, IPPROTO_UDP);
		if(!CHIAKI_SOCKET_IS_INVALID(takion->sock))
			CHIAKI_LOGI(takion->log, "Takion UDP socket created");
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_SOCKET_OLD}" PLUS_VITA_TAKION_SOCKET_POS)
if(PLUS_VITA_TAKION_SOCKET_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion UDP socket patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_SOCKET_OLD}" "${PLUS_VITA_TAKION_SOCKET_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")
set(PLUS_VITA_TAKION_SOCKET_LOG_OLD [=[
			CHIAKI_LOGE(takion->log, "Takion failed to create socket");
]=])
set(PLUS_VITA_TAKION_SOCKET_LOG_NEW [=[
			CHIAKI_LOGE(takion->log, "Takion failed to create socket: native=0x%08x net_errno=%d errno=%d",
				(unsigned int)plus_vita_socket_last_native_result(), plus_vita_socket_last_net_errno(), errno);
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_SOCKET_LOG_OLD}" PLUS_VITA_TAKION_SOCKET_LOG_POS)
if(PLUS_VITA_TAKION_SOCKET_LOG_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion UDP socket log patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_SOCKET_LOG_OLD}" "${PLUS_VITA_TAKION_SOCKET_LOG_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")

# Vita's SceNet can reject Takion's normal 4 MiB receive buffer even when the
# global network pool is large enough. Keep the protocol's advertised window,
# but progressively reduce only the local kernel buffer until SceNet accepts
# it. Datacenter probes exchange tiny packets, and the selected value is logged
# so later streaming work can tune it independently.
set(PLUS_VITA_TAKION_RCVBUF_OLD [=[
		const int rcvbuf_val = takion->a_rwnd;
		int r = setsockopt(takion->sock, SOL_SOCKET, SO_RCVBUF, (const CHIAKI_SOCKET_BUF_TYPE)&rcvbuf_val, sizeof(rcvbuf_val));
]=])
set(PLUS_VITA_TAKION_RCVBUF_NEW [=[
		int rcvbuf_val = takion->a_rwnd;
		int r = setsockopt(takion->sock, SOL_SOCKET, SO_RCVBUF, (const CHIAKI_SOCKET_BUF_TYPE)&rcvbuf_val, sizeof(rcvbuf_val));
		if(r < 0 && errno == ENOBUFS)
		{
			static const int vita_rcvbuf_fallbacks[] = {
				2 * 1024 * 1024, 1024 * 1024, 512 * 1024,
				256 * 1024, 128 * 1024, 64 * 1024
			};
			for(size_t i = 0; i < sizeof(vita_rcvbuf_fallbacks) / sizeof(vita_rcvbuf_fallbacks[0]); i++)
			{
				rcvbuf_val = vita_rcvbuf_fallbacks[i];
				r = setsockopt(takion->sock, SOL_SOCKET, SO_RCVBUF,
					(const CHIAKI_SOCKET_BUF_TYPE)&rcvbuf_val, sizeof(rcvbuf_val));
				if(r == 0 || errno != ENOBUFS)
					break;
			}
		}
		if(r == 0)
			CHIAKI_LOGI(takion->log, "Takion SO_RCVBUF requested=%u selected=%d", takion->a_rwnd, rcvbuf_val);
]=])
string(FIND "${PLUS_VITA_TAKION_TEXT}" "${PLUS_VITA_TAKION_RCVBUF_OLD}" PLUS_VITA_TAKION_RCVBUF_POS)
if(PLUS_VITA_TAKION_RCVBUF_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Takion receive-buffer patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_TAKION_RCVBUF_OLD}" "${PLUS_VITA_TAKION_RCVBUF_NEW}"
  PLUS_VITA_TAKION_TEXT "${PLUS_VITA_TAKION_TEXT}")
file(WRITE "${PLUS_VITA_TAKION_SOURCE}" "${PLUS_VITA_TAKION_TEXT}")

# Vita's pthread condition variables use CLOCK_REALTIME for absolute deadlines.
# Requesting CLOCK_MONOTONIC is accepted by the compatibility layer but the
# resulting timed wait is still interpreted as real time, causing an immediate
# timeout. Generate a Vita-only Chiaki thread implementation with one consistent
# clock for condition waits and timed joins.
set(PLUS_VITA_THREAD_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/thread_vita.c")
file(READ "${PYLUX_ROOT}/lib/src/thread.c" PLUS_VITA_THREAD_TEXT)
string(REPLACE "#if !__APPLE__\n\tr = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);"
  "#if !__APPLE__ && !defined(__vita__)\n\tr = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);"
  PLUS_VITA_THREAD_TEXT "${PLUS_VITA_THREAD_TEXT}")
string(REPLACE "clock_gettime(CLOCK_MONOTONIC, timeout);"
  "clock_gettime(CLOCK_REALTIME, timeout);"
  PLUS_VITA_THREAD_TEXT "${PLUS_VITA_THREAD_TEXT}")
string(REPLACE "#if !__APPLE__ && !__SWITCH__"
  "#if !__APPLE__ && !__SWITCH__ && !defined(__vita__)"
  PLUS_VITA_THREAD_TEXT "${PLUS_VITA_THREAD_TEXT}")
file(WRITE "${PLUS_VITA_THREAD_SOURCE}" "${PLUS_VITA_THREAD_TEXT}")

# Pylux currently targets the Mbed TLS 2.x multipart GCM signature. VitaSDK
# ships Mbed TLS 3.x, whose one-shot crypt_and_tag call performs the complete
# GMAC operation and whose context members require the documented compatibility
# opt-in. Keep this adaptation build-local.
set(PLUS_VITA_GKCRYPT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/gkcrypt_vita.c")
file(READ "${PYLUX_ROOT}/lib/src/gkcrypt.c" PLUS_VITA_GKCRYPT_TEXT)
set(PLUS_VITA_GKCRYPT_START_OLD [=[
	// encrypt without additional data
	if(mbedtls_gcm_starts(&actx, MBEDTLS_GCM_ENCRYPT, iv, CHIAKI_GKCRYPT_BLOCK_SIZE, NULL, 0) != 0)
	{
		mbedtls_gcm_free(&actx);
		return CHIAKI_ERR_UNKNOWN;
	}
]=])
string(FIND "${PLUS_VITA_GKCRYPT_TEXT}" "${PLUS_VITA_GKCRYPT_START_OLD}" PLUS_VITA_GKCRYPT_START_POS)
if(PLUS_VITA_GKCRYPT_START_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Mbed TLS 3 GKCrypt patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_GKCRYPT_START_OLD}" ""
  PLUS_VITA_GKCRYPT_TEXT "${PLUS_VITA_GKCRYPT_TEXT}")
file(WRITE "${PLUS_VITA_GKCRYPT_SOURCE}" "${PLUS_VITA_GKCRYPT_TEXT}")

# Mbed TLS 3 stores the software ECDH implementation in a tagged context
# rather than exposing the legacy fields at the top level. Generate a local
# source adaptation while retaining the upstream API and P-256 behavior.
set(PLUS_VITA_ECDH_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/ecdh_vita.c")
file(READ "${PYLUX_ROOT}/lib/src/ecdh.c" PLUS_VITA_ECDH_TEXT)
string(REPLACE "mbedtls_ecp_group_load(&ecdh->ctx.grp, MBEDTLS_ECP_DP_SECP256K1)"
  "mbedtls_ecdh_setup(&ecdh->ctx, MBEDTLS_ECP_DP_SECP256K1)"
  PLUS_VITA_ECDH_TEXT "${PLUS_VITA_ECDH_TEXT}")
string(REPLACE "mbedtls_ecp_group_load(&ecdh->ctx.grp, MBEDTLS_ECP_DP_SECP256R1)"
  "mbedtls_ecdh_setup(&ecdh->ctx, MBEDTLS_ECP_DP_SECP256R1)"
  PLUS_VITA_ECDH_TEXT "${PLUS_VITA_ECDH_TEXT}")
foreach(PLUS_VITA_ECDH_FIELD grp d Q Qp z)
  string(REPLACE "ecdh->ctx.${PLUS_VITA_ECDH_FIELD}"
    "ecdh->ctx.ctx.mbed_ecdh.${PLUS_VITA_ECDH_FIELD}"
    PLUS_VITA_ECDH_TEXT "${PLUS_VITA_ECDH_TEXT}")
endforeach()
# Populate the Mbed TLS 3 ECDH context through its supported keypair import
# state machine. Calling the low-level gen_public() directly fills coordinates
# but leaves read_public() without the context state it validates.
set(PLUS_VITA_ECDH_INIT_K1_OLD [=[
	CHECK(mbedtls_ecdh_setup(&ecdh->ctx, MBEDTLS_ECP_DP_SECP256K1));
	// build key
	CHECK(mbedtls_ecdh_gen_public(&ecdh->ctx.ctx.mbed_ecdh.grp, &ecdh->ctx.ctx.mbed_ecdh.d,
		&ecdh->ctx.ctx.mbed_ecdh.Q, mbedtls_ctr_drbg_random, &ecdh->drbg));
]=])
set(PLUS_VITA_ECDH_INIT_K1_NEW [=[
	mbedtls_ecp_keypair keypair;
	mbedtls_ecp_keypair_init(&keypair);
	int key_result = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256K1,
		&keypair, mbedtls_ctr_drbg_random, &ecdh->drbg);
	if(key_result == 0)
		key_result = mbedtls_ecdh_get_params(&ecdh->ctx, &keypair,
			MBEDTLS_ECDH_OURS);
	mbedtls_ecp_keypair_free(&keypair);
	CHECK(key_result);
]=])
set(PLUS_VITA_ECDH_INIT_R1_OLD [=[
CHECK(mbedtls_ecdh_setup(&ecdh->ctx, MBEDTLS_ECP_DP_SECP256R1));

	// Generate key pair
	CHECK(mbedtls_ecdh_gen_public(&ecdh->ctx.ctx.mbed_ecdh.grp, &ecdh->ctx.ctx.mbed_ecdh.d,
		&ecdh->ctx.ctx.mbed_ecdh.Q, mbedtls_ctr_drbg_random, &ecdh->drbg));
]=])
# The upstream block contains a tab-only blank line. Construct it without
# trailing whitespace in this CMake source, preserving the exact patch point.
string(REPLACE "\n\n" "\n\t\n" PLUS_VITA_ECDH_INIT_R1_OLD
  "${PLUS_VITA_ECDH_INIT_R1_OLD}")
set(PLUS_VITA_ECDH_INIT_R1_NEW [=[
	mbedtls_ecp_keypair keypair;
	mbedtls_ecp_keypair_init(&keypair);
	int key_result = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
		&keypair, mbedtls_ctr_drbg_random, &ecdh->drbg);
	if(key_result == 0)
		key_result = mbedtls_ecdh_get_params(&ecdh->ctx, &keypair,
			MBEDTLS_ECDH_OURS);
	mbedtls_ecp_keypair_free(&keypair);
	CHECK(key_result);
]=])
foreach(PLUS_VITA_ECDH_INIT_CURVE K1 R1)
  string(FIND "${PLUS_VITA_ECDH_TEXT}"
    "${PLUS_VITA_ECDH_INIT_${PLUS_VITA_ECDH_INIT_CURVE}_OLD}"
    PLUS_VITA_ECDH_INIT_POS)
  if(PLUS_VITA_ECDH_INIT_POS EQUAL -1)
    message(FATAL_ERROR "Pylux Mbed TLS 3 ECDH init patch point ${PLUS_VITA_ECDH_INIT_CURVE} was not found")
  endif()
  string(REPLACE "${PLUS_VITA_ECDH_INIT_${PLUS_VITA_ECDH_INIT_CURVE}_OLD}"
    "${PLUS_VITA_ECDH_INIT_${PLUS_VITA_ECDH_INIT_CURVE}_NEW}"
    PLUS_VITA_ECDH_TEXT "${PLUS_VITA_ECDH_TEXT}")
endforeach()
# Mbed TLS 3's supported ECDH import API consumes the raw uncompressed
# ECPoint returned by BANG. Avoid reaching into implementation-specific Qp/z
# fields when importing the cloud server's P-256 key and exporting the secret.
set(PLUS_VITA_ECDH_DERIVE_OLD [=[
	GOTO_ERROR(mbedtls_mpi_lset(&ecdh->ctx.ctx.mbed_ecdh.Qp.Z, 1));
	// load Qp point form remote PK
	GOTO_ERROR(mbedtls_ecp_point_read_binary(&ecdh->ctx.ctx.mbed_ecdh.grp,
		&ecdh->ctx.ctx.mbed_ecdh.Qp, remote_key, remote_key_size));

	// build shared secret (diffie-hellman)
	GOTO_ERROR(mbedtls_ecdh_compute_shared(&ecdh->ctx.ctx.mbed_ecdh.grp,
		&ecdh->ctx.ctx.mbed_ecdh.z, &ecdh->ctx.ctx.mbed_ecdh.Qp, &ecdh->ctx.ctx.mbed_ecdh.d,
		mbedtls_ctr_drbg_random, &ecdh->drbg));

	// export shared secret to data buffer
	GOTO_ERROR(mbedtls_mpi_write_binary(&ecdh->ctx.ctx.mbed_ecdh.z,
		secret_out, CHIAKI_ECDH_SECRET_SIZE));
]=])
set(PLUS_VITA_ECDH_DERIVE_NEW [=[
	GOTO_ERROR(mbedtls_ecdh_read_public(&ecdh->ctx, remote_key,
		remote_key_size));

	size_t secret_size = 0;
	GOTO_ERROR(mbedtls_ecdh_calc_secret(&ecdh->ctx, &secret_size,
		secret_out, CHIAKI_ECDH_SECRET_SIZE, mbedtls_ctr_drbg_random,
		&ecdh->drbg));
	if(secret_size != CHIAKI_ECDH_SECRET_SIZE)
		goto error;
]=])
string(FIND "${PLUS_VITA_ECDH_TEXT}" "${PLUS_VITA_ECDH_DERIVE_OLD}"
  PLUS_VITA_ECDH_DERIVE_POS)
if(PLUS_VITA_ECDH_DERIVE_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Mbed TLS 3 ECDH derive patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_ECDH_DERIVE_OLD}" "${PLUS_VITA_ECDH_DERIVE_NEW}"
  PLUS_VITA_ECDH_TEXT "${PLUS_VITA_ECDH_TEXT}")
file(WRITE "${PLUS_VITA_ECDH_SOURCE}" "${PLUS_VITA_ECDH_TEXT}")

# Vita has a small descriptor/thread budget. Each datacenter probe already
# owns a Takion worker, socket, and stop pipe, so launching one outer worker per
# datacenter can exhaust resources. Generate a Vita-only copy that runs one
# complete probe at a time; other platforms retain the upstream parallel path.
set(PLUS_VITA_GAIKAI_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/cloudsession_gaikai_vita.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${PYLUX_ROOT}/lib/src/cloudsession_gaikai.c")
file(READ "${PYLUX_ROOT}/lib/src/cloudsession_gaikai.c" PLUS_VITA_GAIKAI_TEXT)
set(PLUS_VITA_GAIKAI_PING_OLD [=[
			if(chiaki_thread_create(&threads[i], gk_ping_thread, j) == CHIAKI_ERR_SUCCESS)
				threaded[i] = true;
			else
			{
				gk_ping_thread(j); // fall back to inline if a thread won't start
				// gk_ping_thread set done+handoff (exchange returned 0, so it did NOT
				// free); the collect loop treats the slot as complete and frees it.
			}
]=])
set(PLUS_VITA_GAIKAI_PING_NEW [=[
#ifdef __vita__
			// One outer probe at a time. The probe itself still uses the normal
			// Chiaki pthread worker inside Takion.
			gk_ping_thread(j);
#else
			if(chiaki_thread_create(&threads[i], gk_ping_thread, j) == CHIAKI_ERR_SUCCESS)
				threaded[i] = true;
			else
			{
				gk_ping_thread(j); // fall back to inline if a thread won't start
				// gk_ping_thread set done+handoff (exchange returned 0, so it did NOT
				// free); the collect loop treats the slot as complete and frees it.
			}
#endif
]=])
string(FIND "${PLUS_VITA_GAIKAI_TEXT}" "${PLUS_VITA_GAIKAI_PING_OLD}" PLUS_VITA_GAIKAI_PING_POS)
if(PLUS_VITA_GAIKAI_PING_POS EQUAL -1)
  message(FATAL_ERROR "Pylux Gaikai ping-concurrency patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_GAIKAI_PING_OLD}" "${PLUS_VITA_GAIKAI_PING_NEW}"
  PLUS_VITA_GAIKAI_TEXT "${PLUS_VITA_GAIKAI_TEXT}")
# Do not log allocation response bodies: they may contain session material.
# Accept either pristine Pylux or an existing local copy with this fix.
set(PLUS_VITA_GAIKAI_ALLOC_OLD [=[
		if(resp.status_code != 200) { CHIAKI_LOGE(c->log, "[GAIKAI] step13 allocate http %ld: %s", resp.status_code, resp.data ? resp.data : ""); cc_http_response_fini(&resp); return CHIAKI_ERR_UNKNOWN; }
]=])
set(PLUS_VITA_GAIKAI_ALLOC_NEW [=[
		if(resp.status_code != 200) {
			CHIAKI_LOGE(c->log, "[GAIKAI] allocate HTTP status=%ld", resp.status_code);
			cc_http_response_fini(&resp);
			return CHIAKI_ERR_UNKNOWN;
		}
]=])
string(FIND "${PLUS_VITA_GAIKAI_TEXT}" "${PLUS_VITA_GAIKAI_ALLOC_OLD}"
  PLUS_VITA_GAIKAI_ALLOC_OLD_POS)
if(NOT PLUS_VITA_GAIKAI_ALLOC_OLD_POS EQUAL -1)
  string(REPLACE "${PLUS_VITA_GAIKAI_ALLOC_OLD}" "${PLUS_VITA_GAIKAI_ALLOC_NEW}"
    PLUS_VITA_GAIKAI_TEXT "${PLUS_VITA_GAIKAI_TEXT}")
else()
  string(FIND "${PLUS_VITA_GAIKAI_TEXT}" "${PLUS_VITA_GAIKAI_ALLOC_NEW}"
    PLUS_VITA_GAIKAI_ALLOC_NEW_POS)
  if(PLUS_VITA_GAIKAI_ALLOC_NEW_POS EQUAL -1)
    message(FATAL_ERROR "Pylux Gaikai allocation-log patch point was not found")
  endif()
endif()
file(WRITE "${PLUS_VITA_GAIKAI_SOURCE}" "${PLUS_VITA_GAIKAI_TEXT}")

# Preserve a release and a new touch in one update when a contact slot is
# reused. Keep this patch in the generated Vita source, not the submodule.
set(PLUS_VITA_FEEDBACK_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/feedbacksender_vita.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${PYLUX_ROOT}/lib/src/feedbacksender.c")
file(READ "${PYLUX_ROOT}/lib/src/feedbacksender.c" PLUS_VITA_FEEDBACK_TEXT)
set(PLUS_VITA_FEEDBACK_SLOT_OLD [=[
		else if(state_now->touches[i].id >= 0
]=])
set(PLUS_VITA_FEEDBACK_SLOT_NEW [=[
		/* A new contact can reuse this slot in the same update. Send both its
		 * predecessor's release and its own down event. */
		if(state_now->touches[i].id >= 0
]=])
string(FIND "${PLUS_VITA_FEEDBACK_TEXT}" "${PLUS_VITA_FEEDBACK_SLOT_OLD}"
  PLUS_VITA_FEEDBACK_SLOT_OLD_POS)
if(NOT PLUS_VITA_FEEDBACK_SLOT_OLD_POS EQUAL -1)
  string(REPLACE "${PLUS_VITA_FEEDBACK_SLOT_OLD}" "${PLUS_VITA_FEEDBACK_SLOT_NEW}"
    PLUS_VITA_FEEDBACK_TEXT "${PLUS_VITA_FEEDBACK_TEXT}")
else()
  string(FIND "${PLUS_VITA_FEEDBACK_TEXT}" "${PLUS_VITA_FEEDBACK_SLOT_NEW}"
    PLUS_VITA_FEEDBACK_SLOT_NEW_POS)
  if(PLUS_VITA_FEEDBACK_SLOT_NEW_POS EQUAL -1)
    message(FATAL_ERROR "Pylux touch-slot patch point was not found")
  endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cmake/feedback_latency.cmake")
file(WRITE "${PLUS_VITA_FEEDBACK_SOURCE}" "${PLUS_VITA_FEEDBACK_TEXT}")

# Generate a Vita-only curl transport copy which seeds CURLOPT_RESOLVE from a
# synchronous IPv4 lookup. This avoids VitaSDK libcurl's crashing threaded
# resolver cleanup while preserving TLS hostname verification.
set(PLUS_VITA_CURL_HTTP_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/curl_http_vita.c")
file(READ "${PYLUX_ROOT}/lib/src/curl_http.c" PLUS_VITA_CURL_HTTP_TEXT)
string(REPLACE "#include \"curl_http.h\""
  "#include \"curl_http.h\"\n#include \"curl_resolve.h\"\n#include \"request_cancel.h\""
  PLUS_VITA_CURL_HTTP_TEXT "${PLUS_VITA_CURL_HTTP_TEXT}")
string(REPLACE "GrowBuffer header_buf = { 0 };"
  "GrowBuffer header_buf = { 0 };\n\tstruct curl_slist *resolve_list = NULL;"
  PLUS_VITA_CURL_HTTP_TEXT "${PLUS_VITA_CURL_HTTP_TEXT}")
set(PLUS_VITA_CURL_URL_OLD [=[
	curl_easy_setopt(curl, CURLOPT_URL, request->url);
]=])
set(PLUS_VITA_CURL_URL_NEW [=[
	curl_easy_setopt(curl, CURLOPT_URL, request->url);
	CURLcode resolve_result = plus_vita_curl_prepare_resolve(curl, request->url, &resolve_list);
	if(resolve_result != CURLE_OK)
	{
		if(log)
			CHIAKI_LOGE(log, "cc_http_perform: synchronous DNS failed: %s", curl_easy_strerror(resolve_result));
		err = CHIAKI_ERR_NETWORK;
		goto cleanup;
	}
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, plus_vita_request_progress);
]=])
string(FIND "${PLUS_VITA_CURL_HTTP_TEXT}" "${PLUS_VITA_CURL_URL_OLD}" PLUS_VITA_CURL_URL_POS)
if(PLUS_VITA_CURL_URL_POS EQUAL -1)
  message(FATAL_ERROR "Pylux curl URL patch point was not found")
endif()
string(REPLACE "${PLUS_VITA_CURL_URL_OLD}" "${PLUS_VITA_CURL_URL_NEW}"
  PLUS_VITA_CURL_HTTP_TEXT "${PLUS_VITA_CURL_HTTP_TEXT}")
string(REPLACE "if(header_list)\n\t\tcurl_slist_free_all(header_list);"
  "if(header_list)\n\t\tcurl_slist_free_all(header_list);\n\tif(resolve_list)\n\t\tcurl_slist_free_all(resolve_list);"
  PLUS_VITA_CURL_HTTP_TEXT "${PLUS_VITA_CURL_HTTP_TEXT}")
file(WRITE "${PLUS_VITA_CURL_HTTP_SOURCE}" "${PLUS_VITA_CURL_HTTP_TEXT}")

add_executable(${PROJECT_NAME}
  ${PLUS_VITA_ENTRY_SOURCE}
  generated/takion.pb.c
  "${PYLUX_ROOT}/lib/src/audio.c"
  "${PYLUX_ROOT}/lib/src/base64.c"
  "${PYLUX_ROOT}/lib/src/bitstream.c"
  "${PYLUX_ROOT}/lib/src/controller.c"
  "${PYLUX_ROOT}/lib/src/feedback.c"
  "${PYLUX_ROOT}/lib/src/fec.c"
  "${PLUS_VITA_FEEDBACK_SOURCE}"
  "${PLUS_VITA_ECDH_SOURCE}"
  "${PLUS_VITA_GKCRYPT_SOURCE}"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_consts.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_cache.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_fetch.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_merge.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_util.c"
  "${PYLUX_ROOT}/lib/src/cloudcatalog_unified.c"
  "${PYLUX_ROOT}/lib/src/cloudsession.c"
  "${PLUS_VITA_GAIKAI_SOURCE}"
  "${PYLUX_ROOT}/lib/src/cloudsession_kamaji.c"
  "${PYLUX_ROOT}/lib/src/cloudsession_ping.c"
  "${PLUS_VITA_CURL_HTTP_SOURCE}"
  "${PYLUX_ROOT}/lib/src/log.c"
  "${PYLUX_ROOT}/lib/src/reorderqueue.c"
  "${PYLUX_ROOT}/lib/src/senkusha.c"
  "${PYLUX_ROOT}/lib/src/sock.c"
  src/platform/stoppipe_vita.c
  src/platform/socket_vita.c
  src/platform/h264_config.c
  src/platform/stream_probe.c
  src/platform/video_fec.c
  src/platform/video_reorder.c
  src/platform/audio_playback_vita.c
  src/platform/video_decoder_vita.c
  src/platform/video_present_vita.c
  "${PLUS_VITA_TAKION_SOURCE}"
  "${PYLUX_ROOT}/lib/src/takionsendbuffer.c"
  "${PLUS_VITA_THREAD_SOURCE}"
  "${NANOPB_ROOT}/pb_common.c"
  "${NANOPB_ROOT}/pb_decode.c"
  "${NANOPB_ROOT}/pb_encode.c"
  src/platform/random.c
  src/platform/request_cancel.c
  src/platform/curl_resolve.c
  src/platform/diagnostic_log.c
  src/platform/latency_stats.c
  src/platform/input_vita.c
  src/platform/auth_probe.c
  src/platform/catalog_fetch_guard.c
  src/platform/credential.c
  src/platform/credential_file.c
  src/platform/credential_ime.c
  src/platform/gkcrypt_unencrypted.c
  src/platform/network.c
  src/platform/provision_probe.c
  src/platform/secure_http.c
  src/platform/thread_compat.c
  src/platform/time.c
  src/platform/vita_cc_http.c
  "${VITASDK_COMMON}/debugScreen.c"
)

set_source_files_properties("${PYLUX_ROOT}/lib/src/bitstream.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=shift-negative-value;-Wno-error=sign-compare"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/fec.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=sign-compare"
)

# Pylux currently has one signed/unsigned comparison in its formatting path.
# Keep -Werror for our code and suppress only that upstream diagnostic.
set_source_files_properties("${PYLUX_ROOT}/lib/src/log.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=sign-compare"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_merge.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=format-truncation"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_fetch.c" PROPERTIES
  COMPILE_DEFINITIONS
    "cc_fetch_psnow_native=plus_vita_disabled_psnow_native_impl;cc_fetch_imagic=plus_vita_disabled_imagic_impl;cc_imagic_result_fini=plus_vita_disabled_imagic_fini_impl;cc_fetch_owned=plus_vita_disabled_owned_impl"
  COMPILE_OPTIONS "-Wno-error=unused-but-set-variable"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudsession.c" PROPERTIES
  COMPILE_OPTIONS "-include;arpa/inet.h"
)
set_source_files_properties("${PLUS_VITA_GAIKAI_SOURCE}" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=format-truncation"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/senkusha.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-parameter;-Wno-error=unused-variable"
)
set_source_files_properties("${PLUS_VITA_TAKION_SOURCE}" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-parameter;-Wno-error=unused-variable;-Wno-error=unused-but-set-variable;-Wno-error=sign-compare"
)
set_source_files_properties("${PYLUX_ROOT}/lib/src/takionsendbuffer.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-but-set-variable"
)
set_source_files_properties("${PLUS_VITA_ECDH_SOURCE}" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-parameter"
)
set_source_files_properties("${PLUS_VITA_GKCRYPT_SOURCE}" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-parameter;-Wno-error=unused-variable;-Wno-error=sign-compare"
)
set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/src/platform/stream_probe.c" PROPERTIES
  COMPILE_OPTIONS "-Wno-error=unused-parameter"
)
set_source_files_properties("${PLUS_VITA_CURL_HTTP_SOURCE}" PROPERTIES
  COMPILE_DEFINITIONS
    "cc_http_perform=plus_vita_curl_http_perform;cc_http_response_fini=plus_vita_curl_http_response_fini;cc_http_make_bearer_header=plus_vita_curl_make_bearer_header;cc_http_make_cookie_header=plus_vita_curl_make_cookie_header"
)

# This pure catalog module does not use json-c. Its private umbrella header
# includes json-c for sibling modules, so skip that header body for this source.
set_source_files_properties("${PYLUX_ROOT}/lib/src/cloudcatalog_consts.c" PROPERTIES
  COMPILE_DEFINITIONS "CHIAKI_CLOUDCATALOG_INTERNAL_H=1"
)

# Supply the Vita-only declaration/redirect for GNU pthread_clockjoin_np.
set_source_files_properties("${PYLUX_ROOT}/lib/src/thread.c" PROPERTIES
  COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/src/platform/thread_compat.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/src/platform/compat"
	"${CMAKE_CURRENT_LIST_DIR}/src/platform"
	"${CMAKE_CURRENT_LIST_DIR}/../external"
  "${PYLUX_ROOT}/lib/include"
  "${PYLUX_ROOT}/lib/src"
  "${NANOPB_ROOT}"
  "${CMAKE_CURRENT_LIST_DIR}/generated"
  "${VITASDK_COMMON}"
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall
  -Wextra
  -Werror
  -ffunction-sections
  -fdata-sections
)

if(PLUS_VITA_INPUT_WORKER)
  target_sources(${PROJECT_NAME} PRIVATE src/platform/input_worker.c)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
  PLUS_VITA_INPUT_WORKER=$<BOOL:${PLUS_VITA_INPUT_WORKER}>
  PLUS_VITA_DEFERRED_LOG=$<OR:$<BOOL:${PLUS_VITA_DEFERRED_LOG}>,$<NOT:$<BOOL:${PLUS_VITA_DIAGNOSTIC_APP}>>>
  CHIAKI_LIB_ENABLE_MBEDTLS=1
  MBEDTLS_ALLOW_PRIVATE_ACCESS=1
)

if(PLUS_VITA_TLS_PROBE_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_TLS_PROBE_URL="${PLUS_VITA_TLS_PROBE_URL}"
  )
endif()

set(PLUS_VITA_VPK_FILES)
list(APPEND PLUS_VITA_VPK_FILES FILE
  "${CMAKE_CURRENT_LIST_DIR}/cacert.pem" "cacert.pem")
list(APPEND PLUS_VITA_VPK_FILES FILE
  "${CMAKE_CURRENT_LIST_DIR}/../external/opus-1.6.1/COPYING" "licenses/opus.txt")
list(APPEND PLUS_VITA_VPK_FILES FILE
  "${PLUS_VITA_GF_ROOT}/License.txt" "licenses/gf-complete.txt")
list(APPEND PLUS_VITA_VPK_FILES FILE
  "${PLUS_VITA_JERASURE_ROOT}/License.txt" "licenses/jerasure.txt")
if(NOT PLUS_VITA_DIAGNOSTIC_APP)
  list(APPEND PLUS_VITA_VPK_FILES FILE "${CMAKE_CURRENT_LIST_DIR}/third_party/stb-image-license.txt" "licenses/stb-image.txt")
endif()
if(PLUS_VITA_TLS_TRUSTED_CA_FILE)
  if(NOT EXISTS "${PLUS_VITA_TLS_TRUSTED_CA_FILE}")
    message(FATAL_ERROR "Trusted test CA does not exist: ${PLUS_VITA_TLS_TRUSTED_CA_FILE}")
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE PLUS_VITA_TLS_TRUSTED_PROBE=1)
  list(APPEND PLUS_VITA_VPK_FILES FILE
    "${PLUS_VITA_TLS_TRUSTED_CA_FILE}" "local_probe_ca.der")
endif()
if(PLUS_VITA_PUBLIC_HTTPS_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_PUBLIC_HTTPS_URL="${PLUS_VITA_PUBLIC_HTTPS_URL}"
  )
endif()
if(PLUS_VITA_POST_ECHO_URL)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_POST_ECHO_URL="${PLUS_VITA_POST_ECHO_URL}"
  )
endif()
if(PLUS_VITA_PUBLIC_APOLLO_TEST)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUS_VITA_PUBLIC_APOLLO_TEST=1)
endif()

target_link_options(${PROJECT_NAME} PRIVATE
  -Wl,--gc-sections
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	plus_vita_jerasure
	opus
	SceAudio_stub
	json-c
  curl
  mbedtls
  mbedx509
  mbedcrypto
  z
  zstd
  pthread
  SceLibKernel_stub
  SceCtrl_stub
  SceTouch_stub
  SceDisplay_stub
  SceGxm_stub
  SceAppUtil_stub
  SceCommonDialog_stub
  SceKernelThreadMgr_stub
  SceKernelModulemgr_stub
  SceIofilemgr_stub
  SceProcessmgr_stub
  SceRtc_stub
  SceSysmem_stub
  SceNet_stub
  SceNetCtl_stub
  SceHttp_stub
  SceSsl_stub
  SceSysmodule_stub
	ScePgf_stub
	ScePvf_stub
  SceVideodec_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  ${PLUS_VITA_VPK_FILES}
)
