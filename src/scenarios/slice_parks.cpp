#include "scenarios/slice_parks.h"

#include "sim/command_queue.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <stdexcept>
#include <string>

namespace tpj {
namespace {

// tests/parks/facilities.park's depot, beside the both shop: its front door, at (-13, 93), is 1 m
// from the backstage path's paving.
constexpr Pose STOCKED_DEPOT{-17.0, 93.0, 1.0, 0.0};

// The backstage path behind facilities.park's shops. The drink shop's back door, at (8.5, 107.5),
// and the both shop's, at (-8.5, 104.5), are each 1.5 m from its paving. The both shop's route to
// the depot is short and the drink shop's long, so the both shop's goods arrive while the drink
// shop's drink is still on its way.
AddPath stockedPath() {
  return AddPath{PathKind::Backstage,
                 {NewPoint{{11.0, 107.5}}, NewPoint{{11.0, 93.0}}, NewPoint{{-11.0, 93.0}},
                  NewPoint{{-11.0, 104.5}}}};
}

// Applies the command, or throws naming it when the world refuses it.
template <typename Command>
void applyAccepted(World &world, const Command &command, const std::string &what) {
  if (!isAccepted(world, command)) {
    throw std::logic_error("stocked park: the world refuses " + what);
  }
  applyCommand(world, command);
}

// Whether a shop holds food, a shop holds drink, and drink is in transit.
bool isStocked(const World &world) {
  bool food = false;
  bool drink = false;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      food = food || unitsHeld<Supplies>(world, box.Key, box.Key) > 0;
      drink = drink || unitsHeld<DrinkSupplies>(world, box.Key, box.Key) > 0;
    }
  }
  return food && drink && unitsInTransit<DrinkSupplies>(world) > 0;
}

} // namespace

SliceParks makeSliceParks(std::string_view text, uint64_t warmTicks) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  SliceParks parks;
  parks.Fed = saveWorld(world);
  TickBuffers buffers;
  for (uint64_t i = 0; i < warmTicks; ++i) {
    stepWorld(world, buffers);
  }
  parks.Warm = saveWorld(world);
  // Deleting in a cycle, not in the text, lets the resolution clear the shop's stepped offer.
  CommandQueue cut;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Backstage) {
      cut.push(DeleteSegment{segment.Key});
    }
  }
  stepWorld(world, cut, buffers);
  parks.Cut = saveWorld(world);
  return parks;
}

std::string makeWarmPark(std::string_view text, uint64_t warmTicks) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  TickBuffers buffers;
  for (uint64_t i = 0; i < warmTicks; ++i) {
    stepWorld(world, buffers);
  }
  return saveWorld(world);
}

std::string makeStockedPark(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  applyAccepted(world, AddBox{BoxKind::Depot, STOCKED_DEPOT}, "the depot");
  applyAccepted(world, stockedPath(), "the backstage path");
  resolveWorld(world);
  TickBuffers buffers;
  for (uint64_t i = 0; i < STOCKED_LIMIT_TICKS; ++i) {
    stepWorld(world, buffers);
    if (isStocked(world)) {
      return saveWorld(world);
    }
  }
  throw std::logic_error("stocked park: no shop held food and drink with drink on its way");
}

} // namespace tpj
