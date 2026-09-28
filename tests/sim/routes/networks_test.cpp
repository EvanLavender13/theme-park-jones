#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::ParkIntent;
using test::worldOf;

// Ground-line distances are sums of rounded steps, so a distance stated from the geometry may
// differ from the carrier's by rounding, far below the tolerance.
constexpr double SLACK = 1e-9;
constexpr double REACH = JUNCTION_TOLERANCE + SLACK;

ParkPath pathOf(uint64_t key, PathKind kind, std::vector<ParkPoint> points) {
  return ParkPath{.Key = EntityKey{key}, .Kind = kind, .Points = std::move(points)};
}

ParkPath guest(uint64_t key, std::vector<ParkPoint> points) {
  return pathOf(key, PathKind::Guest, std::move(points));
}

ParkIntent pathsOnly(std::vector<ParkPath> paths) {
  return ParkIntent{.Entrances = {}, .Paths = std::move(paths), .Boxes = {}};
}

World resolvedWorld(ParkIntent intent) {
  World world = worldOf(std::move(intent));
  resolveWorld(world);
  return world;
}

const Carrier *findCarrier(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  return found == network.carriers().end() ? nullptr : &*found;
}

// The nodes of the carrier's stops within the tolerance of the distance.
std::set<uint32_t> nodesNear(const Carrier &carrier, double distance) {
  std::set<uint32_t> nodes;
  for (const CarrierStop &stop : carrier.Stops) {
    if (std::abs(stop.Distance - distance) <= REACH) {
      nodes.insert(stop.Node);
    }
  }
  return nodes;
}

// Where two segments meet: a carrier and a distance on each, which may be the same carrier.
struct Meeting {
  EntityKey First = NULL_KEY;
  double FirstDistance = 0.0;
  EntityKey Second = NULL_KEY;
  double SecondDistance = 0.0;
};

// A guest park whose meetings are all known.
struct MeetingCase {
  const char *Name = "";
  ParkIntent Intent;
  std::vector<Meeting> Meetings;
};

constexpr EntityKey FIRST{1};
constexpr EntityKey SECOND{2};

// A two-point path's ground line is its chord, since both phantom neighbors lie on it, so a point
// on it lies at its ground distance from the first point. FIRST runs along z = 0 from x = -10 to
// x = 10, with a line point at every whole x, so a point (x, 0) is at distance x + 10 on it.
ParkPath firstLine() { return guest(1, {{-10.0, 0.0}, {10.0, 0.0}}); }

// Its segments cross between line points of both.
MeetingCase crossing() {
  return {"a crossing",
          pathsOnly({firstLine(), guest(2, {{0.3, -10.0}, {0.3, 10.0}})}),
          {{FIRST, 10.3, SECOND, 10.0}}};
}

// The second path's first point was snapped onto the first's line, between its line points.
MeetingCase snapped() {
  return {"a snapped point",
          pathsOnly({firstLine(), guest(2, {{2.5, 0.0}, {2.5, 10.0}})}),
          {{FIRST, 12.5, SECOND, 0.0}}};
}

// The second line passes through the first's line point (2, 0) between two of its own, so the
// meeting is at an end of the first's segments that touches the second's without crossing it.
MeetingCase throughLinePoint() {
  return {"a line through another's line point",
          pathsOnly({firstLine(), guest(2, {{2.0, -5.5}, {2.0, 4.5}})}),
          {{FIRST, 12.0, SECOND, 5.5}}};
}

// The second line runs along z = 0 from x = 0.5 to x = 20.5, sharing the stretch from 0.5 to 10
// with the first. Every line point of either along that stretch lies on the other's line: the
// first's at whole x and the second's at half x, including the stretch's two ends.
MeetingCase collinearOverlap() {
  MeetingCase overlap{
      "a collinear overlap", pathsOnly({firstLine(), guest(2, {{0.5, 0.0}, {20.5, 0.0}})}), {}};
  for (int half = 1; half <= 20; ++half) {
    const double x = 0.5 * half;
    overlap.Meetings.push_back({FIRST, x + 10.0, SECOND, x - 0.5});
  }
  return overlap;
}

// A path whose first span, from (-10, 10) to (10, -10), crosses its last, from (10, 10) to
// (-10, -10). The path reversed is its mirror across z = 0, so the crossing lies on z = 0, where
// the first and last of its line's descents through z = 0 are.
MeetingCase selfCrossing() {
  MeetingCase self{
      "a path crossing itself",
      pathsOnly({guest(1, {{-10.0, 10.0}, {10.0, -10.0}, {10.0, 10.0}, {-10.0, -10.0}})}),
      {}};
  const std::vector<CarrierPoint> line = groundLine(self.Intent.Paths.front().Points);
  std::vector<double> descents;
  for (std::size_t index = 0; index + 1 < line.size(); ++index) {
    const CarrierPoint &a = line[index];
    const CarrierPoint &b = line[index + 1];
    if (a.Z > 0.0 && b.Z <= 0.0) {
      const double u = a.Z / (a.Z - b.Z);
      descents.push_back(a.Distance + (u * (b.Distance - a.Distance)));
    }
  }
  REQUIRE(descents.size() >= 2);
  self.Meetings.push_back({FIRST, descents.front(), FIRST, descents.back()});
  return self;
}

std::vector<MeetingCase> meetingCases() {
  return {crossing(), snapped(), throughLinePoint(), collinearOverlap(), selfCrossing()};
}

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

TEST_CASE("A resolved world's network of each kind has exactly the paths of that kind with a "
          "ground line as carriers, each keyed by its path and running along its ground line") {
  // Kinds interleave by key, and one path of each kind has an empty ground line: one with a single
  // point, and one with a point outside the park.
  const ParkIntent intent = pathsOnly({
      guest(3, {{-20.0, 0.0}, {0.0, 10.0}, {20.0, 0.0}}),
      pathOf(4, PathKind::Backstage, {{0.0, -20.0}, {0.0, 20.0}}),
      guest(5, {{5.0, 5.0}}),
      pathOf(6, PathKind::Backstage, {{0.0, 0.0}, {130.0, 0.0}}),
      guest(7, {{-30.0, -30.0}, {-10.0, -40.0}}),
      pathOf(8, PathKind::Backstage, {{40.0, 40.0}, {60.0, 40.0}}),
  });
  const World world = resolvedWorld(intent);

  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    std::vector<EntityKey> expectedKeys;
    for (const ParkPath &path : intent.Paths) {
      if (path.Kind == kind && !groundLine(path.Points).empty()) {
        expectedKeys.push_back(path.Key);
      }
    }
    const Network &network = parkNetwork(world, kind);
    std::vector<EntityKey> keys;
    for (const Carrier &carrier : network.carriers()) {
      keys.push_back(carrier.Key);
    }
    CHECK(keys == expectedKeys);
    for (const ParkPath &path : intent.Paths) {
      if (const Carrier *carrier = findCarrier(network, path.Key)) {
        INFO("path " << static_cast<uint64_t>(path.Key));
        CHECK(carrier->Points == groundLine(path.Points));
      }
    }
  }
}

TEST_CASE("A world never resolved has an empty network of each kind") {
  const World world =
      worldOf(pathsOnly({guest(1, {{-10.0, 0.0}, {10.0, 0.0}}),
                         pathOf(2, PathKind::Backstage, {{0.0, -10.0}, {0.0, 10.0}})}));
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    CHECK(parkNetwork(world, kind).carriers().empty());
    CHECK(parkNetwork(world, kind).nodeCount() == 0);
  }
}

TEST_CASE("Where two segments meet, each carrier has a stop within the tolerance of the meeting's "
          "distance on it, and the two stops are at one node") {
  for (const MeetingCase &meetingCase : meetingCases()) {
    INFO(meetingCase.Name);
    const World world = resolvedWorld(meetingCase.Intent);
    const Network &network = parkNetwork(world, PathKind::Guest);
    for (const Meeting &meeting : meetingCase.Meetings) {
      INFO("meeting at " << meeting.FirstDistance << " and " << meeting.SecondDistance);
      const Carrier *first = findCarrier(network, meeting.First);
      const Carrier *second = findCarrier(network, meeting.Second);
      REQUIRE(first != nullptr);
      REQUIRE(second != nullptr);
      const std::set<uint32_t> firstNodes = nodesNear(*first, meeting.FirstDistance);
      const std::set<uint32_t> secondNodes = nodesNear(*second, meeting.SecondDistance);
      CHECK_FALSE(firstNodes.empty());
      CHECK_FALSE(secondNodes.empty());
      CHECK(std::ranges::any_of(firstNodes,
                                [&](uint32_t node) { return secondNodes.contains(node); }));
    }
  }
}

TEST_CASE("Every stop but a carrier's first and last lies within the tolerance of a meeting's "
          "distance on that carrier") {
  for (const MeetingCase &meetingCase : meetingCases()) {
    INFO(meetingCase.Name);
    const World world = resolvedWorld(meetingCase.Intent);
    const Network &network = parkNetwork(world, PathKind::Guest);
    REQUIRE(network.carriers().size() == meetingCase.Intent.Paths.size());
    for (const Carrier &carrier : network.carriers()) {
      INFO("carrier " << static_cast<uint64_t>(carrier.Key));
      std::vector<double> distances;
      for (const Meeting &meeting : meetingCase.Meetings) {
        if (meeting.First == carrier.Key) {
          distances.push_back(meeting.FirstDistance);
        }
        if (meeting.Second == carrier.Key) {
          distances.push_back(meeting.SecondDistance);
        }
      }
      for (std::size_t index = 1; index + 1 < carrier.Stops.size(); ++index) {
        const double stop = carrier.Stops[index].Distance;
        INFO("stop at " << stop);
        CHECK(std::ranges::any_of(
            distances, [stop](double distance) { return std::abs(stop - distance) <= REACH; }));
      }
    }
  }
}

TEST_CASE("A path whose line meets no line of its kind, nor itself, stops only at its two ends, "
          "at nodes no other stop has") {
  // The backstage path crosses the first guest path, which is of the other kind, and the second
  // guest path lies apart from both.
  const World world =
      resolvedWorld(pathsOnly({guest(1, {{-10.0, 0.0}, {10.0, 0.0}}),
                               pathOf(2, PathKind::Backstage, {{0.3, -10.0}, {0.3, 10.0}}),
                               guest(3, {{50.0, 50.0}, {60.0, 55.0}, {70.0, 50.0}})}));
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    const Network &network = parkNetwork(world, kind);
    REQUIRE_FALSE(network.carriers().empty());
    std::vector<uint32_t> nodes;
    for (const Carrier &carrier : network.carriers()) {
      INFO("carrier " << static_cast<uint64_t>(carrier.Key));
      REQUIRE(carrier.Stops.size() == 2);
      CHECK(carrier.Stops.front().Distance == 0.0);
      CHECK(carrier.Stops.back().Distance == carrier.Points.back().Distance);
      nodes.push_back(carrier.Stops.front().Node);
      nodes.push_back(carrier.Stops.back().Node);
    }
    CHECK(std::set<uint32_t>(nodes.begin(), nodes.end()).size() == nodes.size());
  }
}

// FIRST is crossed by lines 0.5 mm from its start and from its end, which fall into its end stops,
// and by three lines 0.8 mm apart, whose meetings with it chain past the tolerance from the first.
// The three lie within the tolerance of each other, so they meet along their whole length too.
ParkIntent clusteredMeetings() {
  ParkIntent intent = pathsOnly({firstLine()});
  uint64_t key = 2;
  for (const double x : {-9.9995, 0.3, 0.3008, 0.3016, 9.9995}) {
    intent.Paths.push_back(guest(key++, {{x, -5.0}, {x, 5.0}}));
  }
  return intent;
}

TEST_CASE("Consecutive stops of a carrier lie more than the tolerance apart") {
  const World clustered = resolvedWorld(clusteredMeetings());
  const World self = resolvedWorld(selfCrossing().Intent);
  const World overlap = resolvedWorld(collinearOverlap().Intent);
  for (const World *world : {&clustered, &self, &overlap}) {
    const Network &network = parkNetwork(*world, PathKind::Guest);
    REQUIRE_FALSE(network.carriers().empty());
    for (const Carrier &carrier : network.carriers()) {
      INFO("carrier " << static_cast<uint64_t>(carrier.Key));
      for (std::size_t index = 0; index + 1 < carrier.Stops.size(); ++index) {
        INFO("stops at " << carrier.Stops[index].Distance << " and "
                         << carrier.Stops[index + 1].Distance);
        CHECK(carrier.Stops[index + 1].Distance - carrier.Stops[index].Distance >
              JUNCTION_TOLERANCE);
      }
    }
  }
}

TEST_CASE("Nodes are numbered in order of first stop, walking carriers in key order and each "
          "carrier's stops in ascending distance") {
  World routes = loadWorld(makeParkSchema(), routesText());
  resolveWorld(routes);
  const World clustered = resolvedWorld(clusteredMeetings());
  const World self = resolvedWorld(selfCrossing().Intent);
  const std::vector<std::pair<const char *, const Network *>> networks{
      {"routes.park guest", &parkNetwork(routes, PathKind::Guest)},
      {"routes.park backstage", &parkNetwork(routes, PathKind::Backstage)},
      {"clustered meetings", &parkNetwork(clustered, PathKind::Guest)},
      {"a path crossing itself", &parkNetwork(self, PathKind::Guest)},
  };
  for (const auto &[name, network] : networks) {
    INFO(name);
    REQUIRE_FALSE(network->carriers().empty());
    std::set<uint32_t> seen;
    uint32_t next = 0;
    EntityKey previousKey = NULL_KEY;
    for (const Carrier &carrier : network->carriers()) {
      CHECK(carrier.Key > previousKey);
      previousKey = carrier.Key;
      for (const CarrierStop &stop : carrier.Stops) {
        INFO("carrier " << static_cast<uint64_t>(carrier.Key) << " stop at " << stop.Distance);
        if (!seen.contains(stop.Node)) {
          CHECK(stop.Node == next);
          seen.insert(stop.Node);
          ++next;
        }
      }
    }
    CHECK(network->nodeCount() == next);
  }
}

} // namespace
} // namespace tpj
