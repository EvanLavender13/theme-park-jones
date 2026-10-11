# Feature: Day Panels

## Summary

A Departures panel shows today's departures by reason, one fixed row for each of hunger, thirst, tiredness, the toilet, and closing, and their total, counted from the park's last opening. The Hours panel draws the arrival curve through the day, the guests per game hour each entrance admits, for the hours it shows, with a line marking the park's time of day. While the player's chosen hours differ from the park's, the curve is read from a candidate with only those hours applied, so it shows the new hours before Apply, whatever other edit the ghost previews.

## Acceptance criteria

1. The Departures panel shows six rows: Hunger, Thirst, Tiredness, Toilet, Closing, and Total, each with a count. The counts are departureTally's, which counts from the park's last opening, and Total is their sum. They follow every tick, and the rows stay the same whatever the park holds. (play: day.park at 4x through closing; capture: `--park tests/parks/evening.park --ticks 3000 --speed 0`, which shows guests counted under Closing)
2. The Hours panel draws the arrival curve from 00:00 to 24:00 below its Apply button: zero while the park is closed, starting at Opens, peaking early in the hours, and falling to zero before Closes, at the same peak height whatever the hours. A vertical line marks the park's time of day, and moves with the clock. (capture: `--park tests/parks/noon.park --speed 0`, the mark at 12:00 inside the hump; play: day.park at 4x, watching the mark move)
3. Pressing - or + on Opens or Closes redraws the curve for the chosen hours at once, before Apply. With chosen hours that differ from the park's, choosing the bench tool and hovering a placement keeps the curve on the chosen hours. Apply keeps the curve where it is. (play: day.park)
4. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after exits 0. src/legible/SPEC.md and src/app/SPEC.md describe the curve and the panels. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Tasks 1 and 11)

## Tests

None. The panels are checked by captures and play, and the scenarios comparison shows the simulation unchanged (docs/testing.md).

## Medium

No fields or flows are added, and the simulation uses nothing this feature produces.

- Inspection records (believable-guests, decision 0025): departureTally, for the Departures panel, and arrivalCurve, for the Hours panel.
- Candidate resolution (deterministic-simulation, decision 0025): a candidate with only a SetOpeningHours of the chosen hours applied, made through previewEdit, which the Hours panel reads the curve from.
- Park intent (effortless-building): the park's opening hours, to tell whether the chosen hours differ.
- The clock (deterministic-simulation): the park's time of day, which the Hours panel marks on the curve.

The curve and the counts are read for display and never enter the world.

## Spec changes

src/legible/SPEC.md:

- Boundary, under "The app calls:", add a bullet after inspect.h's: "arrivals.h for keepArrivalDay".
- Add a section after Inspectors:

      ## Arrivals

      An arrival day is the arrival curve through a day as the park would have it with some opening hours: the guests per game hour each entrance admits at the start of each game minute from 00:00 (sim/guests/SPEC.md). If the hours differ from the park's, it is read from a candidate with only those hours applied (decision 0025), and otherwise from the park itself. keepArrivalDay keeps one between frames and makes it again only when the hours differ from the kept day's. The arrival curve reads only the hours, so a kept day stays exact while the park ticks and changes.

src/app/SPEC.md, Tooling UI:

- In the Hours panel's paragraph, before its last sentence, add: "Below Apply the panel draws the arrival curve from 00:00 to 24:00 for the hours it shows, the guests per game hour each entrance admits, at a fixed height scale (legible/SPEC.md, Arrivals). A vertical line marks the park's time of day. The curve follows the player's choice at once, before Apply, whatever edit the ghost shows."
- After the Hours panel's paragraph, add a paragraph: "The Departures panel shows how many guests have left the park since it last opened, one row for each reason, Hunger, Thirst, Tiredness, Toilet, and Closing, and their Total (sim/guests/SPEC.md). It follows every tick. With no saved layout it starts at the window's top right, below the Debug panel."

The public interface:

- src/legible/arrivals.h, new:

      // The game minutes of a day, at whose starts an arrival day samples the arrival curve.
      inline constexpr size_t ARRIVAL_SAMPLES = DAY_SECONDS / 60;

      // The guests per game hour each entrance admits at the start of each game minute of the day,
      // from 00:00.
      using ArrivalDay = std::array<double, ARRIVAL_SAMPLES>;

      // An arrival day kept between frames, and the hours it was made for, or none before the first.
      struct KeptArrivalDay {
        std::optional<OpeningHours> Hours;
        ArrivalDay Day{};
      };

      // Makes the kept day again for the hours when nothing is kept or the kept day's hours differ,
      // and returns whether it did: arrivalCurve's samples of a candidate with only a SetOpeningHours
      // of the hours queued, or of the world itself when it has those hours. Changes nothing.
      bool keepArrivalDay(KeptArrivalDay &kept, const World &world, OpeningHours hours);

- src/app/ui/departures_panel.h, new:

      // Draws the Departures panel: a row for each reason in HOME_REASONS order, with its count from
      // the tally, and a Total row with their sum. Call between ImGui::NewFrame and ImGui::Render.
      void drawDeparturesPanel(const DepartureTally &tally);

- src/app/ui/hours_panel.h: drawHoursPanel gains two parameters, `const ArrivalDay &curve, uint64_t second`, and its comment adds: "Below Apply it draws the curve from 00:00 to 24:00, scaled to ARRIVAL_CURVE's peak, with a vertical line at the second of the day."

## Files affected

- Create: src/legible/arrivals.h
- Create: src/legible/arrivals.cpp
- Create: src/app/ui/departures_panel.h
- Create: src/app/ui/departures_panel.cpp
- Modify: src/legible/CMakeLists.txt
- Modify: src/legible/SPEC.md
- Modify: src/app/SPEC.md
- Modify: src/app/ui/hours_panel.h
- Modify: src/app/ui/hours_panel.cpp
- Modify: src/app/ui/tooling_ui.h
- Modify: src/app/ui/tooling_ui.cpp
- Modify: src/app/CMakeLists.txt

## Dependencies

- believable-guests' a-day-out: departureTally and arrivalCurve. Met.
- effortless-building's facilities-and-hours: the Hours panel and its chosen hours. Met.
- legible's previewEdit, which makes candidates. Met.

## Out of scope

- Departures by reason over past days. It is in the milestone's deepening candidates.
- Hovering the curve to read a time and rate.

## Open questions

None.
