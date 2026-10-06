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

double cornerRadiusFor(const ParkCorner &corner, double depth) {
  if (!(depth > 0.0)) {
    return 0.0;
  }
  const double dot = corner.Along[0].X * corner.Along[1].X + corner.Along[0].Z * corner.Along[1].Z;
  // The sine of half the angle between the two segments, below 1 because they don't run straight
  // on.
  const double sine = std::sqrt((1.0 - dot) / 2.0);
  return depth * sine / (1.0 - sine);
}

CornerArc arcOf(const ParkCorner &corner) {
  const double radius = corner.Radius > 0.0 ? corner.Radius : 0.0;
  const ParkPoint &a = corner.Along[0];
  const ParkPoint &b = corner.Along[1];
  const double dot = a.X * b.X + a.Z * b.Z;
  const double cross = std::abs(a.X * b.Z - a.Z * b.X);
  // The tangent length is the radius times cross / (1 - dot), and the center lies along Toward as
  // far from the point as the hypotenuse of the tangent length and the radius.
  const double tangent = radius > 0.0 ? radius * cross / (1.0 - dot) : 0.0;
  const double depth = std::sqrt(tangent * tangent + radius * radius);
  const auto from = [&corner](ParkPoint direction, double distance) {
    return ParkPoint{corner.At.X + direction.X * distance, corner.At.Z + direction.Z * distance};
  };
  return CornerArc{from(corner.Toward, depth), radius, {from(a, tangent), from(b, tangent)}};
}

ParkPoint bendPlace(const ParkCorner &corner, double along) {
  const CornerArc arc = arcOf(corner);
  const ParkPoint chord{arc.Touch[0].X + along * (arc.Touch[1].X - arc.Touch[0].X),
                        arc.Touch[0].Z + along * (arc.Touch[1].Z - arc.Touch[0].Z)};
  const ParkPoint out{chord.X - arc.Center.X, chord.Z - arc.Center.Z};
  const double length = std::sqrt(out.X * out.X + out.Z * out.Z);
  if (!(length > 0.0)) {
    return chord;
  }
  return {arc.Center.X + out.X * arc.Radius / length, arc.Center.Z + out.Z * arc.Radius / length};
}

double bendAlong(const ParkCorner &corner, ParkPoint point) {
  const CornerArc arc = arcOf(corner);
  const ParkPoint chord{arc.Touch[1].X - arc.Touch[0].X, arc.Touch[1].Z - arc.Touch[0].Z};
  const ParkPoint out{point.X - arc.Center.X, point.Z - arc.Center.Z};
  const ParkPoint first{arc.Touch[0].X - arc.Center.X, arc.Touch[0].Z - arc.Center.Z};
  const auto cross = [](ParkPoint u, ParkPoint v) { return u.X * v.Z - u.Z * v.X; };
  // The line from the center through the point meets the chord at first + s chord, where the cross
  // product of that with out is 0.
  const double across = cross(chord, out);
  const double toward = out.X * chord.X + out.Z * chord.Z;
  if (across == 0.0) {
    return toward > 0.0 ? 1.0 : 0.0;
  }
  const double s = -cross(first, out) / across;
  const ParkPoint meet{first.X + s * chord.X, first.Z + s * chord.Z};
  // A point behind the center, away from the arc, lies nearest one of its ends.
  if (meet.X * out.X + meet.Z * out.Z <= 0.0) {
    return toward > 0.0 ? 1.0 : 0.0;
  }
  return std::clamp(s, 0.0, 1.0);
}

} // namespace tpj
