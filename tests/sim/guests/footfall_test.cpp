#include "support/guest_parks.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/footfall.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/kept_field.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::midpointOf;
using test::recordOf;

const EntityKey FOOTFALL_SOURCE = fieldKey(HungryFootfall::Name);

std::span<const KeptEntry> footfallEntries(const World &world) {
  return keptEntries<HungryFootfall>(world, FOOTFALL_SOURCE);
}

// The entry's value read at the world's tick.
double readValue(const World &world, const KeptEntry &entry) {
  const std::optional<double> value = keptValue<HungryFootfall>(world, FOOTFALL_SOURCE, entry.At);
  REQUIRE(value.has_value());
  return value.value_or(0.0);
}

double carrierLength(const Network &network, EntityKey carrier) {
  const auto found = std::ranges::find(network.carriers(), carrier, &Carrier::Key);
  REQUIRE(found != network.carriers().end());
  return found->Points.back().Distance;
}

// Whether the place lies in the stretch: on its carrier, at or above its FromDistance and below its
// ToDistance, or at exactly the carrier's length when the stretch is the carrier's last.
bool liesIn(const Network &network, const Place &place, const NetworkEdge &stretch) {
  if (place.Carrier != stretch.Carrier || !(place.Distance >= stretch.FromDistance)) {
    return false;
  }
  return place.Distance < stretch.ToDistance ||
         (place.Distance == stretch.ToDistance &&
          stretch.ToDistance == carrierLength(network, stretch.Carrier));
}

// The footfall entries whose places lie in the stretch, in held order.
std::vector<KeptEntry> entriesIn(const World &world, const NetworkEdge &stretch) {
  std::vector<KeptEntry> found;
  std::ranges::copy_if(
      footfallEntries(world), std::back_inserter(found),
      [&](const KeptEntry &entry) { return liesIn(test::guestNetwork(world), entry.At, stretch); });
  return found;
}

// The stretch's value: 0.0 with the read value of each entry strictly inside it added in held
// order.
double stretchValue(const World &world, const NetworkEdge &stretch) {
  double value = 0.0;
  for (const KeptEntry &entry : footfallEntries(world)) {
    if (entry.At.Carrier == stretch.Carrier && entry.At.Distance > stretch.FromDistance &&
        entry.At.Distance < stretch.ToDistance) {
      value += readValue(world, entry);
    }
  }
  return value;
}

// The guests whose places lie in the stretch, in ascending key order.
std::vector<GuestRecord> guestsIn(const World &world, const NetworkEdge &stretch) {
  std::vector<GuestRecord> found;
  for (const EntityKey guest : parkGuests(world)) {
    const GuestRecord record = recordOf(world, guest);
    if (liesIn(test::guestNetwork(world), record.At, stretch)) {
      found.push_back(record);
    }
  }
  return found;
}

// The world's footfall entries that carryOver from its guest network to the candidate's brings
// into the stretch of the candidate's, in held order.
std::vector<KeptEntry> carriedInto(const World &world, const World &candidate,
                                   const NetworkEdge &stretch) {
  std::vector<KeptEntry> carried;
  for (const KeptEntry &entry : footfallEntries(world)) {
    const std::optional<Place> place =
        carryOver(entry.At, test::guestNetwork(world), test::guestNetwork(candidate));
    if (place.has_value() &&
        liesIn(test::guestNetwork(candidate), place.value_or(Place{}), stretch)) {
      carried.push_back(entry);
    }
  }
  return carried;
}

std::vector<NetworkEdge> edgesOn(const Network &network, EntityKey carrier) {
  std::vector<NetworkEdge> edges;
  std::ranges::copy_if(network.edges(), std::back_inserter(edges),
                       [carrier](const NetworkEdge &edge) { return edge.Carrier == carrier; });
  return edges;
}

// Whether some stretch holds a footfall entry and no guest: one guests have walked and since left.
bool hasLeftStretch(const World &world) {
  const std::vector<NetworkEdge> &stretches = test::guestNetwork(world).edges();
  return std::ranges::any_of(stretches, [&world](const NetworkEdge &stretch) {
    return !entriesIn(world, stretch).empty() && guestsIn(world, stretch).empty();
  });
}

// Whether guests have walked the spine past the near and starved shops' junctions, so its first
// two stretches hold footfall.
bool hasSpread(const World &world) {
  const std::vector<NetworkEdge> spine = edgesOn(test::guestNetwork(world), test::EATING_SPINE);
  return spine.size() > 2 && stretchValue(world, spine[0]) != 0.0 &&
         stretchValue(world, spine[1]) != 0.0;
}

// The spread world, the eating park stepped with no commands until guests have spread over its
// first stretches, leaving stretches they have walked and since left, and that world stepped one
// more cycle with no commands, which still holds such a stretch.
struct SpreadRun {
  World Spread;
  World Stepped;
};

const SpreadRun &spreadRun() {
  static const SpreadRun run = [] {
    // Far more cycles than the run takes, a guard against stepping for ever.
    constexpr uint64_t LONGEST_RUN = 6000;
    World spread = test::eatingWorld();
    while (true) {
      REQUIRE(spread.Tick < LONGEST_RUN);
      stepWorld(spread);
      if (!hasSpread(spread) || !hasLeftStretch(spread)) {
        continue;
      }
      World stepped = copyWorld(spread);
      stepWorld(stepped);
      if (hasLeftStretch(stepped)) {
        return SpreadRun{.Spread = std::move(spread), .Stepped = std::move(stepped)};
      }
    }
  }();
  return run;
}

const World &spreadWorld() { return spreadRun().Spread; }

// The spread world stepped one more cycle with no commands.
const World &steppedWorld() { return spreadRun().Stepped; }

// A guest path crossing the eating park's gate path a quarter of the way along it, off its
// midpoint, so a split leaves the old midpoint in the far half, whose midpoint differs from it.
AddPath gateCrossing() {
  const std::vector<ParkPoint> gate = test::eatingGatePoints();
  const double z = gate.front().Z + ((gate.back().Z - gate.front().Z) / 4.0);
  return AddPath{PathKind::Guest, {{-2.0, z}, {2.0, z}}};
}

World candidateWith(const World &world, const ParkEdit &edit) {
  REQUIRE(isAccepted(world, edit));
  CommandQueue commands;
  queueEdit(commands, edit);
  return makeCandidate(world, commands);
}

TEST_CASE("After a cycle with no commands, the stretches whose footfall entry has the world's tick "
          "are exactly the stretches some guest's place lies in") {
  const World &world = steppedWorld();
  const std::vector<NetworkEdge> &stretches = test::guestNetwork(world).edges();
  REQUIRE_FALSE(stretches.empty());

  bool sawGuests = false;
  bool sawIdleEntry = false;
  for (const NetworkEdge &stretch : stretches) {
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance);
    const std::vector<KeptEntry> entries = entriesIn(world, stretch);
    const bool changed = std::ranges::any_of(
        entries, [&world](const KeptEntry &entry) { return entry.Tick == world.Tick; });
    const bool occupied = !guestsIn(world, stretch).empty();
    CHECK(changed == occupied);
    sawGuests = sawGuests || occupied;
    sawIdleEntry = sawIdleEntry || (!occupied && !entries.empty());
  }
  CHECK(sawGuests);
  // A stretch guests have walked and left, so an entry's tick could be wrongly renewed.
  CHECK(sawIdleEntry);
}

TEST_CASE("Hungry footfall holds at most one kept entry in each stretch of the guest network, at "
          "the stretch's midpoint") {
  // A crossing path splits the gate path's stretch, so a carried entry must move to its new
  // stretch's midpoint, and a cycle on the new network keeps entries there.
  World world = candidateWith(spreadWorld(), gateCrossing());
  stepWorld(world);
  const Network &network = test::guestNetwork(world);
  REQUIRE(edgesOn(network, test::EATING_GATE_PATH).size() == 2);

  const std::span<const KeptEntry> entries = footfallEntries(world);
  REQUIRE_FALSE(entries.empty());
  for (const KeptEntry &entry : entries) {
    CAPTURE(entry.At.Carrier, entry.At.Distance);
    CHECK(std::ranges::any_of(network.edges(), [&entry](const NetworkEdge &stretch) {
      return entry.At == midpointOf(stretch);
    }));
  }
  for (const NetworkEdge &stretch : network.edges()) {
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance);
    CHECK(entriesIn(world, stretch).size() <= 1);
  }
}

TEST_CASE("Each stretch's hungry footfall moves toward the summed hunger of the guests on it, and "
          "a stretch with none never moves away from 0 as it is read later, and never passes it") {
  const World &before = spreadWorld();
  const World &after = steppedWorld();

  bool sawMove = false;
  bool sawDecay = false;
  for (const NetworkEdge &stretch : test::guestNetwork(after).edges()) {
    const double held = stretchValue(before, stretch);
    const double value = stretchValue(after, stretch);
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance, held, value);
    const std::vector<GuestRecord> guests = guestsIn(after, stretch);
    if (guests.empty()) {
      CHECK(value >= 0.0);
      CHECK(value <= held);
      sawDecay = sawDecay || held > 0.0;
      continue;
    }
    double hunger = 0.0;
    for (const GuestRecord &guest : guests) {
      hunger += guest.Hunger;
    }
    CAPTURE(hunger);
    CHECK(value >= std::min(held, hunger));
    CHECK(value <= std::max(held, hunger));
    if (held != hunger) {
      CHECK(value != held);
      sawMove = sawMove || held != 0.0;
    }
  }
  CHECK(sawMove);
  CHECK(sawDecay);

  // Read far later than any stretch is left, a kept entry still lies between 0 and its value read
  // now.
  constexpr uint64_t FAR_LATER = 1'000'000;
  for (const KeptEntry &entry : footfallEntries(after)) {
    REQUIRE(entry.Tick <= after.Tick);
    const double now = readValue(after, entry);
    const double later = HungryFootfall::readKept(entry.Value, after.Tick - entry.Tick + FAR_LATER);
    CAPTURE(entry.At.Carrier, entry.At.Distance, entry.Value, now, later);
    CHECK(later >= 0.0);
    CHECK(later <= now);
  }
}

TEST_CASE("Hungry footfall's value anywhere strictly inside a stretch is the stretch's value") {
  const World &world = steppedWorld();
  const Network &network = test::guestNetwork(world);

  bool sawFootfall = false;
  for (std::size_t index = 0; index < network.edges().size(); ++index) {
    const NetworkEdge &stretch = network.edges()[index];
    const double value = stretchValue(world, stretch);
    sawFootfall = sawFootfall || value != 0.0;
    // Near the From end, away from the entry at the midpoint, where a rule blending toward the
    // nodes or reading only the entry's own place would differ from the stretch's value.
    const Place place{stretch.Carrier, stretch.FromDistance + (0.1 * stretch.length())};
    CAPTURE(index);
    const std::optional<NetworkPosition> position = network.resolve(place);
    REQUIRE(position.has_value());
    const NetworkPosition resolved = position.value_or(NetworkPosition{});
    const auto *inEdge = std::get_if<EdgePosition>(&resolved);
    REQUIRE(inEdge != nullptr);
    REQUIRE(inEdge->Edge == index);
    CHECK(test::footfallAt(world, place) == value);
  }
  CHECK(sawFootfall);
}

TEST_CASE("fieldValue of hungry footfall at a node is the mean, over the node's edge ends, of the "
          "values of the stretches those ends belong to") {
  const World &world = steppedWorld();
  const Network &network = test::guestNetwork(world);

  bool sawMixedJunction = false;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::span<const EdgeEnd> ends = network.edgeEnds(node);
    REQUIRE_FALSE(ends.empty());
    double sum = 0.0;
    std::vector<double> values;
    for (const EdgeEnd &end : ends) {
      const double value = stretchValue(world, network.edges()[end.Edge]);
      sum += value;
      values.push_back(value);
    }
    CAPTURE(node, values);
    CHECK(test::footfallAt(world, network.nodePlace(node)) ==
          sum / static_cast<double>(ends.size()));
    sawMixedJunction =
        sawMixedJunction ||
        (values.size() > 2 &&
         std::ranges::adjacent_find(values, std::ranges::not_equal_to{}) != values.end());
  }
  CHECK(sawMixedJunction);
}

TEST_CASE("After a resolution, each stretch's footfall is the sum of the values, read at the "
          "world's tick, of the entries carryOver brings into it from the network before") {
  const World &world = spreadWorld();
  // The near shop's connector meets the spine at its first junction from the gate, so deleting the
  // shop joins the spine's first two stretches into one and drops the connector's entries.
  const std::vector<NetworkEdge> spine = edgesOn(test::guestNetwork(world), test::EATING_SPINE);
  REQUIRE(spine.size() > 2);
  REQUIRE(stretchValue(world, spine[0]) != 0.0);
  REQUIRE(stretchValue(world, spine[1]) != 0.0);
  const World candidate = candidateWith(world, DeleteBox{test::NEAR_SHOP});
  REQUIRE(candidate.Tick == world.Tick);

  bool sawJoin = false;
  for (const NetworkEdge &stretch : test::guestNetwork(candidate).edges()) {
    const std::vector<KeptEntry> carried = carriedInto(world, candidate, stretch);
    double sum = 0.0;
    for (const KeptEntry &entry : carried) {
      sum += readValue(world, entry);
    }
    sawJoin = sawJoin || carried.size() > 1;
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance, carried.size());
    CHECK(stretchValue(candidate, stretch) == sum);
  }
  CHECK(sawJoin);
}

TEST_CASE("A stretch that exactly one footfall entry is carried into holds that entry's value and "
          "tick") {
  const World &world = spreadWorld();
  const World candidate = candidateWith(world, gateCrossing());
  REQUIRE(edgesOn(test::guestNetwork(candidate), test::EATING_GATE_PATH).size() == 2);

  bool sawMoved = false;
  bool sawIdle = false;
  for (const NetworkEdge &stretch : test::guestNetwork(candidate).edges()) {
    const std::vector<KeptEntry> carried = carriedInto(world, candidate, stretch);
    if (carried.size() != 1) {
      continue;
    }
    const KeptEntry &from = carried.front();
    const std::vector<KeptEntry> held = entriesIn(candidate, stretch);
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance);
    REQUIRE(held.size() == 1);
    CHECK(held.front().Value == from.Value);
    CHECK(held.front().Tick == from.Tick);
    sawMoved = sawMoved || !(from.At == midpointOf(stretch));
    sawIdle = sawIdle || from.Tick < world.Tick;
  }
  // A split half, whose entry moves, and an entry kept before the last tick, whose tick shows.
  CHECK(sawMoved);
  CHECK(sawIdle);
}

} // namespace
} // namespace tpj
