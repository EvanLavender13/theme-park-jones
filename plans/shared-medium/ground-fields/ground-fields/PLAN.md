# Implementation Plan: Ground Fields

## Goal

Add the ground field kind to the shared medium, with registration, changes while stepping, the swap, and sampling within a radius, and exercise it in the stalls scenario.

## Approach

One header-only kind, src/sim/medium/ground_field.h, follows kept_field.h. A state component on the field's entity holds the readable entries in key order and a working list for the current tick. The first change of a tick copies the readable entries into the working list. Each change then finds its entity by binary search and stamps the tick, so a second change is seen at once. The swap makes the working list readable and drops marked and gone entries. A sample reads an order by cell, made on first use after each swap and never visited, and sorts what it finds by key. The milestone's RESEARCH.md and this feature's RESEARCH.md give the reasons.

## Placement

Decision 0027 places each behavior this feature adds:

- The ground field kind, with its registration, changes, swap, and sampling: sim/medium, new public header sim/medium/ground_field.h. The medium owns every kind of field (decision 0005), and each kind beyond plain fields has its own header there, as kept_field.h does.
- The visitor presence field and its use: scenarios, src/scenarios/stalls.h and stalls.cpp. The stalls scenario is the synthetic scenario that exercises the medium for the cross-build check (src/scenarios/SPEC.md).

## Tasks

### Task 1: Record the scenario output before the change

Files: none.

Step 1: Build tpj_scenarios on the unchanged tree and save its output.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 600 > build/windows-debug/ground-before.txt`
Expected: the build succeeds, and the file starts with `scenario walkers tick 0 hash`.

### Task 2: Describe ground fields in the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md:3`, `src/sim/medium/SPEC.md:9-14`, `src/sim/medium/SPEC.md:39`, and a new section after Kept fields, before `## Flows` (line 78)

Step 1: Make the opening, Boundary, Places, and new Ground fields changes exactly as FEATURE.md's Spec changes section gives them.

### Task 3: Name ground fields in the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md:25`

Step 1: Change "the medium's fields and flows" to "the medium's fields, ground fields, and flows".

### Task 4: Describe ground fields in the capability brief

Files:
- Modify: `plans/shared-medium/CAPABILITY.md` (Medium section from line 68, and the Principle 4 bullet at line 113)

Step 1: In Medium, change "Every field is sampled at places." to "Every field on a network is sampled at places." After the paragraph that starts "Resolved entries are derived", add: "A ground field holds one entry for each entity at a ground position, instead of entries at places. Systems set and drop entries while stepping, and an entry stays until it changes. A consumer samples it within a radius of a ground point and gets the entries there in key order. When an entity is gone, its entry goes too. believable-guests' guest presence is the first ground field."

Step 2: Replace the Principle 4 bullet with: "Principle 4 is kept by measuring every distance between places along the network, by navigable-networks. The medium's straight-line measures are the nearest-point queries, carry-over's check that a snapped place lies on a place's old ground, the direction query, and a ground field's sampling radius. The first two are used only to snap a ground position onto the network, the direction query only to scale a direction to unit length, and the radius only to find the entities near a point. None of them measures how far anything travels."

### Task 5: Declare the ground field interface

Files:
- Create: `src/sim/medium/ground_field.h`
- Modify: `src/scenarios/stalls.h`

Step 1: Create the header with the include guard TPJ_SIM_MEDIUM_GROUND_FIELD_H. It includes sim/entity_key.h, sim/medium/field.h, sim/medium/network.h, sim/mix.h, sim/schema.h, and sim/world.h, and the standard headers it uses. Everything is in namespace tpj, with these declarations and a comment on each in the style of kept_field.h:

- `concept GroundFieldDefinition`: `typename F::Entry`, and `F::Name` convertible to std::string_view. Its comment shows an example definition, as FieldDefinition's does.
- `constexpr double GROUND_CELL_SIZE = 4.0;`: the side of the square cells the cell order groups entries by, in meters.
- `template <typename Entry> struct GroundEntry`: `EntityKey Key = NULL_KEY; GroundPoint At; Entry Value{}; uint64_t Changed = 0; bool Dropped = false;`. Changed is the tick of its last change, and Dropped marks a drop in the working list. Its visitFields lists key, x (At.X), z (At.Z), value, changed, and dropped. GroundPoint has no visitFields of its own.
- `struct GroundCell`: `uint64_t Cell = 0; uint32_t Position = 0;`. This is one readable entry's packed cell and its position in the readable list.
- `template <GroundFieldDefinition F> struct GroundEntries`: `std::vector<GroundEntry<typename F::Entry>> Readable;` in ascending key order, `std::vector<GroundEntry<typename F::Entry>> Next;` the working list for the tick, `bool Changing = false;` true from the tick's first change to the swap, `mutable std::vector<GroundCell> ByCell;`, and `mutable bool CellsCurrent = false;`. Its visitFields lists readable, next, and changing, and never the cell order.
- `template <typename Entry> struct GroundSample`: `EntityKey Key = NULL_KEY; GroundPoint At; Entry Value{};`.
- `template <GroundFieldDefinition F> void addGroundField(WorldSchema &schema);`
- `template <GroundFieldDefinition F> void setGroundEntry(World &world, EntityKey entity, GroundPoint at, typename F::Entry value);`
- `template <GroundFieldDefinition F> void dropGroundEntry(World &world, EntityKey entity);`
- `template <GroundFieldDefinition F> void sampleGroundField(const World &world, GroundPoint point, double radius, std::vector<GroundSample<typename F::Entry>> &sampled);`

Give the four functions empty bodies for now. sampleGroundField clears sampled.

Step 2: In src/scenarios/stalls.h, include sim/medium/ground_field.h and add, after Crowd:

```cpp
// Where each visitor stands, with its distance along the track as the value.
struct VisitorPresence {
  using Entry = double;
  static constexpr std::string_view Name = "visitor-presence";
};
```

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 6: Register a ground field

Files:
- Modify: `src/sim/medium/ground_field.h`

Step 1: Add `findGroundEntries<F>(const World &)` and `findGroundEntries<F>(World &)`. Each gives the GroundEntries<F> on the entity keyed fieldKey(F::Name), or null, as findKeptEntries does.

Step 2: Add the resolver `emptyGroundEntries<F>(World &)`. It creates the derived entity `world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name))` and calls `get_or_emplace<GroundEntries<F>>` on it, so resolution never changes entries already held.

Step 3: Declare `swapGroundEntries<F>(World &)` with an empty body, which Task 8 fills. Fill addGroundField. It registers `GroundEntries<F>` as `<name>-ground` with DataKind::State, the resolver `<name>-field` (emptyGroundEntries<F>), and the swap swapGroundEntries<F>. Every field kind registers a resolver named `<name>-field`, so addResolver throws std::invalid_argument when another field has the name. The comment on addGroundField says so.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 7: Set and drop entries

Files:
- Modify: `src/sim/medium/ground_field.h`

Step 1: Write a shared helper, used by setGroundEntry and dropGroundEntry, that checks a change and finds its slot in the working list. Build each message only when throwing, as keepEntry does. It throws:
- std::logic_error when `!world.isStepping()`, or when findGroundEntries gives null ("resolve the world first");
- std::invalid_argument for NULL_KEY.
Then, if `Changing` is false, it assigns Next from Readable, reusing Next's storage, and sets Changing. It finds the entity with lower_bound on Key in Next. If an entry there has the entity's key and `Changed == world.Tick`, it throws std::invalid_argument naming the field and saying the entity has already changed this tick. It returns the position.

Step 2: setGroundEntry also throws std::invalid_argument when `at.X` or `at.Z` isn't finite, before the helper changes anything. If the entity has an entry in Next, the helper's result updates At, Value, Changed = world.Tick, and Dropped = false. Otherwise it inserts `{entity, at, value, world.Tick, false}` at that position.

Step 3: dropGroundEntry marks the entity's entry `Dropped = true, Changed = world.Tick`. If the entity has no entry, it inserts `{entity, {}, {}, world.Tick, true}`, so a later change in the same tick is refused too.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 8: The swap

Files:
- Modify: `src/sim/medium/ground_field.h`

Step 1: Add `swapGroundEntries<F>(World &)`. If the field has no entries, it returns. If Changing is set, it swaps Readable and Next, clears Next, and clears Changing. In every case it then erases from Readable each entry that is Dropped or whose key `world.findEntity` no longer finds, and sets `CellsCurrent = false`. Readable stays in key order, because Next was.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 9: Sampling

Files:
- Modify: `src/sim/medium/ground_field.h`

Step 1: Add the cell helpers. `groundCellIndex(double v)` gives `floor(v / GROUND_CELL_SIZE)`, clamped to the int32 range, as int64_t. `packGroundCell(int64_t x, int64_t z)` gives `((x + 2^31) << 32) | (z + 2^31)` as uint64_t, so packed cells sort by x and then by z, and one x's cells from z0 to z1 are contiguous.

Step 2: Fill sampleGroundField. It clears sampled. It returns with nothing when `!(radius >= 0.0)`, when the point isn't finite, or when the field has no entries. Otherwise:
- It computes the cell box from `groundCellIndex(point.X - radius)` to `groundCellIndex(point.X + radius)`, and the same for Z. It counts the box's cells as a double.
- If that count is more than Readable's size, it reads every readable entry in order.
- Otherwise it makes the cell order if `CellsCurrent` is false. It clears ByCell, pushes `{packGroundCell(...), i}` for each readable entry, sorts by Cell and then Position with std::ranges::sort, and sets CellsCurrent. Then, for each x in the box, it takes the range from lower_bound of `packGroundCell(x, zFrom)` to upper_bound of `packGroundCell(x, zTo)` and reads those entries.
- An entry it reads goes into sampled when `dx * dx + dz * dz <= radius * radius` and `world.findEntity(entry.Key) != entt::null`.
- Finally, unless it read every entry in order, it sorts sampled by Key.

It allocates only when sampled or ByCell grows.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 10: Visitor presence in the stalls scenario

Files:
- Modify: `src/scenarios/stalls.cpp:1-17` (includes), `src/scenarios/stalls.cpp:85` (constants), `src/scenarios/stalls.cpp:207-241` (stepVisitors), `src/scenarios/stalls.cpp:271` (schema)

Step 1: Include sim/medium/ground_field.h and <bit>. Add `constexpr double VISITOR_REACH = 2.0;` with a comment saying it is how far a visitor looks for other visitors when it chooses. A sample at that reach covers at most 4 cells, and the field holds at least 6 visitors from tick 1 on, so samples read through the cell order. Only tick 0's samples, with the field still empty, read every entry, so the scenario runs both paths.

Step 2: In the schema, add `addGroundField<VisitorPresence>(*schema);` after `addField<Crowd>(*schema);`.

Step 3: In stepVisitors, declare `std::vector<GroundSample<double>> nearby;` before the loop. After `visitor.Age += 1;`, if `visitor.Age >= RETIREMENT_AGE` and the key is even, call `dropGroundEntry<VisitorPresence>(world, key)`. Otherwise call `setGroundEntry<VisitorPresence>(world, key, {.X = visitor.Distance, .Z = 0.0}, visitor.Distance)`. The track runs along X from the origin, so that is where the visitor stands.

Step 4: In the branch that chooses a stall, before the draw, call `sampleGroundField<VisitorPresence>(world, {.X = visitor.Distance, .Z = 0.0}, VISITOR_REACH, nearby)`. Fold every sample whose key isn't the visitor's own into a Hasher (sim/mix.h), in the order given: add the key's value, then `std::bit_cast<uint64_t>(sample.Value)`. Pass `hasher.value()` as drawKey's index in place of 0. Update the comment above stepVisitors to say a visitor's draw also depends on the visitors near it.

Step 5: Build, run, and compare with Task 1's output.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 600 > build/windows-debug/ground-after.txt; build/windows-debug/tpj_scenarios.exe --compare build/windows-debug/ground-before.txt build/windows-debug/ground-after.txt; echo "status $?"`
Expected: the first differing line on each side is a `scenario stalls tick` line, and `status 1`.

Run: `diff <(grep -v '^scenario stalls ' build/windows-debug/ground-before.txt) <(grep -v '^scenario stalls ' build/windows-debug/ground-after.txt) && echo same`
Expected: `same`

### Task 11: Full checks

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports no warnings.

### Task 12: Commit

Files: everything above, and this feature's FEATURE.md, PLAN.md, and RESEARCH.md.

Step 1: Commit with the commit-hygiene skill. Subject: `Sim: Add ground fields to the shared medium`.
