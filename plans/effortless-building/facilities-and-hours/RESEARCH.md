# Research: facilities-and-hours

## How do park games let the player set opening hours and what a shop sells?

Planet Coaster keeps the park's opening hours in the park management panel, where the player sets an opening and a closing time. By players' reports the hours there are mostly cosmetic, which the slice's RESEARCH.md already rejects: here they will drive arrivals and closing. RollerCoaster Tycoon has no hours at all. In these games a shop's goods are set in the shop's own window, picked from the items that kind of shop can sell, and the change takes effect at once. None of them previews what a change will do before the player commits it, which principle 8 asks for here.

This suggests two small controls. The hours sit in a panel of their own, as two times of day in whole steps the player can't mistype, with an Apply button, so the slice's preview of the arrival curve has a tentative value to show before it is committed. A shop's stock sits in the shop's Inspector, as Food, Drink, or Both, again with Apply, so the overlay can preview the coverage a change would add. Both are edits like any other: commands queued between ticks, previewed on a candidate, saved, and undone.

## How should the new intent fit the park file?

A park file lists a section for each component type its entities hold, and a type no entity holds has no section. Adding a field to an existing type, such as a stock on every box, would break every park file saved before it, since their box lines lack the field. Keeping each new setting in a component of its own leaves every existing park file valid, and a missing component reads as the default: no hours entity means the default hours, and a shop with no stock component stocks food only. This is the slice's rule for parks made before it.

Benches and toilet blocks are boxes of new kinds. A kind is saved by name, so existing files, which name only shop and depot, load unchanged.

Rejected: a stock field on every box — every existing park file would fail to load, and a depot, bench, or toilet block would carry a meaningless setting. Hours on the entrance — a park could have several entrances later, and the hours belong to the park. Applying a stock or hours change the moment the control moves — the slice previews both before the player commits (principle 8). Refusing hours whose closing isn't after their opening — a convenience gate on a player policy (principle 5). Instead the Hours panel keeps closing after opening, and a pair that isn't means hours past midnight, or all day when the two are equal.

Sources: https://steamcommunity.com/app/493340/discussions/0/3779119714748339083/ — Planet Coaster's hours in park management; https://forums.planetcoaster.com/showthread.php/17954-Am-I-missing-something-or-are-park-opening-times-irrelevant — hours that do little; the slice's RESEARCH.md; src/sim/save.cpp and tests/parks/new.park — how sections are written.
