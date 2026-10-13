#!/bin/sh
# Run the actual scalar backend and video wrapper with host memory/UB checks.
set -eu
cd "$(dirname "$0")/.."
build_dir=build/debug/video-fec-host
mkdir -p "$build_dir"
gf_root=external/pylux/third-party/gf-complete
jerasure_root=external/pylux/third-party/jerasure
includes="-Ivita-client/src/platform -Iexternal/pylux/lib/include -Iexternal/pylux/test -I$gf_root/include -I$jerasure_root/include"
for source in "$gf_root"/src/gf.c "$gf_root"/src/gf_wgen.c \
  "$gf_root"/src/gf_w4.c "$gf_root"/src/gf_w8.c "$gf_root"/src/gf_w16.c \
  "$gf_root"/src/gf_w32.c "$gf_root"/src/gf_w64.c "$gf_root"/src/gf_w128.c \
  "$gf_root"/src/gf_rand.c "$gf_root"/src/gf_general.c "$gf_root"/src/gf_cpu.c \
  "$jerasure_root"/src/galois.c "$jerasure_root"/src/jerasure.c \
  "$jerasure_root"/src/reed_sol.c "$jerasure_root"/src/cauchy.c \
  "$jerasure_root"/src/liberation.c external/pylux/lib/src/fec.c \
  external/pylux/lib/src/base64.c
do
  cc -g -O1 -fsanitize=address -fno-omit-frame-pointer $includes \
    -c "$source" -o "$build_dir/$(basename "$source" .c).o"
done
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer $includes -c vita-client/src/platform/video_fec.c \
  -o "$build_dir/video_fec.o"
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer $includes -c vita-client/src/platform/video_reorder.c \
  -o "$build_dir/video_reorder.o"
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer $includes -c vita-client/tests/video_fec_test.c \
  -o "$build_dir/video_fec_test.o"
cc -fsanitize=address,undefined "$build_dir"/*.o -o "$build_dir/video_fec_test"
"$build_dir/video_fec_test"
