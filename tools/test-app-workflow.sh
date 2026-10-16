#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/app-workflow-host
mkdir -p "$build_dir"
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
for test in app_workflow_test app_account_test; do
  cc -std=c11 -g -O1 -Wall -Wextra -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer \
    -Ivita-client/src/app vita-client/src/app/app.c \
    vita-client/src/platform/credential.c \
    "vita-client/tests/$test.c" -o "$build_dir/$test"
  "$build_dir/$test"
done
