#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
dir=build/debug/app-covers-host
mkdir -p "$dir"
cc -D_POSIX_C_SOURCE=200809L -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
 -Ivita-client/src/app -Ivita-client/src/platform vita-client/src/app/covers.c \
 vita-client/src/platform/cover_image.c vita-client/tests/app_covers_test.c \
 -Wl,--wrap=plus_vita_cover_download -o "$dir/test"
timeout 15 "$dir/test"
cc -D_POSIX_C_SOURCE=200809L -std=c11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
 -Ivita-client/src/platform -Ivita-client/tests/cover_stubs -Iexternal/pylux/third-party/curl/include \
 vita-client/src/platform/cover_download.c vita-client/src/platform/cover_resolve.c \
 vita-client/tests/cover_download_test.c -o "$dir/download-test"
"$dir/download-test"
