#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MUAUTILS_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASELINE_FILE="$MUAUTILS_ROOT/.vcpkg-ffmpeg-baseline"
VCPKG_DIR="${VCPKG_DIR:-/home/fn/vcpkg}"
PORT_PATH="ports/ffmpeg"

usage() {
    echo "Usage: $0 [--init] [--mark-synced <hash>]"
    echo ""
    echo "  (no args)              Sync upstream ffmpeg port changes into overlay port"
    echo "  --init                 Record current vcpkg HEAD as baseline without applying changes"
    echo "  --mark-synced <hash>   Update baseline to <hash> after manual conflict resolution"
    exit 0
}

require_vcpkg() {
    if [[ ! -d "$VCPKG_DIR/.git" ]]; then
        echo "error: vcpkg repo not found at $VCPKG_DIR (set VCPKG_DIR to override)"
        exit 1
    fi
}

cmd_init() {
    require_vcpkg
    local head
    head=$(git -C "$VCPKG_DIR" rev-parse HEAD)
    echo "$head" > "$BASELINE_FILE"
    echo "Baseline initialized to $head"
}

cmd_mark_synced() {
    local hash="$1"
    # Validate it's a real commit in vcpkg
    if ! git -C "$VCPKG_DIR" cat-file -e "${hash}^{commit}" 2>/dev/null; then
        echo "error: '$hash' is not a valid commit in $VCPKG_DIR"
        exit 1
    fi
    echo "$hash" > "$BASELINE_FILE"
    echo "Baseline updated to $hash"
}

cmd_sync() {
    require_vcpkg

    if [[ ! -f "$BASELINE_FILE" ]]; then
        echo "error: no baseline found. Run '$0 --init' first."
        exit 1
    fi

    local baseline
    baseline=$(cat "$BASELINE_FILE" | tr -d '[:space:]')
    local upstream_head
    upstream_head=$(git -C "$VCPKG_DIR" rev-parse HEAD)

    if [[ "$baseline" == "$upstream_head" ]]; then
        echo "Already up to date (vcpkg @ ${upstream_head:0:12})"
        exit 0
    fi

    # Check if there are any ffmpeg port changes between baseline and HEAD
    local changed_files
    changed_files=$(git -C "$VCPKG_DIR" diff --name-only "$baseline" "$upstream_head" -- "$PORT_PATH")

    if [[ -z "$changed_files" ]]; then
        echo "No ffmpeg port changes between ${baseline:0:12} and ${upstream_head:0:12}"
        echo "$upstream_head" > "$BASELINE_FILE"
        echo "Baseline fast-forwarded to $upstream_head"
        exit 0
    fi

    echo "Upstream changes in $PORT_PATH:"
    echo "$changed_files" | sed 's/^/  /'
    echo ""

    # Apply upstream diff with 3-way merge so conflicts are marked rather than rejected
    local diff_output
    diff_output=$(git -C "$VCPKG_DIR" diff "$baseline" "$upstream_head" -- "$PORT_PATH")

    # Rewrite paths: ports/ffmpeg/foo -> ports/ffmpeg/foo (already correct relative to muautils root)
    if echo "$diff_output" | git -C "$MUAUTILS_ROOT" apply --3way 2>&1; then
        echo ""
        echo "Applied cleanly. Committing..."
        git -C "$MUAUTILS_ROOT" add "$PORT_PATH"
        echo "$upstream_head" > "$BASELINE_FILE"
        git -C "$MUAUTILS_ROOT" add "$BASELINE_FILE"
        git -C "$MUAUTILS_ROOT" commit -m "chore(ports/ffmpeg): sync upstream vcpkg ${upstream_head:0:12}

Synced from microsoft/vcpkg@${upstream_head}"
        echo "Done. Synced to ${upstream_head:0:12}"
    else
        echo ""
        echo "Conflicts detected. Resolve them, then run:"
        echo "  git -C $MUAUTILS_ROOT add $PORT_PATH"
        echo "  git -C $MUAUTILS_ROOT commit"
        echo "  $0 --mark-synced $upstream_head"
        exit 1
    fi
}

case "${1:-}" in
    --init)         cmd_init ;;
    --mark-synced)  [[ -n "${2:-}" ]] || usage; cmd_mark_synced "$2" ;;
    --help|-h)      usage ;;
    "")             cmd_sync ;;
    *)              usage ;;
esac
