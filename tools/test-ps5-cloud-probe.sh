#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/ps5-cloud-probe-host
mkdir -p "$build_dir"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  -Ivita-client/src/platform tools/ps5-cloud-probe/result_test.c \
  tools/ps5-cloud-probe/result.c vita-client/src/platform/h264_config.c \
  -o "$build_dir/result_test"
"$build_dir/result_test"
