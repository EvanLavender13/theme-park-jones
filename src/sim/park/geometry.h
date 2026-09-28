#ifndef TPJ_SIM_PARK_GEOMETRY_H
#define TPJ_SIM_PARK_GEOMETRY_H

#include "sim/medium/network.h"
#include "sim/park/intent.h"

#include <array>
#include <optional>
#include <vector>

namespace tpj {

// A point closer than this to the last kept point of a path is dropped.
inline constexpr double MIN_POINT_SPACING = 0.01;
// A ground line segment takes one point per this many meters of chord, and at least
// MIN_SEGMENT_SAMPLES.
inline constexpr double GROUND_LINE_SPACING = 1.0;
inline constexpr int MIN_SEGMENT_SAMPLES = 8;

// The path's centripetal Catmull-Rom curve through its points as a line with distances, or an empty
// line when a point is not finite or outside the park, or fewer than two points are kept. See
// sim/park/SPEC.md.
std::vector<CarrierPoint> groundLine(const std::vector<ParkPoint> &points);

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
