#!/bin/sh
set -eu
cd "$(dirname "$0")"
for part in snake app render; do
  case "$part" in
    snake) sources="test_snake.c snake.c";;
    app) sources="test_app.c app.c snake.c";;
    render) sources="test_render.c render.c app.c snake.c";;
  esac
  clang-18 -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined $sources -o "/tmp/oa_snake_${part}_test"
  "/tmp/oa_snake_${part}_test"
  clang-18 --target=arm-none-eabi -mcpu=cortex-a9 -marm -std=c99 -Os -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c "$part.c" -o "$part.arm.o"
done
llvm-size-18 snake.arm.o app.arm.o render.arm.o
llvm-nm-18 -u snake.arm.o app.arm.o render.arm.o
