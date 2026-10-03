#!/bin/bash
# runtime-report: times the gameplay runtime on the stress parks (src/bench/SPEC.md).
#
#   scripts/runtime-report.sh [--launches N] REPORT
#   scripts/runtime-report.sh --compare BEFORE AFTER
#
# The first builds tpj_bench with windows-release and windows-debug, launches it N times, 10 by
# default, on every park in tests/parks/stress/ with each build, in rounds, windows-debug stepping
# 300 ticks and skipping the full park, which it cannot step in reasonable time. Each round also
# launches the windows-release app on every park and keeps its frame times. It writes the report
# tpj_bench_report makes of the launches to REPORT and prints it against the budget. The second
# prints tpj_bench_report's comparison of two reports. Run from WSL. Nothing checks a time: reports
# are read, never gated on.
set -uo pipefail
export LC_ALL=C

say() { echo "runtime-report: $*" >&2; }
fail() {
    echo "runtime-report: $*" >&2
    exit 1
}
usage() {
    echo "usage: scripts/runtime-report.sh [--launches N] REPORT" >&2
    echo "       scripts/runtime-report.sh --compare BEFORE AFTER" >&2
    exit 2
}

# Configures the preset when it has no build directory yet, and builds the targets given.
build() {
    local preset=$1
    shift
    if [ ! -f "build/$preset/CMakeCache.txt" ]; then
        say "configuring $preset"
        cmake.exe --preset "$preset" >&2 || fail "$preset failed to configure"
    fi
    say "building $* with $preset"
    cmake.exe --build --preset "$preset" --target "$@" >&2 || fail "$preset failed to build"
}

if [ "$(uname -s)" != "Linux" ] || ! command -v cmake.exe >/dev/null ||
    ! command -v wslpath >/dev/null; then
    fail "run this from WSL with cmake.exe on the PATH"
fi

# Paths the user gives are made absolute before moving to the repository's root.
launches=10
if [ "${1:-}" = --compare ]; then
    [ $# -eq 3 ] || usage
    [ -f "$2" ] || fail "no report at $2"
    [ -f "$3" ] || fail "no report at $3"
    before=$(realpath -- "$2")
    after=$(realpath -- "$3")
    cd "$(git rev-parse --show-toplevel)" || exit 1
    build windows-release tpj_bench_report
    build/windows-release/tpj_bench_report.exe compare "$(wslpath -w "$before")" \
        "$(wslpath -w "$after")" | tr -d '\r'
    exit "${PIPESTATUS[0]}"
fi
if [ "${1:-}" = --launches ]; then
    [ $# -ge 2 ] || usage
    [[ "$2" =~ ^[1-9][0-9]*$ ]] || fail "--launches takes a positive count, not '$2'"
    launches=$2
    shift 2
fi
[ $# -eq 1 ] || usage
case "$1" in -*) usage ;; esac
report=$(realpath -m -- "$1")
cd "$(git rev-parse --show-toplevel)" || exit 1

start=$SECONDS
build windows-release tpj_bench tpj_bench_report tpj_app
build windows-debug tpj_bench

shopt -s nullglob
parks=(tests/parks/stress/*.park)
[ ${#parks[@]} -gt 0 ] || fail "no stress parks in tests/parks/stress/"
out=build/runtime-report
# windows-debug takes over an hour to step a minute of the full park, so only windows-release
# times it.
release_only=tests/parks/stress/full.park
# windows-debug steps fewer ticks, the same in every report, since it is reported beside release.
debug_ticks=300
# Frames of the app per launch; the first, after loading, is not timed.
app_frames=300
app_output="$out/app-output.txt"
mkdir -p "$out"
: >"$out/launches.txt"

# Rounds: each park's launches span the whole report, so drift during it widens every park's
# spread alike rather than moving one park's median.
for ((round = 1; round <= launches; round++)); do
    say "round $round of $launches"
    for preset in windows-release windows-debug; do
        for park in "${parks[@]}"; do
            if [ "$preset" = windows-debug ] && [ "$park" = "$release_only" ]; then
                continue
            fi
            ticks=()
            [ "$preset" = windows-debug ] && ticks=(--ticks "$debug_ticks")
            echo "build $preset" >>"$out/launches.txt"
            "build/$preset/tpj_bench.exe" "${ticks[@]}" "$park" | tr -d '\r' >>"$out/launches.txt" ||
                fail "tpj_bench failed on $park with $preset in round $round"
        done
    done
    for park in "${parks[@]}"; do
        build/windows-release/ThemeParkJones.exe --park "$park" --frames "$app_frames" \
            --frame-times >"$app_output" || fail "the app failed on $park in round $round"
        echo "build windows-release-app" >>"$out/launches.txt"
        build/windows-release/tpj_bench_report.exe frames "$park" "$(wslpath -w "$app_output")" |
            tr -d '\r' >>"$out/launches.txt" ||
            fail "converting the app's frames on $park in round $round failed"
    done
done

build/windows-release/tpj_bench_report.exe summarize "$out/launches.txt" | tr -d '\r' \
    >"$report.partial" || fail "summarizing the launches failed"
mv "$report.partial" "$report" || fail "cannot write $report"
build/windows-release/tpj_bench_report.exe show "$(wslpath -w "$report")" | tr -d '\r' ||
    fail "showing the report failed"
elapsed=$((SECONDS - start))
echo "runtime-report: wrote $report in $((elapsed / 60))m $(printf '%02d' $((elapsed % 60)))s"
