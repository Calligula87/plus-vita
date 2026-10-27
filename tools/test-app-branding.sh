#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-branding-host
mkdir -p "$dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$dir/json_config.h"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -DPLUS_VITA_RELEASE_CANDIDATE=1 -DPLUS_VITA_APP_VERSION='"v000"' \
 -ffunction-sections -fdata-sections -pthread -Ivita-client/tests/vita_font_stubs \
 -Ivita-client/src/app -Iexternal/json-c -I"$dir" vita-client/src/app/font_vita.c \
 vita-client/src/app/text.c vita-client/src/app/utf8.c vita-client/src/app/app.c \
 vita-client/src/app/branding.c vita-client/src/app/render.c vita-client/src/app/ui_flow.c \
 vita-client/src/app/covers.c vita-client/src/platform/credential.c vita-client/tests/app_branding_test.c \
 -Wl,--gc-sections -l:libjson-c.so.5 -o "$dir/test"
(cd "$dir"; ./test)
