#include "support/guest_parks.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/footfall.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::footfallAt;
using test::midpointOf;
using test::recordOf;

// Long enough for guests to have spread from the eating park's gate down the spine, past the near
// and starved shops' junctions, and for some to be waiting at a shop.
constexpr uint64_t SPREAD_TICK = 900;

// The eating park stepped with no commands until guests have spread over its first stretches.
const World &spreadWorld() {
  static const World world = [] {
    World stepped = test::eatingWorld();
    test::stepUntil(stepped, SPREAD_TICK);
    return stepped;
  }();
  return world;
}

// The value of hungry footfall inside the stretch.
double stretchValue(const World &world, const NetworkEdge &edge) {
  return footfallAt(world, midpointOf(edge));
}

std::vector<NetworkEdge> edgesOn(const Network &network, EntityKey carrier) {
  std::vector<NetworkEdge> edges;
  std::ranges::copy_if(network.edges(), std::back_inserter(edges),
                       [carrier](const NetworkEdge &edge) { return edge.Carrier == carrier; });
  return edges;
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

TEST_CASE("After a cycle with no commands, hungry footfall has one value strictly inside each "
          "stretch, and at each node the mean of the values at the stretch ends meeting it") {
  const World &world = spreadWorld();
  const Network &network = test::guestNetwork(world);
  const std::vector<NetworkEdge> &edges = network.edges();
  REQUIRE_FALSE(edges.empty());

  std::vector<double> sums(network.nodeCount(), 0.0);
  std::vector<uint64_t> ends(network.nodeCount(), 0);
  std::vector<std::vector<double>> meeting(network.nodeCount());
  bool sawFootfall = false;
  for (std::size_t index = 0; index < edges.size(); ++index) {
    const NetworkEdge &edge = edges[index];
    CAPTURE(index, edge.Carrier, edge.FromDistance, edge.ToDistance);
    const double value = stretchValue(world, edge);
    sawFootfall = sawFootfall || value != 0.0;
    // Near either end, where a rule that blends toward the nodes' values would differ.
    for (const double fraction : {0.1, 0.9}) {
      const Place place{edge.Carrier, edge.FromDistance + (fraction * edge.length())};
      CAPTURE(fraction);
      const std::optional<NetworkPosition> position = network.resolve(place);
      REQUIRE(position.has_value());
      const NetworkPosition resolved = position.value_or(NetworkPosition{});
      const auto *inEdge = std::get_if<EdgePosition>(&resolved);
      REQUIRE(inEdge != nullptr);
      REQUIRE(inEdge->Edge == index);
      CHECK(footfallAt(world, place) == value);
    }
    sums[edge.From] += value;
    ++ends[edge.From];
    meeting[edge.From].push_back(value);
    sums[edge.To] += value;
    ++ends[edge.To];
    meeting[edge.To].push_back(value);
  }
  CHECK(sawFootfall);

  // Every stop names its node, so a junction is sampled from each carrier meeting there.
  bool sawMixedJunction = false;
  for (const Carrier &carrier : network.carriers()) {
    for (const CarrierStop &stop : carrier.Stops) {
      CAPTURE(carrier.Key, stop.Distance, stop.Node);
      REQUIRE(ends[stop.Node] > 0);
      const double mean = sums[stop.Node] / static_cast<double>(ends[stop.Node]);
      CHECK(footfallAt(world, Place{carrier.Key, stop.Distance}) == mean);
      const std::vector<double> &values = meeting[stop.Node];
      sawMixedJunction =
          sawMixedJunction ||
          (values.size() > 2 &&
           std::ranges::adjacent_find(values, std::ranges::not_equal_to{}) != values.end());
    }
  }
  CHECK(sawMixedJunction);
}

// S: 0.0 with the Hunger of each guest standing in the stretch added, in the guests' order.
double hungerIn(const Network &network, const std::vector<GuestRecord> &guests,
                const NetworkEdge &stretch) {
  double hunger = 0.0;
  for (const GuestRecord &guest : guests) {
    if (liesIn(network, guest.At, stretch)) {
      hunger += guest.Hunger;
    }
  }
  return hunger;
}

// A stretch of the world before an edit: its midpoint carried to the network after, and its value.
struct HeldStretch {
  std::optional<Place> Midpoint;
  double Value = 0.0;
};

// V: 0.0 with the value of each held stretch whose carried midpoint lies in the stretch added, in
// the held stretches' order.
double carriedInto(const Network &network, const std::vector<HeldStretch> &held,
                   const NetworkEdge &stretch) {
  double value = 0.0;
  for (const HeldStretch &stretchBefore : held) {
    if (stretchBefore.Midpoint.has_value() &&
        liesIn(network, stretchBefore.Midpoint.value_or(Place{}), stretch)) {
      value += stretchBefore.Value;
    }
  }
  return value;
}

// Checks every stretch E of the world a cycle with no commands leaves after the candidate made
// from the world with the commands: its value is V + (S - V) / FOOTFALL_TIME, where S sums the
// hunger of the guests standing in E and V the values of the world's stretches whose midpoints
// carry over into E.
void checkAveraged(const World &world, const CommandQueue &commands) {
  const World candidate = makeCandidate(world, commands);
  World stepped = copyWorld(candidate);
  stepWorld(stepped);

  const Network &before = test::guestNetwork(world);
  const Network &carried = test::guestNetwork(candidate);
  const Network &after = test::guestNetwork(stepped);
  std::vector<HeldStretch> held;
  for (const NetworkEdge &edge : before.edges()) {
    held.push_back({carryOver(midpointOf(edge), before, carried), stretchValue(world, edge)});
  }
  std::vector<GuestRecord> guests;
  for (const EntityKey guest : parkGuests(stepped)) {
    guests.push_back(recordOf(stepped, guest));
  }
  REQUIRE_FALSE(guests.empty());

  REQUIRE_FALSE(after.edges().empty());
  for (const NetworkEdge &stretch : after.edges()) {
    const double hunger = hungerIn(after, guests, stretch);
    const double value = carriedInto(after, held, stretch);
    const double expected = value + ((hunger - value) / static_cast<double>(FOOTFALL_TIME));
    CAPTURE(stretch.Carrier, stretch.FromDistance, stretch.ToDistance, hunger, value);
    CHECK(stretchValue(stepped, stretch) == expected);
  }
}

TEST_CASE("A cycle stepped from a candidate moves each stretch's footfall 1/FOOTFALL_TIME of the "
          "way from the footfall carried into it toward the summed hunger of the guests on it") {
  const World &world = spreadWorld();
  const Network &network = test::guestNetwork(world);
  CommandQueue commands;

  SECTION("with no commands, every stretch keeps its own footfall") {
    const std::vector<NetworkEdge> &edges = network.edges();
    REQUIRE(std::ranges::any_of(
        edges, [&](const NetworkEdge &edge) { return stretchValue(world, edge) != 0.0; }));
  }
  SECTION("a path crossing the gate path splits its stretch, whose footfall goes to the half "
          "holding its midpoint") {
    const std::vector<NetworkEdge> gate = edgesOn(network, test::EATING_GATE_PATH);
    REQUIRE(gate.size() == 1);
    REQUIRE(stretchValue(world, gate.front()) != 0.0);
    // Off the stretch's midpoint, so one half holds it and the other starts empty.
    const AddPath crossing{PathKind::Guest, {{-2.0, 121.5}, {2.0, 121.5}}};
    REQUIRE(isAccepted(world, crossing));
    commands.push(crossing);
    CommandQueue check;
    check.push(crossing);
    REQUIRE(
        edgesOn(test::guestNetwork(makeCandidate(world, check)), test::EATING_GATE_PATH).size() ==
        2);
  }
  SECTION("deleting the near shop takes its connector's node off the spine, joining two stretches "
          "whose footfalls add") {
    const std::vector<NetworkEdge> spine = edgesOn(network, test::EATING_SPINE);
    // The near shop's connector meets the spine at its first junction from the gate.
    REQUIRE(spine.size() > 2);
    REQUIRE(stretchValue(world, spine[0]) != 0.0);
    REQUIRE(stretchValue(world, spine[1]) != 0.0);
    const DeleteBox removal{test::NEAR_SHOP};
    REQUIRE(isAccepted(world, removal));
    commands.push(removal);
    CommandQueue check;
    check.push(removal);
    REQUIRE(edgesOn(test::guestNetwork(makeCandidate(world, check)), test::EATING_SPINE).size() +
                1 ==
            spine.size());
  }
  SECTION("deleting the gate path drops its footfall") {
    const std::vector<NetworkEdge> gate = edgesOn(network, test::EATING_GATE_PATH);
    REQUIRE(gate.size() == 1);
    REQUIRE(stretchValue(world, gate.front()) != 0.0);
    const DeletePath removal{test::EATING_GATE_PATH};
    REQUIRE(isAccepted(world, removal));
    commands.push(removal);
  }

  checkAveraged(world, commands);
}

} // namespace
} // namespace tpj
