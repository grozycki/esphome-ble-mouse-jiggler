#!/bin/bash

set -e

# Redirect all output to stderr to prevent QEMU from capturing stdout
exec >&2

echo
echo "========================================="
echo "--- Starting QEMU run script ---"
echo "========================================="
echo

if [ -z "$1" ]; then
    echo "Error: Project name not provided."
    echo "Usage: $0 <project-name>"
    exit 1
fi

PROJECT_NAME=$1

# The build directory is in the current directory, which is the root of the /builds volume.
BUILD_DIR_PATH="./$PROJECT_NAME"

# The log clearly shows `firmware.factory.bin` is the final artifact.
FIRMWARE_PATH=$(find "$BUILD_DIR_PATH" -name "firmware.factory.bin" | head -n 1)

if [ -z "$FIRMWARE_PATH" ]; then
    echo "Error: Final firmware file (firmware.factory.bin) not found in: $BUILD_DIR_PATH"
    exit 1
fi

echo "Found firmware for '$PROJECT_NAME' at: $FIRMWARE_PATH"
echo "Starting QEMU..."

# Use if=mtd for the ESP32 machine type, as it expects a raw flash image.
qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -drive file="$FIRMWARE_PATH",format=raw \
    -d guest_errors,unimp,trace:all

