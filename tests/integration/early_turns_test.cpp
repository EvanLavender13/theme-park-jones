// Shaping points on a checked-in park: inserting a point into a straight guest segment ahead of a
// wandering guest whose stay is over leaves it wandering as it walks through the new point. If the
// guest chose there, it would head home. The test compares the park with the same park after the
// insert, so it doesn't depend on how far before a node a guest makes its choice.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <set>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// Hang guards only: far more cycles than walking a meter or a few meters takes.
constexpr uint64_t LONGEST_START = 600;
constexpr uint64_t LONGEST_WALK = 3000;

// How far the guest walks along the segment before the test reads which way it goes.
constexpr double START_WALK = 0.5;
// How far ahead of the guest the point goes in, and how far past it the guest walks.
constexpr double INSERT_AHEAD = 3.0;
constexpr double WALK_PAST = 2.0;

GroundPoint positionOf(const World &world, EntityKey guest) {
  const std::optional<GroundPoint> position = recordOf(world, guest).Position;
  REQUIRE(position.has_value());
  return position.value_or(GroundPoint{});
}

std::optional<ParkPathPoint> pointAt(const World &world, ParkPoint at) {
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    if (point.At == at) {
      return point;
    }
  }
  return std::nullopt;
}

std::optional<ParkPathPoint> pointWithKey(const World &world, EntityKey key) {
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    if (point.Key == key) {
      return point;
    }
  }
  return std::nullopt;
}

TEST_CASE("A point inserted ahead of a wandering guest whose stay is over leaves it wandering") {
  World unchanged = openPark(parkFile("new.park"));

  // new.park's guest path runs along x = 0 from in front of the entrance to z = 103. The new path
  // carries it straight on for 60 m, so the guest's segment has no node near its middle.
  const ParkPoint farEnd{0.0, 103.0};
  const std::optional<ParkPathPoint> joint = pointAt(unchanged, farEnd);
  REQUIRE(joint.has_value());
  std::set<EntityKey> before;
  for (const ParkSegment &segment : parkSegments(unchanged)) {
    before.insert(segment.Key);
  }
  const AddPath extend{PathKind::Guest,
                       {OnPoint{joint.value_or(ParkPathPoint{}).Key}, NewPoint{{0.0, 43.0}}}};
  REQUIRE(isAccepted(unchanged, extend));
  applyCommand(unchanged, extend);
  resolveWorld(unchanged);

  std::optional<ParkSegment> added;
  for (const ParkSegment &segment : parkSegments(unchanged)) {
    if (!before.contains(segment.Key)) {
      REQUIRE_FALSE(added.has_value());
      added = segment;
    }
  }
  REQUIRE(added.has_value());
  const ParkSegment segment = added.value_or(ParkSegment{});
  REQUIRE_FALSE(segment.Line.empty());

  const Place middle{segment.Key, segment.Line.back().Distance / 2.0};
  const EntityKey guest = addGuest(unchanged, middle, 0);

  // Step until the guest has moved along the segment, to learn which way it walks.
  const double startZ = positionOf(unchanged, guest).Z;
  const uint64_t startTick = unchanged.Tick;
  while (std::abs(positionOf(unchanged, guest).Z - startZ) < START_WALK) {
    REQUIRE(unchanged.Tick < startTick + LONGEST_START);
    stepWorld(unchanged);
  }
  const double guestZ = positionOf(unchanged, guest).Z;
  const double way = guestZ > startZ ? 1.0 : -1.0;
  // Its stay is over, so the guest would be heading home if it had chosen.
  REQUIRE(recordOf(unchanged, guest).Activity == GuestActivity::Wandering);

  World changed = copyWorld(unchanged);
  const std::optional<ParkPathPoint> from = pointWithKey(changed, segment.From);
  const std::optional<ParkPathPoint> to = pointWithKey(changed, segment.To);
  REQUIRE(from.has_value());
  REQUIRE(to.has_value());
  const double fromZ = from.value_or(ParkPathPoint{}).At.Z;
  const double toZ = to.value_or(ParkPathPoint{}).At.Z;
  const double insertZ = guestZ + (way * INSERT_AHEAD);
  const InsertPoint insert{OnSegment{segment.Key, (insertZ - fromZ) / (toZ - fromZ)}};
  REQUIRE(isAccepted(changed, insert));
  applyCommand(changed, insert);
  resolveWorld(changed);

  const uint64_t walkTick = changed.Tick;
  while ((positionOf(changed, guest).Z - insertZ) * way < WALK_PAST) {
    REQUIRE(changed.Tick < walkTick + LONGEST_WALK);
    stepWorld(unchanged);
    stepWorld(changed);
    REQUIRE(recordOf(changed, guest).Activity == recordOf(unchanged, guest).Activity);
  }
  CHECK(recordOf(changed, guest).Activity == GuestActivity::Wandering);
}

} // namespace
} // namespace tpj
