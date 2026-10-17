#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/app-account-integration-host
mkdir -p "$build_dir"
# Public json-c headers need only these standard-integer feature declarations.
# Link the installed ABI-5 runtime; no Vita configuration or real account files.
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$build_dir/json_config.h"
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
cc -D_POSIX_C_SOURCE=200809L -std=c11 -g -O1 -Wall -Wextra -Werror \
  -fsanitize="$sanitizers" -fno-omit-frame-pointer -ffunction-sections -fdata-sections -pthread \
  -Ivita-client/src/app -Ivita-client/src/platform -Iexternal/pylux/lib/include -Iexternal/pylux/lib/src \
  -Iexternal/pylux/third-party/curl/include -Iexternal/json-c -I"$build_dir" \
  vita-client/src/app/app.c vita-client/src/app/utf8.c vita-client/src/app/account_vita.c \
  vita-client/src/platform/credential.c vita-client/src/platform/auth_probe.c \
  vita-client/src/platform/request_cancel.c vita-client/src/platform/random.c \
  external/pylux/lib/src/curl_http.c vita-client/tests/app_account_integration_test.c \
  -Wl,--wrap=cc_http_perform,--gc-sections -l:libjson-c.so.5 -o "$build_dir/app_account_integration_test"
run_dir=$(mktemp -d "$build_dir/run.XXXXXX")
(cd "$run_dir"; ../app_account_integration_test)
rmdir "$run_dir"
