#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-recovery-host
mkdir -p "$dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$dir/json_config.h"
source=${PLUS_VITA_CLOUD_SESSION_TEST_SOURCE:-$dir/cloudsession_vita.c}
if [ -z "${PLUS_VITA_CLOUD_SESSION_TEST_SOURCE:-}" ]; then
 cmake -DPYLUX_ROOT="$PWD/external/pylux" -DPLUS_VITA_CLOUD_SESSION_SOURCE="$PWD/$source" \
  -P vita-client/cmake/cloudsession_source.cmake
fi
test -f "$source" || { echo 'Configure the Vita build first to generate the production preflight' >&2; exit 1; }
cc -D_POSIX_C_SOURCE=200809L -DPLUS_VITA_DEFERRED_LOG=1 -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-error=sign-compare \
 -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -pthread \
 -Ivita-client/src/app -Ivita-client/src/platform -Ivita-client/tests/recovery_stubs \
 -Iexternal/pylux/lib/include -Iexternal/pylux/lib/src -Iexternal/pylux/third-party/curl/include \
 -Iexternal/json-c -I"$dir" vita-client/src/app/app.c vita-client/src/app/utf8.c \
 vita-client/src/app/play_vita.c vita-client/src/app/ui_flow.c vita-client/src/platform/credential.c \
 vita-client/src/app/attempt_log.c vita-client/src/platform/diagnostic_log.c vita-client/src/platform/latency_stats.c \
 vita-client/src/platform/recovery_vita.c vita-client/src/platform/request_cancel.c \
 "$source" external/pylux/lib/src/log.c vita-client/tests/app_recovery_test.c \
 -Wl,--gc-sections -l:libjson-c.so.5 -o "$dir/test"
(cd "$dir"; ./test)
