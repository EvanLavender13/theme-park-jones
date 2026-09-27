# Feature: Keyed Draws

## Summary

keyed-draws gives the simulation random numbers that depend only on a key, never on a generator's history. A DrawKey holds the world's seed, an entity key, a purpose, the tick, and an index. drawBits folds those five words with the Hasher of sim/mix.h into 64 bits, drawUniform turns the top 53 bits into a double in [0, 1), and drawPick picks an index from integer or double weights with one draw. drawKey fills the seed and tick from a world, so a system names only the entity, the purpose, and the index. Draws hold no state, so there is nothing to copy, hash, or save. This feature does not include printing draws from tpj_scenarios (cross-build-check), softmax over scores (believable-guests), or other distributions.

## Acceptance criteria

1. A checked-in table of expected values matches: for each key in the table, drawBits, drawUniform, and drawPick over a fixed integer weight list and a fixed double weight list give the values the table records. The table's values are computed from the definitions in src/sim/SPEC.md by an implementation independent of tpj_sim, such as a short script, and the test file says how. The table passes under both the linux-debug and windows-debug test presets.
2. A draw depends on its key alone. Draws made for a set of keys in shuffled orders, interleaved with other draws, give the same values for the same keys. drawKey(world, entity, purpose, index) equals the DrawKey of world.Seed, entity, purpose, world.Tick, and index at the time of the call.
3. drawUniform is the draw's top 53 bits times 2^-53, so it lies in [0, 1). Over a million keys differing in index, Pearson's chi-square statistic for drawUniform over 1000 equal bins lies between 866.55 and 1142.85, the lower and upper 0.001 critical values for 999 degrees of freedom.
4. Over a million keys differing in index, each option's pick count from either drawPick overload is within five standard deviations, sqrt(n p (1 - p)), of n p, where n is the number of picks and p is the option's share of the total weight.
5. Neither drawPick overload returns an index whose weight is zero.
6. drawPick throws std::invalid_argument for an empty weight list, integer weights that total zero or more than 2^64 - 1, a negative, NaN, or infinite double weight, and double weights whose total is below the smallest normal double (zero included) or overflows to infinity.

## Medium

This feature introduces no fields or flows. It provides:

- drawKey, drawBits, drawUniform, and drawPick: used by believable-guests' softmax pick (decision 0019) and its per-guest variation, and by any system that needs randomness.
- The expected-draw table's keys: cross-build-check prints the draws for them from tpj_scenarios on both builds.

## Principle checks

- Principle 10: criteria 1 and 2. A draw depends only on its key, so draws are the same in any order, in a copy of the world, and on both builds, and the distributions are project code rather than the standard library's.
- Principle 1: drawing reads a world and never changes it. There is no generator state, so nothing about draws is saved, and a world's hash is the same before and after any number of draws.
- Principle 8: a candidate has the source world's seed and tick, so the draws a preview's candidate makes are the draws the committed world makes for the same keys.
- Principle 6: a draw takes an entity's key, never its components.

## Spec changes

src/sim/SPEC.md: after the paragraph on makeCandidate, add:

"Random draws are keyed, never sequential. A draw is a pure function of its DrawKey: the world's seed, an entity key, a purpose (usually a hashName), the tick, and an index. drawBits folds those five words, in that order, with the Hasher of sim/mix.h and returns its 64-bit value. drawKey fills the seed and tick from a world at the time of the call. Because a draw depends on nothing but its key, draws give the same values in any order, in a copy, and on every build, and drawing changes nothing in the world, so there is no generator state to copy or save.

drawUniform is the draw's top 53 bits times 2^-53, a double in [0, 1). drawPick picks an index with probability proportional to its weight, from one draw. For integer weights it maps the draw to r in [0, total) as the high 64 bits of the draw times the total, then returns the first index whose running sum exceeds r. The map's bias toward any index is below total / 2^64, and there is no rejection, so a pick never takes a second draw. For double weights it sums the weights in order, multiplies drawUniform by the total, and returns the first index whose running sum exceeds that target. The running sum at the last index repeats the total's additions, so it equals the total, and the target is always below a normal total, so some index is returned. A weight of zero, including -0.0, which is not negative, is never picked. drawPick throws std::invalid_argument for an empty list, an integer total of zero or above 2^64 - 1, a negative, NaN, or infinite double weight, and a double total that overflows or is below the smallest normal double, where rounding the target would make picks disproportionate. Simulation code draws only through these functions, never through the standard library's distributions, whose results differ between implementations (decision 0019)."

## Files affected

- Create: src/sim/draw.h
- Create: src/sim/draw.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Test pass: files under tests/, written by the test-writer agent.

## Dependencies

- registered-walk: EntityKey, World's Seed and Tick, and Hasher and hashName in sim/mix.h. Merged.

## Out of scope

- Printing draws, uniform doubles, and picks from tpj_scenarios, and comparing them across builds: cross-build-check.
- Softmax over scores: believable-guests, on top of drawPick with double weights.
- Other distributions, such as normal or exponential: added when a system needs one, with sim-math's exp and log.
- Exact integer picks by rejection over further indices: the bias is below total / 2^64.

## Open questions

- Whether resolvers may draw. drawKey reads the tick, which is state, while resolvers derive from intent alone; a resolver would need draws keyed without the tick to give the same result on every resolution. Resolved when a resolver first needs randomness.
- Where the expected-draw table lives once tpj_scenarios prints draws for its keys. It is now tests/expected_draws.h, generated by tests/expected_draws.py. Resolved when cross-build-check is planned.
