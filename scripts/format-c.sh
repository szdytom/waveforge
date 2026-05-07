#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

find_clang_format() {
    # Prefer clang-format-22
    if command -v clang-format-22 &>/dev/null; then
        echo "clang-format-22"
        return 0
    fi

    # Fallback: clang-format must be 22+
    if command -v clang-format &>/dev/null; then
        local ver
        ver="$(clang-format --version 2>/dev/null | grep -oP '\d+' | head -1)"
        if [[ -n "$ver" && "$ver" -ge 22 ]]; then
            echo "clang-format"
            return 0
        fi
    fi

    return 1
}

CLANG_FORMAT="$(find_clang_format)" || {
    echo "Error: no suitable clang-format found (need clang-format-22 or clang-format >= 22)" >&2
    exit 1
}

echo "Using: $CLANG_FORMAT"

cd "$PROJECT_DIR"

find . \
    -path ./.git -prune -o \
    -path ./build -prune -o \
    \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.c' \) \
    -print |
    xargs -r "$CLANG_FORMAT" -i -style=file

echo "Formatted all C/C++ files."
