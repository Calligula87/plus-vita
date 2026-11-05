#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/app-preview-host
mkdir -p "$build_dir"
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$build_dir/json_config.h"
sanitizers=${PLUS_VITA_TEST_SANITIZERS:-address,undefined}
cc -D_POSIX_C_SOURCE=200809L -std=c11 -g -O1 -Wall -Wextra -Werror \
  -fsanitize="$sanitizers" -fno-omit-frame-pointer -ffunction-sections -fdata-sections -pthread \
  -Ivita-client/src/app -Ivita-client/src/platform -Ivita-client/tests/vita_font_stubs -Ivita-client/tests/cover_stubs -Iexternal/pylux/lib/include -Iexternal/pylux/lib/src \
  -Iexternal/pylux/third-party/curl/include -Iexternal/json-c -I"$build_dir" \
  vita-client/src/app/app.c vita-client/src/app/presentation.c vita-client/src/platform/control_layout.c vita-client/src/app/branding.c vita-client/src/app/utf8.c vita-client/src/app/ui_flow.c \
  vita-client/src/platform/credential.c \
  vita-client/src/app/font_vita.c vita-client/src/app/text.c vita-client/src/app/render.c vita-client/src/app/covers.c \
  vita-client/src/platform/cover_download.c vita-client/src/platform/cover_resolve.c vita-client/src/platform/cover_image.c \
  vita-client/tests/app_preview_test.c \
  -Wl,--gc-sections -l:libjson-c.so.5 -o "$build_dir/test"
timeout 20 "$build_dir/test"
