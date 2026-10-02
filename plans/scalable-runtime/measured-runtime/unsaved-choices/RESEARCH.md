# Research: unsaved-choices

## How should a guest's decision stay explainable without storing it?

Each guest kept its last choice in its saved state: the tick, place, and hunger it chose at, and one option per reachable shop, plus carrying on and heading home, each with four terms, a score, and a probability. Only guestRecord read it, for the guest panel. Nothing in the simulation did. Every save, world copy, and hash grew with guests times shops. A guest line in tests/parks/warm.park is about 400 characters before its last choice, and each option adds about 190, so the full park's 2,000 guests and 30 shops would save about 13 MB, almost all of it last choices.

Evan ruled that a save holds what behavior depends on, and that the explanation is not worth storing. The guest panel keeps principle 8's trace by working out, when a guest is inspected, what it would weigh if it chose now. That is the same scoring a guest does at a junction, with its current place, hunger, and target and the park's current offers, and the probabilities softmax gives, but with no draw. It costs one choice, for the one guest inspected, and stores nothing.

This is not the last choice rebuilt. That choice was made at an earlier tick, place, and hunger, and its draw is gone. The panel shows the guest's present weighing, and the Target row shows what it is doing.

Rejected: keeping the last choice as display-only data, never saved, compared, hashed, or copied — it adds a fourth kind of data to the simulation core for a panel, and the choice still grows with shops in memory. Dropping the panel's choice table outright — guest decisions would go unexplained, against principle 8, until fuzzy choice is designed. Computing the options inside guestRecord — the guest mesh, picking, and the park summary read every guest's record every frame, so every frame would score every guest against every shop. A separate function, called only by the inspector, keeps that cost to the one guest shown.

## Will the change show that guests behave the same?

tests/parks/warm.park and cut.park are tpj_scenarios --slice-parks's output from fed.park after 1,920 ticks of play, with guests arriving, choosing, walking, queueing, and eating. last-choice is the last field on a guest's line, so removing everything from ` last-choice=` to the end of each line of the committed files gives what they should hold without it. If the remade files equal those, byte for byte, then 1,920 ticks of guests behaved identically with the field gone.

Sources: src/sim/guests/guests.cpp (choose, guestRecord); src/legible/inspect.cpp; src/render/guest_mesh.cpp, src/render/picking.cpp, and src/legible/park_summary.cpp (the per-frame readers of guestRecord); tests/parks/warm.park.
