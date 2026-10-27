#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=clang-18
SYSROOT=/usr/aarch64-linux-gnu
# Resolve libc/pthread symbols from the native host process, without importing
# modern host GLIBC symbol versions into the older camera userspace.
$CC --target=aarch64-linux-gnu --sysroot="$SYSROOT" -O2 -shared -fPIC -nostdlib tools/rtmp_web/bridge.c -o tools/rtmp_web/oa_rtmp_bridge.so
$CC --target=aarch64-linux-gnu --sysroot="$SYSROOT" -O2 -static -DPERSISTENT_BRIDGE tools/rtmp_web/rtmp.c -o tools/rtmp_web/rtmp-firmware
$CC --target=aarch64-linux-gnu --sysroot="$SYSROOT" -O2 -static -Wall -Wextra -Werror tools/rtmp_web/network.c -o tools/rtmp_web/network
sh tools/snake/build_payload.sh
python3 tools/snake/make_loader_header.py
$CC --target=aarch64-linux-gnu --sysroot="$SYSROOT" -O2 -static -Wall -Wextra -Werror tools/snake/loader.c -o tools/snake/oa_snake
