#!/bin/sh
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${BUILD_DIR:-cmake-build-vcpkg}"
TARGET="${TARGET:-mua}"
CONFIG="${CMAKE_BUILD_TYPE:-Release}"

case "$BUILD_DIR" in
    /*) BUILD_DIR_ABS="$BUILD_DIR" ;;
    *) BUILD_DIR_ABS="${SOURCE_DIR}/${BUILD_DIR}" ;;
esac

OUTPUT_DIR="${BUILD_DIR_ABS}/${CONFIG}"

if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "error: VCPKG_ROOT is not set (point it at your vcpkg clone)" >&2
    exit 1
fi

TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "error: toolchain file not found: $TOOLCHAIN" >&2
    exit 1
fi

run_checked() {
    printf '%s\n' "-> $*"
    "$@"
}

echo ""
echo "==> Configuring & building $TARGET (build dir: $BUILD_DIR_ABS, config: $CONFIG)"
run_checked cmake -S "$SOURCE_DIR" -B "$BUILD_DIR_ABS" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$OUTPUT_DIR" \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$OUTPUT_DIR" \
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY="$OUTPUT_DIR" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE="$OUTPUT_DIR" \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE="$OUTPUT_DIR" \
    -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE="$OUTPUT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
run_checked cmake --build "$BUILD_DIR_ABS" --config "$CONFIG" --target "$TARGET"

echo ""
echo "==> Done."
