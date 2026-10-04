#!/bin/bash
# cross-build-check: every build that ships or is developed on must simulate bit-identically
# (decisions 0022 and 0036). Builds tpj_scenarios with windows-debug, the build work is tested on,
# and with windows-release and linux-release, the builds players run. Runs each over the registered
# scenarios and every park file in tests/parks/, and fails on the first line where a release
# build's output differs from windows-debug's. Run from WSL at the repository root.
# scripts/release-check.sh runs it at release (decision 0030).
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

REFERENCE=windows-debug
PRESETS=(windows-debug windows-release linux-release)

for preset in "${PRESETS[@]}"; do
    case "$preset" in windows-*) cmake=cmake.exe ;; *) cmake=cmake ;; esac
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

for preset in "${PRESETS[@]}"; do
    stage "running $preset over the scenarios and ${#parks[@]} park files"
    case "$preset" in
    windows-*)
        "build/$preset/tpj_scenarios.exe" "${parks[@]}" | tr -d '\r' >"$out/$preset.txt" ||
            fail "the $preset run failed"
        ;;
    *)
        "build/$preset/tpj_scenarios" "${parks[@]}" >"$out/$preset.txt" ||
            fail "the $preset run failed"
        ;;
    esac
done

for preset in "${PRESETS[@]}"; do
    [ "$preset" = "$REFERENCE" ] && continue
    stage "comparing $preset with $REFERENCE"
    "build/$REFERENCE/tpj_scenarios.exe" --compare "$out/$REFERENCE.txt" "$out/$preset.txt"
    status=$?
    if [ $status -eq 1 ]; then
        fail "$preset differs from $REFERENCE; see the first differing line above"
    elif [ $status -ne 0 ]; then
        fail "the comparison of $preset failed"
    fi
done
stage "all ${#PRESETS[@]} builds wrote the same $(wc -l <"$out/$REFERENCE.txt") lines; passed."
