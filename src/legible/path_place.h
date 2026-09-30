#ifndef TPJ_LEGIBLE_PATH_PLACE_H
#define TPJ_LEGIBLE_PATH_PLACE_H

#include "sim/medium/network.h"

#include <optional>

namespace tpj {

class World;

// A place on the paths and its straight distance, in meters, from a ground point.
struct PathPlace {
  Place At;
  double Distance = 0.0;

  bool operator==(const PathPlace &) const = default;
};

// The place on a guest path nearest the point, ties to the lower path key, or none when the point
// is not finite or no guest path is a carrier of the guest network. The straight distance only
// locates the point; what is explained there is measured along routes.
std::optional<PathPlace> nearestGuestPathPlace(const World &world, GroundPoint point);

} // namespace tpj

#endif
