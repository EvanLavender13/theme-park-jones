# 0041. A mechanical check finds allocations in the work of steady play

Status: Accepted, 2026-10-12.

## Context

docs/conventions.md says not to allocate inside loops in per-tick or per-frame code: allocate buffers once, reuse them, and grow them only while they are smaller than the largest item so far. Allocating and freeing the same memory every tick costs far more than reusing it. Its time varies from call to call, so frames become uneven. Inside a loop over guests or shops, the cost also grows with the park.

Nothing checked the rule. Reviews were left to apply it, and they didn't. A plan review rated an allocating sort in per-tick overlay code Minor. An audit of src/ on main then found 20 places that break the rule, in the tick, in display work the app repeats every tick, and in code the app runs every frame. Reviews of the plan to fix them found more.

Most of that per-frame work doesn't need a window. Snapping, the frontage search, Edit path's handles, the tool's tentative edit, the scene's intent check, the panels' data, and the tooltips all run on a world and a pointer position. Only uploading meshes, drawing, and ImGui's own drawing need one. A fix that frame time measures by hand can't show that one allocation is gone, because one allocation costs far less than the noise between frames.

CLAUDE.md says that if a machine can check a rule, the rule goes into a test, hook, or check. Decision 0037 keeps mechanical checks in the suite because they need no judgment. This rule can be checked the same way.

## Decision

A mechanical check joins tests/checks: the allocation check. It is its own executable, so it can count every heap allocation and free by replacing the global operator new and operator delete. It runs in ctest with windows-debug, so the pre-push hook blocks a change that breaks it.

The check covers all the work the app repeats in steady play: the park running with nothing changing but the clock, the pointer's hover, and the views. That includes the tick, the scene's sync, the guest mesh, the pointer and the tool's hover, the kept preview, the overlay and the ghost, the panels' data, the Inspector, and the tooltips. It runs each piece of that work on tests/parks/stress/full.park after a warm-up, repeated with the same buffers, and fails if a heap block is both allocated and freed during the repeats. Only uploading meshes, drawing, and ImGui's own drawing stay outside it.

The check lands area by area. Each feature that adds an area to the check makes that area pass in the same change, so the check never fails on main.

Previews of an edit that changes every frame, such as dragging a path, resolve a new world each time. They stay outside the check until a milestone makes resolution reuse its storage.

## Consequences

docs/testing.md's list of mechanical checks gains the allocation check. A feature doesn't add its own allocation test. If a feature adds work to steady play, the check covers it once full.park holds what the feature adds, as keeping full.park current already requires.

scalable-runtime's allocation-free-loops milestone builds the check and fixes steady play area by area. Its changing-previews milestone extends the check to previews of a changing edit. Features in other capabilities wait until allocation-free-loops closes.

Reviews keep the rule too. The reviewer rates a broken docs/conventions.md rule as a Blocker.
