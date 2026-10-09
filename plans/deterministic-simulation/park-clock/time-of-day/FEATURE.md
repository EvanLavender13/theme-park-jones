# Feature: Time of Day

## Summary

The park gets a clock. parkTime gives the day number and the time of day for a tick: tick 0 is 9:00 on day 1, and each tick is two game seconds, so a day is 43200 ticks and at normal speed a game minute passes each real second. The clock is computed from World::Tick alone, so it adds no state, and the simulation doesn't change. The app shows the park's day and time in a new Time panel at the top of the window. Later members of day-at-the-park read the clock for arrivals, closing, the light, and the hours panel. game-speed, the milestone's next feature, adds the speed buttons to the Time panel.

## Acceptance criteria

1. parkTime gives a tick's day, counted from 1, and its second of the day, from 0 to 86399. Tick 0 is 9:00 on day 1. Each tick adds GAME_SECONDS_PER_TICK, two game seconds, so DAY_TICKS is 43200. A new day starts at midnight. The clock is computed in whole numbers from the tick alone. (captures in criterion 3, and review)
2. The clock adds no state, so saves, hashes, and every park's simulation are unchanged. (`tpj_scenarios --compare` on windows-debug output from before and after the feature)
3. The Time panel shows the park's day and its time of day on a 24-hour clock in hours and minutes, as `Day 1  09:00`. It follows every tick. With no saved layout it starts at the top center of the window, clear of the Tools and Debug panels. A capture of main-street.park after `--ticks 5400` shows `Day 1  12:00`, and after `--ticks 27000` shows `Day 2  00:00`. (`ThemeParkJones.exe --park tests/parks/main-street.park --ticks N --capture`, and play: the minutes advance one each real second)
4. src/sim/SPEC.md describes the clock and lists sim/clock.h among the sim's public headers. src/app/SPEC.md describes the Time panel. (review)

## Tests

None. The clock adds no state, and showing it is checked by capture and play (docs/testing.md).

## Medium

None. The clock is part of the tick cycle, not an interaction between entities (the slice's medium map). It reads only World::Tick. The app reads it and feeds nothing back (principle 10).

## Spec changes

src/sim/SPEC.md:

- In Boundary, add `- sim/clock.h` to the list of headers, after `- sim/sim_math.h`.
- Add a section after "The tick cycle":

    ## The clock

    parkTime(tick) gives the park's time at a tick, a ParkTime: its Day, counted from 1, and its Second of that day, from 0 to 86399. Tick 0 is 9:00 on day 1. Each tick adds GAME_SECONDS_PER_TICK to the clock, so a day is DAY_TICKS ticks, and a new day starts at midnight. A park's time is parkTime of its World::Tick. The clock is computed in whole numbers from the tick alone, so it is the same on every build, and it adds nothing to a world's state or save.

src/app/SPEC.md, in Tooling UI, add a paragraph after the one about the Debug panel:

    The Time panel shows the park's day and its time of day on a 24-hour clock in hours and minutes, such as `Day 1  09:00` (sim/SPEC.md, The clock). It follows every tick. With no saved layout it starts at the window's top center.

## Files affected

- Create: src/sim/clock.h, src/sim/clock.cpp
- Modify: src/sim/CMakeLists.txt (adds clock.cpp)
- Modify: src/sim/SPEC.md
- Create: src/app/ui/time_panel.h, src/app/ui/time_panel.cpp
- Modify: src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp (draws the Time panel)
- Modify: src/app/CMakeLists.txt (adds ui/time_panel.cpp to tpj_app)
- Modify: src/app/SPEC.md

## Dependencies

- world-as-value: World::Tick, which saves hold. Met.

## Out of scope

- Pause and the game speeds, and their buttons in the Time panel. game-speed adds them.
- Opening hours, arrivals by the hour, and the light. Later day-at-the-park members read the clock for them.
- A calendar beyond the day number. It is the milestone's deepening candidate.

## Open questions

None.
