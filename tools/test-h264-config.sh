#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/h264-config-host
mkdir -p "$dir"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Ivita-client/src/platform vita-client/src/platform/h264_config.c \
  vita-client/tests/h264_config_test.c -o "$dir/test"
"$dir/test" "$@"
