#!/bin/bash
# release-check: the checks made before a release rather than on every change (decision 0030).
# Builds linux-debug with AddressSanitizer and UBSan, warnings as errors, runs its tests, and runs
# the cross-build check, which compares windows-debug with the windows-release and linux-release
# builds players run (decisions 0022 and 0036). Run from WSL at the repository root.
set -uo pipefail

say() { echo "release-check: $*"; }
fail() {
    echo "release-check: $*" >&2
    exit 1
}

[ "$(uname -s)" = "Linux" ] || fail "run this from WSL"
cd "$(git rev-parse --show-toplevel)" || exit 1

if [ ! -f build/linux-debug/CMakeCache.txt ]; then
    say "configuring linux-debug"
    cmake --preset linux-debug || fail "linux-debug failed to configure"
fi

say "building linux-debug (sanitizers); this takes a while"
BUILD_LOG=$(mktemp)
trap 'rm -f "$BUILD_LOG"' EXIT
cmake --build --preset linux-debug 2>&1 | tee "$BUILD_LOG"
BUILD_STATUS=${PIPESTATUS[0]}
# Compiler diagnostics only (file:line:column:), not make's own messages such as clock-skew
# notices on the Windows drive, and not dependency code.
WARNINGS=$(grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" "$BUILD_LOG" | grep -v "/.cpm-cache/" || true)
if [ "$BUILD_STATUS" -ne 0 ] || [ -n "$WARNINGS" ]; then
    echo "$WARNINGS"
    fail "linux-debug build failed or has warnings"
fi

say "running linux-debug tests"
ctest --preset linux-debug || fail "linux-debug tests failed"

say "running the cross-build check"
scripts/cross-build-check.sh || fail "the cross-build check failed"

say "passed"
