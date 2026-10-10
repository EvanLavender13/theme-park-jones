# Research: light-preview

The milestone's RESEARCH.md settles the light itself. This file covers how the Light panel shows and picks a time of day.

## What should the panel let the user change?

SteelJones's day-cycle panel edits its tuning live: a time-of-day slider, an advance rate, and the sun's heading, all ImGui sliders over the live values. The user chose the hour alone. The user scrubs to an hour, sees its light, and names the values to change, which keeps the panel small and leaves nothing to save.

Rejected: live editing of the keyframes' colors and strengths — about three times the panel, and the values are lost at exit unless an export is added. The milestone's deepening candidates hold it. A player-facing daylight toggle — the capability's deepening candidates keep it as a separate idea with player wording.

Sources: /mnt/c/Users/EvanUhhh/source/repos/SteelJones src/render/day_cycle.cpp — its tuning panel, with time of day as a SliderFloat over [0, 1).

## How should the slider show a time of day?

SteelJones's slider runs over a fraction of the day, which reads poorly as an hour. An integer slider over the minutes of the day, 0 to 1439, steps by whole minutes. ImGui formats a slider's value with its format string, and a format with no conversion is shown as it is, so the panel can pass the minute already written as HH:MM. ImGui's demo uses the same pattern to show an enum's name on a SliderInt. Ctrl+click turns a slider into a text box that parses the format, so the slider should take ImGuiSliderFlags_NoInput. A minute per step is fine for scrubbing; across a slider about 200 pixels wide, one pixel is about seven minutes.

While the preview is off, the slider should follow the clock's time of day. Then turning the preview on starts from the light already on screen, with no jump, and the panel doubles as a readout of where the clock is in the day's light.

Rejected: a SliderFloat over hours — decimal hours such as 18.50 read worse than 18:30. A preview clock that runs on its own — scrubbing and the park's own clock at 4x already cover watching a day.

Sources: .cpm-cache/imgui/5839/imgui.h and imgui_widgets.cpp (1.92.9b) — slider formats, ImGuiSliderFlags_NoInput, and DataTypeFormatString, which passes the format straight to the formatter.

## How should --light-hour take its value?

The milestone's captures need 12, 18, and 23, all whole hours. A whole hour from 0 to 23, parsed like --ticks, keeps the option as strict as --speed. Minutes can come later if a capture needs one.
