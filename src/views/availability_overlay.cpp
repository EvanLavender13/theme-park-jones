#include "views/availability_overlay.h"

#include <utility>

namespace tpj {

OverlayShades availabilityShades(const World &world, const OverlayShape &shape,
                                 const AvailabilityDefinition &definition) {
  AvailabilityShadeLists lists;
  availabilityShades(world, shape, definition, lists);
  return std::move(lists.Shades);
}

void availabilityShades(const World &world, const OverlayShape &shape,
                        const AvailabilityDefinition &definition, AvailabilityShadeLists &lists) {
  availabilityBends(world, definition, lists.Availability, lists.Bends);
  availabilityValues(world, definition, lists.Bends, lists.Availability, lists.Values);
  shadeOverlay(shape, lists.Bends, lists.Values, definition.Half, lists.Shading, lists.Shades);
}

} // namespace tpj
