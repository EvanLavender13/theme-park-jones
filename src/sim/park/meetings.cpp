#include "sim/park/internal/meetings.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace tpj {

namespace {

// The cross product of a less the origin with b less the origin.
double crossOf(ParkPoint origin, ParkPoint a, ParkPoint b) {
  return (a.X - origin.X) * (b.Z - origin.Z) - (a.Z - origin.Z) * (b.X - origin.X);
}

double distanceBetween(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The distance from the point to the line through two distinct positions.
double lineDistance(ParkPoint point, ParkPoint from, ParkPoint to) {
  return std::abs(crossOf(from, to, point)) / distanceBetween(from, to);
}

// How far along the segment between two distinct positions the point's nearest place on it lies,
// as a fraction from 0 at the first to 1 at the second.
double alongOf(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::clamp(((point.X - from.X) * dx + (point.Z - from.Z) * dz) / (dx * dx + dz * dz), 0.0,
                    1.0);
}

// The distance from the point to the segment between two distinct positions.
double segmentDistance(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double t = alongOf(point, from, to);
  return distanceBetween(point, {from.X + t * (to.X - from.X), from.Z + t * (to.Z - from.Z)});
}

} // namespace

std::optional<Meeting> meetingOf(ParkPoint a0, ParkPoint a1, ParkPoint b0, ParkPoint b1) {
  constexpr double TOLERANCE = MIN_SEGMENT_LENGTH;
  if (lineDistance(a0, b0, b1) <= TOLERANCE && lineDistance(a1, b0, b1) <= TOLERANCE &&
      lineDistance(b0, a0, a1) <= TOLERANCE && lineDistance(b1, a0, a1) <= TOLERANCE) {
    return std::nullopt;
  }
  const std::array<ParkPoint, 2> a{a0, a1};
  const std::array<ParkPoint, 2> b{b0, b1};
  for (const ParkPoint &end : a) {
    if (distanceBetween(end, b0) <= TOLERANCE || distanceBetween(end, b1) <= TOLERANCE) {
      return std::nullopt;
    }
  }
  for (size_t end = 0; end < a.size(); ++end) {
    if (segmentDistance(a[end], b0, b1) <= TOLERANCE) {
      return Meeting{MeetingKind::AEndOnB, end, end == 0 ? 0.0 : 1.0, alongOf(a[end], b0, b1),
                     a[end]};
    }
  }
  for (size_t end = 0; end < b.size(); ++end) {
    if (segmentDistance(b[end], a0, a1) <= TOLERANCE) {
      return Meeting{MeetingKind::BEndOnA, end, alongOf(b[end], a0, a1), end == 0 ? 0.0 : 1.0,
                     b[end]};
    }
  }
  // Each segment's ends lie strictly on opposite sides of the other's line.
  const double d0 = crossOf(a0, a1, b0);
  const double d1 = crossOf(a0, a1, b1);
  const double e0 = crossOf(b0, b1, a0);
  const double e1 = crossOf(b0, b1, a1);
  if (d0 * d1 < 0.0 && e0 * e1 < 0.0) {
    const double alongA = e0 / (e0 - e1);
    const double alongB = d0 / (d0 - d1);
    return Meeting{MeetingKind::Crossing, 0, alongA, alongB,
                   ParkPoint{a0.X + alongA * (a1.X - a0.X), a0.Z + alongA * (a1.Z - a0.Z)}};
  }
  return std::nullopt;
}

} // namespace tpj
