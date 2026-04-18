#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "error: clang-format not found in PATH" >&2
    exit 1
fi

find "$ROOT/src" "$ROOT/tests" -type f \( \
    -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o \
    -name '*.c' -o -name '*.cc' -o -name '*.cxx' \
\) -print0 | sort -z | xargs -0 clang-format -i

echo "clang-format: done"
