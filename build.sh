#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
compiler="${XTENSA_GCC:-xtensa-esp32s3-elf-gcc}"
runtime=.venv/bin/python
mkdir -p build firmware
"$compiler" -Os -ffreestanding -fno-builtin -fno-common -fno-stack-protector \
    -ffunction-sections -fdata-sections -mlongcalls -nostdlib \
    -Wl,--gc-sections,-Map=build/game.map -T ram.ld game.c -o build/game.elf
"$runtime" -m esptool --chip esp32s3 elf2image --ram-only-header \
    -o firmware/game.bin build/game.elf
"$runtime" load_game.py --check
