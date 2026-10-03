#include "support/synthetic_fields.h"

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::addSource;
using test::addStepper;
using test::CARRIER_A;
using test::CARRIER_B;
using test::CARRIER_C;
using test::entryAt;
using test::Footfall;
using test::makeFieldWorld;
using test::makeSteppedFieldSchema;
using test::networkOf;
using test::Reach;
using test::setSteps;

using Placed = std::vector<std::pair<Place, double>>;

constexpr Place place(EntityKey carrier, double distance) {
  return {.Carrier = carrier, .Distance = distance};
}

// A key no entity of these worlds holds.
constexpr EntityKey ABSENT_SOURCE{777};

// The slot's source and entries, or none for no slot.
std::optional<std::pair<EntityKey, Placed>> contentsOf(const FieldSlot<double> *slot) {
  if (slot == nullptr) {
    return std::nullopt;
  }
  Placed entries;
  for (const PlacedEntry<double> &entry : slot->Entries) {
    entries.emplace_back(entry.At, entry.Value);
  }
  return std::pair{slot->Source, entries};
}

std::optional<std::pair<EntityKey, Placed>> expectedSlot(EntityKey source, Placed entries) {
  return std::pair{source, std::move(entries)};
}

TEST_CASE("sourceSlot gives a source's readable stepped slot when it has one, even an empty one, "
          "otherwise its resolved slot, and none when the world holds neither for it") {
  World world = makeFieldWorld(makeSteppedFieldSchema());
  const EntityKey both = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  setSteps<Footfall>(world, both, {entryAt(CARRIER_A, 4, 10.0)});
  const EntityKey resolvedOnly = addSource<Footfall>(world, {entryAt(CARRIER_A, 2, 2.0)});
  const EntityKey emptied = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 3.0)});
  setSteps<Footfall>(world, emptied, {});
  const EntityKey steppedOnly = addStepper<Footfall>(world, {entryAt(CARRIER_B, 3, 40.0)});
  const EntityKey neither = world.createEntity();

  // Before the field's resolver first runs the world holds no entries for the field.
  CHECK(sourceSlot<Footfall>(world, both) == nullptr);
  CHECK(sourceSlot<Footfall>(world, resolvedOnly) == nullptr);

  // Before any swap, every slot is the resolved one.
  resolveWorld(world);
  CHECK(contentsOf(sourceSlot<Footfall>(world, both)) ==
        expectedSlot(both, {{place(CARRIER_A, 4), 1.0}}));
  CHECK(contentsOf(sourceSlot<Footfall>(world, emptied)) ==
        expectedSlot(emptied, {{place(CARRIER_A, 4), 3.0}}));
  CHECK(sourceSlot<Footfall>(world, steppedOnly) == nullptr);

  // Tick 0 publishes each stepped value raised by 0.
  stepWorld(world);
  CHECK(contentsOf(sourceSlot<Footfall>(world, both)) ==
        expectedSlot(both, {{place(CARRIER_A, 4), 10.0}}));
  CHECK(contentsOf(sourceSlot<Footfall>(world, resolvedOnly)) ==
        expectedSlot(resolvedOnly, {{place(CARRIER_A, 2), 2.0}}));
  CHECK(contentsOf(sourceSlot<Footfall>(world, emptied)) == expectedSlot(emptied, {}));
  CHECK(contentsOf(sourceSlot<Footfall>(world, steppedOnly)) ==
        expectedSlot(steppedOnly, {{place(CARRIER_B, 3), 40.0}}));
  CHECK(sourceSlot<Footfall>(world, neither) == nullptr);
  CHECK(sourceSlot<Footfall>(world, ABSENT_SOURCE) == nullptr);
  CHECK(sourceSlot<Footfall>(world, NULL_KEY) == nullptr);
  // A source of one field has no slot in another.
  CHECK(sourceSlot<Reach>(world, both) == nullptr);
}

// The first of the source's entries in the sample, or none.
std::optional<double> firstOf(const std::vector<SampledEntry<double>> &sample, EntityKey source) {
  for (const SampledEntry<double> &entry : sample) {
    if (entry.Source == source) {
      return entry.Value;
    }
  }
  return std::nullopt;
}

int countOf(const std::vector<SampledEntry<double>> &sample, EntityKey source) {
  int count = 0;
  for (const SampledEntry<double> &entry : sample) {
    count += entry.Source == source ? 1 : 0;
  }
  return count;
}

// Compares sourceEntryAtNode with the first of the source's entries in sampleField at each node's
// nodePlace, and counts the nodes where the source had an entry.
template <typename F>
int checkAgainstSample(const World &world, const Network &network,
                       const std::vector<EntityKey> &sources) {
  int found = 0;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::vector<SampledEntry<double>> sample =
        sampleField<F>(world, network, network.nodePlace(node));
    for (const EntityKey source : sources) {
      CAPTURE(node, source);
      const std::optional<double> expected = firstOf(sample, source);
      CHECK(sourceEntryAtNode<F>(world, network, node, source) == expected);
      found += expected ? 1 : 0;
    }
  }
  return found;
}

TEST_CASE("sourceEntryAtNode gives the first of a source's entries that sampleField gives at the "
          "node's nodePlace, and none when that sample has none for the source") {
  World world = makeFieldWorld(makeSteppedFieldSchema());
  // Two entries at the junction, node 1, the first on a carrier that is not its nodePlace's, so
  // the first in the source's order is not the entry at the nodePlace itself.
  const EntityKey ordered =
      addSource<Footfall>(world, {entryAt(CARRIER_B, 0, 1.0), entryAt(CARRIER_A, 4, 2.0)});
  // After a step, its stepped entry at node 3, on a stop that is not node 3's nodePlace, replaces
  // its resolved entry at node 0.
  const EntityKey layered = addSource<Footfall>(world, {entryAt(CARRIER_A, 0, 3.0)});
  setSteps<Footfall>(world, layered, {entryAt(CARRIER_C, 8, 5.0)});
  // An empty stepped slot hides its resolved entry at node 2.
  const EntityKey emptied = addSource<Footfall>(world, {entryAt(CARRIER_A, 10, 6.0)});
  setSteps<Footfall>(world, emptied, {});
  // Its only entry lies strictly inside an edge, at no node.
  const EntityKey inside = addSource<Footfall>(world, {entryAt(CARRIER_A, 2, 7.0)});
  // A field with its own rule inside edges is sampled by the default rule at a node.
  const EntityKey reach =
      addSource<Reach>(world, {entryAt(CARRIER_A, 4, 8.0), entryAt(CARRIER_A, 2, 9.0)});
  const World unresolved = copyWorld(world);
  resolveWorld(world);
  stepWorld(world);
  const Network &network = networkOf(world);
  const std::vector<EntityKey> sources = {ordered, layered,       emptied, inside,
                                          reach,   ABSENT_SOURCE, NULL_KEY};

  REQUIRE(countOf(sampleField<Footfall>(world, network, network.nodePlace(1)), ordered) == 2);
  CHECK(checkAgainstSample<Footfall>(world, network, sources) > 0);
  CHECK(checkAgainstSample<Reach>(world, network, sources) > 0);
  // A world holding no entries for the field.
  CHECK(checkAgainstSample<Footfall>(unresolved, network, sources) == 0);
}

TEST_CASE("sourceEntryAtNode throws std::out_of_range for a node not below the node count") {
  World world = makeFieldWorld(makeSteppedFieldSchema());
  const EntityKey source = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  resolveWorld(world);
  const Network &network = networkOf(world);

  REQUIRE_THROWS_AS(sourceEntryAtNode<Footfall>(world, network, network.nodeCount(), source),
                    std::out_of_range);
  REQUIRE_THROWS_AS(sourceEntryAtNode<Footfall>(world, Network(), 0, source), std::out_of_range);
}

} // namespace
} // namespace tpj
