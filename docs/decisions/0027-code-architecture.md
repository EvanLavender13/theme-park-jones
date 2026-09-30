# 0027. Code architecture: layered modules, owned components, a composing entry point

Status: Accepted, 2026-09-30

## Context

The principles govern how things in the park interact, and docs/conventions.md governs naming and file layout. Nothing governs where code lives. Features have been placed wherever was locally cheapest, and plans have directed edits into existing functions by name. src/app/main.cpp grew to hold command-line parsing, the park file workflow and its dialog threading, mesh caching, input mapping, debug statistics, graph drawing, frame timing, and platform setup and teardown, all in one anonymous namespace. Its state lives in the main loop's locals, and replacing the world resets each derived cache by hand, so a new cache that misses that reset is a silent bug. None of it can be tested without a window.

Principle 6 keeps entities from reading each other's internals, and decision 0016 enforces that with private headers. The code that is not an entity has no equivalent rule.

## Decision

Modules form layers, and a module depends only on the layers below it. core is at the bottom. sim depends on core. tools, render, and scenarios each depend on sim and core and not on each other. app depends on everything and nothing depends on app. A module's own parts, such as the sim's submodules, may form layers of their own, declared with the modules' layers. A module outside the sim never names sim internals beyond what the sim's public headers give, which keeps principle 10's independence of simulation from rendering a property of the build.

Each concern a module orchestrates is a component: a type or a set of functions behind its own header, owning its state, with one reason to change. A behavior that fits no existing component gets a new component rather than growing an unrelated one.

An executable's entry point only composes. It parses its options, builds the components, runs them, and returns. The order a frame runs in is stated in one place, and each step of it is a call into a component.

State derived from other state, such as a mesh built from the world or a cache of what was last drawn, has one owner, and a change of its source reaches it through one path. This carries principle 1 into the code: derived state is never the source of truth and is always rebuilt from it.

Every acquired resource, such as a window, a GPU device, or a UI context, has one owner that releases it, in the reverse of the order they were acquired.

Code bound to the platform, meaning calls into SDL and Dear ImGui, sits at the edge of a component. Logic that can be stated without a window is, and its tests reach it through the component's header.

Every plan names the module and component that owns each behavior it adds. The reviewer checks the placement against this record.

## Consequences

The layering is enforced mechanically: a check in ctest fails when a file includes a header from a layer it does not depend on. Function size is already limited by clang-tidy's readability-function-size in .clang-tidy. Whether a component holds one concern is not mechanically checkable, so the placement section and review carry it. The sound-architecture capability owns the check, the restructuring of code that predates this record, and the rule's deepening.

src/app/main.cpp and src/scenarios/main.cpp do not meet this record until sound-architecture restructures them. Until then, a feature that must touch either adds no new concern to it.

planning-features adds a placement section to PLAN.md, and the reviewer cites this record when a plan or diff puts a behavior in a component that does not own it.
