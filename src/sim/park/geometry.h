#ifndef TPJ_SIM_PARK_GEOMETRY_H
#define TPJ_SIM_PARK_GEOMETRY_H

#include "sim/medium/network.h"
#include "sim/park/intent.h"

#include <array>
#include <optional>
#include <vector>

namespace tpj {

// Two positions closer together than this, in meters, make no ground line.
inline constexpr double MIN_SEGMENT_LENGTH = 0.01;

// The straight ground line from one position to the other: two CarrierPoints, at distance 0 and
// at the length between them. Empty when either position is not finite or lies outside the park,
// or the two lie closer together than MIN_SEGMENT_LENGTH.
std::vector<CarrierPoint> segmentLine(ParkPoint from, ParkPoint to);

// The rectangle a box or entrance covers. Forward and Right are unit directions.
struct Footprint {
  ParkPoint Forward;
  ParkPoint Right;
  // Front left, front right, back right, back left.
  std::array<ParkPoint, 4> Corners;
};

// The pose's footprint for the size, or none when the pose is not finite or its facing has zero
// length.
std::optional<Footprint> footprintOf(const Pose &pose, FootprintSize size);

} // namespace tpj

#endif
