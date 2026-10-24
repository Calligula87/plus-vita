#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/deferred-log-host
mkdir -p "$dir"
flags="-g -O1 -Wall -Wextra -Werror -fsanitize=${PLUS_VITA_TEST_SANITIZERS:-address,undefined} -fno-omit-frame-pointer"
cc $flags -DPLUS_VITA_DEFERRED_LOG=1 -pthread -Ivita-client/src/platform -Iexternal/pylux/lib/include \
  vita-client/src/platform/latency_stats.c vita-client/src/platform/diagnostic_log.c \
  vita-client/tests/deferred_log_test.c -Wl,--wrap=fflush -Wl,--wrap=pthread_create -Wl,--wrap=fopen -o "$dir/test"
(cd "$dir"; ./test)
