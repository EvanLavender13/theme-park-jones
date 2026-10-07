#include "views/availability_overlay.h"

#include <vector>

namespace tpj {

OverlayShades availabilityShades(const World &world, const OverlayShape &shape,
                                 const AvailabilityDefinition &definition) {
  const std::vector<Place> bends = availabilityBends(world, definition);
  const std::vector<double> values = availabilityValues(world, definition, bends);
  return shadeOverlay(shape, bends, values, definition.Full);
}

} // namespace tpj
