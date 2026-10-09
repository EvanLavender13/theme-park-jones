# Feature: Opening Hours

## Summary

The park gets opening hours: the time of day it opens and the time it closes. A park with no hours set has the default hours, 9:00 to 21:00, so every existing park keeps them. The player sets the hours in a new Hours panel, which shows the two times with - and + buttons in quarter-hour steps and an Apply button. A choice not yet applied is a tentative edit, which the preview resolves on a candidate as it does a tool's edit. Apply commits it with a new SetOpeningHours command. The hours are saved park intent, and undo and redo cover them. Nothing reads the hours yet. believable-guests' a-day-out admits and sends home guests by them.

## Acceptance criteria

1. The Hours panel shows Opens and Closes on a 24-hour clock, each with - and + buttons, and an Apply button. Each button moves its time to the quarter hour before or after. Opens runs from 00:00 to 23:45, and Closes from 00:15 to 24:00, which is midnight. A + on Opens that brings it to or past Closes moves Closes to a quarter hour after it, and a - on Closes that brings it to or before Opens moves Opens to a quarter hour before it. A changed choice enables Apply. Apply commits it, and the panel keeps showing the choice, with Apply disabled once the park has those hours. Opening a park or starting a new one drops a choice not yet applied. (play)
2. A choice not yet applied is a tentative edit, a SetOpeningHours of the chosen hours, while the tool and the Inspector give no edit of their own. The preview resolves it on a candidate as it does any edit (legible/SPEC.md, Previews). (review, and the park world checks' chosen edit)
3. SetOpeningHours sets the park's opening hours and changes nothing else. It accepts any two times of day and is refused only when a time is a day or more after midnight. If the closing time is at or before the opening time, the hours run past midnight, and if the two are equal, the park is open all day. parkHours gives the park's hours. Undo and redo restore an hours change. (play: set the hours, undo, redo; and review)
4. A park with no hours set has the default hours, 9:00 to 21:00, and parkHours gives them. Hours set to the default save no opening-hours line, so every park saved before this feature loads, reads the default hours, and saves again to its own text. The parks remade by `tpj_scenarios --slice-parks`, fed.park loaded and saved again among them, are the same text as before the feature, which shows the simulation unchanged over thousands of ticks. (the park world checks, and the remade parks compared with cmp)
5. tests/parks/facilities.park holds hours of 10:00 to 22:00, so the park world checks cover the new state. (ctest)

## Tests

- The park world checks gain one chosen edit of warm.park, in tests/sim/park_world_checks_test.cpp: setting warm.park's opening hours to 10:00 to 22:00, that is opening 36000 and closing 79200 seconds after midnight, with SetOpeningHours, while guests are in the park. Like the other chosen edits, its candidate must equal the world that commits it, and both are among the checked worlds.

## Medium

The feature adds no fields or flows. The opening hours are park intent, which decision 0025 places outside principle 3.

- parkHours gives the park's hours. believable-guests' a-day-out will read them for arrivals and closing, and explained-needs will preview the arrival curve from a candidate with tentative hours. Nothing reads them in this feature.
- The app reads the hours through parkHours for the Hours panel and changes them only through SetOpeningHours.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: "addParkIntent registers the intent types entrance, path-point, path-segment, box, and shop-stock." becomes "... box, shop-stock, and opening-hours." "addParkEdits registers the fourteen commands." becomes "fifteen". The second paragraph gains a last sentence: "Other modules read the opening hours through parkHours, which returns the public value OpeningHours." In the third paragraph's list of public names, "DEFAULT_TAPER_LENGTH, which is 0," becomes "DEFAULT_TAPER_LENGTH, which is 0, DEFAULT_HOURS, which is 9:00 to 21:00,".
- Intent, after the entrance item: "- The park has opening hours: the second of the day it opens and the second it closes, each less than DAY_SECONDS. If the closing time is at or before the opening time, the hours run past midnight, and if the two are equal, the park is open all day. A park with no hours set has DEFAULT_HOURS, 9:00 to 21:00. parkHours gives the park's hours."
- Intent, the save lines gain: "- the park's opening and closing seconds, only when they are not DEFAULT_HOURS, in an opening-hours line."
- Commands: "Fourteen commands" becomes "Fifteen commands", and the list gains SetOpeningHours after SetShopStock. The paragraph naming the commands' arguments gains "SetOpeningHours names an opening and a closing second." RestoreIntent's paragraph gains, after its second sentence: "It can also name opening hours to set." The acceptance paragraph gains "For SetOpeningHours, it means both seconds are less than DAY_SECONDS." RestoreIntent's acceptance list gains "the hours it sets, if any, have both seconds less than DAY_SECONDS, and no record it adds holds hours;" before "and every box it sets or adds".
- Commands, the effects list gains after SetShopStock: "- SetOpeningHours sets the park's opening hours and changes nothing else. A park set to DEFAULT_HOURS holds no opening-hours line, as if none had been set." RestoreIntent's item gains: "If it names opening hours, it sets them last, as SetOpeningHours does."
- editedSegments and editedSurface: "for edits of boxes" becomes "for edits of boxes and of the opening hours" in both paragraphs.
- "A ParkEdit holds any one of the fourteen commands." becomes "fifteen".

src/tools/SPEC.md, History: after "A box's record holds its kind, pose, and stock setting." add "When an edit changes the opening hours, it also holds them before and after, and its reversal sets them back."

src/app/SPEC.md, Tooling UI, after the Time panel paragraph: "The Hours panel shows the park's opening hours on a 24-hour clock, as Opens and Closes, each with - and + buttons, and an Apply button below them. Each button moves its time to the quarter hour before or after. Opens runs from 00:00 to 23:45, and Closes from 00:15 to 24:00, which is midnight. A + on Opens that brings it to or past Closes moves Closes to a quarter hour after it, and a - on Closes that brings it to or before Opens moves Opens to a quarter hour before it. The panel shows the player's choice once they press a button, and the park's hours otherwise. While the tool and the Inspector give no edit of their own, a choice that differs from the park's hours is the edit the ghost and the preview show, a SetOpeningHours. Apply commits it, and it applies at the next tick, or at once while paused. The choice stays until the park has those hours, and Apply is disabled while the choice is the park's hours. Replacing the world drops the choice. With no saved layout the panel starts at the window's top center, below the Time panel."

Public interface:

- src/sim/park/intent.h: `struct OpeningHours { uint64_t Opening = 9 * 3600; uint64_t Closing = 21 * 3600; };` with operator==, `inline constexpr OpeningHours DEFAULT_HOURS{};`, and `OpeningHours parkHours(const World &world);`. addParkIntent's comment names opening-hours.
- src/sim/park/edits.h: `struct SetOpeningHours { OpeningHours Hours; };` with operator==, and isAccepted and applyCommand overloads for it. ParkEdit holds it between SetShopStock and RestoreIntent. IntentRecords gains `std::optional<OpeningHours> Hours;`, which a RestoreIntent's Set uses to set the park's hours.
- src/app/input/interaction.h: `hoursChoice()`, `chooseHours(OpeningHours)`, `applyHours(CommandQueue &) const`, and `settleHours(const World &)` on Interaction.
- src/app/ui/hours_panel.h: `HoursControls`, `HoursUse`, and `HoursUse drawHoursPanel(const HoursControls &hours);`.

## Files affected

- Modify: src/sim/park/intent.h
- Modify: src/sim/park/intent.cpp
- Modify: src/sim/park/internal/components.h
- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/tools/history.cpp
- Modify: src/tools/SPEC.md
- Modify: src/app/input/interaction.h
- Modify: src/app/input/interaction.cpp
- Create: src/app/ui/hours_panel.h
- Create: src/app/ui/hours_panel.cpp
- Modify: src/app/CMakeLists.txt
- Modify: src/app/ui/tooling_ui.h
- Modify: src/app/ui/tooling_ui.cpp
- Modify: src/app/scene/scene_sync.h
- Modify: src/app/SPEC.md
- Modify: tests/parks/facilities.park
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)

## Dependencies

- benches-and-toilets: tests/parks/facilities.park. Merged.
- shop-stock: the Interaction's chosen stock, which the hours choice follows, and facilities.park's shop-stock section. Merged.
- deterministic-simulation/park-clock: the second of the day the hours are stated in, and the Time panel. Complete.

## Out of scope

- Admitting guests by the hours and sending them home at closing: believable-guests' a-day-out.
- Previewing the arrival curve in the Hours panel: legible-simulation's explained-needs.
- Setting hours past midnight or all day from the panel. The command accepts them, and a park file can hold them.
- Hours that differ by day: the milestone's deepening candidates hold it.

## Open questions

None.
