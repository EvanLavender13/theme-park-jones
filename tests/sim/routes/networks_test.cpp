#include "support/connected_parks.h"
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

using test::findCarrier;
using test::nodesNear;
using test::ParkIntent;
using test::pathOf;
using test::resolvedWorld;
using test::SLACK;
using test::worldOf;

ParkPath guest(uint64_t key, std::vector<ParkPoint> points) {
  return pathOf(key, PathKind::Guest, std::move(points));
}

ParkIntent pathsOnly(std::vector<ParkPath> paths) {
  return ParkIntent{.Entrances = {}, .Paths = std::move(paths), .Boxes = {}};
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
// x = 10, so a point (x, 0) is at distance x + 10 on it.
constexpr double FIRST_START = -10.0;
constexpr double FIRST_END = 10.0;
ParkPath firstLine() { return guest(1, {{FIRST_START, 0.0}, {FIRST_END, 0.0}}); }

// Its segments cross.
MeetingCase crossing() {
  return {"a crossing",
          pathsOnly({firstLine(), guest(2, {{0.3, -10.0}, {0.3, 10.0}})}),
          {{FIRST, 10.3, SECOND, 10.0}}};
}

// The second path's first point was snapped onto the first's line.
MeetingCase snapped() {
  return {"a snapped point",
          pathsOnly({firstLine(), guest(2, {{2.5, 0.0}, {2.5, 10.0}})}),
          {{FIRST, 12.5, SECOND, 0.0}}};
}

// The second line passes through one of the first's line points midway between two of its own, so
// the meeting is at an end of the first's segments that touches the second's without crossing it.
MeetingCase throughLinePoint() {
  const std::vector<CarrierPoint> first = groundLine(firstLine().Points);
  REQUIRE(first.size() >= 3);
  const CarrierPoint &point = first[first.size() / 2];
  // The second line laid from the point, and then moved back along itself so that the point falls
  // midway between two of its line points.
  const std::vector<ParkPoint> laid{{point.X, point.Z}, {point.X, point.Z + 10.0}};
  const std::vector<CarrierPoint> second = groundLine(laid);
  REQUIRE(second.size() >= 3);
  const std::size_t before = (second.size() / 2) - 1;
  const double midway = (second[before].Distance + second[before + 1].Distance) / 2.0;
  return {"a line through another's line point",
          pathsOnly({firstLine(),
                     guest(2, {{point.X, point.Z - midway}, {point.X, point.Z + 10.0 - midway}})}),
          {{FIRST, point.Distance, SECOND, midway}}};
}

// The second line runs along z = 0 from x = 0.5 to x = 20.5, sharing the stretch from 0.5 to 10
// with the first. Every line point of either along that stretch lies on the other's line,
// including the stretch's two ends.
MeetingCase collinearOverlap() {
  constexpr double SECOND_START = 0.5;
  const ParkPath second = guest(2, {{SECOND_START, 0.0}, {20.5, 0.0}});
  MeetingCase overlap{"a collinear overlap", pathsOnly({firstLine(), second}), {}};
  for (const CarrierPoint &point : groundLine(firstLine().Points)) {
    if (point.X >= SECOND_START) {
      overlap.Meetings.push_back({FIRST, point.Distance, SECOND, point.X - SECOND_START});
    }
  }
  for (const CarrierPoint &point : groundLine(second.Points)) {
    if (point.X <= FIRST_END) {
      overlap.Meetings.push_back({FIRST, point.X - FIRST_START, SECOND, point.Distance});
    }
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

// A park and the distances on each path where some line or connector meets it.
struct KnownStops {
  const char *Name = "";
  ParkIntent Intent;
  std::vector<test::PathMeeting> Meetings;
};

std::vector<KnownStops> knownStops() {
  std::vector<KnownStops> parks;
  for (const MeetingCase &meetingCase : meetingCases()) {
    KnownStops park{meetingCase.Name, meetingCase.Intent, {}};
    for (const Meeting &meeting : meetingCase.Meetings) {
      park.Meetings.push_back({meeting.First, meeting.FirstDistance});
      park.Meetings.push_back({meeting.Second, meeting.SecondDistance});
    }
    parks.push_back(std::move(park));
  }
  for (const test::ConnectedPark &connected : test::connectedParks()) {
    KnownStops park{connected.Name, connected.Intent, connected.Meetings};
    for (const test::Connection &connection : connected.Connections) {
      park.Meetings.push_back({connection.Path, connection.PathDistance});
    }
    parks.push_back(std::move(park));
  }
  return parks;
}

TEST_CASE("Every stop of a path carrier other than its first and last lies within the tolerance "
          "of a meeting's or a connection's distance on it") {
  int interiorStops = 0;
  for (const KnownStops &park : knownStops()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const ParkPath &path : park.Intent.Paths) {
      INFO("path " << static_cast<uint64_t>(path.Key));
      std::vector<double> distances;
      for (const test::PathMeeting &meeting : park.Meetings) {
        if (meeting.Path == path.Key) {
          distances.push_back(meeting.Distance);
        }
      }
      const Carrier *carrier = findCarrier(parkNetwork(world, path.Kind), path.Key);
      REQUIRE(carrier != nullptr);
      for (std::size_t index = 1; index + 1 < carrier->Stops.size(); ++index) {
        ++interiorStops;
        const double stop = carrier->Stops[index].Distance;
        INFO("stop at " << stop);
        CHECK(std::ranges::any_of(distances, [stop](double distance) {
          return std::abs(stop - distance) <= JUNCTION_TOLERANCE + SLACK;
        }));
      }
    }
  }
  CHECK(interiorStops > 0);
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

// FIRST is crossed by lines half the tolerance from its start and from its end, which fall into its
// end stops, and by three lines 0.8 of the tolerance apart, whose meetings with it chain past the
// tolerance from the first. The three lie within the tolerance of each other, so they meet along
// their whole length too.
ParkIntent clusteredMeetings() {
  constexpr double NEAR_END = JUNCTION_TOLERANCE / 2.0;
  constexpr double CHAINED = 0.8 * JUNCTION_TOLERANCE;
  ParkIntent intent = pathsOnly({firstLine()});
  uint64_t key = 2;
  for (const double x :
       {FIRST_START + NEAR_END, 0.3, 0.3 + CHAINED, 0.3 + (2.0 * CHAINED), FIRST_END - NEAR_END}) {
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
