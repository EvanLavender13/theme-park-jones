# Research: park-clock

## How do games with a fixed simulation tick offer game speeds and pause?

Games with a fixed tick speed up by running more ticks in each real second, never by making a tick longer. Each tick stays the same length, so the simulation does exactly the same work at any speed. Implementations scale the real time a frame owes the simulation by the speed, then step as many fixed ticks as that time covers. Factorio's game speed works this way: at speed 1 it runs 60 ticks each real second, and its deterministic lockstep depends on the ticks, not the speed. A cap on the ticks one frame steps keeps a slow park from locking the frame up. Past the cap, the game runs slower than the chosen speed.

Pause stops the simulation's clock and nothing else. The frame keeps running, so the camera, the tools, and the panels stay live. While paused, no time accumulates toward the next tick, so unpausing doesn't step a burst of ticks. Building games such as RollerCoaster Tycoon and Cities: Skylines let the player build while paused. Here edits apply only inside a cycle, so a paused park needs a way to apply its queued edits without stepping. Applying and resolving them without stepping is what makeCandidate already does to a copy, so the world after a paused edit equals the candidate the preview showed.

A clock computed from the tick count in whole numbers is the same on every build, and saving it would be redundant, since the tick is already saved.

Rejected: a longer tick at higher speeds — guests and shipments would behave differently at each speed, and the hash at a tick would depend on the speed (principle 10). Holding edits until the player unpauses — the player can't build while paused, and the preview and the result would come a tick apart. A saved time of day — derived from the saved tick, so it would duplicate state (principle 1).

Sources: https://jakubtomsu.github.io/posts/fixed_timestep_without_interpolation/ — fixed-tick loops and their caps; https://domwillia.ms/devlog1/ — scaling the time owed by a speed multiplier; https://balaurengine.org/blog/pausing-a-game/ — pause stops the simulation clock while frame work continues; https://wiki.factorio.com/Time — ticks per second and game speed; https://forums.factorio.com/viewtopic.php?t=60842 — game speed as ticks per real second.
