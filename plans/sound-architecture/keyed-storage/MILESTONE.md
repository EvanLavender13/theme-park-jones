# Milestone: Keyed Storage

Slice: none

Status: planned

## Summary

keyed-storage drops EnTT and has the world store its components itself, keyed by EntityKey (decision 0039). EnTT was chosen before world-as-value. That design then routed around it: systems walk keys instead of views, the registered walk does copy, hash, and save, and World keeps its own key maps in front of EnTT's handles. What's left is a library that every call site names but whose main features the sim can't use. This milestone gives the world one way to reach a component, by key, with iteration in key order by construction. The simulation doesn't change. World's public contract does, and 0039 records the tests that change with it. Later speedups to the store belong to scalable-runtime and are measured there.

## Acceptance criteria

- No file under src or tests includes an EnTT header, and the build doesn't fetch EnTT. tpj_sim links only tpj_core.
- Code reaches components only through World, by EntityKey. World has no public registry, and findEntity, keyOf, and entt::entity are gone from the sim's interface.
- If a system needs every entity that holds one type, World gives them in ascending key order.
- If code adds a component type that the world's schema doesn't register, the world throws WorldInvariantError when it is added.
- The simulation is unchanged. `tpj_scenarios --compare` on windows-debug output from before and after shows no difference, and scripts/cross-build-check.sh passes.
- The existing tests pass. They change only in how they reach components. Cases that check EnTT handles, such as findEntity and keyOf being inverses, are deleted, because the handles no longer exist. The world checks case for an unregistered component checks the throw at the add instead of at the walk.
- scripts/runtime-report.sh is run before and after, and the comparison is reported to the user. If a stage is marked slower, the user decides before the merge whether it ships. Timings aren't gated.
- Decision 0039 is accepted and 0016 is marked superseded by it. src/sim/SPEC.md, src/sim/park/SPEC.md, docs/conventions.md, and cmake/check_private_headers.cmake cite 0039 for the private-header rule.

## Medium

This milestone adds, samples, and publishes no fields or flows. It changes how the world stores the components that fields, flows, and entities already use. Every module's components keep the same visibility: private components stay in internal headers, and shared-medium components stay public.

## Dependencies

- Decision 0039 accepted: met.
- World-as-value's keys and registered walk (deterministic-simulation): met.
- The private header check (shared-medium): met.
- tpj_scenarios --compare and scripts/cross-build-check.sh: met.
- scripts/runtime-report.sh and the stress parks (scalable-runtime): met.

## Core feature

`keyed-access` comes first. Once every call site reaches components through World by key, nothing outside World and the schema names EnTT. The storage swap then touches a handful of files instead of thirty, and the scenarios comparison can show each step unchanged on its own.

## Features

1. `keyed-access`: World gains component access by EntityKey, including key-order iteration over one type, implemented over the registry for now. Every call site in src and tests moves to it, and Registry becomes private. Depends on: none.
2. `own-storage`: World keeps one key-ordered store per registered type, the schema identifies types without EnTT, entity handles leave the interface, and EnTT leaves the build, with the specs and decision references updated. Depends on: feature 1.

## Deepening candidates

None. Faster stores and the speedups they allow go to scalable-runtime's pool.

## Open questions

None.

## Research notes

- A std::map per registered type meets every need of a first store: lookup by key, key-order iteration, and references that stay valid when other entities gain or lose the type. Sparse sets, sorted vectors, slot maps, and hash maps each fail one of those.
- The address of a non-const static in a class template gives each type a unique id without EnTT or RTTI. It's stable within a run only, so it never reaches a save or a hash.

The details are in RESEARCH.md.
