#ifndef TPJ_SIM_ROUTES_INTERNAL_ROUTE_STEPS_H
#define TPJ_SIM_ROUTES_INTERNAL_ROUTE_STEPS_H

#include "sim/routes/route_distance.h"

namespace tpj {

// Whether a step comes before another among those achieving a node's distance: the lower carrier
// key, then toward lower distances, then the lower From.
inline bool isPreferredStep(const RouteStep &step, const RouteStep &other) {
  if (step.Carrier != other.Carrier) {
    return step.Carrier < other.Carrier;
  }
  const bool isDown = step.To < step.From;
  const bool isOtherDown = other.To < other.From;
  if (isDown != isOtherDown) {
    return isDown;
  }
  return step.From < other.From;
}

} // namespace tpj

#endif
