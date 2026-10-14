#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/audio-queue-host
mkdir -p "$dir"
flags="-g -O1 -Wall -Wextra -Werror -fsanitize=${PLUS_VITA_TEST_SANITIZERS:-address,undefined} -fno-omit-frame-pointer"
cc $flags -Wno-error=sign-compare -Iexternal/pylux/lib/include -c external/pylux/lib/src/log.c -o "$dir/log.o"
cc $flags -pthread -Ivita-client/tests/audio-stubs -Ivita-client/src/platform -Iexternal/pylux/lib/include \
  vita-client/src/platform/audio_playback_vita.c vita-client/tests/audio_queue_test.c "$dir/log.o" -o "$dir/test"
"$dir/test"
