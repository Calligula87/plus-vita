#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-controls-host
mkdir -p "$dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$dir/json_config.h"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Ivita-client/tests/vita-stubs -Ivita-client/src/app -Iexternal/json-c -I"$dir" \
 -Iexternal/pylux/lib/include vita-client/src/app/app.c vita-client/src/app/utf8.c \
 vita-client/src/platform/credential.c vita-client/src/platform/control_layout.c \
 vita-client/src/platform/input_vita.c vita-client/src/platform/controller_vita.c external/pylux/lib/src/controller.c \
 vita-client/tests/app_controls_test.c -Wl,--wrap=rename -l:libjson-c.so.5 -o "$dir/test"
(cd "$dir"; ./test)
