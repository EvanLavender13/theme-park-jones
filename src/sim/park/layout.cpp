#include "sim/park/internal/layout.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// Ground line points nearer together than this, in meters, are one point.
constexpr double SAME_POINT = 1e-6;
// A half arc is halved into equal pieces at most this many times.
constexpr int MAX_ARC_LEVELS = 6;

double distanceBetween(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The position the step takes from the position along the unit direction.
ParkPoint stepFrom(ParkPoint at, ParkPoint direction, double step) {
  return {at.X + direction.X * step, at.Z + direction.Z * step};
}

// The unit direction from one position to another at least MIN_SEGMENT_LENGTH from it.
ParkPoint unitToward(ParkPoint from, ParkPoint to) {
  const double length = distanceBetween(from, to);
  return {(to.X - from.X) / length, (to.Z - from.Z) / length};
}

// The distance from the point to the line through two positions at least MIN_SEGMENT_LENGTH
// apart.
double lineDistance(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double cross = (to.X - from.X) * (point.Z - from.Z) - (to.Z - from.Z) * (point.X - from.X);
  return std::abs(cross) / distanceBetween(from, to);
}

// The point of the circle halfway along its arc between two of its points less than a half turn
// apart: their chord's middle, pushed out to the radius.
ParkPoint arcMiddle(ParkPoint center, double radius, ParkPoint a, ParkPoint b) {
  const ParkPoint out{(a.X + b.X) / 2.0 - center.X, (a.Z + b.Z) / 2.0 - center.Z};
  const double length = std::sqrt(out.X * out.X + out.Z * out.Z);
  return {center.X + out.X * radius / length, center.Z + out.Z * radius / length};
}

// How far an arc of the circle bulges from its chord of the length.
double sagittaOf(double radius, double chord) {
  const double half = chord / 2.0;
  return radius - std::sqrt(std::max(radius * radius - half * half, 0.0));
}

// Appends points along the circle's arc from one of its points to another, both included: the arc
// halved into equal pieces until each lies within ARC_TOLERANCE of it, at most MAX_ARC_LEVELS
// times. The piece count is found first, so the points are written in place.
void appendArc(std::vector<ParkPoint> &out, ParkPoint center, double radius, ParkPoint from,
               ParkPoint to) {
  size_t pieces = 1;
  double chord = distanceBetween(from, to);
  for (int level = 0; level < MAX_ARC_LEVELS && sagittaOf(radius, chord) > ARC_TOLERANCE; ++level) {
    // The chord of half the arc is the square root of twice the radius times the sagitta.
    chord = std::sqrt(2.0 * radius * sagittaOf(radius, chord));
    pieces *= 2;
  }
  const size_t first = out.size();
  out.resize(first + pieces + 1);
  out[first] = from;
  out[first + pieces] = to;
  for (size_t step = pieces; step > 1; step /= 2) {
    for (size_t i = first; i < first + pieces; i += step) {
      out[i + step / 2] = arcMiddle(center, radius, out[i], out[i + step]);
    }
  }
}

// Adds the point to the end of the ground line at its distance along it, unless it lies within
// SAME_POINT of the line's last point.
void extendLine(std::vector<CarrierPoint> &line, ParkPoint point) {
  if (line.empty()) {
    line.push_back(CarrierPoint{point.X, point.Z, 0.0});
    return;
  }
  const double step = distanceBetween(ParkPoint{line.back().X, line.back().Z}, point);
  if (step >= SAME_POINT) {
    const double distance = line.back().Distance + step;
    line.push_back(CarrierPoint{point.X, point.Z, distance});
  }
}

// Ends the ground line at the point, after leaving out each point but the first that lies within
// SAME_POINT of it.
void endLine(std::vector<CarrierPoint> &line, ParkPoint point) {
  while (line.size() > 1 &&
         distanceBetween(ParkPoint{line.back().X, line.back().Z}, point) < SAME_POINT) {
    line.pop_back();
  }
  const double distance =
      line.back().Distance + distanceBetween(ParkPoint{line.back().X, line.back().Z}, point);
  line.push_back(CarrierPoint{point.X, point.Z, distance});
}

// The path point the key holds, or null.
const PathPointIntent *findPoint(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  return found != layout.Points.end() && found->Key == key ? &found->Intent : nullptr;
}

// The straight ground line between the segment's points: empty unless they are two different path
// points and its width is finite and above zero.
std::vector<CarrierPoint> straightLine(const PathLayout &layout, const PathSegmentIntent &segment) {
  const PathPointIntent *from = findPoint(layout, segment.From);
  const PathPointIntent *to = findPoint(layout, segment.To);
  if (from == nullptr || to == nullptr || segment.From == segment.To ||
      !std::isfinite(segment.Width) || !(segment.Width > 0.0)) {
    return {};
  }
  return segmentLine(from->At, to->At);
}

// A corner, and its two segments as indices into the layout's segments. For each segment, the
// points of its ground line from the arc's middle to where the arc touches it lie in a shared arc
// buffer, from HalfBegin up to HalfEnd.
struct Rounding {
  ParkCorner Corner;
  std::array<size_t, 2> Segments{};
  std::array<size_t, 2> HalfBegin{};
  std::array<size_t, 2> HalfEnd{};
};

// The corner the point makes with its two segments, the lower-keyed first, each with a straight
// ground line, with its halves appended to the arc buffer. None where their far ends are one point
// or too close, or the segments run straight on or fold straight back.
std::optional<Rounding> roundingOf(const PathLayout &layout, EntityKey key,
                                   const PathPointIntent &point, std::array<size_t, 2> segments,
                                   std::vector<ParkPoint> &arcs) {
  std::array<EntityKey, 2> farKeys{};
  std::array<ParkPoint, 2> far{};
  for (size_t k = 0; k < 2; ++k) {
    const PathSegmentIntent &segment = layout.Segments[segments[k]].Intent;
    farKeys[k] = segment.From == key ? segment.To : segment.From;
    far[k] = findPoint(layout, farKeys[k])->At;
  }
  const ParkPoint at = point.At;
  if (farKeys[0] == farKeys[1] || distanceBetween(far[0], far[1]) < MIN_SEGMENT_LENGTH ||
      lineDistance(at, far[0], far[1]) <= MIN_SEGMENT_LENGTH) {
    return std::nullopt;
  }
  const std::array<ParkPoint, 2> along{unitToward(at, far[0]), unitToward(at, far[1])};
  const double dot = along[0].X * along[1].X + along[0].Z * along[1].Z;
  const double cross = std::abs(along[0].X * along[1].Z - along[0].Z * along[1].X);
  // The arc's tangent length is the radius times cross / (1 - dot), so the largest radius that
  // fits touches the shorter segment halfway along it.
  const double shorter = std::min(distanceBetween(at, far[0]), distanceBetween(at, far[1]));
  const double maxRadius = 0.5 * shorter * (1.0 - dot) / cross;
  const double asked = std::isfinite(point.Radius) && point.Radius >= 0.0 ? point.Radius : 0.0;
  const double radius = std::min(asked, maxRadius);
  const double tangent = radius * cross / (1.0 - dot);
  const ParkPoint sum{along[0].X + along[1].X, along[0].Z + along[1].Z};
  const double sumLength = std::sqrt(sum.X * sum.X + sum.Z * sum.Z);
  const ParkPoint toward{sum.X / sumLength, sum.Z / sumLength};
  // The arc's center lies along toward, as far from the point as the hypotenuse of the tangent
  // length and the radius. The arc's middle lies the radius nearer the point.
  const double centerDepth = std::sqrt(tangent * tangent + radius * radius);
  const ParkPoint middle = stepFrom(at, toward, centerDepth - radius);
  Rounding rounding{
      ParkCorner{key, at, along, toward, radius, maxRadius, middle}, segments, {}, {}};
  for (size_t k = 0; k < 2; ++k) {
    rounding.HalfBegin[k] = arcs.size();
    if (radius > 0.0) {
      appendArc(arcs, stepFrom(at, toward, centerDepth), radius, middle,
                stepFrom(at, along[k], tangent));
    } else {
      arcs.push_back(at);
    }
    rounding.HalfEnd[k] = arcs.size();
  }
  return rounding;
}

// Each straight ground line, by the segment's index in the layout.
std::vector<std::vector<CarrierPoint>> straightLines(const PathLayout &layout) {
  std::vector<std::vector<CarrierPoint>> lines;
  lines.reserve(layout.Segments.size());
  for (const LayoutSegment &segment : layout.Segments) {
    lines.push_back(straightLine(layout, segment.Intent));
  }
  return lines;
}

// Each corner the layout's points make, in ascending key order of its point, with their halves in
// the arc buffer.
std::vector<Rounding> roundingsOf(const PathLayout &layout,
                                  const std::vector<std::vector<CarrierPoint>> &straight,
                                  std::vector<ParkPoint> &arcs) {
  // Each segment end, by its point's key, and then by the segment's index.
  std::vector<std::pair<EntityKey, size_t>> ends;
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    ends.emplace_back(layout.Segments[i].Intent.From, i);
    ends.emplace_back(layout.Segments[i].Intent.To, i);
  }
  std::ranges::sort(ends);
  std::vector<Rounding> roundings;
  for (size_t first = 0; first < ends.size();) {
    size_t last = first;
    while (last < ends.size() && ends[last].first == ends[first].first) {
      ++last;
    }
    const EntityKey key = ends[first].first;
    const PathPointIntent *point = findPoint(layout, key);
    if (last - first == 2 && point != nullptr && !straight[ends[first].second].empty() &&
        !straight[ends[first + 1].second].empty()) {
      if (std::optional<Rounding> rounding =
              roundingOf(layout, key, *point, {ends[first].second, ends[first + 1].second}, arcs)) {
        roundings.push_back(*rounding);
      }
    }
    first = last;
  }
  return roundings;
}

} // namespace

PathLayout layoutOf(const World &world) {
  PathLayout layout;
  for (const EntityKey key : world.keys()) {
    const entt::entity entity = world.findEntity(key);
    if (const auto *point = world.Registry.try_get<PathPointIntent>(entity)) {
      layout.Points.push_back(LayoutPoint{key, *point});
    }
    if (const auto *segment = world.Registry.try_get<PathSegmentIntent>(entity)) {
      layout.Segments.push_back(LayoutSegment{key, *segment});
    }
  }
  return layout;
}

PathGround groundOf(const PathLayout &layout) {
  std::vector<std::vector<CarrierPoint>> lines = straightLines(layout);
  std::vector<ParkPoint> arcs;
  const std::vector<Rounding> roundings = roundingsOf(layout, lines, arcs);
  // The rounded corner at the point, or null.
  const auto roundingAt = [&roundings](EntityKey point) -> const Rounding * {
    const auto found = std::ranges::lower_bound(
        roundings, point, {}, [](const Rounding &rounding) { return rounding.Corner.Point; });
    return found != roundings.end() && found->Corner.Point == point ? &*found : nullptr;
  };
  std::vector<ParkSegment> segments;
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const auto &[key, intent] = layout.Segments[i];
    std::vector<CarrierPoint> line = std::move(lines[i]);
    const Rounding *start = line.empty() ? nullptr : roundingAt(intent.From);
    const Rounding *end = line.empty() ? nullptr : roundingAt(intent.To);
    if (start != nullptr || end != nullptr) {
      const ParkPoint from{line.front().X, line.front().Z};
      const ParkPoint to{line.back().X, line.back().Z};
      line.clear();
      if (start != nullptr) {
        // From the arc's middle to where it touches this segment.
        const size_t k = start->Segments[0] == i ? 0 : 1;
        for (size_t j = start->HalfBegin[k]; j < start->HalfEnd[k]; ++j) {
          extendLine(line, arcs[j]);
        }
      } else {
        extendLine(line, from);
      }
      if (end != nullptr) {
        // From where the arc touches this segment back to its middle.
        const size_t k = end->Segments[0] == i ? 0 : 1;
        for (size_t j = end->HalfEnd[k] - 1; j > end->HalfBegin[k]; --j) {
          extendLine(line, arcs[j]);
        }
        endLine(line, arcs[end->HalfBegin[k]]);
      } else {
        endLine(line, to);
      }
    }
    segments.push_back(
        ParkSegment{key, intent.Kind, intent.From, intent.To, intent.Width, std::move(line)});
  }
  std::vector<ParkCorner> corners;
  corners.reserve(roundings.size());
  for (const Rounding &rounding : roundings) {
    corners.push_back(rounding.Corner);
  }
  return PathGround{std::move(segments), std::move(corners)};
}

std::vector<ParkSegment> segmentsOf(const PathLayout &layout) { return groundOf(layout).Segments; }

std::vector<ParkCorner> cornersOf(const PathLayout &layout) { return groundOf(layout).Corners; }

} // namespace tpj
