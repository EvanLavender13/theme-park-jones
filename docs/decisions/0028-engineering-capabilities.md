# 0028. Engineering capabilities deepen what the player cannot perceive

Status: Accepted, 2026-10-01

## Context

Capabilities are qualities, disciplines, or principle implementations the game deepens (decision 0024), and work that cuts across capabilities lives under the one whose quality it primarily deepens. That rule has no home for work whose value is in the software rather than the park. sound-architecture already deepens the structure of the code. Performance work has had no home of its own, so it landed wherever it was found: affordable-sampling, a milestone that changes no sample's result and only makes sampling faster, sits in shared-medium, and explained-food's deepening candidates hold a cheaper food overlay and a frame budget check. Measured on a park with 3 km of guest path, the food overlay's rebuild takes 38 ms on windows-debug, and its remedies reach into legible-simulation, shared-medium, and deterministic-simulation at once. Filed under whichever capability surfaces them, such remedies scatter, and no capability owns the measuring that would find them first.

## Decision

A capability may deepen a property of the software itself. The test is whether a player would notice the work missing if the machine were infinitely fast and the code never changed again. Work that fails that test is engineering: performance, code structure, tooling, and the checks that guard them. Work a player would notice is gameplay, even when it is motivated by speed. Throttling an overlay or re-resolving entities only on meaningful change alters what the player sees, so it is planned in the capability whose quality it affects. By the test, deterministic-simulation is not engineering: previews matching the committed park and saves reloading exactly rest on it.

An engineering capability is named after the software property it deepens, and the tenth-milestone test still applies. It introduces no fields or flows, and its Medium section says so. Its work may change code in any module, provided observable behavior does not change: the existing tests pass unchanged, the cross-build check's output is identical where the simulation is touched, and a module's public contract changes only with its SPEC.md in the same work. Its foundation criteria are checks it owns, such as the layer check or a frame budget. The principles gate applies to it as to any capability.

An idea that passes the test goes to an engineering capability's pools, whichever capability's work surfaced it.

A capability does not declare its kind. The test is applied when work is planned or routed.

## Consequences

sound-architecture is an engineering capability. planning-overview, planning-capabilities, and maintaining-backlog state the test and its routing.

Shipped engineering work stays where it was planned: affordable-sampling remains in shared-medium. Open engineering candidates move to an engineering capability once one exists that fits them. A performance capability is the expected home of affordable-sampling's entries indexed by place, world-as-value's incremental resolution and candidates off the frame's thread, and explained-food's cheaper food overlay and frame budget check, and planning it moves them.
