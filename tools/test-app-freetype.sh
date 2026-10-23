#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-freetype-host
mkdir -p "$dir"
# Build the same pinned source used by Vita, without relying on host font libs.
lib="$dir/libfreetype.a"
if [ ! -f "$lib" ] || [ "$0" -nt "$lib" ] || [ external/freetype/CMakeLists.txt -nt "$lib" ] \
 || [ -n "$(find external/freetype/src external/freetype/include -type f -newer "$lib" -print -quit 2>/dev/null)" ]; then
 awk '{sub(/\r$/, "")} /^set\(BASE_SRCS$/ {copy=1;next} copy && /^\)/ {exit} copy {print}' external/freetype/CMakeLists.txt > "$dir/sources"
 printf '%s\n' src/base/ftsystem.c src/base/ftdebug.c >> "$dir/sources"
 objects=
 while read -r source; do
  [ -n "$source" ] || continue
  cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DFT2_BUILD_LIBRARY \
   -Iexternal/freetype/include -c "external/freetype/$source" -o "$dir/$(basename "$source" .c).o"
  objects="$objects $dir/$(basename "$source" .c).o"
 done < "$dir/sources"
 ar rcs "$lib.tmp" $objects
 mv "$lib.tmp" "$lib"
fi
printf '#define JSON_C_HAVE_INTTYPES_H 1\n#define JSON_C_HAVE_STDINT_H 1\n' > "$dir/json_config.h"
cc -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
 -ffunction-sections -fdata-sections -DPLUS_VITA_FONT_COMPARISON=1 \
 -Ivita-client/src/app -Ivita-client/tests/vita_font_stubs -Iexternal/json-c -I"$dir" -Iexternal/freetype/include \
 vita-client/src/app/font_freetype.c vita-client/src/app/font_vita.c vita-client/src/app/app.c \
 vita-client/src/app/render.c vita-client/src/app/covers.c vita-client/src/app/comparison_capture.c \
 vita-client/src/platform/credential.c vita-client/src/app/text.c vita-client/src/app/utf8.c \
 vita-client/tests/app_freetype_test.c "$lib" -lm -l:libjson-c.so.5 -Wl,--gc-sections -o "$dir/test"
"$dir/test"
