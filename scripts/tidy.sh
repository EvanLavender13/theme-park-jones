#!/bin/bash
# Runs clang-tidy.exe, warnings as errors, over every project source file in windows-debug's
# compile database, the build that ships, 16 runs at a time (decision 0035). Headers are checked
# through the sources that include them, as .clang-tidy's HeaderFilterRegex allows. Run it once
# work is done, before review and push; the pre-push hook runs it. Prints only diagnostics, and
# exits nonzero on any.

cd "$(git rev-parse --show-toplevel)" || exit 1

if ! command -v clang-tidy.exe >/dev/null; then
    echo "tidy: clang-tidy.exe is not on the PATH; install MSYS2's clang-tools-extra."
    exit 1
fi

# The database lists the sources CMake knew at windows-debug's last configure. Sources enter and
# leave only through CMake files, and a build reconfigures when one changed, but tidy may run before
# that build, so configure here when a CMake file is newer than the database.
DATABASE=build/windows-debug/compile_commands.json
if [ ! -f "$DATABASE" ] || [ -n "$(find src tests cmake CMakeLists.txt CMakePresets.json \
    \( -name CMakeLists.txt -o -name '*.cmake' -o -name CMakePresets.json \) \
    -newer "$DATABASE" -print -quit)" ]; then
    echo "tidy: configuring windows-debug..."
    cmake.exe --preset windows-debug >/dev/null 2>&1 || { echo "tidy: configuring failed."; exit 1; }
    touch "$DATABASE"
fi

# Project sources only: dependencies are built from .cpm-cache and build/.
# The database names files by Windows path, such as C:/Users/.../src/app/main.cpp.
SOURCES=$(python3 -c '
import json, sys
root = sys.argv[2].rstrip("/") + "/"
for entry in json.load(open(sys.argv[1])):
    path = entry["file"].replace("\\", "/")
    if path.lower().startswith(root.lower()) and path.endswith(".cpp"):
        relative = path[len(root):]
        if relative.startswith(("src/", "tests/")):
            print(relative)
' "$DATABASE" "$(wslpath -m "$(pwd)")" | sort -u)
if [ -z "$SOURCES" ]; then
    echo "tidy: no project sources in $DATABASE."
    exit 1
fi

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT
echo "$SOURCES" | xargs -P 16 -n 4 clang-tidy.exe -p build/windows-debug -quiet \
    --warnings-as-errors='*' -extra-arg=-Wno-unknown-warning-option 2>&1 | tr -d '\r' > "$LOG"
STATUS=${PIPESTATUS[1]}

DIAGNOSTICS=$(grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" "$LOG" | sort -u)
if [ "$STATUS" -ne 0 ] || [ -n "$DIAGNOSTICS" ]; then
    if [ -n "$DIAGNOSTICS" ]; then
        echo "$DIAGNOSTICS"
    else
        tail -20 "$LOG"
    fi
    echo "tidy: clang-tidy found problems."
    exit 1
fi
echo "tidy: clean."
