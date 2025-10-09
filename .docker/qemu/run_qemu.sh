#!/bin/bash

set -e

PROJECT_NAME="test-mouse-jiggler-arduino"

# The build directory is inside the .esphome directory
BUILD_DIR_PATH="./.esphome/build/$PROJECT_NAME"

# Find the firmware file.
FIRMWARE_PATH=$(find "$BUILD_DIR_PATH" -name "firmware.bin" | head -n 1)

if [ -z "$FIRMWARE_PATH" ]; then
    echo "Error: Firmware file (firmware.bin) not found in $BUILD_DIR_PATH"
    exit 1
fi

echo "Found firmware at: $FIRMWARE_PATH"
echo "Starting QEMU..."

# This assumes qemu-system-xtensa is in the PATH
qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -drive file="$FIRMWARE_PATH",if=pflash,format=raw
