# Research: ground-fields feature

The milestone's RESEARCH.md covers how a sample finds nearby entries: entries sorted by a packed cell key, found by binary search, and sorted by entity key before returning. This file covers what the feature adds to that.

## How can a ground field refuse a second change to one entity in a tick without a search over every change?

Fields on networks refuse a second publish from one source by inserting each source's slot into a list kept in source order. Kept fields don't refuse; the last change at a place wins. A ground field must refuse (the milestone's criterion 3), and with 2,000 guests changing their entries every tick, a sorted insert for each change would move about a million elements a tick.

Every guest changes its entry nearly every tick, so the swap has to touch every entry anyway. The ground field can therefore copy its readable entries into a working list at the first change of a tick, and apply each change to that list in place. The list is in key order, so finding an entity's entry is one binary search. Each entry carries the tick it last changed, so a second change in the same tick is seen at once. Only arrivals and departures insert or erase, and those are rare. At the swap the working list becomes the readable one. Dropped entries are marked in the working list instead of erased, so a drop also blocks a later change in the same tick. A drop for an entity with no entry inserts such a mark, so set-then-drop and drop-then-set are refused alike, whichever runs first.

Rejected: a hash set of the keys changed this tick — it allocates a node per key, inside a per-tick loop, which docs/conventions.md forbids. Appending changes and checking for repeats at the swap — the refusal would come after the system that caused it had returned, so a refused change could not "change nothing". Letting the last change win, as kept fields do — then two systems changing one entity would give a result that depends on their order (principle 10).

## Where does the cell order live, and when is it built?

The field's ByPlace orders are made on first use and kept in the slot, never visited by the walk. The cell order follows the same pattern. It is a list of each readable entry's packed cell and position, sorted by cell and then by position. The readable entries are in key order, so a stable sort keeps key order within a cell. The swap marks it stale, and the next sample rebuilds it in place, reusing its storage. A loaded world starts with it stale. If the box of cells a sample would visit holds more cells than the field has entries, the sample reads every entry in key order instead. Its work never exceeds a plain scan, and an infinite radius is handled too.

Rejected: rebuilding the order at the swap — a world loaded from a save or copied would still need the rebuild on first use, so building on first use covers both with one path.

Sources: src/sim/medium/field.h — ByPlace, made on first use and never visited; src/sim/medium/kept_field.h — a kept field's pending changes and swap.
