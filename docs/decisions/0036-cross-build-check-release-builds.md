# 0036. The cross-build check compares the builds that ship

Status: Accepted, 2026-10-03

## Context

Decision 0022 requires a park to simulate bit-identically on every supported build, and the cross-build check showed it by comparing linux-debug with windows-debug. Its reason was that Linux tests stood for what players see, but since decision 0030 the Windows tests are the evidence, and Linux runs only at release. The game ships on Windows and Linux, so players run windows-release and linux-release, which nothing compared. Those builds optimize differently from the debug builds, so the simulation work is tested on and the one players run could drift apart unnoticed.

## Decision

The cross-build check builds tpj_scenarios with windows-debug, windows-release, and linux-release, runs each over the registered scenarios and every park file in tests/parks/, and fails when a release build's output differs from windows-debug's. Release against release shows Windows and Linux players get the same simulation, so saves move between them; each release against windows-debug shows that what is tested is what ships. linux-debug, the sanitizer build, is no longer compared. The check still runs only at release, from scripts/release-check.sh, so it costs development nothing.

## Consequences

A release builds linux-release as well as linux-debug, which takes longer on the Windows drive. A divergence an optimizer introduces, such as a contracted multiply-add or a reordered sum, is caught before players see it.
