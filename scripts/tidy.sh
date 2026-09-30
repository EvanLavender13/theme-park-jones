#!/bin/bash
# Runs clang-tidy, warnings as errors, over every project source file in linux-debug's compile
# database, 16 files at a time (decision 0026). Headers are checked through the sources that
# include them, as .clang-tidy's HeaderFilterRegex allows. Run it once work is done, before review
# and push; the pre-push hook runs it. Prints only diagnostics, and exits nonzero on any.

cd "$(git rev-parse --show-toplevel)" || exit 1

if [ ! -f build/linux-debug/compile_commands.json ]; then
    echo "tidy: configure linux-debug first (cmake --preset linux-debug)."
    exit 1
fi

# Project sources only: dependencies are built from .cpm-cache and build/.
ROOT=$(pwd)
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
run-clang-tidy -p build/linux-debug -j 16 -quiet -warnings-as-errors='*' \
    -extra-arg=-Wno-unknown-warning-option "^$ROOT/(src|tests)/.*\.cpp$" > "$LOG" 2>&1
STATUS=$?

DIAGNOSTICS=$(grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" "$LOG" | sort -u)
if [ $STATUS -ne 0 ] || [ -n "$DIAGNOSTICS" ]; then
    if [ -n "$DIAGNOSTICS" ]; then
        echo "$DIAGNOSTICS"
    else
        tail -20 "$LOG"
    fi
    echo "tidy: clang-tidy found problems."
    exit 1
fi
echo "tidy: clean."
