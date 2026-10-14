#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/latency-host
mkdir -p "$build_dir"
# Use the exact generated source compiled into the Vita package. Configure/
# build vita-client first; never instrument or edit the Pylux submodule.
feedback_source=${PLUS_VITA_FEEDBACK_TEST_SOURCE:-build/vita/generated/feedbacksender_vita.c}
test -f "$feedback_source" || { echo "Configure the Vita build to generate the feedback sender first" >&2; exit 1; }
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
flags="-g -O1 -Wall -Wextra -Werror -fsanitize=$sanitizers -fno-omit-frame-pointer"
cc $flags -Wno-error=sign-compare -Iexternal/pylux/lib/include \
  -c external/pylux/lib/src/log.c -o "$build_dir/log.o"
cc $flags -Ivita-client/src/platform \
  vita-client/src/platform/latency_stats.c vita-client/tests/latency_stats_test.c \
  -o "$build_dir/latency_stats_test"
"$build_dir/latency_stats_test"
cc $flags -pthread -Ivita-client/src/platform -Iexternal/pylux/lib/include \
  vita-client/src/platform/latency_stats.c vita-client/src/platform/diagnostic_log.c \
  vita-client/tests/diagnostic_latency_test.c -o "$build_dir/diagnostic_latency_test"
# The synthetic diagnostic file belongs in ignored build output, not repo root.
(cd "$build_dir"; ./diagnostic_latency_test)
for mode in upstream instrumented; do
  source=external/pylux/lib/src/feedbacksender.c
  define=
  if [ "$mode" = instrumented ]; then
    source=$feedback_source
    define=-DPLUS_VITA_INSTRUMENTED=1
  fi
  cc $flags $define -pthread -Ivita-client/src/platform -Iexternal/pylux/lib/include \
    "$source" external/pylux/lib/src/thread.c external/pylux/lib/src/time.c \
    external/pylux/lib/src/controller.c external/pylux/lib/src/feedback.c \
    "$build_dir/log.o" vita-client/src/platform/latency_stats.c \
    vita-client/tests/feedback_latency_test.c -lm -o "$build_dir/feedback_$mode"
  "$build_dir/feedback_$mode" > "$build_dir/feedback_$mode.txt"
done
cmp "$build_dir/feedback_upstream.txt" "$build_dir/feedback_instrumented.txt"
echo "feedback wire output matches upstream; instrumented timing assertions passed"
