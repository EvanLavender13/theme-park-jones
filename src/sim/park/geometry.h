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

// Each straight piece of a rounded corner's ground line lies within this many meters of its arc.
inline constexpr double ARC_TOLERANCE = 0.05;

// The straight ground line from one position to the other: two CarrierPoints, at distance 0 and
// at the length between them. Empty when either position is not finite or lies outside the park,
// or the two lie closer together than MIN_SEGMENT_LENGTH.
std::vector<CarrierPoint> segmentLine(ParkPoint from, ParkPoint to);
// As the returning form, into a list the caller keeps, which it clears first. Allocates nothing
// once the list has grown.
void segmentLine(ParkPoint from, ParkPoint to, std::vector<CarrierPoint> &line);

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

// The radius whose arc's middle lies the depth from the corner's point, along Toward. 0 for a depth
// of 0 or less.
double cornerRadiusFor(const ParkCorner &corner, double depth);

// Where a corner's arc lies: its center, its radius, and where it touches each of the corner's two
// segments, in the order of the corner's Along.
struct CornerArc {
  ParkPoint Center;
  double Radius = 0.0;
  std::array<ParkPoint, 2> Touch;
};

// The corner's arc. With a radius of 0, the center and both touches are the corner's point.
CornerArc arcOf(const ParkCorner &corner);
// The place on the corner's arc where the line from the arc's center through the point along of the
// way from where the arc touches its first segment to where it touches the second meets the arc.
ParkPoint bendPlace(const ParkCorner &corner, double along);
// The fraction bendPlace takes for the place on the corner's arc nearest the point, from 0 to 1.
double bendAlong(const ParkCorner &corner, ParkPoint point);

} // namespace tpj

#endif
