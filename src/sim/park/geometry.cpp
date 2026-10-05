#include "sim/park/geometry.h"

#include <algorithm>
#include <cmath>

namespace tpj {

namespace {

bool isInsidePark(const ParkPoint &point) {
  constexpr double HALF = PARK_SIZE / 2.0;
  return std::isfinite(point.X) && std::isfinite(point.Z) && point.X >= -HALF && point.X <= HALF &&
         point.Z >= -HALF && point.Z <= HALF;
}

} // namespace

std::vector<CarrierPoint> segmentLine(ParkPoint from, ParkPoint to) {
  if (!isInsidePark(from) || !isInsidePark(to)) {
    return {};
  }
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double length = std::sqrt(dx * dx + dz * dz);
  if (length < MIN_SEGMENT_LENGTH) {
    return {};
  }
  return {CarrierPoint{from.X, from.Z, 0.0}, CarrierPoint{to.X, to.Z, length}};
}

std::optional<Footprint> footprintOf(const Pose &pose, FootprintSize size) {
  if (!std::isfinite(pose.X) || !std::isfinite(pose.Z) || !std::isfinite(pose.FacingX) ||
      !std::isfinite(pose.FacingZ)) {
    return std::nullopt;
  }
  // Dividing by the larger magnitude first keeps the sum of squares between 1 and 2, so it neither
  // overflows nor underflows.
  const double larger = std::max(std::abs(pose.FacingX), std::abs(pose.FacingZ));
  if (larger == 0.0) {
    return std::nullopt;
  }
  const double x = pose.FacingX / larger;
  const double z = pose.FacingZ / larger;
  const double length = std::sqrt(x * x + z * z);
  const ParkPoint forward{x / length, z / length};
  const ParkPoint right{-forward.Z, forward.X};
  const double halfWidth = size.Width / 2.0;
  const double halfDepth = size.Depth / 2.0;
  const auto corner = [&](double along, double across) {
    return ParkPoint{pose.X + forward.X * along + right.X * across,
                     pose.Z + forward.Z * along + right.Z * across};
  };
  return Footprint{forward,
                   right,
                   {corner(halfDepth, -halfWidth), corner(halfDepth, halfWidth),
                    corner(-halfDepth, halfWidth), corner(-halfDepth, -halfWidth)}};
}

} // namespace tpj
