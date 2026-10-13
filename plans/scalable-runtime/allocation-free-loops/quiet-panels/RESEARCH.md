# Research: quiet-panels

## What the panels do each frame today

ToolingUi::build, in src/app/ui/tooling_ui.cpp, works out each panel's data and draws it with ImGui in the same function. The data is:

- The Debug panel's guest count: parkGuests(world).size(), which copies every guest's key into a new list each frame.
- The Departures panel's tally: departureTally, which makes a list of the keys holding the tally to read the first.
- The Hours panel's arrival curve: keepArrivalDay, which is kept and makes the day again only when the hours shown change. It allocates nothing while they stay.
- The Inspector: inspectSubject, which builds a new Inspection with every string formatted again each frame. For a guest it calls guestOptions, which makes its walk lists, route sample, entrance lists, and weights again. boxText calls parkBoxes and parkFurniture, which return new lists, for each row that names a box or a piece. For a shop it calls shopRecord, which makes the shop and depot key lists, copies the queue's holdings, and samples route lengths into new lists. For a bench or toilet block it calls facilityRecord, which calls parkBoxes.
- The Inspector's stock controls: a loop over parkBoxes(world) to find the shop's stock setting.
- The need tooltip: overlayPlace makes a list of line indices, and availabilityAt makes a new route sample and a new list of contributions.
- The Graph view: buildGraphOverlay copies every segment's key into a new list and returns new lists of lines and nodes.
- The shop ghost's tooltip: it only reads the kept preview's shop context. kept-preview made reading its footfall again allocation-free, and the check already holds a shop move.

The app also records every frame's time in FrameTimes for as long as it runs, whether or not --frame-times asked for them.

None of this needs ImGui, which only draws the result. The data can move into a component in tpj_app_core, which the check links, while the drawing stays in tpj_app.

## Filling text in place

A std::string keeps its heap buffer through clear(), and std::format_to with a back_inserter appends into it, so filling a string again reuses its memory once it has grown. Assigning the result of std::format replaces the buffer, freeing the old one. libstdc++ keeps strings of up to 15 characters inside the string object, so they never use the heap. Many inspector values are longer, such as "shop 1234 for hunger", "home for tiredness", and a last meal's line.

Destroying a string frees its heap buffer, which happens when a list of rows shrinks. In steady play, an inspection's lists keep their sizes, with one exception. A guest's rows are always 11, and its choice rows are always 6 while guestOptions gives options, which it stops doing only when the guest's place no longer resolves, after an edit to the guest paths. Heading home's terms drop out while the guest carries food and come back after it eats. Their labels and two-decimal values all fit the short-string buffer, so dropping them frees nothing. Reusing each list's rows in place, and resizing it to the rows used, is therefore enough.

A list of strings that grows moves its strings to a new block. A short string's object points into itself, so the new block doesn't hold the old block's bytes, and the check's rule for a growing list doesn't recognize the move. Each inspection list has a known largest size: 11 rows for a guest, 6 choices, and 6 home terms. Reserving the lists to those sizes at each fill means none of them grows after the first.

Rejected: fixed-size character arrays for the text — a full pieces line has no small bound, and truncating it changes what the player sees. sim's FixedList — emplace_back assigns a new value to the slot, which frees a string's buffer. Spare lists that keep the rows a list drops — in steady play the only rows dropped hold short strings, so they would add code that the check can't see doing anything.

Sources: https://en.cppreference.com/w/cpp/string/basic_string/clear — clear keeps the capacity; https://en.cppreference.com/w/cpp/utility/format/format_to — format_to writes through an output iterator; https://gcc.gnu.org/onlinedocs/libstdc++/manual/strings.html — libstdc++'s strings and their short-string buffer.

## Kept lists for the sim's record functions

The tick already keeps its lists in TickBuffers, which a module fills with a type of its own (src/sim/tick_buffers.h). guestOptions and shopRecord can take TickBuffers the caller keeps, and keep their lists in it under types private to their modules. Their callers then need to know nothing about walk lists or route lengths. The Inspector keeps its own TickBuffers, apart from the session's, so the two never share lists.

Several reads need one box or one piece of furniture: the Inspector names a target or facility, facilityRecord finds its box's kind, and the stock controls find a shop's setting. Today each copies every box or piece to find one. A lookup by key reads one entity's components and needs no list. shopRecord still needs every depot's key, for its orders on their way and its nearest depot.

Rejected: a public lists struct for each record function — it would expose the guests module's walk lists and the operations module's route lengths, which are internal. Keeping the box and furniture lists and refilling them only when the park's intent changes — it needs the session's edit count in legible and sim, and a lookup by key is cheaper still.
