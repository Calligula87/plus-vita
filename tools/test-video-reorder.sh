#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/video-reorder-host
mkdir -p "$build_dir"
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Ivita-client/src/platform \
  vita-client/src/platform/video_reorder.c vita-client/tests/video_reorder_test.c \
  -o "$build_dir/video_reorder_test"
"$build_dir/video_reorder_test"
