#include "sim/park/geometry.h"

#include <algorithm>
#include <cmath>
#include <stddef.h>

namespace tpj {

namespace {

bool isInsidePark(const ParkPoint &point) {
  constexpr double HALF = PARK_SIZE / 2.0;
  return std::isfinite(point.X) && std::isfinite(point.Z) && point.X >= -HALF && point.X <= HALF &&
         point.Z >= -HALF && point.Z <= HALF;
}

double chord(const ParkPoint &from, const ParkPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The points in order, without any closer than MIN_POINT_SPACING to the last kept one.
std::vector<ParkPoint> keptPoints(const std::vector<ParkPoint> &points) {
  constexpr double MIN_SQUARED = MIN_POINT_SPACING * MIN_POINT_SPACING;
  std::vector<ParkPoint> kept;
  for (const ParkPoint &point : points) {
    if (!kept.empty()) {
      const double dx = point.X - kept.back().X;
      const double dz = point.Z - kept.back().Z;
      if (dx * dx + dz * dz < MIN_SQUARED) {
        continue;
      }
    }
    kept.push_back(point);
  }
  return kept;
}

// The neighbor beyond an end of the path: the end's other neighbor reflected through it.
ParkPoint phantom(const ParkPoint &end, const ParkPoint &neighbor) {
  return {2.0 * end.X - neighbor.X, 2.0 * end.Z - neighbor.Z};
}

ParkPoint blend(double fromWeight, const ParkPoint &from, double toWeight, const ParkPoint &to) {
  return {fromWeight * from.X + toWeight * to.X, fromWeight * from.Z + toWeight * to.Z};
}

// The curve between p[1] and p[2] at t, from t1 to t2: the Barry and Goldman pyramid.
ParkPoint evaluateSegment(const std::array<ParkPoint, 4> &p, const std::array<double, 4> &knots,
                          double t) {
  const auto [t0, t1, t2, t3] = knots;
  const ParkPoint a1 = blend((t1 - t) / (t1 - t0), p[0], (t - t0) / (t1 - t0), p[1]);
  const ParkPoint a2 = blend((t2 - t) / (t2 - t1), p[1], (t - t1) / (t2 - t1), p[2]);
  const ParkPoint a3 = blend((t3 - t) / (t3 - t2), p[2], (t - t2) / (t3 - t2), p[3]);
  const ParkPoint b1 = blend((t2 - t) / (t2 - t0), a1, (t - t0) / (t2 - t0), a2);
  const ParkPoint b2 = blend((t3 - t) / (t3 - t1), a2, (t - t1) / (t3 - t1), a3);
  return blend((t2 - t) / (t2 - t1), b1, (t - t1) / (t2 - t1), b2);
}

// Appends the point with its distance: the previous distance plus the step's length.
void appendPoint(std::vector<CarrierPoint> &line, const ParkPoint &point) {
  double distance = 0.0;
  if (!line.empty()) {
    const CarrierPoint &last = line.back();
    const double dx = point.X - last.X;
    const double dz = point.Z - last.Z;
    distance = last.Distance + std::sqrt(dx * dx + dz * dz);
  }
  line.push_back(CarrierPoint{point.X, point.Z, distance});
}

} // namespace

std::vector<CarrierPoint> groundLine(const std::vector<ParkPoint> &points) {
  if (!std::ranges::all_of(points, isInsidePark)) {
    return {};
  }
  const std::vector<ParkPoint> kept = keptPoints(points);
  if (kept.size() < 2) {
    return {};
  }
  std::vector<CarrierPoint> line;
  const size_t last = kept.size() - 1;
  for (size_t i = 0; i < last; ++i) {
    const std::array<ParkPoint, 4> p{
        i == 0 ? phantom(kept[0], kept[1]) : kept[i - 1], kept[i], kept[i + 1],
        i + 1 == last ? phantom(kept[last], kept[last - 1]) : kept[i + 2]};
    std::array<double, 4> knots{};
    for (size_t j = 1; j < knots.size(); ++j) {
      knots[j] = knots[j - 1] + std::sqrt(chord(p[j - 1], p[j]));
    }
    const int samples =
        static_cast<int>(std::max(static_cast<double>(MIN_SEGMENT_SAMPLES),
                                  std::ceil(chord(p[1], p[2]) / GROUND_LINE_SPACING)));
    // The segment's first point is the kept point itself, so the line passes through it exactly.
    appendPoint(line, kept[i]);
    for (int k = 1; k < samples; ++k) {
      const double t =
          knots[1] + (knots[2] - knots[1]) * static_cast<double>(k) / static_cast<double>(samples);
      appendPoint(line, evaluateSegment(p, knots, t));
    }
  }
  appendPoint(line, kept[last]);
  return line;
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
