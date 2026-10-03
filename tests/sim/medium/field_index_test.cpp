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
using test::ReachCall;
using test::reachCalls;
using test::setSteps;

using Sampled = std::vector<std::pair<EntityKey, double>>;
using Positions = std::vector<uint32_t>;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();
constexpr EntityKey ABSENT_CARRIER{99};
constexpr EntityKey ABSENT_SOURCE{777};

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

// One source's entries in an order unlike place order, at every kind of place on the standard
// layout, each valued so a sample names the entries it gave. Node 1's stop places are (A, 4) and
// (B, 0), node 3's are (B, 6), (C, 0), and (C, 8), and C's one edge has node 3 at both ends.
std::vector<PlacedEntry<double>> mixedEntries() {
  return {
      entryAt(CARRIER_C, 8, 1.0),                              // node 3
      entryAt(CARRIER_A, 7, 2.0),                              // inside A between 4 and 10
      entryAt(CARRIER_A, 4, 3.0),                              // node 1
      entryAt(CARRIER_B, 0, 4.0),                              // node 1
      entryAt(CARRIER_A, NOT_A_NUMBER, 5.0),                   // nowhere
      entryAt(CARRIER_A, 7, 6.0),                              // the same place as 2.0
      entryAt(CARRIER_A, -0.0, 7.0),                           // node 0
      entryAt(CARRIER_C, 0, 8.0),                              // node 3
      entryAt(CARRIER_A, 4, 9.0),                              // node 1, the same place as 3.0
      entryAt(CARRIER_B, 6, 10.0),                             // node 3
      entryAt(CARRIER_A, 10, 11.0),                            // node 2
      entryAt(CARRIER_A, 5, 12.0),                             // inside A between 4 and 10
      entryAt(CARRIER_A, 0.0, 13.0),                           // node 0
      entryAt(CARRIER_B, 3, 14.0),                             // inside B
      entryAt(CARRIER_A, 2, 15.0),                             // inside A between 0 and 4
      entryAt(CARRIER_A, std::nextafter(10.0, 0.0), 16.0),     // nearest inside to node 2
      entryAt(CARRIER_A, std::nextafter(4.0, INFINITE), 17.0), // nearest inside to node 1
      entryAt(ABSENT_CARRIER, 4, 18.0),                        // nowhere
  };
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

EntityKey addBothSource(World &world, const std::vector<PlacedEntry<double>> &entries) {
  const EntityKey key = addSource<Footfall>(world, entries);
  world.Registry.emplace<Emits<Reach>>(world.findEntity(key), Emits<Reach>{.Entries = entries});
  return key;
}

MixedWorld mixedWorld() {
  MixedWorld world{.Value = makeFieldWorld(test::makeFieldSchema())};
  world.Mixed = addBothSource(world.Value, mixedEntries());
  world.Sparse = addBothSource(world.Value, {entryAt(CARRIER_A, 2, 201.0)});
  world.Junction =
      addBothSource(world.Value, {entryAt(CARRIER_B, 0, 301.0), entryAt(CARRIER_A, 7, 302.0),
                                  entryAt(CARRIER_A, 4, 303.0)});
  resolveWorld(world.Value);
  return world;
}

template <typename F> Sampled sampled(const World &world, const Place &at) {
  Sampled result;
  for (const SampledEntry<double> &entry : sampleField<F>(world, networkOf(world), at)) {
    result.emplace_back(entry.Source, entry.Value);
  }
  return result;
}

TEST_CASE("at a node, a sample gives each source's entries at any of the node's stop places, "
          "repeated places and signed zeros included and NaN distances never, in the source's "
          "order") {
  const MixedWorld world = mixedWorld();
  const World &value = world.Value;
  const EntityKey mixed = world.Mixed;
  const EntityKey junction = world.Junction;

  const Sampled node0 = {{mixed, 7.0}, {mixed, 13.0}};
  const Sampled node1 = {
      {mixed, 3.0}, {mixed, 4.0}, {mixed, 9.0}, {junction, 301.0}, {junction, 303.0}};
  const Sampled node2 = {{mixed, 11.0}};
  const Sampled node3 = {{mixed, 1.0}, {mixed, 8.0}, {mixed, 10.0}};
  const std::vector<std::pair<Place, Sampled>> cases = {
      {place(CARRIER_A, 0.0), node0}, {place(CARRIER_A, -0.0), node0}, {place(CARRIER_A, 4), node1},
      {place(CARRIER_B, 0), node1},   {place(CARRIER_A, 10), node2},   {place(CARRIER_B, 6), node3},
      {place(CARRIER_C, 0), node3},   {place(CARRIER_C, 8), node3},
  };
  for (const auto &[at, expected] : cases) {
    CAPTURE(at.Carrier, at.Distance);
    CHECK(sampled<Footfall>(value, at) == expected);
    // A field with sampleEdge is sampled by the same rule at a node.
    CHECK(sampled<Reach>(value, at) == expected);
  }
}

TEST_CASE("strictly inside an edge, a field without sampleEdge gives each source's entries at "
          "exactly the sampled place, in the source's order") {
  const MixedWorld world = mixedWorld();
  const World &value = world.Value;
  const EntityKey mixed = world.Mixed;

  const std::vector<std::pair<Place, Sampled>> cases = {
      {place(CARRIER_A, 7), {{mixed, 2.0}, {mixed, 6.0}, {world.Junction, 302.0}}},
      {place(CARRIER_A, 5), {{mixed, 12.0}}},
      {place(CARRIER_A, 6), {}},
      {place(CARRIER_A, std::nextafter(4.0, INFINITE)), {{mixed, 17.0}}},
      {place(CARRIER_A, std::nextafter(10.0, 0.0)), {{mixed, 16.0}}},
      {place(CARRIER_A, 2), {{mixed, 15.0}, {world.Sparse, 201.0}}},
      {place(CARRIER_B, 3), {{mixed, 14.0}}},
      {place(CARRIER_C, 4), {}},
  };
  for (const auto &[at, expected] : cases) {
    CAPTURE(at.Carrier, at.Distance);
    CHECK(sampled<Footfall>(value, at) == expected);
  }
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

// Checks each call's lists against the expected ones, offsets by their bits as resolve's
// subtractions give them, and that the sample is each call's result under its source.
void requireEdgeCalls(const NetworkEdge &edge, const Sampled &sample,
                      const std::vector<ExpectedCall> &expected) {
  const std::vector<ReachCall> &calls = reachCalls();
  REQUIRE(calls.size() == expected.size());
  Sampled returned;
  for (size_t call = 0; call < expected.size(); ++call) {
    CAPTURE(call);
    const EdgeSample<double> &given = calls[call].Sample;
    CHECK(given.AtFrom == expected[call].AtFrom);
    CHECK(given.AtTo == expected[call].AtTo);
    REQUIRE(given.Along.size() == expected[call].Along.size());
    for (size_t i = 0; i < given.Along.size(); ++i) {
      CAPTURE(i);
      const ExpectedAlong &along = expected[call].Along[i];
      CHECK(std::bit_cast<uint64_t>(given.Along[i].FromOffset) ==
            std::bit_cast<uint64_t>(along.Distance - edge.FromDistance));
      CHECK(std::bit_cast<uint64_t>(given.Along[i].ToOffset) ==
            std::bit_cast<uint64_t>(edge.ToDistance - along.Distance));
      CHECK(given.Along[i].Value == along.Value);
    }
    for (const double value : calls[call].Returned) {
      returned.emplace_back(expected[call].Source, value);
    }
  }
  CHECK(sample == returned);
}

const NetworkEdge &edgeAt(const Network &network, EntityKey carrier, double fromDistance) {
  for (const NetworkEdge &edge : network.edges()) {
    if (edge.Carrier == carrier && edge.FromDistance == fromDistance) {
      return edge;
    }
  }
  FAIL("the standard layout has an edge of the carrier from the distance");
  return network.edges().front();
}

TEST_CASE("strictly inside an edge, sampleEdge is given each source's entries at the stop places "
          "of the edge's two ends and those strictly inside it, in the source's order, and only "
          "sources with any are given") {
  const MixedWorld world = mixedWorld();
  const World &value = world.Value;
  const Network &network = networkOf(value);
  const EntityKey mixed = world.Mixed;
  const EntityKey sparse = world.Sparse;
  const EntityKey junction = world.Junction;
  const std::vector<double> atNode0 = {7.0, 13.0};
  const std::vector<double> atNode1 = {3.0, 4.0, 9.0};
  const std::vector<double> atNode3 = {1.0, 8.0, 10.0};

  SECTION("A between nodes 0 and 1") {
    reachCalls().clear();
    const Sampled sample = sampled<Reach>(value, place(CARRIER_A, 1));
    requireEdgeCalls(edgeAt(network, CARRIER_A, 0), sample,
                     {{.Source = mixed, .AtFrom = atNode0, .AtTo = atNode1, .Along = {{2, 15.0}}},
                      {.Source = sparse, .AtFrom = {}, .AtTo = {}, .Along = {{2, 201.0}}},
                      {.Source = junction, .AtFrom = {}, .AtTo = {301.0, 303.0}, .Along = {}}});
  }
  SECTION("A between nodes 1 and 2, with entries nearest each end") {
    reachCalls().clear();
    const Sampled sample = sampled<Reach>(value, place(CARRIER_A, 7));
    requireEdgeCalls(
        edgeAt(network, CARRIER_A, 4), sample,
        {{.Source = mixed,
          .AtFrom = atNode1,
          .AtTo = {11.0},
          .Along = {{7, 2.0},
                    {7, 6.0},
                    {5, 12.0},
                    {std::nextafter(10.0, 0.0), 16.0},
                    {std::nextafter(4.0, INFINITE), 17.0}}},
         {.Source = junction, .AtFrom = {301.0, 303.0}, .AtTo = {}, .Along = {{7, 302.0}}}});
  }
  SECTION("B between nodes 1 and 3") {
    reachCalls().clear();
    const Sampled sample = sampled<Reach>(value, place(CARRIER_B, 1));
    requireEdgeCalls(edgeAt(network, CARRIER_B, 0), sample,
                     {{.Source = mixed, .AtFrom = atNode1, .AtTo = atNode3, .Along = {{3, 14.0}}},
                      {.Source = junction, .AtFrom = {301.0, 303.0}, .AtTo = {}, .Along = {}}});
  }
  SECTION("C, whose two ends are node 3") {
    reachCalls().clear();
    const Sampled sample = sampled<Reach>(value, place(CARRIER_C, 4));
    requireEdgeCalls(edgeAt(network, CARRIER_C, 0), sample,
                     {{.Source = mixed, .AtFrom = atNode3, .AtTo = atNode3, .Along = {}}});
  }
}

std::vector<uint64_t> bitsOf(const std::vector<SampledEntry<double>> &entries) {
  std::vector<uint64_t> bits;
  for (const SampledEntry<double> &entry : entries) {
    bits.push_back(static_cast<uint64_t>(entry.Source));
    bits.push_back(std::bit_cast<uint64_t>(entry.Value));
  }
  return bits;
}

TEST_CASE("with no stepped entries, sampleResolvedField gives what sampleField gives, and "
          "fieldValue is 0.0 with its entries added in order, at nodes and inside edges") {
  const MixedWorld world = mixedWorld();
  const World &value = world.Value;
  const Network &network = networkOf(value);
  for (const Place &at :
       {place(CARRIER_A, -0.0), place(CARRIER_B, 0), place(CARRIER_C, 8), place(CARRIER_A, 7),
        place(CARRIER_A, 2), place(CARRIER_A, std::nextafter(10.0, 0.0)), place(CARRIER_C, 4)}) {
    CAPTURE(at.Carrier, at.Distance);
    const std::vector<SampledEntry<double>> footfall = sampleField<Footfall>(value, network, at);
    CHECK(bitsOf(sampleResolvedField<Footfall>(value, network, at)) == bitsOf(footfall));
    CHECK(bitsOf(sampleResolvedField<Reach>(value, network, at)) ==
          bitsOf(sampleField<Reach>(value, network, at)));
    double sum = 0.0;
    for (const SampledEntry<double> &entry : footfall) {
      sum += entry.Value;
    }
    CHECK(std::bit_cast<uint64_t>(fieldValue<Footfall>(value, network, at)) ==
          std::bit_cast<uint64_t>(sum));
  }
}

TEST_CASE("sourceEntryAtNode gives the first of the source's entries in the source's order at "
          "the node's stop places, not the first by place") {
  const MixedWorld world = mixedWorld();
  const World &value = world.Value;
  const Network &network = networkOf(value);
  const EntityKey mixed = world.Mixed;
  const EntityKey junction = world.Junction;

  // Node 3's nodePlace is (B, 6), but the mixed source's first entry there is at (C, 8); node 1's
  // is (A, 4), but the junction source's first entry there is at (B, 0).
  CHECK(sourceEntryAtNode<Footfall>(value, network, 3, mixed) == std::optional<double>(1.0));
  CHECK(sourceEntryAtNode<Reach>(value, network, 3, mixed) == std::optional<double>(1.0));
  CHECK(sourceEntryAtNode<Footfall>(value, network, 1, junction) == std::optional<double>(301.0));
  // Node 0's entries are at -0.0 and then 0.0.
  CHECK(sourceEntryAtNode<Footfall>(value, network, 0, mixed) == std::optional<double>(7.0));
  CHECK(sourceEntryAtNode<Footfall>(value, network, 2, mixed) == std::optional<double>(11.0));
  // The sparse source's only entry is inside an edge, and the junction source has none at node 2.
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    CAPTURE(node);
    CHECK_FALSE(sourceEntryAtNode<Footfall>(value, network, node, world.Sparse).has_value());
    CHECK_FALSE(sourceEntryAtNode<Footfall>(value, network, node, ABSENT_SOURCE).has_value());
  }
  CHECK_FALSE(sourceEntryAtNode<Footfall>(value, network, 2, junction).has_value());
}

} // namespace
} // namespace tpj
