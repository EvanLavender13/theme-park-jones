# Feature: Sim Root Reshaped

## Summary

The sim root's spec states properties, not formulas or tuning values, and its 42 test cases that plans/focused-tests/RESEARCH.md lists as rewrite assert the property each entry gives, merged where the entries say. The rewrites come from the test-writer's rewrite mode, which this feature lets implementing-features dispatch with a module's inventory entries, as every later feature of reshaped-tests does. The sim root's entries leave the inventory, and the test support only the old cases or the trimmed formulas used is deleted.

## Acceptance criteria

1. The sim root's tests assert these properties, each once, in place of the cases the inventory lists for cycle_test, draw_test, fp_environment_test, save_test, schema_test, sim_math_test, walk_test, and world_checks_test:
    - A resolution runs every resolver, then every finisher, once each in registration order.
    - A queue holding a command of an unregistered type is refused before anything changes.
    - A candidate equals the world that commits the same commands, shown with commands whose order matters, so they apply in submission order.
    - isStepping, isResolving, and isFinishing are each true exactly while their stage runs.
    - drawUniform lies in [0, 1).
    - drawPick refuses weights it cannot pick from in proportion, shown with one refused input.
    - In debug builds, constructing a World by any route refuses a non-default floating-point environment.
    - A loaded world is pending resolution.
    - A save is independent of derived data.
    - A failed load names the first line it cannot read, shown with one malformed save.
    - A load ignores carriage returns and blank lines.
    - A schema refuses a repeated or reserved registration and is left unchanged.
    - A schema accepts a name exactly when it is lowercase letters, digits, and hyphens.
    - simExp and simLog return IEEE's special values.
    - World equality compares schemas by content.
    - In debug builds, the walk refuses a world holding state it cannot cover, and accepts every world it can, shown through one walk call.
2. src/sim/SPEC.md states no formula or tuning value that nothing outside the sim root depends on, as Spec changes lists.
3. implementing-features' test pass can dispatch the test-writer in rewrite mode with a feature's inventory entries verbatim, and the test-writer's rewrite mode takes them as it takes closing-audit findings.
4. The sim root's entries are removed from plans/focused-tests/RESEARCH.md.
5. Test support that no remaining test or tool uses is deleted, tests/sim/support/expected_draws.py and the expected values in expected_draws.h among it.
6. Nothing under src/ changes except src/sim/SPEC.md and src/scenarios/SPEC.md, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

None. The feature changes tests, one spec, and the test-writer's dispatch; it samples, emits, draws, and supplies no fields or flows. The world-as-value standards stay proven once, by the shared world checks; the candidate property above is the cycle's submission order, shown through the candidate, not a second proof of the candidate standard.

## Spec changes

src/sim/SPEC.md:

- "The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS (1/30 s) at a time." becomes "The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS at a time."
- "and the hash folds those words with SplitMix64's finalizer." becomes "and the hash is a 64-bit function of those words alone."
- "drawBits folds those five words, in that order, with the Hasher of sim/mix.h and returns its 64-bit value." becomes "drawBits returns a 64-bit value that is a function of those five words alone."
- "drawUniform is the draw's top 53 bits times 2^-53, a double in [0, 1)." becomes "drawUniform is a double in [0, 1), uniformly distributed."
- "For integer weights it maps the draw to r in [0, total) as the high 64 bits of the draw times the total, then returns the first index whose running sum exceeds r. The map's bias toward any index is below total / 2^64, and there is no rejection, so a pick never takes a second draw. For double weights it sums the weights in order, multiplies drawUniform by the total, and returns the first index whose running sum exceeds that target. The running sum at the last index repeats the total's additions, so it equals the total, and the target is always below a normal total, so some index is returned." becomes "A pick never takes a second draw, always returns an index, and for integer weights is biased toward no index by more than total / 2^64."
- In the refusal sentence that follows, "where rounding the target would make picks disproportionate" becomes "where rounding would make picks disproportionate".

src/scenarios/SPEC.md, since the draw table is no longer generated:

- "reads the generated tables in tests/sim/support" becomes "reads the tables in tests/sim/support: sim_math_reference.h, generated by sim_math_reference.py, and the draw keys and weights of expected_draws.h".

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/scenarios/SPEC.md
- Modify: .claude/skills/implementing-features/SKILL.md
- Modify: .claude/agents/test-writer.md
- Modify: tests/sim/cycle_test.cpp, draw_test.cpp, fp_environment_test.cpp, save_test.cpp, schema_test.cpp, sim_math_test.cpp, walk_test.cpp, world_checks_test.cpp (by the test-writer)
- Modify: tests/sim/support/expected_draws.h
- Delete: tests/sim/support/expected_draws.py
- Modify: tests/sim/support/synthetic_types.h, the only support header the rewritten files include besides sim_math_reference.h, for definitions no remaining test uses
- Modify: plans/focused-tests/RESEARCH.md

## Dependencies

shared-world-checks, complete: the world-as-value standards these cases no longer re-prove are proven there.

## Out of scope

- The check_sim_symbols report line and denylist, which the check's planted violation reads; checks-reshaped leaves the symbol check as it is.
- Other modules' entries and specs, which the milestone's later features take.

## Open questions

None.
