#include "views/availability_overlay.h"

#include "render/band_overlay.h"

#include <span>
#include <vector>

namespace tpj {

ParkMesh buildAvailabilityOverlay(const World &world, const AvailabilityDefinition &definition) {
  return buildBandOverlay(
      world,
      [&world, &definition](std::span<const Place> places) {
        return availabilityValues(world, definition, places);
      },
      definition.Full);
}

} // namespace tpj
