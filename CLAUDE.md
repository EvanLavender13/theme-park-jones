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

scripts/runtime-report.sh times the gameplay runtime on the stress parks with windows-release and compares two reports (src/bench/SPEC.md). Timings are reported, not gated.

To check rendering, run the app with `--capture out.bmp`. It renders a few frames, saves the last one, and exits.

## Authority

docs/principles.md is the highest authority. If a request or plan conflicts with a principle, tell the user instead of coding around it. Principles change only through a decision record.

Decisions that are expensive to reverse are recorded in docs/decisions/ (see its README). docs/design-notes.md is provisional thinking, not a spec. The questions in docs/open-questions.md are the user's to decide. Ask about them; don't decide them in code.

## Workflow

Planning and building go through the skills in .claude/skills. Start with planning-overview, which points to the others. Write commit messages with the commit-hygiene skill.

## Specs and verification

Each module has a SPEC.md next to its code. It states the module's observable behavior, in the format docs/conventions.md gives. Update it in the same change as the behavior. Implementation details go in the feature's PLAN.md and the code, not the spec. Match the process to the size of the change. A small fix doesn't need a spec edit.

docs/testing.md decides which tests exist and what a feature adds. It overrides every skill and agent on testing (decision 0037). Tests check copies, saves, hashes, and replay. Check gameplay by playing it and with --capture.

A feature's FEATURE.md lists the few tests it adds. The test-writer agent writes them in a separate pass, never the context that writes the code. After implementing, the reviewer agent checks the diff against the spec and the principles with fresh context. It reports only gaps that affect correctness or stated requirements.

Report results with evidence, such as test output, a capture, or a scenarios comparison. Don't just say it works.

## Exceptions

If something doesn't fit fields, flows, or encapsulation, list it in docs/exceptions.md with a reason. If that list keeps growing, tell the user, because it means a principle is wrong.

## Code

Follow docs/conventions.md for naming and file layout.

## Writing

Write so the reader understands on the first read. This applies to every document, plan, spec, skill, memory, and comment. Use short sentences and common words, with one idea in each sentence.

The most common problem here is a rule written as a fact about an imaginary thing: "A plan that finds this broken in code it touches fixes it, never defers it." It has four signs:

- The subject is a generic thing, such as "a plan", "a test", "a caller", or "a result", instead of the reader who has to act.
- A clause is attached to that subject, so the reader holds a long phrase before reaching the verb: "a result that a caller reads one number from", "state a feature registers".
- The verb is plain present tense, such as "fixes", "is not built", or "never depends", so a command reads like a description.
- A "never" or "not" tail restates the opposite.

Fix it by putting the condition in an "if" or "when" clause and stating the action directly. "A plan that finds this broken fixes it" becomes "If you find this broken, fix it." Specs describe behavior, so they keep the present tense, but they take the same fix: "A place that does not resolve gives nothing" becomes "If a place doesn't resolve, sampling it gives nothing."

When you split a long sentence, rewrite both halves. Don't start the second half with "So" or "That is".

Don't write mannered prose, which uses metaphor and flourish instead of a plain statement. Write "still matters", not "earns its keep". Write "the fields and flows a module uses", not "the medium a module speaks". If a literal phrase exists, use it.

Use no bold emphasis. Use bullets for lists of separate items. Call the person who directs the project "the user", not by name.

Here is a rule written badly:

    Code that runs every tick or every frame, and anything it calls, does not allocate inside its loops. A list a loop needs is made once, before the loop or by the caller, reused with clear() for each item, and grown only while it is smaller than the largest item so far. A plan that finds this broken in code it touches fixes it in the same feature, never defers it.

The same rule written well:

    Don't allocate inside loops in per-tick or per-frame code. Allocate buffers once and reuse them. If you find an allocation in such a loop, fix it now; don't defer it.

If a machine can check a rule, put the rule in a test, hook, or check instead of a document.
