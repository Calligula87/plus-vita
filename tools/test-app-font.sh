#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/app-font-host
mkdir -p "$build_dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$build_dir/json_config.h"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Ivita-client/tests/vita_font_stubs -Ivita-client/src/app -Iexternal/json-c -I"$build_dir" \
 vita-client/src/app/font_vita.c vita-client/src/app/text.c vita-client/src/app/utf8.c \
 vita-client/src/app/app.c vita-client/src/app/render.c vita-client/src/app/ui_flow.c vita-client/src/platform/credential.c \
 vita-client/tests/app_font_test.c -l:libjson-c.so.5 -o "$build_dir/app_font_test"
"$build_dir/app_font_test"
