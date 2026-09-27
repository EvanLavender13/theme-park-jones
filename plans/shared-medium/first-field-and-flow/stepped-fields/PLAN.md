# Implementation Plan: Stepped Fields

## Goal

Add every field's stepped layer: publishStepped while stepping, the swap that makes it readable for one tick, the layer rule in sampling, and the finisher that clears a changed source's stepped entries, with schema finishers and World::isStepping in the sim core.

## Approach

The schema gains a list of finishers, which resolveWorld runs after its resolvers, and stepWorld raises a Stepping flag around its systems. Each field gains a state component, SteppedEntries<F>, holding Readable and Pending slot lists. publishStepped inserts into Pending, the field's swap moves Pending into Readable, and sampleField merges the resolved and readable stepped slots by source key, taking the stepped slot where both exist. The field's resolver now keeps the previous resolved slots in ResolvedEntries, with a flag saying they exist, and the field's finisher compares each stepped source's previous and current resolved entries by the walk's words, erases the stepped slots of those that differ, and empties the previous slots again.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Make the three edits in FEATURE.md's Spec changes for src/sim/SPEC.md, with their exact text.

### Task 2: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: Make the five edits in FEATURE.md's Spec changes for src/sim/medium/SPEC.md, with their exact text.

### Task 3: Declare finishers and isStepping

Files:
- Modify: `src/sim/schema.h`
- Modify: `src/sim/schema.cpp`
- Modify: `src/sim/world.h`

Step 1: In src/sim/schema.h, in class WorldSchema, after addResolver's declaration, add:

```cpp
  // Every resolution runs the finishers after its resolvers, in registration order.
  void addFinisher(WorldFunction finish);
```

After `resolvers()`, add:

```cpp
  [[nodiscard]] const std::vector<WorldFunction> &finishers() const { return Finishers; }
```

After the `Resolvers` member, add:

```cpp
  std::vector<WorldFunction> Finishers;
```

Step 2: In src/sim/schema.cpp, after addSwap's definition, add the stub:

```cpp
void WorldSchema::addFinisher(WorldFunction /*finish*/) {
  // Stub until implemented.
}
```

Step 3: In src/sim/world.h, after isResolving, add:

```cpp
  // True only while the systems step.
  [[nodiscard]] bool isStepping() const { return Stepping; }
```

After the `Resolving` member and its comment, add:

```cpp
  // Set only while the systems step, so that functions meant for systems can refuse others.
  bool Stepping = false;
```

### Task 4: Declare the stepped layer

Files:
- Modify: `src/sim/medium/field.h`

Step 1: After ResolvedEntries' visitFields, add:

```cpp
// A field's stepped entries, sources in ascending key order. State. Readable holds what the last
// swap made readable, and Pending what systems publish during the current tick.
template <FieldDefinition F> struct SteppedEntries {
  std::vector<FieldSlot<typename F::Entry>> Readable;
  std::vector<FieldSlot<typename F::Entry>> Pending;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, SteppedEntries<F> &stepped) {
  visitor.field("readable", stepped.Readable);
  visitor.field("pending", stepped.Pending);
}
```

Step 2: After publishResolved's definition, add the stub:

```cpp
// Gives a source's entries for this tick, readable after the swap that ends it. Systems only.
// Throws std::logic_error when the world is not stepping or holds no stepped entries for the
// field, and std::invalid_argument for the null key or a source that has already published into
// the field in this tick.
template <FieldDefinition F>
void publishStepped(World & /*world*/, EntityKey /*source*/,
                    std::vector<PlacedEntry<typename F::Entry>> /*entries*/) {
  // Stub until implemented.
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 174 existing tests pass.

### Task 5: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, and the public headers src/sim/medium/field.h, src/sim/schema.h, and src/sim/world.h. It creates tests/sim/medium/stepped_field_test.cpp, extends tests/sim/support/synthetic_fields.h with publishing systems, adds the finisher and isStepping tests to tests/sim/cycle_test.cpp, and adds the new file to tpj_sim_tests in tests/sim/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 174 existing tests pass. The new tests of finishers, isStepping, stepped publication, the layer rule, clearing, saves, shuffled systems, and candidates fail against the stubs.

### Task 6: Run finishers

Files:
- Modify: `src/sim/schema.cpp`
- Modify: `src/sim/cycle.cpp`

Step 1: Replace addFinisher's stub:

```cpp
void WorldSchema::addFinisher(WorldFunction finish) { Finishers.push_back(finish); }
```

Step 2: In resolveWorld in src/sim/cycle.cpp, inside the try block, after the loop over resolvers, add:

```cpp
    for (const WorldFunction finish : world.schema().finishers()) {
      finish(world);
    }
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the finisher tests pass.

### Task 7: Mark stepping

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: In stepWorld, replace the loop over systems with:

```cpp
  world.Stepping = true;
  try {
    for (const WorldFunction step : world.schema().systems()) {
      step(world);
    }
  } catch (...) {
    world.Stepping = false;
    throw;
  }
  world.Stepping = false;
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the isStepping test passes.

### Task 8: Hold the stepped layer and swap it

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Give ResolvedEntries the previous resolution's slots, replacing its definition and visitFields:

```cpp
// A field's resolved entries, sources in ascending key order. Derived data. From the field's
// resolver to its finisher, Previous holds the previous resolution's slots and HasPrevious says
// there was one. Both are empty between resolutions.
template <FieldDefinition F> struct ResolvedEntries {
  std::vector<FieldSlot<typename F::Entry>> Slots;
  bool HasPrevious = false;
  std::vector<FieldSlot<typename F::Entry>> Previous;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, ResolvedEntries<F> &resolved) {
  visitor.field("slots", resolved.Slots);
  visitor.field("has-previous", resolved.HasPrevious);
  visitor.field("previous", resolved.Previous);
}
```

Step 2: Replace emptyResolvedEntries with:

```cpp
// The field's resolver: empties its resolved entries, keeping the previous ones for its finisher,
// and creates the entity that holds both layers, with an empty stepped layer if it has none.
template <FieldDefinition F> void emptyResolvedEntries(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  const entt::entity entity = world.findEntity(key);
  ResolvedEntries<F> next;
  if (auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity)) {
    next.HasPrevious = true;
    next.Previous = std::move(resolved->Slots);
  }
  world.Registry.emplace_or_replace<ResolvedEntries<F>>(entity, std::move(next));
  static_cast<void>(world.Registry.get_or_emplace<SteppedEntries<F>>(entity));
}

// The field's swap: makes the entries published during the tick readable, replacing the last.
template <FieldDefinition F> void swapSteppedEntries(World &world) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *stepped =
      entity == entt::null ? nullptr : world.Registry.try_get<SteppedEntries<F>>(entity);
  if (stepped == nullptr) {
    return;
  }
  stepped->Readable = std::move(stepped->Pending);
  stepped->Pending.clear();
}
```

Step 3: In addField, after the addComponent call for ResolvedEntries, add:

```cpp
  schema.addComponent<SteppedEntries<F>>(name + "-stepped", DataKind::State);
```

and after the addResolver call, add:

```cpp
  schema.addSwap(&swapSteppedEntries<F>);
```

Update addField's comment to: "Registers the field's derived component, <name>-resolved, its state component, <name>-stepped, its resolver, <name>-field, its swap, and its finisher. Throws std::invalid_argument when a name is malformed or already registered."

Step 4: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, the 174 earlier tests pass, and the registration test of -stepped passes.

### Task 9: Publish stepped entries

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace publishStepped's stub:

```cpp
template <FieldDefinition F>
void publishStepped(World &world, EntityKey source,
                    std::vector<PlacedEntry<typename F::Entry>> entries) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::string name(F::Name);
  if (!world.isStepping()) {
    throw std::logic_error("field " + name + " takes stepped entries only from systems");
  }
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *stepped =
      entity == entt::null ? nullptr : world.Registry.try_get<SteppedEntries<F>>(entity);
  if (stepped == nullptr) {
    throw std::logic_error("field " + name + " holds no stepped entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  std::vector<Slot> &slots = stepped->Pending;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + name + " this tick");
  }
  slots.insert(at, Slot{source, std::move(entries)});
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the publishStepped refusal tests pass.

### Task 10: Sample by the layer rule

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Before sampleField, add:

```cpp
// Each source's slot, in ascending key order: its readable stepped slot when it has one, and
// otherwise its resolved one.
template <FieldDefinition F>
std::vector<const FieldSlot<typename F::Entry> *> layeredSlots(const World &world,
                                                               entt::entity entity) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::vector<Slot> none;
  const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);
  const auto *stepped = world.Registry.try_get<SteppedEntries<F>>(entity);
  const std::vector<Slot> &fromResolved = resolved != nullptr ? resolved->Slots : none;
  const std::vector<Slot> &fromStepped = stepped != nullptr ? stepped->Readable : none;
  std::vector<const Slot *> slots;
  auto r = fromResolved.begin();
  auto s = fromStepped.begin();
  while (r != fromResolved.end() || s != fromStepped.end()) {
    if (s == fromStepped.end() || (r != fromResolved.end() && r->Source < s->Source)) {
      slots.push_back(&*r);
      ++r;
    } else {
      if (r != fromResolved.end() && r->Source == s->Source) {
        ++r;
      }
      slots.push_back(&*s);
      ++s;
    }
  }
  return slots;
}
```

Step 2: In sampleField, replace everything from `const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);` through the end of the loop with:

```cpp
  for (const FieldSlot<typename F::Entry> *slot : layeredSlots<F>(world, entity)) {
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      sampleSlotAtNode(network, *slot, node->Node, sampled);
    } else {
      sampleSlotInEdge<F>(network, *slot, place, std::get<EdgePosition>(*position), sampled);
    }
  }
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the publication, one-tick visibility, layer rule, save, shuffled-system tests pass. The clearing tests and any candidate test that depends on clearing still fail.

### Task 11: Clear changed sources' stepped entries

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Before emptyResolvedEntries, add a word collector and the comparison:

```cpp
// Collects the words the walk emits for a value, so values compare as worlds do.
class WordCollector : public WordSink {
public:
  void word(uint64_t value) override { Words.push_back(value); }
  void real(std::string_view /*field*/, double /*value*/) override {}
  [[nodiscard]] const std::vector<uint64_t> &words() const { return Words; }

private:
  std::vector<uint64_t> Words;
};

// The walk's words for a source's entries among the slots, an empty list when it has no slot.
template <typename Entry>
std::vector<uint64_t> entryWords(const std::vector<FieldSlot<Entry>> &slots, EntityKey source) {
  std::vector<PlacedEntry<Entry>> entries;
  const auto at = std::ranges::lower_bound(slots, source, {}, &FieldSlot<Entry>::Source);
  if (at != slots.end() && at->Source == source) {
    entries = at->Entries;
  }
  WordCollector collector;
  emitField(collector, "entries", entries);
  return collector.words();
}
```

Step 2: After swapSteppedEntries, add the finisher:

```cpp
// The field's finisher: after a resolution with a previous one to compare with, clears the
// readable stepped entries of each source whose resolved entries changed, then drops the previous.
template <FieldDefinition F> void settleSteppedEntries(World &world) {
  using Slot = FieldSlot<typename F::Entry>;
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr || !resolved->HasPrevious) {
    return;
  }
  auto &stepped = world.Registry.get<SteppedEntries<F>>(entity);
  std::erase_if(stepped.Readable, [resolved](const Slot &slot) {
    return entryWords(resolved->Previous, slot.Source) != entryWords(resolved->Slots, slot.Source);
  });
  resolved->HasPrevious = false;
  resolved->Previous.clear();
}
```

Step 3: In addField, after the addSwap call, add:

```cpp
  schema.addFinisher(&settleSteppedEntries<F>);
```

Step 4: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and every test passes.

### Task 12: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: the script prints its stages and passes, with the scenarios' output unchanged, since no scenario has a field yet.

### Task 13: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Medium: Add stepped fields and the layer rule`.
