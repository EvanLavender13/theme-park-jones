# Research: overlay-attribution

## Which place does a hover explain?

The capability asks for the value at the cursor's nearest path place, the rule the band's shading follows. The medium already measures that: nearestPlaceOn projects a ground point onto one carrier's segments and gives the nearest place, ties to the lower distance. Taking it on each guest path's carrier and keeping the nearest, ties to the lower path key, gives the nearest guest path place, never a connector's, and its straight distance from the cursor says whether the cursor lies within the band. This straight-line measure only finds where the cursor stands, as nearestPlace does for snapping, and the food there is still measured along routes (principle 4).

The shading interpolates between samples a meter apart and uses an averaged direction at bends, so on the outer side of a bend, or between samples, the hover's exact place can differ a little from the one a pixel's color came from. The hover is the exact value there; the shading is its picture.

Rejected: nearestPlace on the whole guest network. It can give a place on a shop's connector, which has no band. Rejected: reading the value back from the overlay mesh under the cursor. It would give the interpolated color's value, not an attributable one.

Sources: src/sim/medium/SPEC.md, nearestPlace and nearestPlaceOn; src/render/SPEC.md, the band's lines and samples.

## How should the attribution be shown?

Dear ImGui's tooltip follows the cursor and closes on its own when not submitted, so it needs no state. Its table API lays out one row per contribution with aligned columns and a header. With six numbers per shop (relief, route distance, wait, effective time, term) a table reads faster than one line of text per shop. The value is shown to three decimals, like the terms, so the terms listed and the total agree to the digit shown. They sum exactly in full precision, and the rounding of what is printed can leave the shown sum a unit off in the last digit.

Rejected: a pinned panel. It would need its own open state and a way to close it, which the inspectors bring later. Rejected: printing each value in full precision. Seventeen digits hide the explanation the tooltip exists to give.

Sources: .cpm-cache/imgui/5839/imgui.h, BeginTooltip and BeginTable, Dear ImGui 1.92.9b.
