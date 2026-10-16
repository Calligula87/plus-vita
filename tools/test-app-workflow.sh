#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/app-workflow-host
mkdir -p "$build_dir"
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
cc -std=c11 -g -O1 -Wall -Wextra -Werror \
  -fsanitize="$sanitizers" -fno-omit-frame-pointer \
  -Ivita-client/src/app vita-client/src/app/app.c \
  vita-client/tests/app_workflow_test.c -o "$build_dir/app_workflow_test"
"$build_dir/app_workflow_test"
