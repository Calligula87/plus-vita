#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-catalogue-host
mkdir -p "$dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$dir/json_config.h"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Ivita-client/src/app -Iexternal/json-c -I"$dir" vita-client/src/app/app.c \
 vita-client/src/app/utf8.c vita-client/src/platform/credential.c vita-client/tests/app_catalogue_test.c \
 -l:libjson-c.so.5 -o "$dir/test"
"$dir/test"
