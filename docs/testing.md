# Testing

This document says which tests to write and what they may assert (decision 0037). It ranks below docs/principles.md and above every spec, plan, skill, and agent. Change it only through a decision record.

Tests check what playing the game cannot show: that a world copies, saves, hashes, and replays exactly, on every build players run. Check everything else by playing the game and with `--capture`. If your work must leave the simulation unchanged, also check it with `tpj_scenarios --compare`.

## The suite

The suite has three parts. The sim root's tests in tests/sim prove the world-as-value standards over the park files in tests/parks/ and the worlds a few edits reach from them: a copy equals its world, a save loads back and resolves equal, the hash covers all registered state, a candidate equals the world that commits the same commands, and two runs give the same state. The same directory tests the pieces those standards rest on: the tick cycle, keyed draws, entity keys, schemas, sim math, and the floating-point environment. It also tests that resolution derives the units an edit can reach and no others. Resolving with nothing changed derives no unit, an edit no unit reads derives none, and an edit derives the same units however much else the park holds. The integration tests in tests/integration each check one consequence across modules that the player depends on, stated as cause and effect, such as cutting a shop's supply leaving its guests hungrier. The checks in tests/checks, and the sim symbol check, enforce the code's layers, its private headers, and the sim's runtime independence. The allocation check in tests/checks finds allocations in the work the app repeats in steady play: the tick, the scene's sync, the kept preview and the overlays, the tools, the panels' data, the renderer's uploads and drawing, and the app's work after an undo or a redo, at a click's pick, at each tool's press and release, and when it queues a command (decision 0041). Only ImGui's own drawing, and the resolvers and finishers changing-previews hasn't moved yet, stay outside it.

## What a feature adds

Add a test for a feature in only three cases, and list each one in the feature's FEATURE.md:

- to reproduce a bug the feature fixes;
- as an integration test of a consequence across modules that the feature exists to make, stated as cause and effect;
- when the feature registers unit kinds, as a test of which of its units an edit derives, stated as the edit and the units it predicts.

If a feature registers state, add that state to a park file in tests/parks/ instead of writing a test for it. Once a park file holds the state, the park world checks cover it. The world checks fail if no checked world holds a registered type.

Don't add an allocation test for a feature. If a feature adds to the tick, the allocation check covers it once full.park holds what the feature adds. If it adds other work the app repeats in steady play, such as a panel's data or a tool's hover, add that work to the check in the same feature.

Don't derive a test from a spec or from a feature's other criteria, and don't write a test that checks a single module's internals. Don't write tests for test utilities, such as tpj_bench, tpj_scenarios, the park generators, and the scripts. Check them by running them.

## What a test may assert

Don't make a test's expected outcome or its setup depend on a tuning constant's value, a tick count, an exact text layout, or a formula copied from the code. In an integration test, compare a park with the same park after a change, not a tuned run with a remembered result.
