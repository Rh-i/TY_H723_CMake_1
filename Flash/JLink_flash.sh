#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_NAME="$(basename "$PROJECT_DIR")"
BUILD_TYPE="${1:-Debug}"
BUILD_DIR="build/${BUILD_TYPE}"
INTERNAL_IMAGE="${BUILD_DIR}/${PROJECT_NAME}_internal.hex"
EXTERNAL_IMAGE="${BUILD_DIR}/${PROJECT_NAME}_usb_xip.bin"

cd "$PROJECT_DIR"

if [ ! -f "$INTERNAL_IMAGE" ]; then
    echo "Error: internal image not found at $INTERNAL_IMAGE"
    exit 1
fi
if [ ! -s "$EXTERNAL_IMAGE" ]; then
    echo "Error: external USB XIP image not found or empty at $EXTERNAL_IMAGE"
    exit 1
fi
if ! command -v openocd >/dev/null 2>&1; then
    echo "Error: openocd not found; the J-Link script uses OpenOCD's jlink adapter."
    exit 1
fi

echo "Programming internal Flash through J-Link: $INTERNAL_IMAGE"
openocd -f Flash/jlink.cfg \
    -c "program \"${INTERNAL_IMAGE}\" verify reset exit"

echo "Programming W25Q64JV USB XIP payload through J-Link: $EXTERNAL_IMAGE"
openocd -f Flash/jlink.cfg \
    -c "init" \
    -c "reset halt" \
    -c "mww 0x38003ffc 0x55535031" \
    -c "resume" \
    -c "sleep 1500" \
    -c "halt" \
    -c "flash probe stm32h7x.octospi2" \
    -c "flash write_image erase \"${EXTERNAL_IMAGE}\" 0x70110000 bin" \
    -c "verify_image \"${EXTERNAL_IMAGE}\" 0x70110000 bin" \
    -c "reset run" \
    -c "shutdown"
