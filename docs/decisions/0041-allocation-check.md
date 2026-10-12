# 0041. A mechanical check finds allocations in the tick and per-tick display work

Status: Accepted, 2026-10-12.

## Context

docs/conventions.md says not to allocate inside loops in per-tick or per-frame code: allocate buffers once, reuse them, and grow them only while they are smaller than the largest item so far. Allocating and freeing the same memory every tick costs far more than reusing it. Its time varies from call to call, so frames become uneven. Inside a loop over guests or shops, the cost also grows with the park.

Nothing checked the rule. Reviews were left to apply it, and they didn't. A plan review rated an allocating sort in per-tick overlay code Minor. An audit of src/ on main then found 20 places that break the rule:

- 6 in the simulation's tick, the worst of them in a guest's choice;
- 5 in display work the app repeats every tick while an overlay is shown, such as overlay shading, the ghost, and the Inspector;
- 9 in code the app runs every frame, such as checking whether intent changed, Edit path's handles, and snapping.

CLAUDE.md says that if a machine can check a rule, the rule goes into a test, hook, or check. Decision 0037 keeps mechanical checks in the suite because they need no judgment. This rule can be checked the same way.

## Decision

A mechanical check joins tests/checks: the allocation check. It is its own executable, so it can count every heap allocation by replacing the global operator new and operator delete.

It loads tests/parks/stress/full.park and steps it past a warm-up, long enough for every reused buffer to reach the size the park needs. It then counts the allocations made by:

- each of a run of ticks;
- shading an overlay of each kind again on the same world;
- building the ghost of an edit again on the same candidate;
- building a guest's inspection again.

It fails if any of them allocates, unless the allocation grows a buffer past its largest size so far. How the check tells that growth apart from an allocation that comes back every tick is the first feature's to design and to show working.

The check runs in ctest with windows-debug, so the pre-push hook blocks a change that breaks it.

Code that runs only per frame in the app, such as panels, tooltips, and tool hover, stays outside the check, because the app needs a window to run. The rule still applies there. It is fixed by hand, and each fix is measured by frame time on full.park.

## Consequences

docs/testing.md's list of mechanical checks gains the allocation check. A feature doesn't add its own allocation test. If a feature adds per-tick work, the check covers it once full.park holds what the feature adds, as keeping full.park current already requires.

The check fails on main when it lands, because of the audit's 11 breaks in the tick and the per-tick display work. A scalable-runtime milestone builds the check first and then fixes those 11, area by area, until it passes. The same milestone fixes the 9 frame-only breaks by hand, with frame times on full.park before and after. Features in other capabilities wait until the milestone closes.

Reviews keep the rule too. The reviewer rates a broken docs/conventions.md rule as a Blocker.
