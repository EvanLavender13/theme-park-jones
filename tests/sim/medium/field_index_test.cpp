#include "support/synthetic_fields.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::addSource;
using test::CARRIER_A;
using test::CARRIER_B;
using test::CARRIER_C;
using test::Emits;
using test::entryAt;
using test::Footfall;
using test::makeFieldWorld;
using test::makeSteppedFieldSchema;
using test::networkOf;
using test::Reach;
using test::setSteps;

using Sampled = std::vector<std::pair<EntityKey, double>>;
using Positions = std::vector<uint32_t>;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();

constexpr Place place(EntityKey carrier, double distance) {
  return {.Carrier = carrier, .Distance = distance};
}

TEST_CASE("orderByPlace gives the positions of the entries whose distance is not NaN, each once, "
          "ordered by carrier key, then distance, then position") {
  // Carriers out of key order, NaN on two carriers, repeated places, 0.0 before -0.0 so a tie
  // broken by sign rather than position would show, and a negative and an infinite distance,
  // which are not NaN.
  const std::vector<PlacedEntry<double>> entries = {
      entryAt(CARRIER_B, 3, 0.0),            // 0
      entryAt(CARRIER_A, NOT_A_NUMBER, 0.0), // 1
      entryAt(CARRIER_A, 4, 0.0),            // 2
      entryAt(NULL_KEY, 5, 0.0),             // 3
      entryAt(CARRIER_A, 0.0, 0.0),          // 4
      entryAt(CARRIER_B, 3, 0.0),            // 5
      entryAt(CARRIER_A, -0.0, 0.0),         // 6
      entryAt(CARRIER_A, -1, 0.0),           // 7
      entryAt(CARRIER_A, INFINITE, 0.0),     // 8
      entryAt(CARRIER_C, NOT_A_NUMBER, 0.0), // 9
      entryAt(CARRIER_A, 4, 0.0),            // 10
  };
  CHECK(orderByPlace<double>(entries) == Positions{3, 7, 4, 6, 2, 10, 8, 0, 5});

  CHECK(orderByPlace<double>(std::vector<PlacedEntry<double>>{}).empty());
  CHECK(
      orderByPlace<double>(std::vector<PlacedEntry<double>>{entryAt(CARRIER_A, NOT_A_NUMBER, 1.0),
                                                            entryAt(CARRIER_B, NOT_A_NUMBER, 2.0)})
          .empty());
}

// Commands that change a source's resolved entries and its stepped entries.
struct SetFootfall {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<double>> Entries;
};

[[maybe_unused]] void applyCommand(World &world, const SetFootfall &command) {
  world.Registry.emplace_or_replace<Emits<Footfall>>(world.findEntity(command.Source),
                                                     Emits<Footfall>{.Entries = command.Entries});
}

struct SetFootfallSteps {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<double>> Entries;
};

[[maybe_unused]] void applyCommand(World &world, const SetFootfallSteps &command) {
  setSteps<Footfall>(world, command.Source, command.Entries);
}

std::shared_ptr<WorldSchema> makeCommandedSchema() {
  auto schema = makeSteppedFieldSchema();
  schema->addCommand<SetFootfall>();
  schema->addCommand<SetFootfallSteps>();
  return schema;
}

// Every slot of the field the world holds, in both layers.
template <typename F> std::vector<const FieldSlot<double> *> slotsOf(const World &world) {
  std::vector<const FieldSlot<double> *> slots;
  const entt::entity holder = world.findEntity(fieldKey(F::Name));
  if (holder == entt::null) {
    return slots;
  }
  if (const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(holder)) {
    for (const auto *list : {&resolved->Slots, &resolved->Previous}) {
      for (const FieldSlot<double> &slot : *list) {
        slots.push_back(&slot);
      }
    }
  }
  if (const auto *stepped = world.Registry.try_get<SteppedEntries<F>>(holder)) {
    for (const auto *list : {&stepped->Readable, &stepped->Pending}) {
      for (const FieldSlot<double> &slot : *list) {
        slots.push_back(&slot);
      }
    }
  }
  return slots;
}

// Requires each slot's kept order to equal a fresh one, and gives the number of slots checked. No
// entry in these worlds has a NaN distance, so every entry is in the order.
template <typename F> size_t requireSlotsIndexed(const World &world) {
  const std::vector<const FieldSlot<double> *> slots = slotsOf<F>(world);
  for (const FieldSlot<double> *slot : slots) {
    CAPTURE(F::Name, slot->Source);
    const std::span<const uint32_t> kept = entriesByPlace(*slot);
    REQUIRE(Positions(kept.begin(), kept.end()) == orderByPlace<double>(slot->Entries));
    REQUIRE(kept.size() == slot->Entries.size());
  }
  return slots.size();
}

size_t requireAllIndexed(const World &world) {
  return requireSlotsIndexed<Footfall>(world) + requireSlotsIndexed<Reach>(world);
}

// Samples both fields by both layers, at a node and inside an edge, so every slot is read.
void sampleEverySlot(const World &world) {
  const Network &network = networkOf(world);
  for (const Place &at : {place(CARRIER_A, 4), place(CARRIER_A, 7)}) {
    static_cast<void>(sampleField<Footfall>(world, network, at));
    static_cast<void>(sampleResolvedField<Footfall>(world, network, at));
    static_cast<void>(sampleField<Reach>(world, network, at));
    static_cast<void>(sampleResolvedField<Reach>(world, network, at));
  }
}

struct IndexedWorld {
  World Value;
  EntityKey Walker = NULL_KEY;
  EntityKey Middle = NULL_KEY;
};

// Sources with both layers in both fields, their entries out of place order, resolved and stepped
// once, with every slot sampled.
IndexedWorld indexedWorld(std::shared_ptr<const WorldSchema> schema) {
  IndexedWorld world{.Value = makeFieldWorld(std::move(schema))};
  World &value = world.Value;
  world.Walker =
      addSource<Footfall>(value, {entryAt(CARRIER_C, 8, 1.0), entryAt(CARRIER_A, 4, 2.0),
                                  entryAt(CARRIER_B, 0, 3.0), entryAt(CARRIER_A, 2, 4.0)});
  setSteps<Footfall>(value, world.Walker,
                     {entryAt(CARRIER_A, 7, 10.0), entryAt(CARRIER_A, 4, 11.0)});
  world.Middle =
      addSource<Footfall>(value, {entryAt(CARRIER_A, 10, 5.0), entryAt(CARRIER_A, 4, 6.0)});
  setSteps<Footfall>(
      value, world.Middle,
      {entryAt(CARRIER_B, 3, 20.0), entryAt(CARRIER_A, 0, 21.0), entryAt(CARRIER_B, 3, 22.0)});
  const EntityKey last = addSource<Footfall>(value, {entryAt(CARRIER_B, 6, 7.0)});
  setSteps<Footfall>(value, last, {entryAt(CARRIER_C, 0, 30.0), entryAt(CARRIER_A, 4, 31.0)});
  const EntityKey reacher =
      addSource<Reach>(value, {entryAt(CARRIER_A, 7, 8.0), entryAt(CARRIER_A, 4, 9.0)});
  setSteps<Reach>(value, reacher, {entryAt(CARRIER_A, 5, 40.0), entryAt(CARRIER_B, 0, 41.0)});
  resolveWorld(value);
  stepWorld(value);
  sampleEverySlot(value);
  return world;
}

TEST_CASE("every slot's kept order by place equals orderByPlace of its entries after "
          "resolutions after commands, steps, swaps, copies, and loads") {
  const auto schema = makeCommandedSchema();
  IndexedWorld indexed = indexedWorld(schema);
  World &world = indexed.Value;
  REQUIRE(requireAllIndexed(world) > 0);

  // The middle source's resolved entries change, so the settle drops its readable stepped slot and
  // the slot after it moves; the walker's stepped entries move to other places from the next tick.
  CommandQueue queue;
  queue.push(SetFootfall{.Source = indexed.Middle,
                         .Entries = {entryAt(CARRIER_A, 10, 50.0), entryAt(CARRIER_C, 4, 51.0),
                                     entryAt(CARRIER_A, 1, 52.0)}});
  queue.push(SetFootfallSteps{.Source = indexed.Walker,
                              .Entries = {entryAt(CARRIER_B, 1, 60.0), entryAt(CARRIER_A, 9, 61.0),
                                          entryAt(CARRIER_A, 3, 62.0)}});
  stepWorld(world, queue);
  CHECK(requireAllIndexed(world) > 0);
  sampleEverySlot(world);

  // The swap makes the walker's moved entries readable.
  stepWorld(world);
  CHECK(requireAllIndexed(world) > 0);
  sampleEverySlot(world);

  World copy = copyWorld(world);
  CHECK(requireAllIndexed(copy) > 0);
  sampleEverySlot(copy);
  stepWorld(copy);
  CHECK(requireAllIndexed(copy) > 0);

  World loaded = loadWorld(schema, saveWorld(world));
  resolveWorld(loaded);
  CHECK(requireAllIndexed(loaded) > 0);
  sampleEverySlot(loaded);
  stepWorld(loaded);
  CHECK(requireAllIndexed(loaded) > 0);
}

TEST_CASE("a world whose slots have been sampled saves to the same text, hashes the same, and "
          "equals the same world never sampled") {
  const auto schema = makeSteppedFieldSchema();
  World sampled = makeFieldWorld(schema);
  const EntityKey walker =
      addSource<Footfall>(sampled, {entryAt(CARRIER_C, 8, 1.0), entryAt(CARRIER_A, 4, 2.0),
                                    entryAt(CARRIER_A, 2, 3.0)});
  setSteps<Footfall>(sampled, walker, {entryAt(CARRIER_A, 7, 10.0), entryAt(CARRIER_B, 0, 11.0)});
  const EntityKey reacher =
      addSource<Reach>(sampled, {entryAt(CARRIER_A, 7, 4.0), entryAt(CARRIER_A, 4, 5.0)});
  setSteps<Reach>(sampled, reacher, {entryAt(CARRIER_B, 3, 20.0)});
  resolveWorld(sampled);
  stepWorld(sampled);
  const World never = copyWorld(sampled);

  const Network &network = networkOf(sampled);
  sampleEverySlot(sampled);
  static_cast<void>(fieldValue<Footfall>(sampled, network, place(CARRIER_A, 4)));
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    for (const EntityKey source : {walker, reacher}) {
      static_cast<void>(sourceEntryAtNode<Footfall>(sampled, network, node, source));
      static_cast<void>(sourceEntryAtNode<Reach>(sampled, network, node, source));
    }
  }

  CHECK(saveWorld(sampled) == saveWorld(never));
  CHECK(hashWorld(sampled) == hashWorld(never));
  CHECK(worldsEqual(sampled, never));
  CHECK(worldsEqual(never, sampled));
}

// Three sources publishing the same entries into footfall and reach: the mixed one; one with a
// single entry inside A between 0 and 4, so it has nothing at most places; and one whose first
// entry at node 1 is not at node 1's nodePlace. Resolved, with no stepped entries.
struct MixedWorld {
  World Value;
  EntityKey Mixed = NULL_KEY;
  EntityKey Sparse = NULL_KEY;
  EntityKey Junction = NULL_KEY;
};

template <typename F> Sampled sampled(const World &world, const Place &at) {
  Sampled result;
  for (const SampledEntry<double> &entry : sampleField<F>(world, networkOf(world), at)) {
    result.emplace_back(entry.Source, entry.Value);
  }
  return result;
}

// An entry strictly inside an edge as sampleEdge should receive it.
struct ExpectedAlong {
  double Distance = 0.0;
  double Value = 0.0;
};

struct ExpectedCall {
  EntityKey Source = NULL_KEY;
  std::vector<double> AtFrom;
  std::vector<double> AtTo;
  std::vector<ExpectedAlong> Along;
};

} // namespace
} // namespace tpj
