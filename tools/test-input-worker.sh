#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/input-worker-host
mkdir -p "$build_dir"
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
flags="-g -O1 -Wall -Wextra -Werror -fsanitize=$sanitizers -fno-omit-frame-pointer -ffunction-sections -fdata-sections"
cc $flags -Wno-error=sign-compare -Iexternal/pylux/lib/include \
  -c external/pylux/lib/src/log.c -o "$build_dir/log.o"
cc $flags -DPLUS_VITA_INPUT_WORKER=1 -pthread -Ivita-client/tests/vita-stubs -Ivita-client/src/platform -Iexternal/pylux/lib/include \
  vita-client/src/platform/input_worker.c vita-client/src/platform/input_vita.c vita-client/src/platform/latency_stats.c \
  build/vita/generated/feedbacksender_vita.c external/pylux/lib/src/thread.c \
  external/pylux/lib/src/time.c external/pylux/lib/src/controller.c \
  external/pylux/lib/src/feedback.c "$build_dir/log.o" \
  vita-client/src/platform/diagnostic_log.c vita-client/src/platform/video_decoder_vita.c \
  vita-client/tests/input_worker_test.c -lm -Wl,--wrap=pthread_create -Wl,--wrap=fflush -Wl,--gc-sections -o "$build_dir/input_worker_test"
(cd "$build_dir"; ./input_worker_test)
