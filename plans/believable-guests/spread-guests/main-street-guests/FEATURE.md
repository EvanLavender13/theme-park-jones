# Feature: Main Street Guests

## Summary

tpj_scenarios --slice-parks also makes tests/parks/warm-main-street.park: main-street.park warmed for MAIN_STREET_WARM_TICKS, two and a half minutes of play, until guests fill it. The park world checks and the cross-build check then cover a park full of guests walking on wide paving, holding guest presence. tests/parks/stress/full.park is made again, so its guests hold guest presence too. A capture of warm-main-street.park gives the good-paths slice its scripted capture of guests on the main street. The runtime report on main after this feature becomes the slice's new baseline. Nothing about how guests behave changes.

## Acceptance criteria

1. tpj_scenarios --slice-parks fed warm cut main-street warm-main-street makes warm-main-street from main-street warmed MAIN_STREET_WARM_TICKS cycles, as it makes warm from fed. It reads main-street and doesn't write it. Run on tests/parks, it adds tests/parks/warm-main-street.park and leaves fed.park, warm.park, cut.park, and main-street.park unchanged. (the command and `git status --short tests/parks`)
2. warm-main-street.park holds guests in every activity, spread along the main street and the side paths. Every guest that isn't waiting has an entry in guest presence, and every guest is on the paving. The park world checks, food_loop_test, and the rest of ctest pass. The cross-build check covers it at release, as it covers every park in tests/parks. (the presence check, the stays check, and ctest)
3. A capture of warm-main-street.park shows the paths as one paved surface with rounded inside corners, an edge line only along the outline, and square free ends. The shops face the street in rows, and guests stand spread across the main street, every one on the paving. This is the good-paths slice's criterion 4. The capture frames the whole park, as the app does on opening one. (`ThemeParkJones.exe --park tests/parks/warm-main-street.park --capture`, and the stays check)
4. tests/parks/stress/full.park is made again, and its guests hold guest presence. (tpj_bench --full-park and the presence check)
5. The runtime report on main after this feature is kept as the good-paths baseline, and its full.park medians are written into PLAN.md. On full.park, the tick stays inside the 33 ms budget, and a guest's sample of guest presence finds only the few guests near it, so its work grows with the guests near it, not with every guest. This confirms ground-fields' deferred criterion 7. (scripts/runtime-report.sh, and the cost check)
6. src/scenarios/SPEC.md describes warm-main-street.park and the new form of --slice-parks. (review)

## Tests

None. warm-main-street.park is a park file in tests/parks, so the park world checks and the cross-build check cover it with no new test (the good-paths slice's acceptance criteria).

## Medium

None. The feature makes park files with the simulation as it is. warm-main-street.park holds guest presence, so the park world checks cover its entries on a park of wide paving.

## Spec changes

src/scenarios/SPEC.md, in Slice parks:

- Replace "tpj_scenarios --slice-parks fed warm cut makes the boxes-and-tubes slice's parks from fed:" with:

    tpj_scenarios --slice-parks fed warm cut main-street warm-main-street makes the boxes-and-tubes slice's parks from fed, and the good-paths slice's warm park from main-street:

- Add a bullet after the one for cut:

    - warm-main-street is main-street after MAIN_STREET_WARM_TICKS cycles, long enough that guests arrive and leave about equally often, so they fill the park in every activity. main-street is read and never written.

- Replace "tests/parks/fed.park, warm.park, and cut.park are its output." with:

    tests/parks/fed.park, warm.park, cut.park, and warm-main-street.park are its output.

## Files affected

- Modify: src/scenarios/SPEC.md
- Modify: src/scenarios/slice_parks.h, src/scenarios/slice_parks.cpp (MAIN_STREET_WARM_TICKS and warming main-street)
- Modify: src/scenarios/main.cpp (--slice-parks takes five files; composition only)
- Create: tests/parks/warm-main-street.park
- Modify: tests/parks/stress/full.park (made again)

## Dependencies

- paving-walk, early-turns, own-pace-and-side, and sidestep: merged. The parks hold every change.
- main-street.park (effortless-building's path-network): met.

## Out of scope

- The good-paths slice's integration tests, including the crossing over warm-main-street.park's street (slice criterion 1). The slice's closing pass writes them.
- winding-path.park. It holds no guests, so nothing in it changes.

## Open questions

None.
