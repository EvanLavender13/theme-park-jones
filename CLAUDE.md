# Theme Park Jones

A theme park game in C++20 on SDL3 and SDL_GPU. docs/vision.md says what it is for.

## Build and test

Run commands from WSL at the repository root. Windows is the target, and day-to-day work uses the windows-debug preset. It runs through cmake.exe and needs MSYS2 UCRT64 GCC and Ninja on the Windows PATH (decision 0030).

    cmake.exe --preset windows-debug
    cmake.exe --build --preset windows-debug
    build/windows-debug/ThemeParkJones.exe

While iterating, build one test executable and run it with a Catch2 filter:

    cmake.exe --build --preset windows-debug --target tpj_sim_tests
    build/windows-debug/tpj_sim_tests.exe -# "[#save_test]"

Warnings are errors in both builds.

scripts/tidy.sh runs MSYS2's clang-tidy.exe over the whole project with windows-debug's compile commands, warnings as errors. Using those compile commands means it checks the build that ships (decision 0035).

A change is done when windows-debug builds without warnings, `ctest.exe --preset windows-debug` passes, and scripts/tidy.sh is clean (decision 0030). If a change must leave the simulation unchanged, show it with `tpj_scenarios --compare` on windows-debug output from before and after.

Activate the git hooks once per clone with `git config core.hooksPath .githooks`. They format code on commit, check commit messages, reject a new PLAN.md without a Placement section, and block a push unless the windows-debug build, its tests, and tidy pass.

Linux is checked at release, not on every change. scripts/release-check.sh builds linux-debug with AddressSanitizer and UBSan and runs its tests. It also runs scripts/cross-build-check.sh, which checks that windows-release and linux-release, the builds players run, simulate exactly like windows-debug (decisions 0022 and 0036).

scripts/runtime-report.sh times the gameplay runtime on the stress parks with both Windows builds and compares two reports (src/bench/SPEC.md). Timings are reported, not gated.

To check rendering, run the app with `--capture out.bmp`. It renders a few frames, saves the last one, and exits.

## Authority

docs/principles.md is the highest authority. If a request or plan conflicts with a principle, tell Evan instead of coding around it. Principles change only through a decision record.

Decisions that are expensive to reverse are recorded in docs/decisions/ (see its README). docs/design-notes.md is provisional thinking, not a spec. The questions in docs/open-questions.md are Evan's. Ask him about them; don't decide them in code.

## Workflow

Planning and building go through the skills in .claude/skills. Start with planning-overview, which points to the others. Write commit messages with the commit-hygiene skill.

## Specs and verification

Each module has a SPEC.md next to its code. It states the module's observable behavior, in the format docs/conventions.md gives. Update it in the same change as the behavior. Implementation details go in the feature's PLAN.md and the code, not the spec. Match the process to the size of the change. A small fix doesn't need a spec edit.

docs/testing.md decides which tests exist and what a feature adds. It overrides every skill and agent on testing (decision 0037). Tests check copies, saves, hashes, and replay. Check gameplay by playing it and with --capture.

A feature's FEATURE.md lists the few tests it adds. The test-writer agent writes them in a separate pass, never the context that writes the code. After implementing, the reviewer agent checks the diff against the spec and the principles with fresh context. It reports only gaps that affect correctness or stated requirements.

Report results with evidence, such as test output, a capture, or a scenarios comparison. Don't just say it works.

## Exceptions

Anything that doesn't fit fields, flows, or encapsulation goes in docs/exceptions.md with a reason. If that list keeps growing, tell Evan. It means a principle is wrong.

## Code

Follow docs/conventions.md for naming and file layout.

## Writing

Write so the reader gets it on the first read. This applies to every document, plan, spec, skill, memory, and comment.

Use short sentences and common words. Put one idea in each sentence. If a sentence needs a second "that" or "which" clause, split it.

Write rules as instructions, not as descriptions of how things behave. Write "Fix it now", not "A plan that finds it broken fixes it." In a spec, describing behavior is right, but keep the sentences short.

Don't write mannered prose. Mannered prose uses metaphor and flourish instead of a plain statement: "a dial worth turning" instead of "a parameter worth varying", "earns its keep" instead of "still matters", "the medium a module speaks" instead of "the fields and flows a module uses". If a literal phrase exists, use it.

Say the rule, then stop. Don't list every edge case, and don't end sentences with "never X" or "not Y" unless the reader would otherwise get it wrong.

Use no bold emphasis. Use bullets for lists of separate items.

Here is a rule written badly:

    Code that runs every tick or every frame, and anything it calls, does not allocate inside its loops. A list a loop needs is made once, before the loop or by the caller, reused with clear() for each item, and grown only while it is smaller than the largest item so far. A plan that finds this broken in code it touches fixes it in the same feature, never defers it.

The same rule written well:

    Don't allocate inside loops in per-tick or per-frame code. Allocate buffers once and reuse them. If you find an allocation in such a loop, fix it now; don't defer it.

Anything a machine can check belongs in a test, hook, or check, not in a document.
