#include "legible/path_place.h"

#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <math.h>

namespace tpj {

std::optional<PathPlace> nearestGuestPathPlace(const World &world, GroundPoint point) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::optional<PathPlace> nearest;
  // Paths come in ascending key order, and only a strictly nearer one replaces the nearest.
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind != PathKind::Guest) {
      continue;
    }
    const std::optional<Place> place = network.nearestPlaceOn(path.Key, point);
    if (!place) {
      continue;
    }
    const std::optional<GroundPoint> at = network.groundPoint(*place);
    if (!at) {
      continue;
    }
    const double dx = at->X - point.X;
    const double dz = at->Z - point.Z;
    const double distance = sqrt(dx * dx + dz * dz);
    if (!nearest || distance < nearest->Distance) {
      nearest = PathPlace{*place, distance};
    }
  }
  return nearest;
}

} // namespace tpj
