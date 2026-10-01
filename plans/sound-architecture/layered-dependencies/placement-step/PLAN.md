# Implementation Plan: Placement Step

## Goal

Give PLAN.md a Placement section, make the reviewer check placement against decision 0027, and have the pre-commit hook refuse a new plan without the section.

## Approach

The format and the review are text: planning-features's SKILL.md gains the section and a checklist step, and reviewer.md's plan and code lenses gain a placement check. The mechanical part is cmake/check_placement.cmake, a cmake -P script shaped like the other checks in cmake/, which fails naming each plan it is given with no "## Placement" line. The pre-commit hook lists the PLAN.md files the index adds, exports their staged versions to a temporary directory with git checkout-index, and runs the script there before it formats anything.

## Placement

Decision 0027 places each behavior this feature adds:

- Stating where each behavior goes: the planning-features skill, which owns the PLAN.md format and the planning checklist.
- Judging whether a placement is right: the reviewer agent, in its implementation-plan and code lenses, since it is judgement no check can make.
- Recognizing the Placement section: a script of its own, cmake/check_placement.cmake, beside the other repository checks, so the planted cases test the rule without a repository.
- Choosing which plans to check and refusing the commit: .githooks/pre-commit, which owns what happens at commit time and already reads the index for the format pass.

## Tasks

### Task 1: Add the Placement step to planning-features

Files:
- Modify: `.claude/skills/planning-features/SKILL.md:33-40,50,132-136`

Step 1: In the checklist, insert after step 9 the new step 10 with FEATURE.md's exact text (Spec changes, first bullet). Renumber the following steps 10 to 16 as 11 to 17.

Step 2: In the self-review step, now 15, replace "and expected outputs. Fix inline." with "and expected outputs; the Placement section places every behavior the tasks add. Fix inline."

Step 3: In Process notes, add FEATURE.md's paragraph as a new paragraph directly after the paragraph beginning "Order of work inside PLAN.md".

Step 4: In the PLAN.md format's fenced block, insert between the Approach paragraph ("Two to three sentences on the technical approach.") and "## Tasks" the Placement section with FEATURE.md's exact text: the heading, the citing sentence, and the bullet template, each separated by a blank line as the other sections are.

### Task 2: Add the placement check to the reviewer

Files:
- Modify: `.claude/agents/reviewer.md:87-95,97-105`

Step 1: Append FEATURE.md's Implementation plans bullet as the last bullet of "### Implementation plans (`PLAN.md`)".

Step 2: Append FEATURE.md's Code or diffs bullet as the last bullet of "### Code or diffs".

### Task 3: Name the hook in CLAUDE.md

Files:
- Modify: `CLAUDE.md:22`

Step 1: Replace "Git hooks format on commit, check commit messages, and gate pushes on linux-debug and tidy" with "Git hooks format on commit, refuse a new PLAN.md with no Placement section, check commit messages, and gate pushes on linux-debug and tidy".

### Task 4: Stub the placement check

Files:
- Create: `cmake/check_placement.cmake`

Step 1: Create the script. Its header comment says:
- it fails when a plan it is given has no Placement section, meaning no line that is "## Placement" followed by nothing but blanks (decision 0027);
- it prints a line `<plan> has no Placement section` for each, with the plan's path as given, relative to ROOT;
- the pre-commit hook runs it on the staged versions of the PLAN.md files a commit adds;
- its usage, `cmake -DROOT=<tree> -DPLANS=<plans> -P check_placement.cmake`, where PLANS is a list of paths relative to ROOT.

Its body is `cmake_minimum_required(VERSION 3.28)`, then:

```cmake
if(NOT ROOT OR NOT DEFINED PLANS)
    message(FATAL_ERROR "usage: cmake -DROOT=<tree> -DPLANS=<plans> -P check_placement.cmake")
endif()

message(FATAL_ERROR "check_placement.cmake is not implemented")
```

### Task 5: Test pass

Step 1: Dispatch the test-writer agent for this feature with FEATURE.md and the stub cmake/check_placement.cmake. It creates tests/checks/placement_check_test.cmake, planting plans for each case as tests/checks/layer_check_test.cmake does, and registers one test per case in tests/checks/CMakeLists.txt.

Step 2: Configure, so ctest sees the new tests, and run them.

Run: `cmake.exe --preset windows-debug && ctest.exe --preset windows-debug -R "placement"`
Expected: the cases that a missing ROOT or PLANS fails pass against the stub. Every other case fails, since the stub fails before reading any plan.

### Task 6: Implement the placement check

Files:
- Modify: `cmake/check_placement.cmake`

Step 1: Replace the stub line with:

```cmake
# ROOT as an absolute, normalized path with no trailing separator.
cmake_path(ABSOLUTE_PATH ROOT NORMALIZE)
string(REGEX REPLACE "(.)/$" "\\1" ROOT "${ROOT}")

# file(STRINGS) drops carriage returns, so a CRLF plan reads as an LF one. Read as UTF-8, a
# character outside ASCII stays in its line instead of splitting it.
set(PLACEMENT_LINE "^## Placement[ \t]*$")
set(report "")
foreach(plan IN LISTS PLANS)
    set(path "${ROOT}/${plan}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Cannot read the plan ${plan}")
    endif()
    file(STRINGS "${path}" placement ENCODING UTF-8 REGEX "${PLACEMENT_LINE}")
    if(NOT placement)
        list(APPEND report "${plan} has no Placement section")
    endif()
endforeach()

# NOTICE prints each line as it is; FATAL_ERROR would wrap long ones.
if(report)
    list(REMOVE_DUPLICATES report)
    foreach(line IN LISTS report)
        message(NOTICE "${line}")
    endforeach()
    message(FATAL_ERROR "New plans need a Placement section (decision 0027).")
endif()
message(STATUS "Each plan has a Placement section")
```

Step 2: Run the cases.

Run: `ctest.exe --preset windows-debug -R "placement"`
Expected: every placement check case passes.

### Task 7: Run the check from the pre-commit hook

Files:
- Modify: `.githooks/pre-commit`

Step 1: Change the header comment's first line to "# pre-commit: refuse a new PLAN.md with no Placement section (decision 0027), then clang-format staged C/C++ files (decision 0021)." Keep its other lines.

Step 2: Insert directly after the header comment, before the clang-format lookup, so a refused commit is never formatted:

```bash
# Each PLAN.md the commit adds needs a Placement section (decision 0027). The staged versions are
# checked, since they are what is committed, and -M makes a moved plan a rename, not an addition.
PLANS=()
while IFS= read -r -d '' PLAN; do
    PLANS+=("$PLAN")
done < <(git diff --cached --name-only --diff-filter=A -M -z -- ':(glob)plans/**/PLAN.md')
if [ ${#PLANS[@]} -gt 0 ]; then
    if ! command -v cmake &> /dev/null; then
        echo "pre-commit: cmake is needed to check the Placement section of each new PLAN.md; commit aborted."
        exit 1
    fi
    STAGED=$(mktemp -d)
    trap 'rm -rf "$STAGED"' EXIT
    git checkout-index --prefix="$STAGED/" -- "${PLANS[@]}"
    PLAN_LIST=$(IFS=';'; echo "${PLANS[*]}")
    if ! cmake -DROOT="$STAGED" -DPLANS="$PLAN_LIST" \
            -P "$(git rev-parse --show-toplevel)/cmake/check_placement.cmake"; then
        echo "pre-commit: each new PLAN.md needs a Placement section (decision 0027); commit aborted."
        exit 1
    fi
fi
```

### Task 8: Check the hook by hand

Steps 1 and 11 run from the repository root, and Steps 2 to 10 inside build/placement-hook-check. The scratch worktree takes the new hook and script as copies, since they are not committed yet. Each commit message is `Plans: Try the hook` with a blank line and the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, so the commit-msg hook passes it.

Step 1: Make the worktree and copy the hook and the script into it.

Run: `git worktree add -b placement-hook-check build/placement-hook-check HEAD && cp .githooks/pre-commit build/placement-hook-check/.githooks/pre-commit && cp cmake/check_placement.cmake build/placement-hook-check/cmake/check_placement.cmake`
Expected: the worktree is created.

Step 2: In build/placement-hook-check, write plans/try/a/b/PLAN.md holding "# Plan" and "## Tasks" on two lines, `git add` it, and commit.
Expected: the commit is refused, and the output holds "plans/try/a/b/PLAN.md has no Placement section" and "pre-commit: each new PLAN.md needs a Placement section (decision 0027); commit aborted."

Step 3: Add a line "## Placement" to the plan in the working tree only, without staging it, and commit.
Expected: refused as in Step 2, since the staged version has no section.

Step 4: `git add` the plan. Then remove its "## Placement" line in the working tree only, without staging that, and commit.
Expected: the commit succeeds, since the staged version has the section. Restore the line with `git checkout -- plans/try/a/b/PLAN.md`.

Step 5: Write plans/try/c/d/PLAN.md holding "# Plan" alone, and commit it with `git add` and `git commit --no-verify`, so a plan without the section is in the tree. Then append a line to it, `git add` it, and commit without --no-verify.
Expected: the commit succeeds, since the plan is modified, not added.

Step 6: `git mv plans/try/c plans/try/e`, and commit with `git -c diff.renames=false commit`.
Expected: the commit succeeds, since the move is a rename.

Step 7: `git rm plans/try/e/d/PLAN.md` and commit.
Expected: the commit succeeds, since a deletion is not an addition.

Step 8: Write src/try.cpp holding the single line `int  main( ) { return 0 ; }`, and plans/try/h/i/PLAN.md holding "# Plan". `git add` both, note `git diff --cached` and `git diff`, and commit.
Expected: refused as in Step 2, naming plans/try/h/i/PLAN.md, and `git diff --cached` and `git diff` print what they did before the commit, with src/try.cpp unformatted. Then `git rm --cached -q src/try.cpp plans/try/h/i/PLAN.md` and delete both files.

Step 9: Make a directory holding only git and python3, which the commit-msg hook runs, so cmake is not on a PATH set to it. Then write plans/try/f/g/PLAN.md holding "# Plan", `git add` it, and commit with the PATH set to that directory.

Run: `mkdir -p ../nocmake && ln -sf "$(command -v git)" ../nocmake/git && ln -sf "$(command -v python3)" ../nocmake/python3 && PATH="$PWD/../nocmake" git commit -m "Plans: Try the hook"`, from build/placement-hook-check, with the trailer as above.
Expected: refused with "pre-commit: cmake is needed to check the Placement section of each new PLAN.md; commit aborted."

Step 10: `git rm --cached -q plans/try/f/g/PLAN.md`, then write notes.txt holding one line, `git add` it, and commit with the PATH set to the same directory.
Expected: the commit succeeds, since it adds no plan. The format pass prints that clang-format was not found and skips.

Step 11: Remove the scratch worktree and branch, from the repository root.

Run: `git worktree remove --force build/placement-hook-check && git branch -D placement-hook-check && rm -rf build/nocmake`
Expected: no error, and `git worktree list` shows only the repository.

### Task 9: Verify on both builds

Step 1: Run the full checks.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

### Task 10: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `Hooks: Refuse new plans with no Placement section`.
