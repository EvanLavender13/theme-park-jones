# Milestone: Park Clock

Slice: day-at-the-park

Status: complete

## Summary

The park gets a time of day. The clock gives the day number and the time of day as a pure function of the tick. Tick 0 is 9:00 on day 1, and each tick is two game seconds, so at normal speed one game minute passes each real second and a day is 43200 ticks. The app shows the day and time in a Time panel, with buttons to pause or to run at 1x, 2x, or 4x. A faster speed steps more ticks in each frame and never changes what a tick does. While paused, no tick steps, but the player's edits, undo, and redo still apply at once. This is member 1 of day-at-the-park. Arrivals, closing, the light, and the hours panel all read this clock, so it comes first.

## Acceptance criteria

1. The clock gives the day number and the time of day for a tick. Tick 0 is 9:00 on day 1. Each tick adds GAME_SECONDS_PER_TICK, two game seconds, so a day is DAY_TICKS, 43200 ticks. A new day starts at midnight: tick 5400 is 12:00 on day 1, tick 27000 is 0:00 on day 2, and tick 43200 is 9:00 on day 2. The clock uses whole numbers only, so it is the same on every build. (a capture of `--park tests/parks/main-street.park --ticks 5400`, whose Time panel reads 12:00 on day 1, and review)
2. The clock adds no state. Saves, hashes, and every park's simulation are unchanged. (`tpj_scenarios --compare` on windows-debug output from before and after)
3. The Time panel shows the day and the time of day in hours and minutes, and buttons for pause, 1x, 2x, and 4x, with the current speed marked. Space pauses and unpauses, and 1, 2, and 3 choose 1x, 2x, and 4x, unless a panel is taking the keyboard. A park opens at 1x. At 1x the clock advances one game minute each real second, at 2x two, and at 4x four. (play)
4. At each speed, a frame steps the ticks its real time covers times the speed, at most MAX_FRAME_TICKS times the speed, and never a burst after a stall or an unpause. The speed changes only how many ticks a frame steps, so the world at a given tick is the same at any speed (principle 10). On tests/parks/stress/full.park at 4x, the app keeps a steady frame rate and runs the park as fast as its ticks allow. (review; `--speed 4 --frames 600 --frame-times` on full.park, reporting the median and the slowest 5% of frame times; and play: on full.park the clock runs faster at 4x than at 2x)
5. While paused, no tick steps: the clock stops, and guests and shipments stand still. The camera, the tools, the ghost, the overlay, and the panels keep working. An edit, an undo, or a redo the player makes while paused applies at once, and the world after it equals the candidate its preview showed (principle 8). After a paused edit, everything kept from the world is made again as after a tick: the preview, so the ghost, the overlay, and the tooltips show the edited park, and the scene, so guests an edit moves are drawn where they now stand. Unpausing goes on from the same tick. (play, and the park world checks, which commit each chosen edit without stepping as well as through stepWorld, and check it equals its candidate)
6. `--speed N` starts the app at that speed, with 0 meaning paused, so captures and timings can choose it. Any other N prints the usage and exits with a nonzero status. (the app's command line)
7. src/sim/SPEC.md describes the clock and applying edits without stepping. src/app/SPEC.md describes the speeds, the Time panel, the keys, and --speed, and says that edits, undo, and redo apply at once while paused, where it now says they apply at the next tick, and that guests are rebuilt after a paused edit. src/legible/SPEC.md says keepPreview also makes the preview again after an edit applied without a tick. (review)

## Medium

The milestone adds no fields or flows. The clock isn't an interaction between entities. It is part of the tick cycle itself (the slice's medium map). It is a pure function of World::Tick in the sim root, which every capability may read. Later members read it: believable-guests for arrivals and closing, stylized-presentation for the light, and legible-simulation for the hours panel. Rendering and the app only read it and never feed anything back (principle 10).

Applying edits without stepping is a new entry point to the tick cycle, beside stepWorld and makeCandidate. It applies queued commands and resolves exactly as makeCandidate does, but to the world itself. It doesn't step systems, swap buffers, or advance World::Tick. Since the tick doesn't change, whatever legible-simulation and the app keep from a world, such as the preview and the guest meshes, must notice a paused edit by something other than the tick.

## Dependencies

- world-as-value: the tick cycle, World::Tick in saves, and makeCandidate. Met.
- effortless-building/path-network: undo and redo, which must work while paused. Met.

## Core feature

`time-of-day`. It gives the clock and shows it, so every later member has a time to read, and the player sees the day pass. It changes no simulation state, so it lands safely first.

## Features

1. `time-of-day`: the clock in the sim root, with GAME_SECONDS_PER_TICK and DAY_TICKS, and a Time panel in the app showing the day and the time of day. Depends on: none.
2. `game-speed`: pause, 1x, 2x, and 4x in the frame clock, the Time panel's buttons, the keys, --speed, and applying edits, undo, and redo without stepping while paused. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Faster speeds for quiet hours, such as 8x through the night, once ticks are cheap enough to run them. Gated on: the runtime keeping full.park at 4x.
- Skipping the closed hours at a high speed with one button, until the next opening.
- A calendar beyond the day number, such as weekdays and seasons. Gated on: a mechanic that reads them.

## Open questions

None.

## Research notes

Games with a fixed tick speed up by stepping more ticks each frame, never by lengthening a tick, and pause stops only the simulation's clock. A paused park applies edits with the cycle's apply-and-resolve stage alone, as makeCandidate does to a copy. Details are in RESEARCH.md.
