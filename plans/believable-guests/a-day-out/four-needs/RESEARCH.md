# Research: four-needs

The milestone's RESEARCH.md covers the design questions. This file covers how to build four needs in this code.

## How should one guest's code handle four needs without four copies of each path?

served-needs met the same question for goods and facilities. Its answer is a traits template for each kind, listed in one array and walked by a forEach helper (src/sim/operations/operations.cpp, GoodKinds and forEachGood; src/sim/operations/facilities.cpp, FacilityKinds and forEachFacility). Each trait names the kind's flow and field types and its constants. A loop over the array then runs the same template code for each kind. Adding a kind later means adding an entry, not a code path.

Guests need the same shape, keyed by need: each need names its offer field, its visit and serving flows, the cause its serving is consumed with, and how much a serving fills the bladder. A guest's choice, visit, and return then become one template walked over the needs. The guest's runtime target need picks the matching trait by comparing inside the walk, as operations.cpp's visitsHeld does for a good.

The four need levels stay four named fields of the guest's state, not an array. The save writes each field by name, so a park file stays readable by hand. A small accessor maps a need to its field.

Rejected: a switch on the need at each use — every new need would touch each switch. One visit kind for all needs, with the need carried in the visit — the slice's medium map sets one visit kind per need, and the saved flow names are fixed.

Sources: src/sim/operations/operations.cpp — GoodKinds, forEachGood, visitsHeld; src/sim/operations/facilities.cpp — FacilityKinds, forEachFacility.

## How can the feature show that parks without the new facilities simulate as before?

A guest's existing draws are keyed by purpose names, so new draws under new purpose names change none of the old ones (src/sim/draw.h). In a park whose shops all sell food and which has no bench or toilet block, every new need adds no option. The options come in the same order, with the same scores, so every pick is the same. The only differences are in the saved guest lines: the new fields, the renamed serving relief field, and the renamed activity. If those changes are undone with a text substitution, a remade park should equal the checked-in one exactly. That gives a direct check without the scenarios comparison, which can't compare across a change to the saved fields.

Sources: src/sim/guests/guests.cpp — addGuest's draws and scoreOptions.
