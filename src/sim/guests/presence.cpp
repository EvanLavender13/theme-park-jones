#include "sim/guests/internal/presence.h"

#include "sim/guests/guests.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>

namespace tpj {

GroundPoint sidestep(EntityKey self, GroundPoint at, GroundDirection heading, double speed,
                     std::span<const GroundSample<Stride>> near) {
  const double rightX = -heading.Z;
  const double rightZ = heading.X;
  // Meters per second to the right, or to the left when negative.
  double aside = 0.0;
  for (const GroundSample<Stride> &other : near) {
    if (other.Key == self) {
      continue;
    }
    const double rx = other.At.X - at.X;
    const double rz = other.At.Z - at.Z;
    if ((rx * heading.X) + (rz * heading.Z) <= 0.0) {
      continue;
    }
    const double wx = other.Value.X - (heading.X * speed);
    const double wz = other.Value.Z - (heading.Z * speed);
    const double ww = (wx * wx) + (wz * wz);
    const double rw = (rx * wx) + (rz * wz);
    if (ww == 0.0 || rw >= 0.0) {
      continue;
    }
    // The time of their nearest approach, and how far apart they are then.
    const double t = -rw / ww;
    if (t > SIDESTEP_HORIZON) {
      continue;
    }
    const double cx = rx + (wx * t);
    const double cz = rz + (wz * t);
    const double apart = std::sqrt((cx * cx) + (cz * cz));
    if (apart >= SIDESTEP_DISTANCE) {
      continue;
    }
    const double needed = (SIDESTEP_DISTANCE - apart) / t;
    aside += (cx * rightX) + (cz * rightZ) > 0.0 ? -needed : needed;
  }
  aside = std::clamp(aside, -SIDESTEP_SPEED, SIDESTEP_SPEED) * SIM_TICK_SECONDS;
  return {rightX * aside, rightZ * aside};
}

} // namespace tpj
