# Theme Park Jones

A theme park game in C++20 on SDL3 and SDL_GPU. docs/vision.md says what it is for.

## Build and test

Commands run from WSL at the repository root. Windows is the target, and work iterates on windows-debug: it uses cmake.exe and needs MSYS2 UCRT64 GCC and Ninja on the Windows PATH (decision 0030).

    cmake.exe --preset windows-debug
    cmake.exe --build --preset windows-debug
    build/windows-debug/ThemeParkJones.exe

While iterating, build one test executable with `--target` and run it directly with a Catch2 filter, such as `cmake.exe --build --preset windows-debug --target tpj_sim_tests` and `build/windows-debug/tpj_sim_tests.exe -# "[#edits_test]"` for one file's tests. Warnings are errors in both builds.

scripts/tidy.sh runs clang-tidy over the whole project, warnings as errors. It reads linux-debug's compile commands, and configures linux-debug itself whenever a source or CMake file may have been added or removed since; linux-debug never needs building.

A change is finished when windows-debug builds without warnings, its tests pass (`ctest.exe --preset windows-debug`), and scripts/tidy.sh is clean (decision 0030). Work that must leave the simulation unchanged shows it by comparing tpj_scenarios's windows-debug output before and after with `tpj_scenarios --compare`. Git hooks format on commit, refuse a new PLAN.md with no Placement section, check commit messages, and gate pushes on the windows-debug build, its tests, and tidy; activate them once per clone with `git config core.hooksPath .githooks`. Linux is checked at release, not per change: scripts/release-check.sh builds linux-debug with AddressSanitizer and UBSan, runs its tests, and runs scripts/cross-build-check.sh, which compares the Windows and Linux builds' simulation outputs (decision 0022). scripts/runtime-report.sh times the gameplay runtime on the stress parks with both Windows builds, and compares two reports (src/bench/SPEC.md); timings are reported, never gated. To check rendering, run the app with --capture out.bmp; it renders a few frames, saves the last one, and exits.

## Authority

docs/principles.md is the highest authority. When a request or plan conflicts with a principle, surface the conflict to Evan instead of coding around it. Principles change only through a decision record.

Decisions that are expensive to reverse are recorded in docs/decisions/; see its README. docs/design-notes.md is provisional thinking, not a spec. Questions in docs/open-questions.md are Evan's to decide: ask about them, do not decide them in code.

## Workflow

Planning and building go through the skills in .claude/skills; start at planning-overview, which routes to the rest. Commit messages follow the commit-hygiene skill.

## Specs and verification

Each module keeps a SPEC.md beside its code, updated in the same change as the code it describes. Scale ceremony to the change: a small fix needs no spec edit.

docs/testing.md says what a test is, and outranks every skill and agent on it (decision 0032). Tests are written in a separate pass from the implementation, by the test-writer agent, never by the context that writes the code. After implementing, the reviewer agent checks the diff against the spec and the principles with fresh context and reports only gaps that affect correctness or stated requirements. Report results with evidence such as test output, never as self-assessment.

Anything mechanically checkable belongs in a test, hook, or CI check, not in a document.

## Exceptions

Anything that does not fit fields, flows, or encapsulation is listed in docs/exceptions.md with a reason. If the list keeps growing, raise it with Evan: it means a principle is wrong.

## Code

Naming and file layout follow docs/conventions.md.

## Writing

Documents use plain prose, no bold emphasis, and bullets only for genuinely distinct items.
