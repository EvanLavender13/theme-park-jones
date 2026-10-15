# Feature: Watched Resolution

## Summary

watched-resolution flips the allocation check's watch over resolution. Today the drags and commits watch only the resolution steps listed in WATCHED_STEPS, which a step joins after a feature fixes it. Every other resolver, and the finishers, run unwatched. The check therefore only confirms that fixed code stays fixed. It can't find an allocation in a step nobody has fixed yet, or in new code.

After this feature, the drags and commits watch every resolution step by default. Only the steps on a named exception list are excused. Each exception names the feature that removes it. New code is watched from its first build. The check fails when an excused step no longer allocates, naming it, so a step leaves the list as soon as its fix lands. The list can only shrink.

## Acceptance criteria

1. The drags and commits watch every resolution step, every resolver by its name, the finishers, and the candidate's refill, except the steps on the exception list. The list holds exactly the steps that allocate today, found by running the check with every step watched. Each entry names the feature that removes it, or "a later feature" for whole-park work the milestone's Features list names but no feature has taken yet. (check: read allocation_check.cpp; the run that found the list)
2. A block counts against an excused step when that step allocated it or freed it. Every other block allocated and freed while watching fails the check, as now. (check: read the report code)
3. The check fails when an excused step neither allocated nor freed a block it counts in the drags and the commits together, naming the step and saying to remove it from the list. (check: the first run with every unlisted step excused fails naming each step that doesn't allocate)
4. The check passes on main with the list. (ctest: the allocation check)
5. Decision 0041, docs/testing.md, MILESTONE.md, and CAPABILITY.md's watched-previews entry say the drags and commits watch every step but a shrinking exception list. MILESTONE.md says that if the milestone stops before its last feature, each remaining entry is fixed or recorded in docs/exceptions.md with its reason, and none is left only in the test file. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

None beyond the allocation check itself, which this feature changes.

## Medium

None. Only the check changes.

## Spec changes

None. No module's behavior changes.

Decision 0041's paragraph on previews of a changing edit, from "In both, it watches everything except" to the end of the paragraph, becomes:

    In both, it watches every resolution step except those on an exception list, each naming the feature that removes it. The check fails when an excused step no longer allocates, so the list only shrinks, and new code is watched from its first build. watched-previews extends the watch to the preview an overlay keeps at each tick.

docs/testing.md line 9's last sentence, decision 0041's sentence ending its "The check covers all the work" paragraph, and the same sentence in docs/conventions.md's allocation rule each become: "Only ImGui's own drawing, the resolution steps on the check's exception list, and the resolution inside an undo or a redo stay outside it." conventions.md keeps "(decision 0041)" after it.

Decision 0041's Consequences sentence on watched-previews becomes: "Its watched-previews milestone, after changing-previews, extends the check to the preview an overlay keeps at each tick and to applying a command."

MILESTONE.md criterion 4's sentence "The watched-previews milestone extends the allocation check to them." becomes "The allocation check watches every resolution step but those on its exception list (watched-resolution)."

## Files affected

- Modify: tests/checks/allocations/allocation_check.cpp
- Modify: tests/checks/allocations/watch.h
- Modify: tests/checks/allocations/watch.cpp
- Modify: docs/decisions/0041-allocation-check.md
- Modify: docs/testing.md
- Modify: docs/conventions.md
- Modify: plans/scalable-runtime/CAPABILITY.md

## Dependencies

unit-connections, merged.

## Out of scope

- Fixing the excused steps. Each is the named feature's.
- The preview an overlay keeps at each tick: watched-previews.

## Open questions

None.
