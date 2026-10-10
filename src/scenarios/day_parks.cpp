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

#include <format>
#include <stddef.h>
#include <stdexcept>
#include <string>

namespace tpj {
namespace {

// The template's guest segment, which day.park replaces with its own paths.
constexpr EntityKey TEMPLATE_SEGMENT{4};

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

// A guest path in from the entrance to the middle of a guest path running 50 m each way, a T,
// with the food shop at the west end of its arms and the drink shop at the east end, each facing
// the arms 1 m from their paving. A backstage path runs behind both shops, 0.5 m from their back
// doors, past the depot behind the junction. The bench and the toilet block face the path in from
// the entrance, as in tests/parks/facilities.park.
DayBoxes layOut(World &world) {
  applyAccepted(world, DeleteSegment{TEMPLATE_SEGMENT}, "deleting the template's segment");
  addPath(world, PathKind::Guest, {0.0, 123.0}, {0.0, 100.0});
  addPath(world, PathKind::Guest, {-50.0, 100.0}, {50.0, 100.0});
  addPath(world, PathKind::Backstage, {-50.0, 90.0}, {50.0, 90.0});
  addBox(world, BoxKind::Shop, Pose{.X = -44.0, .Z = 94.5, .FacingX = 0.0, .FacingZ = 1.0});
  DayBoxes boxes;
  boxes.DrinkShop =
      addBox(world, BoxKind::Shop, Pose{.X = 44.0, .Z = 94.5, .FacingX = 0.0, .FacingZ = 1.0});
  addBox(world, BoxKind::Depot, Pose{.X = 0.0, .Z = 84.0, .FacingX = 0.0, .FacingZ = 1.0});
  addBox(world, BoxKind::Bench, Pose{.X = 3.0, .Z = 113.0, .FacingX = -1.0, .FacingZ = 0.0});
  boxes.ToiletBlock = addBox(world, BoxKind::ToiletBlock,
                             Pose{.X = -5.0, .Z = 113.0, .FacingX = 1.0, .FacingZ = 0.0});
  applyAccepted(world, SetShopStock{boxes.DrinkShop, ShopStock::Drink}, "the drink shop's stock");
  return boxes;
}

// Throws naming the first shop, in key order, with no depot to supply it, or the first shop,
// bench, or toilet block with no guest connector.
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
