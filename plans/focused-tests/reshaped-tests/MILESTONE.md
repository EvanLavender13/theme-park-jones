# Milestone: Reshaped Tests

Slice: none

Status: planned

## Summary

reshaped-tests brings the tests and specs that predate docs/testing.md into line with it, module by module. plans/focused-tests/RESEARCH.md lists 203 test cases as rewrite, each with the lasting property it should assert, and each module's SPEC.md still fixes layouts, formulas, tuning values, or display text as contract, which is what made those tests change-detectors (rule 8). Each module's feature trims its spec to properties and has the test-writer rewrite its listed cases, in one change, so no test is left asserting what its spec no longer says. The repository checks in cmake/ drop to a run and one planted violation each. It comes after shared-world-checks because several rewrites drop a re-proof of a world-as-value standard, which is only safe now that the sim root proves those standards over the park schema.

## Acceptance criteria

- Every case plans/focused-tests/RESEARCH.md lists as rewrite asserts the property its entry gives, or is deleted when its entry merges it into another case or another test already shows that property. Each entry is removed from the inventory when its module's feature lands, and the inventory has none left at the close. An entry whose property itself breaks a rule of docs/testing.md is corrected in the inventory before its feature dispatches it.
- Each module's SPEC.md states properties as its contract, except bench's and scenarios', whose modules are test utilities (rule 6), not vertex or triangle orders, formulas, tuning values, or display text. A detail stays only where something outside the module depends on it: app's frame-times line and hash line, park's box corner order, render's windowPoint convention and uniform sets, and the save format and the derived keys saves hold. A constant other modules use by name keeps its name in the spec, without its number.
- A trimmed statement that stood in for a property is replaced by that property, such as a tool's finish reach exceeding the park's minimum point spacing.
- The private header, layer, and placement checks each keep one run on what they gate and one planted violation, and no other tests. The placement check's run is the pre-commit hook, so ctest keeps only its planted violation. The sim symbol check already has this shape and is unchanged.
- The test-writer's rewrite mode takes the inventory's entries as it takes closing-audit findings, and implementing-features' test pass has a rewrite dispatch that carries a feature's entries verbatim, for a feature that rewrites existing cases.
- Test support no remaining test uses is deleted.
- Nothing under src/ changes except SPEC.md files, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

This milestone introduces, samples, and emits no fields or flows. It changes tests, specs, and the test-writer's instructions only. Each rewritten test reaches its module through its public headers, as the test-writer does; a property that cannot be stated that way is reported as a gap, not worked around.

## Dependencies

- shared-world-checks: complete.
- The test-writer's rewrite mode and the closing audit: met.

## Core feature

`sim-root-reshaped`, because the sim root is the bottom of the layers and owns the standards other modules' rewrites lean on: the walk's refusals, the save format, the draws, and the simulation's math. It also widens the test-writer's rewrite mode and implementing-features' test pass to the inventory's entries, which every later feature dispatches. Once it lands, one module's spec and tests are in final shape and the pattern every other feature follows is proven.

## Features

Each module feature trims its SPEC.md, dispatches the test-writer in rewrite mode with its inventory entries verbatim, deletes the test support its rewrites leave unused, and removes its entries from the inventory. Features follow cmake/layers.txt bottom-up, so a rewrite that leaves a property to a lower module finds that module's tests already in final shape.

1. `sim-root-reshaped`: sim/SPEC.md trimmed and the sim root's 42 entries rewritten (sim_math, schema, world_checks, cycle, save, fp_environment, draw, walk), and the test-writer's rewrite mode and implementing-features' test pass widened to the inventory. Depends on: none.
2. `medium-reshaped`: sim/medium/SPEC.md trimmed and its 19 entries rewritten, stepped_field_test's publication-order case merged into field_test's. Depends on: feature 1.
3. `park-routes-reshaped`: sim/park/SPEC.md and sim/routes/SPEC.md trimmed and their 22 entries rewritten, edit_sequences_test's copy and save assertions dropped. Depends on: feature 2.
4. `guests-operations-reshaped`: sim/guests/SPEC.md and sim/operations/SPEC.md trimmed and their 33 entries rewritten, with food_loop_test's integration entry. Depends on: feature 3.
5. `legible-reshaped`: legible/SPEC.md trimmed and its 19 entries rewritten. Depends on: feature 4.
6. `render-reshaped`: render/SPEC.md trimmed and its 45 entries rewritten. Depends on: feature 4.
7. `app-tools-reshaped`: app/SPEC.md and tools/SPEC.md trimmed and their 22 entries rewritten. Depends on: features 5 and 6.
8. `checks-reshaped`: the private header, layer, and placement checks' tests cut to a run and one planted violation each. Depends on: none.

## Deepening candidates

None beyond the capability's.

## Open questions

None.

## Research notes

All 203 rewrite entries still name existing cases. A survey of the specs found about 126 statements fixing layout, formulas, tuning, or text, of which a handful stay because code elsewhere or saved files depend on them. Splitting by module keeps each spec trim with the tests that pinned it; splitting by kind of work or by rule broken was rejected. Depth is in RESEARCH.md.
