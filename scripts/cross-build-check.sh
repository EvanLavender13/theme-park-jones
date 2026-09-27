#!/bin/bash
# cross-build-check: the Windows and Linux builds must simulate bit-identically (decision 0022).
# Builds tpj_scenarios with linux-debug and windows-debug, runs both over the registered scenarios
# and every park file in tests/parks/, and fails on the first line where their outputs differ.
# Run from WSL at the repository root. The pre-push hook runs it when the pushed commits touch the
# simulation's build inputs.
set -uo pipefail

stage() { echo "cross-build-check: $*"; }
fail() {
    echo "cross-build-check: $*"
    exit 1
}

if [ "$(uname -s)" != "Linux" ] || ! command -v cmake.exe >/dev/null; then
    fail "run this from WSL with cmake.exe on the PATH"
fi
cd "$(git rev-parse --show-toplevel)" || exit 1

for preset in linux-debug windows-debug; do
    if [ "$preset" = windows-debug ]; then cmake=cmake.exe; else cmake=cmake; fi
    if [ ! -f "build/$preset/CMakeCache.txt" ]; then
        stage "configuring $preset"
        "$cmake" --preset "$preset" || fail "$preset failed to configure"
    fi
    stage "building tpj_scenarios with $preset"
    "$cmake" --build --preset "$preset" --target tpj_scenarios || fail "$preset failed to build"
done

shopt -s nullglob
parks=(tests/parks/*.park)
out=build/cross-build-check
mkdir -p "$out"

stage "running linux-debug over the scenarios and ${#parks[@]} park files"
build/linux-debug/tpj_scenarios "${parks[@]}" >"$out/linux-debug.txt" ||
    fail "the linux-debug run failed"
stage "running windows-debug"
build/windows-debug/tpj_scenarios.exe "${parks[@]}" | tr -d '\r' >"$out/windows-debug.txt" ||
    fail "the windows-debug run failed"

stage "comparing"
build/linux-debug/tpj_scenarios --compare "$out/linux-debug.txt" "$out/windows-debug.txt"
status=$?
if [ $status -eq 1 ]; then
    fail "the builds differ; see the first differing line above"
elif [ $status -ne 0 ]; then
    fail "the comparison failed"
fi
stage "both builds wrote the same $(wc -l <"$out/linux-debug.txt") lines; passed."
