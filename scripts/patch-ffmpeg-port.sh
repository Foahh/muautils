#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUAUTILS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OVERLAY_DIR="$SCRIPT_DIR/ffmpeg-port-overlay"
SRC_PORT="ports/ffmpeg"

resolve_vcpkg_dir() {
    if [[ -n "${VCPKG_DIR:-}" ]]; then
        return
    fi
    if [[ -n "${VCPKG_ROOT:-}" ]]; then
        VCPKG_DIR="$VCPKG_ROOT"
        return
    fi
    echo "error: set VCPKG_DIR or VCPKG_ROOT to your local microsoft/vcpkg clone"
    exit 1
}

usage() {
    echo "Usage: $0 [--help|-h]"
    echo ""
    echo "  Clean ports/ffmpeg, copy ${SRC_PORT} from the vcpkg checkout, then apply"
    echo "  patches from ${OVERLAY_DIR}/ (see muautils-ffmpeg.patch)."
    echo ""
    echo "  Environment:"
    echo "    VCPKG_DIR or VCPKG_ROOT  Path to the vcpkg repository root (required)"
    exit 0
}

require_upstream_port() {
    if [[ ! -d "$VCPKG_DIR/.git" ]]; then
        echo "error: vcpkg repo not found at $VCPKG_DIR (set VCPKG_DIR or VCPKG_ROOT)"
        exit 1
    fi
    if [[ ! -d "$VCPKG_DIR/$SRC_PORT" ]]; then
        echo "error: upstream port missing: $VCPKG_DIR/$SRC_PORT"
        exit 1
    fi
}

cmd_refresh() {
    resolve_vcpkg_dir
    require_upstream_port

    shopt -s nullglob
    local patches=( "$OVERLAY_DIR"/*.patch )
    shopt -u nullglob
    if [[ ${#patches[@]} -eq 0 ]]; then
        echo "error: no *.patch files in $OVERLAY_DIR"
        exit 1
    fi

    echo "Removing $MUAUTILS_ROOT/$SRC_PORT"
    rm -rf "$MUAUTILS_ROOT/$SRC_PORT"

    echo "Copying from $VCPKG_DIR/$SRC_PORT"
    cp -a "$VCPKG_DIR/$SRC_PORT" "$MUAUTILS_ROOT/$SRC_PORT"

    echo "Applying overlay patch(es)"
    for p in "${patches[@]}"; do
        echo "  $(basename "$p")"
        git -C "$MUAUTILS_ROOT" apply "$p"
    done

    echo "Done. Review changes under $SRC_PORT and commit when satisfied."
}

case "${1:-}" in
    --help|-h)  usage ;;
    "")          cmd_refresh ;;
    *)           usage ;;
esac
