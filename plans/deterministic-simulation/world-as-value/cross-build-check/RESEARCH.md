# Research: cross-build-check

## How does a WSL script build and run the Windows side?

cmake.exe accepts a build preset and a target together, so cmake.exe --build --preset windows-debug --target tpj_scenarios builds the runner and what it links without building the app, and the same form works for linux-debug. Configuring windows-debug still fetches the app's packages, but the CPM cache in .cpm-cache already holds them. The top-level CMakeLists.txt puts every executable in the build directory, so the Windows runner is build/windows-debug/tpj_scenarios.exe, and tpj_configure_target links Windows executables statically, so it runs outside an MSYS2 shell.

WSL interop starts a Windows executable in the current directory, translated to its Windows path, when that directory is on a Windows drive. Relative paths with forward slashes, such as tests/parks/fed.park, then mean the same file to both runners, and a line naming a park file is spelled identically on both sides. A MinGW program's standard output is in text mode, so each line feed arrives in WSL as a carriage return and line feed, which a quick check through cmd.exe confirmed. The script strips carriage returns from the Windows output before comparing. A pipeline hides the runner's exit status, so the script sets pipefail.

Rejected: switching the runner's standard output to binary mode, since it needs Windows-only code for what one tr in the script does. Building all of windows-debug, since the app's shaders and SDL add minutes that the check does not need.

Sources: https://cmake.org/cmake/help/latest/manual/cmake.1.html#build-a-project — --preset with --target; https://learn.microsoft.com/en-us/windows/wsl/filesystems — running Windows executables from WSL, and the working directory they get.

## How should the two outputs be compared so that a difference names its scenario and tick?

When every line of the runner's output names its own source and tick, the first differing line is the whole report: it says which scenario or park file diverged, at which tick, and what each build computed. Only the first difference matters, since a divergent world stays divergent.

diff prints hunks rather than one first difference, cmp reports a byte offset, and an awk script would need bash to test on the Windows preset. Putting the comparison in the scenarios library, behind a --compare mode of the runner, makes it a C++ function the test suite checks on both presets, and the script calls the Linux runner to compare. The comparison has to treat a missing line as a difference, or a runner that stopped early would pass.

Rejected: comparing whole-output hashes, since they say that the outputs differ but not where. Letting the comparison ignore carriage returns, since the Linux side never writes one, and the script already strips them from the Windows side.

## How does pre-push decide whether the pushed commits touch the simulation's build inputs?

git feeds pre-push one line per ref: local ref, local sha, remote ref, remote sha. An all-zero local sha is a deletion and pushes nothing. An all-zero remote sha is a new branch, whose new commits are those of the local sha not on any remote ref (git log local --not --remotes). Otherwise they are remote..local. git log --format= --name-only over that range lists every file the commits touch. If git log fails, as when a force push replaces commits this clone has never fetched, the hook runs the check rather than guess. The hook must read standard input before anything else can consume it.

tpj_scenarios's output depends on everything its build reads: src/sim, src/core, src/scenarios, cmake/, the top-level CMakeLists.txt and CMakePresets.json, the park files, the script itself, and tests/sim/support/, whose generated tables supply the draw keys and exp and log arguments.

Rejected: running the check on every push, since the Windows build adds minutes to pushes that touch only plans or rendering. Judging the working tree, since the hook must judge what is pushed.

Sources: https://git-scm.com/docs/githooks#_pre_push — the pre-push input format.

## Do the scenarios' own systems need the simulation's floating-point flags?

The synthetic scenarios' systems compute in the scenarios library, not in tpj_sim, so tpj_sim's -ffp-contract=off does not reach them. GCC defaults to -ffp-contract=off only for C in a strict standard mode, and to fast otherwise, including C++20. Without -mfma, x86-64 has no fused instruction to contract into, so today the flag changes nothing, but the library gets it for the same reason tpj_sim does. The same holds for the symbol check: a scenario system calling std::exp would compare equal across builds only by luck, so the check runs on the scenarios library too.

Timing is the one input that must never reach the compared output. Reading the clock only in the runner's main, around each run, keeps the library that the tests link free of clocks.

Sources: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html — -ffp-contract's defaults.
