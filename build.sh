#!/bin/sh
set -eu

BUILD_DIR="${BUILD_DIR:-cmake-build-vcpkg}"
TARGET="${TARGET:-mua}"

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
echo "==> Configuring & building $TARGET (build dir: $BUILD_DIR)"
run_checked cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
run_checked cmake --build "$BUILD_DIR" --target "$TARGET"

echo ""
echo "==> Done."
