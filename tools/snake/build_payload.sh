#!/bin/sh
set -eu
cd "$(dirname "$0")"
for part in snake app render rtos_adapter; do
  opt=-Oz
  clang-18 --target=arm-none-eabi -mcpu=cortex-a9 -marm -std=c99 "$opt" -flto -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c "$part.c" -o "$part.lto.o"
done
clang-18 --target=arm-none-eabi -mcpu=cortex-a9 -marm -c div.S -o div.arm.o
ld.lld-18 -T payload.ld snake.lto.o app.lto.o render.lto.o rtos_adapter.lto.o div.arm.o --export-dynamic-symbol=oa_hit --export-dynamic-symbol=oa_move --export-dynamic-symbol=oa_timers --export-dynamic-symbol=oa_context_size -o payload.elf
llvm-objcopy-18 -O binary --only-section=.text payload.elf payload.bin
llvm-objcopy-18 -O binary --only-section=.rodata payload.elf payload.labels.bin
llvm-size-18 payload.elf
llvm-nm-18 -u payload.elf
