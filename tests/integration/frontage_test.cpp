// Shops the frontage snap places beside wide paving are accepted and connect to guests.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;

std::set<EntityKey> segmentKeys(const World &world) {
  std::set<EntityKey> keys;
  for (const ParkSegment &segment : parkSegments(world)) {
    keys.insert(segment.Key);
  }
  return keys;
}

std::optional<EntityKey> pointKeyAt(const World &world, ParkPoint at) {
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    if (point.At == at) {
      return point.Key;
    }
  }
  return std::nullopt;
}

// The pointer lies farther than half the width, plus the setback, plus the shop's depth from both
// segments' center lines, so a snap that measures from center lines finds no paving near it.
TEST_CASE("A shop fronting a wide path's sharp outside corner is accepted and connects to guests") {
  World world = openPark(parkFile("new.park"));
  const std::set<EntityKey> before = segmentKeys(world);

  const ParkPoint corner{-30.0, -60.0};
  const AddPath path{PathKind::Guest,
                     {NewPoint{{-60.0, -60.0}}, NewPoint{corner}, NewPoint{{-30.0, -30.0}}}};
  REQUIRE(isAccepted(world, path));
  applyCommand(world, path);
  resolveWorld(world);

  std::vector<EntityKey> added;
  for (const EntityKey key : segmentKeys(world)) {
    if (!before.contains(key)) {
      added.push_back(key);
    }
  }
  REQUIRE(added.size() == 2);
  for (const EntityKey segment : added) {
    const SetSegmentWidth widen{segment, 10.0};
    REQUIRE(isAccepted(world, widen));
    applyCommand(world, widen);
    resolveWorld(world);
  }
  // The corner's inside curves while its outside stays sharp, near (-25, -65).
  const std::optional<EntityKey> middle = pointKeyAt(world, corner);
  REQUIRE(middle.has_value());
  const SetCornerRadius round{middle.value_or(NULL_KEY), 12.0};
  REQUIRE(isAccepted(world, round));
  applyCommand(world, round);
  resolveWorld(world);

  const std::optional<Pose> pose = frontagePose(world, BoxKind::Shop, ParkPoint{-22.5, -67.0});
  REQUIRE(pose.has_value());
  const AddBox place{BoxKind::Shop, pose.value_or(Pose{})};
  REQUIRE(isAccepted(world, place));
  applyCommand(world, place);
  resolveWorld(world);

  const std::vector<ParkBox> boxes = parkBoxes(world);
  REQUIRE(boxes.size() == 1);
  const EntityKey connector = connectorKey(boxes.front().Key, Face::Front);
  const std::vector<Carrier> &carriers = parkNetwork(world, PathKind::Guest).carriers();
  CHECK(std::ranges::any_of(carriers,
                            [&](const Carrier &carrier) { return carrier.Key == connector; }));
}

// At the widest width, the door at the frontage setback lies far beyond connection reach of the
// segment's center line, so this catches reach measured from the line instead of the paving.
TEST_CASE(
    "A shop fronting a guest segment of the widest width the tools allow connects to guests") {
  World world = openPark(parkFile("new.park"));
  const std::set<EntityKey> before = segmentKeys(world);

  // A straight path well clear of new.park's own path and entrance, which lie along x = 0.
  const double lineX = 60.0;
  const AddPath path{PathKind::Guest, {NewPoint{{lineX, -60.0}}, NewPoint{{lineX, 20.0}}}};
  REQUIRE(isAccepted(world, path));
  applyCommand(world, path);
  resolveWorld(world);

  std::vector<EntityKey> added;
  for (const EntityKey key : segmentKeys(world)) {
    if (!before.contains(key)) {
      added.push_back(key);
    }
  }
  REQUIRE(added.size() == 1);
  const SetSegmentWidth widen{added.front(), MAX_PATH_WIDTH};
  REQUIRE(isAccepted(world, widen));
  applyCommand(world, widen);
  resolveWorld(world);

  // The pointer lies just outside the paving's side, halfway along the segment.
  const ParkPoint pointer{lineX + (MAX_PATH_WIDTH / 2.0) + FRONTAGE_SETBACK, -20.0};
  const std::optional<Pose> pose = frontagePose(world, BoxKind::Shop, pointer);
  REQUIRE(pose.has_value());
  const AddBox place{BoxKind::Shop, pose.value_or(Pose{})};
  REQUIRE(isAccepted(world, place));
  applyCommand(world, place);
  resolveWorld(world);

  const std::vector<ParkBox> boxes = parkBoxes(world);
  REQUIRE(boxes.size() == 1);
  const EntityKey connector = connectorKey(boxes.front().Key, Face::Front);
  const std::vector<Carrier> &carriers = parkNetwork(world, PathKind::Guest).carriers();
  CHECK(std::ranges::any_of(carriers,
                            [&](const Carrier &carrier) { return carrier.Key == connector; }));
}

} // namespace
} // namespace tpj
