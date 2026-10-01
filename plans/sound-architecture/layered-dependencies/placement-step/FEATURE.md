# Feature: Placement Step

## Summary

placement-step makes every new plan say where its code goes, and makes review hold it to that. planning-features's PLAN.md format gains a Placement section, between Approach and Tasks, naming for each behavior the plan adds the module and component that own it and why, under decision 0027. The reviewer's lenses for implementation plans and for code check placement against 0027. cmake/check_placement.cmake fails, naming each plan it is given that has no Placement heading, and the pre-commit hook runs it on the staged version of each PLAN.md the commit adds, refusing the commit when any lacks the section. Plans already in the tree are not checked, so none needs an exemption.

## Acceptance criteria

- A plan has a Placement section exactly when one of its lines is "## Placement" followed by nothing but blanks, meaning spaces and tabs. A carriage return at a line's end is ignored, and the file is read as UTF-8, so a character outside ASCII belongs to the line it is on. No other line, such as a heading of another level or other words, or a mention of the section in prose, is the section.
- cmake/check_placement.cmake, run as `cmake -DROOT=<tree> -DPLANS=<plans> -P check_placement.cmake`, where PLANS is a CMake list of paths relative to ROOT, reports each listed plan that has no Placement section, once however many times it is listed, comparing plans as listed, since the hook lists each by its path in the repository, as the whole, unprefixed line `<plan> has no Placement section` on standard error, with the plan's path as listed. When ROOT and PLANS are set and every listed plan can be read, it exits with status 0 exactly when it reports no plan. A PLANS that is set but empty reports nothing.
- Run without ROOT, or with ROOT empty, or without PLANS, the check fails naming the missing variable. Given a plan it cannot read, it fails naming the plan.
- The pre-commit hook refuses a commit that adds a file plans/.../PLAN.md, at any depth under plans, whose staged version has no Placement section, and the plan's path appears in its output. It passes a commit whose added plans each have the section in their staged version, whatever their working-tree version says.
- The hook passes a commit that modifies, deletes, or moves a plan already in the tree, whether or not that plan has the section. A move is a rename under git's rename detection, regardless of the diff.renames setting.
- When a commit adds a plan and no cmake is on the PATH, the hook refuses it, saying cmake is needed to check the plan. A commit that adds no plan never needs cmake.
- The hook checks placement before it formats, so a refused commit leaves the index and working tree as they were.
- planning-features's PLAN.md format has the Placement section, and the reviewer's implementation-plan and code lenses each check placement against decision 0027, as Spec changes states.

The hook's criteria are checked by hand when it is written (PLAN.md's Task 8), not by ctest; RESEARCH.md says why. The check's criteria are tested by planted cases in tests/checks.

## Medium

None. The check and the hook govern plans, and the lenses govern review. Neither is an interaction between things in the park.

## Principle checks

None of principles 1 to 6, 8, and 10 can be violated by this feature's code: it adds no simulation, rendering, or park state. It serves decision 0027, whose layer rule the layer check already enforces.

## Spec changes

.claude/skills/planning-features/SKILL.md:

- The checklist gains a step after step 9, "Draft the spec change", renumbering the steps after it:

  > 10. Place each behavior the feature adds, under decision 0027: the module and component that own it, and why. A behavior that fits no existing component gets a new one. src/app/main.cpp and src/scenarios/main.cpp take no new concern until sound-architecture restructures them; a feature that must touch one says what it adds there, and that it is composition only.

- In the self-review step, "and expected outputs. Fix inline." becomes "and expected outputs; the Placement section places every behavior the tasks add. Fix inline."
- The PLAN.md format gains, between Approach and Tasks:

  > ## Placement
  >
  > Decision 0027 places each behavior this feature adds:
  >
  > - <Behavior>: <module>, <component and its header>. <Why that component owns it.>

- Process notes gain a paragraph after "Order of work inside PLAN.md":

  > Every PLAN.md has a Placement section. The pre-commit hook refuses a commit that adds a PLAN.md without a line reading "## Placement", and the reviewer judges what the section says.

.claude/agents/reviewer.md:

- The Implementation plans lens gains: "A behavior the tasks add that the Placement section does not place, or a placement that breaks decision 0027: a component given a second concern, derived state given a second owner or a second invalidation path, a platform call away from a component's edge, an include cmake/layers.txt forbids, or a new concern in src/app/main.cpp or src/scenarios/main.cpp. Read docs/decisions/0027-code-architecture.md and cmake/layers.txt for this check."
- The Code or diffs lens gains: "Code placed against decision 0027 or against its PLAN.md's Placement section, in the ways the implementation plans lens lists."

CLAUDE.md, in Build and test, "Git hooks format on commit, check commit messages, and gate pushes on linux-debug and tidy" becomes "Git hooks format on commit, refuse a new PLAN.md with no Placement section, check commit messages, and gate pushes on linux-debug and tidy".

## Files affected

- Modify: .claude/skills/planning-features/SKILL.md
- Modify: .claude/agents/reviewer.md
- Modify: CLAUDE.md
- Create: cmake/check_placement.cmake
- Modify: .githooks/pre-commit
- Create (test pass): tests/checks/placement_check_test.cmake
- Modify (test pass): tests/checks/CMakeLists.txt

## Dependencies

- layer-check, whose cmake/layers.txt the reviewer's lens names: met.
- tests/checks's planted-tree pattern: met.
- git's --diff-filter, -M, and checkout-index --prefix, in git 2.43: met.

## Out of scope

- A ctest of the hook in a scratch repository, which needs the hook's temporary paths to survive Git for Windows's path conversion: backlog.
- Checking what the Placement section says, which is the reviewer's.
- Checking plans already in the tree.

## Open questions

- Whether a UTF-8 byte-order mark before a plan's first line is part of that line. No plan carries one, and planning writes none; resolved if a plan with one is ever added and the hook's verdict on it is wrong.
