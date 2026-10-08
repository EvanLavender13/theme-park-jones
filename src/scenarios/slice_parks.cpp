#include "scenarios/slice_parks.h"

#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

namespace tpj {

SliceParks makeSliceParks(std::string_view text, uint64_t warmTicks) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  SliceParks parks;
  parks.Fed = saveWorld(world);
  for (uint64_t i = 0; i < warmTicks; ++i) {
    stepWorld(world);
  }
  parks.Warm = saveWorld(world);
  // Deleting in a cycle, not in the text, lets the resolution clear the shop's stepped offer.
  CommandQueue cut;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Backstage) {
      cut.push(DeleteSegment{segment.Key});
    }
  }
  stepWorld(world, cut);
  parks.Cut = saveWorld(world);
  return parks;
}

std::string makeWarmPark(std::string_view text, uint64_t warmTicks) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  for (uint64_t i = 0; i < warmTicks; ++i) {
    stepWorld(world);
  }
  return saveWorld(world);
}

} // namespace tpj
