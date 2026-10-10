# Research: inspectors

The milestone's RESEARCH.md covers what the inspectors already show and what benches and toilet blocks publish. This file covers how to cut the guest's choice table to a fixed size.

## How should a choice among many offers fit in a few rows?

A guest weighs one offer for each need each facility answers that it can reach, plus carrying on and heading home (guestOptions, src/sim/guests/guests.h). In full.park that is dozens of rows, and the table grows with every shop, bench, and toilet block the player places. The user found it too much.

The guest picks among offers by a softmax over their scores. The probability that it picks some offer of a need is the sum of those offers' probabilities, so one row for each need can show that sum exactly, with no offer left out of it. The offer with the highest score is the one the guest most likely picks for that need, so its facility and terms say where the guest would go and why. One row for each need, plus carrying on and heading home, gives six rows in every park. A need with no offer in reach keeps its row and says so, which is itself an explanation of why the guest won't go for it.

Heading home's reason already shows in its row, and its base and reason terms stay in the table below.

Rejected: the few highest-probability options — the rows would change from tick to tick, and a need with a low chance would vanish, though its absence is what the player needs to see. A row per need that expands to every offer — it brings back the long table, and it stays in the milestone's deepening candidates. Showing only the option the guest last picked — it explains the pick but not the alternatives the guest weighed (principle 8).
