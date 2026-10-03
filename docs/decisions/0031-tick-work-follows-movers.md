# 0031. A tick's work grows with what moves, not with the park

Status: Accepted, 2026-10-02

## Context

The full stress park's tick took about 44 ms against a 33 ms budget, and planning paths-and-plazas showed that deriving the walkable graph from paved ground could make the graph several times larger. Every per-tick system was checked. Guests step once per guest, and shops and depots once per shop. Route distance is derived only in resolution, when intent changes. Hungry footfall is the exception: every tick it recomputes and republishes a value for every stretch and node of the guest network, whether or not any guest is on it, so its cost grows with the network, and any later metric written the same way would too. Planet Coaster and Cities: Skylines stay fast on large parks by the opposite rule: their per-tick work follows their agents, and work over the whole map happens when the map changes, persisted between frames and spread across cores.

## Decision

The work a tick does grows with the things that move, such as guests and packets, and with the things that act, such as shops and depots, never with the size of the park or its networks.

A metric that accumulates where things go, such as footfall, litter, noise, or crowding, is updated by the movers that touch it: a mover adds to the place it is on, and a place holds its value with the tick it last changed, so decay over the ticks since is computed when the value is read. Nothing visits every place of a network each tick. Decay computed on read gives the same value, bit for bit, as decay applied every tick would at that tick, or the metric's spec states the rule it uses instead, and it uses the simulation's own math (principle 10).

Work over the whole park or a whole network, such as deriving networks and route distance, happens in resolution, when intent changes, never in a tick. It may run across cores, provided its result is identical to running it on one (principle 10).

This rule covers the tick. Work per frame, such as building overlays, is the scalable-runtime capability's to measure and budget.

## Consequences

Hungry footfall breaks the rule and is rebuilt to follow it, without changing what its field means; scalable-runtime holds that work. Every new metric is specified this way from the start, and a spec that visits every place of a network in a tick fails review. A field whose entries are published by movers holds entries only where movers have been, so absent places read as the default, as docs/design-notes.md expects. Edits still cost work over the park, which previews feel on every pointer move, so resolution's cost stays measured by the runtime report.
