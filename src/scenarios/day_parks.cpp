#include "scenarios/day_parks.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <algorithm>
#include <format>
#include <stddef.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

// The template's guest segment, which day.park replaces with its own paths.
constexpr EntityKey TEMPLATE_SEGMENT{4};

// The ends of a guest segment of day.park.
struct SegmentEnds {
  ParkPoint A;
  ParkPoint B;
};

// day.park's guest path in from the entrance, and the arms of the T toward the food shop and the
// drink shop.
constexpr SegmentEnds PATH_IN{{0.0, 123.0}, {0.0, 100.0}};
constexpr SegmentEnds FOOD_ARM{{-50.0, 100.0}, {0.0, 100.0}};
constexpr SegmentEnds DRINK_ARM{{0.0, 100.0}, {50.0, 100.0}};

// Where day.park's toilet block stands, facing the path in.
constexpr Pose TOILET_BLOCK_POSE{.X = -5.0, .Z = 113.0, .FacingX = 1.0, .FacingZ = 0.0};

// Applies the command, or throws naming it when the world refuses it.
template <typename Command>
void applyAccepted(World &world, const Command &command, const std::string &what) {
  if (!isAccepted(world, command)) {
    throw std::logic_error("day park: the world refuses " + what);
  }
  applyCommand(world, command);
}

void addPath(World &world, PathKind kind, ParkPoint from, ParkPoint to) {
  applyAccepted(world, AddPath{kind, {NewPoint{from}, NewPoint{to}}},
                std::format("the {} path from ({}, {}) to ({}, {})",
                            kind == PathKind::Guest ? "guest" : "backstage", from.X, from.Z, to.X,
                            to.Z));
}

EntityKey addBox(World &world, BoxKind kind, Pose at) {
  const EntityKey key{world.nextKey()};
  applyAccepted(
      world, AddBox{kind, at},
      std::format("the {} at ({}, {})", enumNames(kind)[static_cast<size_t>(kind)], at.X, at.Z));
  return key;
}

// The keys of the boxes day.park's layout needs again.
struct DayBoxes {
  EntityKey DrinkShop = NULL_KEY;
  EntityKey ToiletBlock = NULL_KEY;
};

// The position of the path point with the key, or the origin when there is none.
ParkPoint positionOf(const std::vector<ParkPathPoint> &points, EntityKey key) {
  const auto found = std::ranges::find(points, key, &ParkPathPoint::Key);
  return found != points.end() ? found->At : ParkPoint{};
}

// The guest segment whose ends stand at the two positions, in either order. Throws
// std::logic_error naming the positions when there is none.
ParkSegment guestSegmentJoining(const World &world, SegmentEnds ends) {
  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  for (const ParkSegment &segment : parkSegments(world)) {
    const ParkPoint from = positionOf(points, segment.From);
    const ParkPoint to = positionOf(points, segment.To);
    if (segment.Kind == PathKind::Guest &&
        ((from == ends.A && to == ends.B) || (from == ends.B && to == ends.A))) {
      return segment;
    }
  }
  throw std::logic_error(std::format("day park: no guest segment joins ({}, {}) and ({}, {})",
                                     ends.A.X, ends.A.Z, ends.B.X, ends.B.Z));
}

// The side of the segment, seen walking from its From point to its To point, that the position
// doesn't lie on. A direction's left, seen from above, is (d.Z, -d.X).
FurnishedSide sideAwayFrom(const World &world, const ParkSegment &segment, ParkPoint away) {
  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  const ParkPoint from = positionOf(points, segment.From);
  const ParkPoint to = positionOf(points, segment.To);
  const double alongX = to.X - from.X;
  const double alongZ = to.Z - from.Z;
  const double onLeft = (alongZ * (away.X - from.X)) - (alongX * (away.Z - from.Z));
  return onLeft > 0.0 ? FurnishedSide::Right : FurnishedSide::Left;
}

void furnish(World &world, EntityKey segment, const Furnishing &to) {
  applyAccepted(world, SetFurnishing{segment, to},
                std::format("the furnishing of segment {}", static_cast<uint64_t>(segment)));
}

// Tables on both sides of a segment, DAY_TABLE_SPACING apart.
Furnishing tablesOnBothSides() {
  const SideFurnishing tables{FurnitureKind::Table, DAY_TABLE_SPACING};
  return Furnishing{tables, tables};
}

// A guest path in from the entrance to the middle of a guest path running 50 m each way, a T,
// with the food shop at the west end of its arms and the drink shop at the east end, each facing
// the arms 1 m from their paving. A backstage path runs behind both shops, 0.5 m from their back
// doors, past the depot behind the junction. The toilet block faces the path in from the entrance,
// as in tests/parks/facilities.park. The arm toward the food shop has tables on both sides, and
// the path in has benches on the side away from the toilet block.
DayBoxes layOut(World &world) {
  applyAccepted(world, DeleteSegment{TEMPLATE_SEGMENT}, "deleting the template's segment");
  addPath(world, PathKind::Guest, PATH_IN.A, PATH_IN.B);
  addPath(world, PathKind::Guest, FOOD_ARM.A, DRINK_ARM.B);
  addPath(world, PathKind::Backstage, {-50.0, 90.0}, {50.0, 90.0});
  addBox(world, BoxKind::Shop, Pose{.X = -44.0, .Z = 94.5, .FacingX = 0.0, .FacingZ = 1.0});
  DayBoxes boxes;
  boxes.DrinkShop =
      addBox(world, BoxKind::Shop, Pose{.X = 44.0, .Z = 94.5, .FacingX = 0.0, .FacingZ = 1.0});
  addBox(world, BoxKind::Depot, Pose{.X = 0.0, .Z = 84.0, .FacingX = 0.0, .FacingZ = 1.0});
  boxes.ToiletBlock = addBox(world, BoxKind::ToiletBlock, TOILET_BLOCK_POSE);
  applyAccepted(world, SetShopStock{boxes.DrinkShop, ShopStock::Drink}, "the drink shop's stock");
  furnish(world, guestSegmentJoining(world, FOOD_ARM).Key, tablesOnBothSides());
  const ParkSegment pathIn = guestSegmentJoining(world, PATH_IN);
  const SideFurnishing benches{FurnitureKind::Bench, DAY_BENCH_SPACING};
  Furnishing benched;
  if (sideAwayFrom(world, pathIn, {TOILET_BLOCK_POSE.X, TOILET_BLOCK_POSE.Z}) ==
      FurnishedSide::Left) {
    benched.Left = benches;
  } else {
    benched.Right = benches;
  }
  furnish(world, pathIn.Key, benched);
  return boxes;
}

// Throws naming the first shop, in key order, with no depot to supply it, or the first shop or
// toilet block with no guest connector.
void refuseUnservedFacilities(const World &world) {
  const Network &guests = parkNetwork(world, PathKind::Guest);
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop && !nearestDepot(world, box.Key)) {
      throw std::logic_error(
          std::format("day park: shop {} is starved", static_cast<uint64_t>(box.Key)));
    }
    if (box.Kind != BoxKind::Depot && !guests.firstAnchoredNode(box.Key)) {
      throw std::logic_error(std::format("day park: {} {} has no guest connector",
                                         enumNames(box.Kind)[static_cast<size_t>(box.Kind)],
                                         static_cast<uint64_t>(box.Key)));
    }
  }
}

} // namespace

DayParks makeDayParks() {
  World world = makeNewPark(DAY_PARK_SEED);
  const DayBoxes boxes = layOut(world);
  resolveWorld(world);
  refuseUnservedFacilities(world);
  DayParks parks;
  parks.Day = saveWorld(world);
  World noToilet = copyWorld(world);
  applyAccepted(noToilet, DeleteBox{boxes.ToiletBlock}, "deleting the toilet block");
  resolveWorld(noToilet);
  parks.NoToilet = saveWorld(noToilet);
  World farTables = copyWorld(world);
  furnish(farTables, guestSegmentJoining(farTables, FOOD_ARM).Key, Furnishing{});
  furnish(farTables, guestSegmentJoining(farTables, DRINK_ARM).Key, tablesOnBothSides());
  resolveWorld(farTables);
  parks.FarTables = saveWorld(farTables);
  World unfurnished = copyWorld(world);
  furnish(unfurnished, guestSegmentJoining(unfurnished, FOOD_ARM).Key, Furnishing{});
  furnish(unfurnished, guestSegmentJoining(unfurnished, PATH_IN).Key, Furnishing{});
  resolveWorld(unfurnished);
  parks.Unfurnished = saveWorld(unfurnished);
  for (uint64_t tick = 0; tick < NOON_TICKS; ++tick) {
    stepWorld(world);
  }
  parks.Noon = saveWorld(world);
  for (uint64_t tick = NOON_TICKS; tick < EVENING_TICKS; ++tick) {
    stepWorld(world);
  }
  parks.Evening = saveWorld(world);
  return parks;
}

} // namespace tpj
