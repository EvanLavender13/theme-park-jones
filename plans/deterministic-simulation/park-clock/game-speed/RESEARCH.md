# Research: game-speed

## How should a paused park apply edits, and how should the app notice them?

The milestone's RESEARCH.md settles speeds and pause: a faster speed steps more fixed ticks each frame, pause stops only the simulation's clock, and the player can build while paused. Two questions are left for this feature.

A paused park applies its queued edits through the cycle's apply-and-resolve stage alone. The sim already does exactly this to a copy in makeCandidate. Its spec says a candidate made from a world a cycle has just left equals the world that queuing the same commands for that cycle gives, because commands apply after the systems and the swaps. Applying them to the world itself while paused therefore gives the world the last cycle would have given with those commands, and the world the preview showed (principle 8). Nothing in the tick's order changes.

The app keeps two things keyed on the tick: the kept preview, made again when the tick or the edit changes, and the guest mesh, rebuilt when the tick changes. An edit applied while paused changes neither the tick nor the tentative edit. Every edit, undo, and redo changes park intent, and the scene already compares intent each frame to rebuild the park mesh. When intent changes, the scene can also rebuild the guest mesh and empty the kept preview. That covers paused edits with no new counter. While the game runs, intent changes only at a tick, which rebuilds both anyway.

Rejected: a counter of changes kept on the world — the world is a value whose every field is walked, saved, and hashed, so an unwalked counter would break copies and equality, and a walked one would change hashes. A counter of changes on the park session passed into keepPreview — it changes legible's interface when the intent comparison the scene already makes is enough. Stepping a tick to apply edits while paused — guests would move, and the clock would advance while paused.

Sources: the milestone's RESEARCH.md; src/sim/SPEC.md, Candidates; src/app/scene/scene_sync.cpp.
