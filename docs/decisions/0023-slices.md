# 0023. Slices for goals that span capabilities

Status: Accepted, 2026-09-26

## Context

The planning tree gives every milestone one parent capability. The first milestone in docs/vision.md, the boxes-and-tubes slice, is not one capability's work: it is the first foundation milestone of several capabilities landing together. A goal like that needs a home for its end-to-end criteria, its cross-capability medium, and the order of its parts, which no single capability owns.

## Decision

A slice is a node beside the capabilities, at plans/slices/<slug>/SLICE.md. It states an end-to-end scenario and acceptance criteria, maps the fields and flows that cross capabilities, and lists its member milestones in build order. Each member milestone belongs to its own capability as before and names its slice. A slice completes when every member milestone has landed and its end-to-end criteria pass. The planning-slices skill plans and closes slices.

## Consequences

The medium map in a slice is where the principles gate applies across capabilities: every cross-capability interaction must appear there as a field or flow with a producer and a consumer. Slices can be planned before their member capabilities exist, naming them as to be planned. A member milestone requires both its approved CAPABILITY.md and its approved SLICE.md.
