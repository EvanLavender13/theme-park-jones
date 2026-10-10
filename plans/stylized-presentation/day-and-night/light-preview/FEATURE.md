# Feature: Light Preview

## Summary

A Light panel previews the light at any time of day without changing the park's clock. It has a Preview checkbox and an Hour slider that shows a time of day in hours and minutes. While Preview is off, the slider follows the clock. Dragging the slider turns Preview on, and the scene is lit by the light at the slider's time. Turning Preview off lights the scene by the clock again. The --light-hour H option starts the app with the preview on at H:00, so a capture shows any hour's light on the same world. With it, the milestone's comparisons of the overlay's and guests' colors under two hours' light can be made, and the user can scrub through the day to tune the light.

## Acceptance criteria

1. The Light panel has a Preview checkbox and an Hour slider showing a time of day as HH:MM. While Preview is off, the slider follows the Time panel's clock. Dragging the slider checks Preview, and the scene shows the light at the slider's time. Unchecking Preview shows the clock's light again. While previewing, the Time panel's clock and the Debug panel's tick keep running as before. (play: day.park in the release app)
2. The light changes smoothly through the whole day, with no jump at sunrise, sunset, or midnight. This is the milestone's criterion 6. (play: drag the slider slowly from 00:00 to 23:59, then compare 23:59 with 00:00)
3. --light-hour H starts the app with Preview checked and the slider at H:00. A capture of noon.park with --light-hour 12 matches one without it, apart from the Debug panel's frame rate and the Light panel's checkbox. A capture of noon.park with --light-hour 23 shows the noon world, guests and all, at night. (capture: `--park tests/parks/noon.park --speed 0`, with no --light-hour, with --light-hour 12, and with --light-hour 23)
4. The app prints the usage and exits with a nonzero status for --light-hour 24, for --light-hour x, and for --light-hour 5 with --hash. (check: PLAN.md Task 8)
5. The food overlay's colors are the same under noon's and night's light. Within the overlay's shape, every pixel of a capture at --light-hour 12 matches the same pixel at --light-hour 23, apart from the Debug and Light panels and from paving farther than 250 m from the eye, which fades to the sky. (capture: `--park tests/parks/noon.park --overlay food --speed 0`, at --light-hour 12 and 23, compared by PLAN.md Task 9)
6. Guests take the light's brightness but not its color. In a capture at --light-hour 18, each guest has the hue it has at --light-hour 12, only darker. (capture: `--park tests/parks/noon.park --speed 0`, at --light-hour 12 and 18, compared)
7. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after exits 0. src/app/SPEC.md describes the Light panel and --light-hour. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Tasks 1 and 10)

The milestone's criterion 7 also asks that ghosts, highlights, handles, and join marks keep their colors at night. hour-light's play at 23:00 confirmed it, and this feature doesn't change how they draw.

## Tests

None. The panel is checked by play and the option by captures and the usage check (docs/testing.md). The scenarios comparison shows the simulation unchanged.

## Medium

No fields or flows. The preview is UI state in the app. The app hands the renderer the light at the preview's second of the day in place of the clock's, which is composition. Nothing reaches the world: the preview queues no edit and changes neither the world nor its tick.

## Spec changes

src/app/SPEC.md:

- The scene, the last sentence becomes: "The scene is lit by the light at the world's time of day (render/SPEC.md, The light), unless the Light panel previews another time."
- Tooling UI, a new paragraph after the Hours panel's: "The Light panel has a Preview checkbox and an Hour slider that shows a time of day on a 24-hour clock in hours and minutes. While Preview is unchecked, the slider shows the park's time of day and follows it. Dragging the slider checks Preview. While Preview is checked, the scene is lit by the light at the slider's time, and the slider stays where it was put. Unchecking Preview lights the scene by the park's time of day again. The preview changes only the light. The clock, the tick, and the world are the same with it as without it. Opening a park or starting a new one keeps the preview. With no saved layout the panel starts at the window's bottom right."
- Command line, a new item after --overlay: "--light-hour H starts the app with the Light panel's Preview checked and its slider at H:00, so a capture shows that hour's light on any world. H is a whole hour from 0 to 23."
- Command line, the list of reasons to print the usage gains "a --light-hour value that is not a whole hour from 0 to 23", after the --speed item.

The public interface:

- src/app/options.h: Options gains `std::optional<uint64_t> LightHour;`, the hour --light-hour gives. parseOptions's comment names --light-hour, its range, and that --hash refuses it.
- src/app/ui/light_panel.h, new:
  - `struct LightPreview { bool On = false; uint64_t Second = 0; };`, whether the preview is on and the second of the day it shows.
  - `void drawLightPanel(uint64_t clockSecond, LightPreview &preview);`
- src/app/ui/tooling_ui.h: the ToolingUi constructor gains a `LightPreview light` parameter after the speed, and ToolingUi gains `[[nodiscard]] const LightPreview &lightPreview() const`.

## Files affected

- Modify: src/app/SPEC.md
- Modify: src/app/options.h, src/app/options.cpp
- Create: src/app/ui/light_panel.h, src/app/ui/light_panel.cpp
- Modify: src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp
- Modify: src/app/application.cpp (composition only: start the preview from the options, and light the frame by the preview or the clock)
- Modify: src/app/CMakeLists.txt

## Dependencies

- hour-light: dayLightAt and dayLightOf (src/render/day_light.h). Met.
- noon.park and day.park in tests/parks. Met.

## Out of scope

- Live editing of the keyframes' colors and strengths in the panel. The user chose the hour alone. It goes to the milestone's deepening candidates.
- A daylight toggle for the player. The capability's deepening candidates keep it.
- Minutes in --light-hour. The milestone's captures need whole hours.

## Open questions

- The tuning values. The user scrubs the day with the panel and names any values to change.
