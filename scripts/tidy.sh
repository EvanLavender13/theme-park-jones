#!/bin/bash
# Runs clang-tidy, warnings as errors, over every project source file in linux-debug's compile
# database, 16 files at a time (decision 0030). Headers are checked through the sources that
# include them, as .clang-tidy's HeaderFilterRegex allows. Run it once work is done, before review
# and push; the pre-push hook runs it. Prints only diagnostics, and exits nonzero on any.

cd "$(git rev-parse --show-toplevel)" || exit 1

# The compile database lists the sources as of linux-debug's last configure, so a source added since
# would go unchecked and one deleted since would fail the run. Configure again when the database is
# missing, or when a CMake file, or a source directory's list of files, changed after it was made.
DATABASE=build/linux-debug/compile_commands.json
if [ ! -f "$DATABASE" ] || [ -n "$(find src tests cmake CMakeLists.txt CMakePresets.json \
    \( -type d -o -name CMakeLists.txt -o -name '*.cmake' -o -name CMakePresets.json \) \
    -newer "$DATABASE" -print -quit)" ]; then
    echo "tidy: configuring linux-debug..."
    cmake --preset linux-debug >/dev/null || { echo "tidy: configuring linux-debug failed."; exit 1; }
    touch "$DATABASE"
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
