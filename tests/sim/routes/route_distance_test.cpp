#include "support/park_worlds.h"
#include "support/route_edits.h"
#include "support/synthetic_network.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <optional>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::ParkIntent;
using test::worldOf;

// Networks built by hand and synthetically, for routeEntries.

// A carrier whose stops are given, with a straight ground line from 0 to its last stop.
Carrier carrierOf(uint64_t key, std::vector<CarrierStop> stops) {
  const double length = stops.back().Distance;
  return Carrier{.Key = EntityKey{key},
                 .Points = {{.X = 0.0, .Z = 0.0, .Distance = 0.0},
                            {.X = length, .Z = 0.0, .Distance = length}},
                 .Stops = std::move(stops)};
}

// Two synthetic networks side by side, sharing no node: the second's nodes are numbered after the
// first's, and its anchors are on entities 10 above the first's, so neither's sources reach the
// other's nodes.
Network separatedSynthetic() {
  test::SyntheticNetwork first = test::makeSyntheticNetwork(1, 12);
  const test::SyntheticNetwork second = test::makeSyntheticNetwork(2, 8);
  for (Carrier carrier : second.Carriers) {
    for (CarrierStop &stop : carrier.Stops) {
      stop.Node += first.NodeCount;
    }
    first.Carriers.push_back(std::move(carrier));
  }
  for (const NodeAnchor &anchor : second.Anchors) {
    first.Anchors.push_back({.Node = anchor.Node + first.NodeCount,
                             .Entity = EntityKey{static_cast<uint64_t>(anchor.Entity) + 10}});
  }
  first.NodeCount += second.NodeCount;
  return first.build();
}

// Carriers with several stops. Carrier 10 stops at nodes 0, 1, and 2 at 0, 4, and 10; carrier 20
// joins node 1 to node 3 over 6; carrier 30 leaves node 3 and returns to it at 8; and carrier 40
// joins node 2 to node 3 over 3, closing a cycle. Carrier 50 joins nodes 4 and 5, apart from the
// rest. Entity 1 anchors node 0, entity 2 node 3, and entity 3 node 5.
Network junctionNetwork() {
  return {{carrierOf(10, {{.Distance = 0.0, .Node = 0},
                          {.Distance = 4.0, .Node = 1},
                          {.Distance = 10.0, .Node = 2}}),
           carrierOf(20, {{.Distance = 0.0, .Node = 1}, {.Distance = 6.0, .Node = 3}}),
           carrierOf(30, {{.Distance = 0.0, .Node = 3}, {.Distance = 8.0, .Node = 3}}),
           carrierOf(40, {{.Distance = 0.0, .Node = 2}, {.Distance = 3.0, .Node = 3}}),
           carrierOf(50, {{.Distance = 0.0, .Node = 4}, {.Distance = 5.0, .Node = 5}})},
          6,
          {{.Node = 0, .Entity = EntityKey{1}},
           {.Node = 3, .Entity = EntityKey{2}},
           {.Node = 5, .Entity = EntityKey{3}}}};
}

// An entity that anchors no node of any network here.
constexpr EntityKey NOWHERE{99};

// The entities anchoring some node of the network, ascending.
std::vector<EntityKey> anchoringEntities(const Network &network) {
  std::set<EntityKey> entities;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    if (network.nodeAnchor(node) != NULL_KEY) {
      entities.insert(network.nodeAnchor(node));
    }
  }
  return {entities.begin(), entities.end()};
}

// Whether some route joins each node to a node the source anchors.
std::vector<bool> reachable(const Network &network, EntityKey source) {
  std::vector<bool> reached(network.nodeCount(), false);
  for (const uint32_t node : network.anchoredNodes(source)) {
    reached[node] = true;
  }
  bool grew = true;
  while (grew) {
    grew = false;
    for (const NetworkEdge &edge : network.edges()) {
      if (reached[edge.From] != reached[edge.To]) {
        reached[edge.From] = true;
        reached[edge.To] = true;
        grew = true;
      }
    }
  }
  return reached;
}

// A step along an edge at a node, from its stop there to the edge's other stop, and the node at
// that other stop.
struct StepAt {
  RouteStep Step;
  uint32_t Other = 0;
  double Length = 0.0;
};

std::vector<StepAt> stepsAt(const Network &network, uint32_t node) {
  std::vector<StepAt> steps;
  for (const NetworkEdge &edge : network.edges()) {
    if (edge.From == node) {
      steps.push_back({{edge.Carrier, edge.FromDistance, edge.ToDistance}, edge.To, edge.length()});
    }
    if (edge.To == node) {
      steps.push_back(
          {{edge.Carrier, edge.ToDistance, edge.FromDistance}, edge.From, edge.length()});
    }
  }
  return steps;
}

// A node's entry, or a default one where it has none, for callers that have checked it has one.
RouteEntry entryAt(const std::vector<std::optional<RouteEntry>> &entries, uint32_t node) {
  return entries[node].value_or(RouteEntry{});
}

bool isAnchoredTo(const Network &network, uint32_t node, EntityKey source) {
  return network.nodeAnchor(node) == source;
}

std::vector<std::pair<const char *, Network>> routeNetworks() {
  return {{"two separated synthetic networks", separatedSynthetic()},
          {"carriers with several stops, one a loop", junctionNetwork()}};
}

TEST_CASE("routeEntries gives one element per node, with an entry exactly at the nodes some route "
          "joins to a node the source anchors") {
  for (const auto &[name, network] : routeNetworks()) {
    INFO(name);
    std::vector<EntityKey> sources = anchoringEntities(network);
    REQUIRE(sources.size() >= 2);
    sources.push_back(NOWHERE);
    for (const EntityKey source : sources) {
      INFO("source " << static_cast<uint64_t>(source));
      const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, source);
      REQUIRE(entries.size() == network.nodeCount());
      const std::vector<bool> reached = reachable(network, source);
      for (uint32_t node = 0; node < network.nodeCount(); ++node) {
        INFO("node " << node);
        CHECK(entries[node].has_value() == reached[node]);
      }
    }
  }
}

// These three properties make each distance the least route length: an anchored node's is 0.0,
// every other node's is its neighbor's along some edge plus the edge's length, so it is the length
// of a route walked back to an anchored node, and no edge shortens a node's distance, so no route
// is shorter.
TEST_CASE("routeEntries gives a source's anchored nodes distance 0.0 and every other node with an "
          "entry the least length of a route from an anchored node, added from the anchored end") {
  for (const auto &[name, network] : routeNetworks()) {
    INFO(name);
    for (const EntityKey source : anchoringEntities(network)) {
      INFO("source " << static_cast<uint64_t>(source));
      const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, source);
      REQUIRE(entries.size() == network.nodeCount());
      for (uint32_t node = 0; node < network.nodeCount(); ++node) {
        INFO("node " << node);
        if (!entries[node]) {
          continue;
        }
        const double distance = entryAt(entries, node).Distance;
        if (isAnchoredTo(network, node, source)) {
          CHECK(distance == 0.0);
          continue;
        }
        const std::vector<StepAt> steps = stepsAt(network, node);
        CHECK(std::ranges::any_of(steps, [&](const StepAt &step) {
          return entries[step.Other] &&
                 entryAt(entries, step.Other).Distance + step.Length == distance;
        }));
      }
      for (const NetworkEdge &edge : network.edges()) {
        if (entries[edge.From] && entries[edge.To]) {
          INFO("edge on carrier " << static_cast<uint64_t>(edge.Carrier) << " from "
                                  << edge.FromDistance);
          CHECK(entryAt(entries, edge.From).Distance <=
                entryAt(entries, edge.To).Distance + edge.length());
          CHECK(entryAt(entries, edge.To).Distance <=
                entryAt(entries, edge.From).Distance + edge.length());
        }
      }
    }
  }
}

TEST_CASE("routeEntries gives Next RouteStep{} at a source's anchored nodes, and at every other "
          "node with an entry a step along an edge at the node that achieves its distance") {
  for (const auto &[name, network] : routeNetworks()) {
    INFO(name);
    for (const EntityKey source : anchoringEntities(network)) {
      INFO("source " << static_cast<uint64_t>(source));
      const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, source);
      REQUIRE(entries.size() == network.nodeCount());
      for (uint32_t node = 0; node < network.nodeCount(); ++node) {
        INFO("node " << node);
        if (!entries[node]) {
          continue;
        }
        const RouteEntry entry = entryAt(entries, node);
        if (isAnchoredTo(network, node, source)) {
          CHECK(entry.Next == RouteStep{});
          continue;
        }
        const std::vector<StepAt> steps = stepsAt(network, node);
        CHECK(std::ranges::any_of(steps, [&](const StepAt &step) {
          return step.Step == entry.Next && entries[step.Other] &&
                 entryAt(entries, step.Other).Distance + step.Length == entry.Distance;
        }));
      }
    }
  }
}

constexpr EntityKey SOURCE{5};

TEST_CASE("Among steps achieving a node's distance, Next is on the lowest carrier key, even when "
          "that step walks toward higher distances") {
  // Carriers 10 and 20 both join node 1 to the source's node 0 over 5, running opposite ways, so
  // carrier 10's step walks toward higher distances and carrier 20's toward lower.
  const Network network{
      {carrierOf(10, {{.Distance = 0.0, .Node = 1}, {.Distance = 5.0, .Node = 0}}),
       carrierOf(20, {{.Distance = 0.0, .Node = 0}, {.Distance = 5.0, .Node = 1}})},
      2,
      {{.Node = 0, .Entity = SOURCE}}};
  const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, SOURCE);
  REQUIRE(entries.size() == 2);
  REQUIRE(entries[1].has_value());
  CHECK(entries[1] ==
        std::optional<RouteEntry>{RouteEntry{
            .Distance = 5.0, .Next = {.Carrier = EntityKey{10}, .From = 0.0, .To = 5.0}}});
}

TEST_CASE("Among steps achieving a node's distance on one carrier, Next walks toward lower "
          "distances before higher, even from a higher stop") {
  // Carrier 10 stops at node 1 twice, at 0 and 8, with the source's node 0 at 4 between them, so
  // node 1 has a step toward higher distances from 0 and one toward lower from 8.
  const Network network{{carrierOf(10, {{.Distance = 0.0, .Node = 1},
                                        {.Distance = 4.0, .Node = 0},
                                        {.Distance = 8.0, .Node = 1}})},
                        2,
                        {{.Node = 0, .Entity = SOURCE}}};
  const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, SOURCE);
  REQUIRE(entries.size() == 2);
  REQUIRE(entries[1].has_value());
  CHECK(entries[1] ==
        std::optional<RouteEntry>{RouteEntry{
            .Distance = 4.0, .Next = {.Carrier = EntityKey{10}, .From = 8.0, .To = 4.0}}});
}

TEST_CASE("Among steps achieving a node's distance on one carrier in one direction, Next leaves "
          "from the lowest stop") {
  // Carrier 10 stops at node 1 at 3 and 9, with the source's nodes 0 and 2 at 0 and 6, so node 1
  // has two steps toward lower distances, from 3 and from 9, and one toward higher, all of length
  // 3.
  const Network network{{carrierOf(10, {{.Distance = 0.0, .Node = 0},
                                        {.Distance = 3.0, .Node = 1},
                                        {.Distance = 6.0, .Node = 2},
                                        {.Distance = 9.0, .Node = 1}})},
                        3,
                        {{.Node = 0, .Entity = SOURCE}, {.Node = 2, .Entity = SOURCE}}};
  const std::vector<std::optional<RouteEntry>> entries = routeEntries(network, SOURCE);
  REQUIRE(entries.size() == 3);
  REQUIRE(entries[1].has_value());
  CHECK(entries[1] ==
        std::optional<RouteEntry>{RouteEntry{
            .Distance = 3.0, .Next = {.Carrier = EntityKey{10}, .From = 3.0, .To = 0.0}}});
}

// sampleRouteEdge.

constexpr EntityKey EDGE_CARRIER{7};

// A place 3 from the From end and 5 from the To end of the edge on carrier 7 from 2 to 10.
EdgeSample<RouteEntry> sampleWith(const std::vector<double> &atFrom,
                                  const std::vector<double> &atTo) {
  EdgeSample<RouteEntry> sample;
  sample.Edge = NetworkEdge{
      .Carrier = EDGE_CARRIER, .From = 0, .To = 1, .FromDistance = 2.0, .ToDistance = 10.0};
  sample.FromOffset = 3.0;
  sample.ToOffset = 5.0;
  // Each end's own step, which the sample never gives, since a place inside the edge steps along
  // it.
  const RouteStep elsewhere{.Carrier = EntityKey{99}, .From = 1.0, .To = 0.0};
  for (const double distance : atFrom) {
    sample.AtFrom.push_back({.Distance = distance, .Next = elsewhere});
  }
  for (const double distance : atTo) {
    sample.AtTo.push_back({.Distance = distance, .Next = elsewhere});
  }
  return sample;
}

constexpr RouteStep TOWARD_FROM{.Carrier = EDGE_CARRIER, .From = 10.0, .To = 2.0};
constexpr RouteStep TOWARD_TO{.Carrier = EDGE_CARRIER, .From = 2.0, .To = 10.0};

TEST_CASE("sampleRouteEdge gives one entry, the least of each end's entries with that end's "
          "offset added, stepping along the edge toward that end") {
  SECTION("the To end nearer") {
    CHECK(sampleRouteEdge(sampleWith({4.0}, {1.0})) ==
          std::vector<RouteEntry>{{.Distance = 6.0, .Next = TOWARD_TO}});
  }
  SECTION("the From end nearer") {
    CHECK(sampleRouteEdge(sampleWith({1.0}, {4.0})) ==
          std::vector<RouteEntry>{{.Distance = 4.0, .Next = TOWARD_FROM}});
  }
  SECTION("the least of several entries at one end") {
    CHECK(sampleRouteEdge(sampleWith({9.0, 1.0}, {4.0})) ==
          std::vector<RouteEntry>{{.Distance = 4.0, .Next = TOWARD_FROM}});
  }
  SECTION("an entry at one end only") {
    CHECK(sampleRouteEdge(sampleWith({}, {4.0})) ==
          std::vector<RouteEntry>{{.Distance = 9.0, .Next = TOWARD_TO}});
  }
}

TEST_CASE("sampleRouteEdge breaks a tie between the two ends toward the From end") {
  // 4 + 3 and 2 + 5 are both exactly 7.
  CHECK(sampleRouteEdge(sampleWith({4.0}, {2.0})) ==
        std::vector<RouteEntry>{{.Distance = 7.0, .Next = TOWARD_FROM}});
}

TEST_CASE("sampleRouteEdge ignores entries inside the edge, and gives none with no entry at "
          "either end") {
  const auto withAlong = [](EdgeSample<RouteEntry> sample) {
    sample.Along.push_back({.FromOffset = 1.0, .ToOffset = 7.0, .Value = RouteEntry{}});
    return sample;
  };
  CHECK(sampleRouteEdge(withAlong(sampleWith({4.0}, {1.0}))) ==
        sampleRouteEdge(sampleWith({4.0}, {1.0})));
  CHECK(sampleRouteEdge(sampleWith({}, {})).empty());
  CHECK(sampleRouteEdge(withAlong(sampleWith({}, {}))).empty());
}

// Resolved parks.

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

World routesWorld() {
  World world = loadWorld(makeParkSchema(), routesText());
  resolveWorld(world);
  return world;
}

ParkPath pathOf(uint64_t key, PathKind kind, double x, double fromZ, double toZ) {
  return ParkPath{.Key = EntityKey{key}, .Kind = kind, .Points = {{x, fromZ}, {x, toZ}}};
}

constexpr EntityKey NEAR_SHOP{1};
constexpr EntityKey FAR_SHOP{3};
constexpr EntityKey DEPOT{7};

// Each box faces -x, so its front door lies half its depth toward -x and its back door half its
// depth toward +x. The near shop's front door is 2 m from guest path 2 and its back door 2 m from
// backstage path 6. The far shop's front door is 2 m from guest path 4, apart from path 2, and its
// back door has no backstage line near. The depot's front door is 2 m from backstage path 8, apart
// from path 6. Guest path 5 is alone and anchors nothing.
World separatedWorld() {
  const Pose facingWest{.X = 0.0, .Z = 0.0, .FacingX = -1.0, .FacingZ = 0.0};
  const double shopHalf = boxSize(BoxKind::Shop).Depth / 2.0;
  const double depotHalf = boxSize(BoxKind::Depot).Depth / 2.0;
  Pose farPose = facingWest;
  farPose.Z = 100.0;
  Pose depotPose = facingWest;
  depotPose.Z = -60.0;
  World world = worldOf(
      ParkIntent{.Entrances = {},
                 .Paths = {pathOf(2, PathKind::Guest, -shopHalf - 2.0, -10.0, 10.0),
                           pathOf(4, PathKind::Guest, -shopHalf - 2.0, 90.0, 110.0),
                           pathOf(5, PathKind::Guest, 60.0, 40.0, 60.0),
                           pathOf(6, PathKind::Backstage, shopHalf + 2.0, -10.0, 10.0),
                           pathOf(8, PathKind::Backstage, -depotHalf - 2.0, -70.0, -50.0)},
                 .Boxes = {ParkBox{.Key = NEAR_SHOP, .Kind = BoxKind::Shop, .At = facingWest},
                           ParkBox{.Key = FAR_SHOP, .Kind = BoxKind::Shop, .At = farPose},
                           ParkBox{.Key = DEPOT, .Kind = BoxKind::Depot, .At = depotPose}}});
  resolveWorld(world);
  const Network &guest = parkNetwork(world, PathKind::Guest);
  const Network &backstage = parkNetwork(world, PathKind::Backstage);
  REQUIRE(anchoringEntities(guest) == std::vector<EntityKey>{NEAR_SHOP, FAR_SHOP});
  REQUIRE(anchoringEntities(backstage) == std::vector<EntityKey>{NEAR_SHOP, DEPOT});
  return world;
}

std::vector<std::pair<const char *, World>> resolvedParks() {
  std::vector<std::pair<const char *, World>> parks;
  parks.emplace_back("tests/parks/routes.park", routesWorld());
  parks.emplace_back("separated networks and a lone path", separatedWorld());
  return parks;
}

template <PathKind Kind>
const ResolvedEntries<RouteDistance<Kind>> *resolvedOf(const World &world) {
  const entt::entity holder = world.findEntity(fieldKey(RouteDistance<Kind>::Name));
  return holder == entt::null
             ? nullptr
             : world.Registry.try_get<ResolvedEntries<RouteDistance<Kind>>>(holder);
}

template <PathKind Kind> void checkSampledAtNodes(const World &world) {
  INFO("kind " << static_cast<int>(Kind));
  const Network &network = parkNetwork(world, Kind);
  const std::vector<EntityKey> sources = anchoringEntities(network);
  std::vector<std::vector<std::optional<RouteEntry>>> entries;
  entries.reserve(sources.size());
  for (const EntityKey source : sources) {
    entries.push_back(routeEntries(network, source));
  }
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    INFO("node " << node);
    std::vector<std::pair<EntityKey, RouteEntry>> expected;
    for (std::size_t index = 0; index < sources.size(); ++index) {
      if (entries[index][node]) {
        expected.emplace_back(sources[index], entryAt(entries[index], node));
      }
    }
    std::vector<std::pair<EntityKey, RouteEntry>> sampled;
    for (const SampledEntry<RouteEntry> &entry :
         sampleField<RouteDistance<Kind>>(world, network, network.nodePlace(node))) {
      sampled.emplace_back(entry.Source, entry.Value);
    }
    CHECK(sampled == expected);
  }
}

TEST_CASE("sampleField of a kind's route distance field at a node gives each source's routeEntries "
          "entry there, in source order, and none for a source that cannot reach it") {
  for (const auto &[name, world] : resolvedParks()) {
    INFO(name);
    checkSampledAtNodes<PathKind::Guest>(world);
    checkSampledAtNodes<PathKind::Backstage>(world);
  }
}

// The source's one sampled entry at the place, or none.
template <PathKind Kind>
std::optional<RouteEntry> sampledFor(const World &world, EntityKey source, const Place &place) {
  std::optional<RouteEntry> found;
  for (const SampledEntry<RouteEntry> &entry :
       sampleField<RouteDistance<Kind>>(world, parkNetwork(world, Kind), place)) {
    if (entry.Source == source) {
      REQUIRE_FALSE(found.has_value());
      found = entry.Value;
    }
  }
  return found;
}

// A walk from a place toward a source: the lengths walked, in walk order, and where it stopped.
struct Walk {
  std::vector<double> Lengths;
  Place Last;
};

// Walks from the place toward the source by sampling and following Next until Next's carrier is
// NULL_KEY. The first entry is the one sampled at the place.
template <PathKind Kind>
Walk walkToward(const World &world, EntityKey source, const Place &start, RouteEntry entry) {
  const Network &network = parkNetwork(world, Kind);
  Walk walk{.Lengths = {}, .Last = start};
  while (entry.Next.Carrier != NULL_KEY) {
    // Each step reaches a node nearer the source, so a walk that arrives visits each node once.
    REQUIRE(walk.Lengths.size() <= network.nodeCount());
    const std::optional<NetworkPosition> position = network.resolve(walk.Last);
    REQUIRE(position.has_value());
    const bool atNode = position && std::holds_alternative<NodePosition>(*position);
    walk.Lengths.push_back(atNode ? std::abs(entry.Next.To - entry.Next.From)
                                  : std::abs(entry.Next.To - walk.Last.Distance));
    walk.Last = Place{.Carrier = entry.Next.Carrier, .Distance = entry.Next.To};
    const std::optional<RouteEntry> next = sampledFor<Kind>(world, source, walk.Last);
    REQUIRE(next.has_value());
    entry = next.value_or(RouteEntry{});
  }
  return walk;
}

// Checks that the walk toward the source from the place ends at a node the source anchors, having
// walked exactly the distance first sampled, the lengths added from the anchored end. Returns
// whether the place had an entry to walk from.
template <PathKind Kind> bool checkWalk(const World &world, EntityKey source, const Place &start) {
  const Network &network = parkNetwork(world, Kind);
  const std::optional<RouteEntry> first = sampledFor<Kind>(world, source, start);
  if (!first) {
    return false;
  }
  const Walk walk = walkToward<Kind>(world, source, start, first.value_or(RouteEntry{}));
  const std::optional<NetworkPosition> arrived = network.resolve(walk.Last);
  REQUIRE(arrived.has_value());
  const auto *node = arrived ? std::get_if<NodePosition>(&*arrived) : nullptr;
  REQUIRE(node != nullptr);
  CHECK(network.nodeAnchor(node->Node) == source);
  double walked = 0.0;
  for (auto length = walk.Lengths.rbegin(); length != walk.Lengths.rend(); ++length) {
    walked += *length;
  }
  CHECK(walked == first->Distance);
  return true;
}

// Walks toward each source from every node and from the middle of every edge.
template <PathKind Kind> void checkWalks(const World &world, int &fromNodes, int &fromEdges) {
  INFO("kind " << static_cast<int>(Kind));
  const Network &network = parkNetwork(world, Kind);
  for (const EntityKey source : anchoringEntities(network)) {
    INFO("source " << static_cast<uint64_t>(source));
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      INFO("from node " << node);
      fromNodes += checkWalk<Kind>(world, source, network.nodePlace(node)) ? 1 : 0;
    }
    for (const NetworkEdge &edge : network.edges()) {
      const Place middle{.Carrier = edge.Carrier,
                         .Distance = edge.FromDistance + (edge.length() / 2.0)};
      INFO("from carrier " << static_cast<uint64_t>(middle.Carrier) << " at " << middle.Distance);
      fromEdges += checkWalk<Kind>(world, source, middle) ? 1 : 0;
    }
  }
}

TEST_CASE(
    "Following Next from any place with a source's entry, sampling at each step, reaches a "
    "node the source anchors, and the lengths walked add up to the sampled distance exactly") {
  for (const auto &[name, world] : resolvedParks()) {
    INFO(name);
    int fromNodes = 0;
    int fromEdges = 0;
    checkWalks<PathKind::Guest>(world, fromNodes, fromEdges);
    checkWalks<PathKind::Backstage>(world, fromNodes, fromEdges);
    CHECK(fromNodes > 0);
    CHECK(fromEdges > 0);
  }
}

// Derivation from intent.

// Whether the worlds a sequence reached had a source with entries beyond its own nodes, and a
// source some node of its network has no entry for.
struct FieldCoverage {
  bool Routed = false;
  bool Unreachable = false;

  template <PathKind Kind> void noteKind(const World &world) {
    const auto *resolved = resolvedOf<Kind>(world);
    if (resolved == nullptr) {
      return;
    }
    const Network &network = parkNetwork(world, Kind);
    for (const FieldSlot<RouteEntry> &slot : resolved->Slots) {
      Routed = Routed || slot.Entries.size() > network.anchoredNodes(slot.Source).size();
      Unreachable = Unreachable || slot.Entries.size() < network.nodeCount();
    }
  }

  void note(const World &world) {
    noteKind<PathKind::Guest>(world);
    noteKind<PathKind::Backstage>(world);
  }
};

constexpr int CYCLES = 60;

// The first edits of every sequence, which reach each case the sequences claim in a corner of the
// park away from the middle, where the random edits gather. A lone guest path comes first. A
// second guest path follows, 40 m from it, and then a shop facing the second across a 1 m gap, so
// its front door is 2.5 m from the line: the shop is a source whose entries reach the second
// path's nodes and never the first's. A depot with no backstage line near is a box with no
// connector.
std::vector<ParkEdit> openingEdits() {
  return {AddPath{PathKind::Guest, {{-100.0, -100.0}, {-80.0, -100.0}}},
          AddPath{PathKind::Guest, {{-100.0, -60.0}, {-80.0, -60.0}}},
          AddBox{BoxKind::Shop, Pose{.X = -90.0,
                                     .Z = -60.0 + (pathWidth(PathKind::Guest) / 2.0) + 1.0 +
                                          (boxSize(BoxKind::Shop).Depth / 2.0),
                                     .FacingX = 0.0,
                                     .FacingZ = -1.0}},
          AddBox{BoxKind::Depot, Pose{.X = -90.0, .Z = -20.0, .FacingX = 0.0, .FacingZ = 1.0}}};
}

// The sequence's edit for the cycle: the opening's, each of which the world must accept, and then
// random ones.
ParkEdit sequenceEdit(int cycle, test::RouteEditDraws &draws, const World &world) {
  const std::vector<ParkEdit> opening = openingEdits();
  if (static_cast<std::size_t>(cycle) < opening.size()) {
    const ParkEdit &edit = opening[static_cast<std::size_t>(cycle)];
    REQUIRE(isAccepted(world, edit));
    return edit;
  }
  return test::routeEdit(draws, world);
}

TEST_CASE("Every world a random edit sequence reaches resolves route distance without throwing, "
          "including a lone path, an unconnected box, and an unreachable source") {
  test::RouteEditDraws draws(33);
  World world = worldOf({});
  FieldCoverage coverage;
  bool reachedLone = false;
  bool reachedUnconnected = false;
  const auto note = [&](const World &resolved) {
    coverage.note(resolved);
    reachedLone = reachedLone || parkPaths(resolved).size() == 1;
    for (const ParkBox &box : parkBoxes(resolved)) {
      reachedUnconnected =
          reachedUnconnected ||
          (parkNetwork(resolved, PathKind::Guest).anchoredNodes(box.Key).empty() &&
           parkNetwork(resolved, PathKind::Backstage).anchoredNodes(box.Key).empty());
    }
  };

  REQUIRE_NOTHROW(resolveWorld(world));
  note(world);
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, sequenceEdit(cycle, draws, world));
    REQUIRE_NOTHROW(stepWorld(world, queue));
    note(world);
  }
  CHECK(reachedLone);
  CHECK(reachedUnconnected);
  CHECK(coverage.Routed);
  CHECK(coverage.Unreachable);
}

} // namespace
} // namespace tpj
