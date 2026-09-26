# Theme Park Jones

A theme park game in C++20 on SDL3 and SDL_GPU. docs/vision.md says what it is for.

## Build and test

Commands run from WSL at the repository root. The Windows build is for playing: it uses cmake.exe and needs MSYS2 UCRT64 GCC and Ninja on the Windows PATH.

    cmake.exe --preset windows-debug
    cmake.exe --build --preset windows-debug
    build/windows-debug/ThemeParkJones.exe

The Linux build is for verification. linux-debug runs AddressSanitizer, UBSan, and clang-tidy, and warnings are errors.

    cmake --preset linux-debug
    cmake --build --preset linux-debug
    ctest --preset linux-debug

A change is finished when linux-debug builds without warnings and its tests pass. Git hooks format on commit, check commit messages, and gate pushes on linux-debug; activate them once per clone with `git config core.hooksPath .githooks`. To check rendering, run the app with --capture out.bmp; it renders a few frames, saves the last one, and exits.

## Authority

docs/principles.md is the highest authority. When a request or plan conflicts with a principle, surface the conflict to Evan instead of coding around it. Principles change only through a decision record.

Decisions that are expensive to reverse are recorded in docs/decisions/; see its README. docs/design-notes.md is provisional thinking, not a spec. Questions in docs/open-questions.md are Evan's to decide: ask about them, do not decide them in code.

## Workflow

Planning and building go through the skills in .claude/skills; start at planning-overview, which routes to the rest. Commit messages follow the commit-hygiene skill.

## Specs and verification

Each module keeps a SPEC.md beside its code, updated in the same change as the code it describes. Scale ceremony to the change: a small fix needs no spec edit.

Tests are derived from the principles and specs and written in a separate pass from the implementation, by the test-writer agent, never by the context that writes the code. After implementing, the reviewer agent checks the diff against the spec and the principles with fresh context and reports only gaps that affect correctness or stated requirements. Report results with evidence such as test output, never as self-assessment.

Anything mechanically checkable belongs in a test, hook, or CI check, not in a document.

## Exceptions

Anything that does not fit fields, flows, or encapsulation is listed in docs/exceptions.md with a reason. If the list keeps growing, raise it with Evan: it means a principle is wrong.

## Code

Naming and file layout follow docs/conventions.md.

## Writing

Documents use plain prose, no bold emphasis, and bullets only for genuinely distinct items.
