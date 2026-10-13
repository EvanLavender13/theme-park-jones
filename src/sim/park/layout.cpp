#include "sim/park/internal/layout.h"

#include "sim/park/geometry.h"
#include "sim/park/internal/around.h"
#include "sim/tick_buffers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <stddef.h>
#include <tuple>
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

// Puts the straight ground line between the segment's points in line: empty unless they are two
// different path points and its width is finite and above zero.
void straightLine(const PathLayout &layout, const PathSegmentIntent &segment,
                  std::vector<CarrierPoint> &line) {
  line.clear();
  const PathPointIntent *from = findPoint(layout, segment.From);
  const PathPointIntent *to = findPoint(layout, segment.To);
  if (from == nullptr || to == nullptr || segment.From == segment.To ||
      !std::isfinite(segment.Width) || !(segment.Width > 0.0)) {
    return;
  }
  segmentLine(from->At, to->At, line);
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

// How a point turns between its two segments, the lower-keyed first: each one's far end, the unit
// direction toward it, and its straight length, and the dot and cross products of the two
// directions.
struct Bend {
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  double Asked = 0.0;
  std::array<size_t, 2> Segments{};
  std::array<EntityKey, 2> FarKeys{};
  std::array<ParkPoint, 2> Along{};
  std::array<double, 2> Lengths{};
  double Dot = 0.0;
  double Cross = 0.0;

  // The tangent length the radius gives.
  double tangentOf(double radius) const { return radius * Cross / (1.0 - Dot); }
};

// The bend the point makes with its two segments, each with a straight ground line. None where
// their far ends are one point or too close, or the segments run straight on or fold straight back.
std::optional<Bend> bendOf(const PathLayout &layout, EntityKey key, const PathPointIntent &point,
                           std::array<size_t, 2> segments) {
  Bend bend;
  bend.Key = key;
  bend.At = point.At;
  bend.Asked = std::isfinite(point.Radius) && point.Radius >= 0.0 ? point.Radius : 0.0;
  bend.Segments = segments;
  std::array<ParkPoint, 2> far{};
  for (size_t k = 0; k < 2; ++k) {
    const PathSegmentIntent &segment = layout.Segments[segments[k]].Intent;
    bend.FarKeys[k] = segment.From == key ? segment.To : segment.From;
    far[k] = findPoint(layout, bend.FarKeys[k])->At;
  }
  if (bend.FarKeys[0] == bend.FarKeys[1] || distanceBetween(far[0], far[1]) < MIN_SEGMENT_LENGTH ||
      lineDistance(bend.At, far[0], far[1]) <= MIN_SEGMENT_LENGTH) {
    return std::nullopt;
  }
  for (size_t k = 0; k < 2; ++k) {
    bend.Along[k] = unitToward(bend.At, far[k]);
    bend.Lengths[k] = distanceBetween(bend.At, far[k]);
  }
  bend.Dot = bend.Along[0].X * bend.Along[1].X + bend.Along[0].Z * bend.Along[1].Z;
  bend.Cross = std::abs(bend.Along[0].X * bend.Along[1].Z - bend.Along[0].Z * bend.Along[1].X);
  return bend;
}

// The corner the bend makes, with its halves appended to the arc buffer. needed gives how far from
// a point along one of its segments that point's own arc and paving reach at least. Each segment
// allows the arc its length less what its far end needs, or half its length if that is more.
template <typename Needed>
Rounding roundingOf(const Bend &bend, const Needed &needed, std::vector<ParkPoint> &arcs) {
  double allowed = 0.0;
  for (size_t k = 0; k < 2; ++k) {
    const double length = bend.Lengths[k];
    const double allowance =
        std::max(length - needed(bend.FarKeys[k], bend.Segments[k]), length / 2.0);
    allowed = k == 0 ? allowance : std::min(allowed, allowance);
  }
  const double maxRadius = allowed * (1.0 - bend.Dot) / bend.Cross;
  const double radius = std::min(bend.Asked, maxRadius);
  const double tangent = bend.tangentOf(radius);
  const ParkPoint sum{bend.Along[0].X + bend.Along[1].X, bend.Along[0].Z + bend.Along[1].Z};
  const double sumLength = std::sqrt(sum.X * sum.X + sum.Z * sum.Z);
  const ParkPoint toward{sum.X / sumLength, sum.Z / sumLength};
  // The arc's center lies along toward, as far from the point as the hypotenuse of the tangent
  // length and the radius. The arc's middle lies the radius nearer the point.
  const double centerDepth = std::sqrt(tangent * tangent + radius * radius);
  const ParkPoint middle = stepFrom(bend.At, toward, centerDepth - radius);
  Rounding rounding{ParkCorner{bend.Key, bend.At, bend.Along, toward, radius, maxRadius, middle},
                    bend.Segments,
                    {},
                    {}};
  for (size_t k = 0; k < 2; ++k) {
    rounding.HalfBegin[k] = arcs.size();
    if (radius > 0.0) {
      appendArc(arcs, stepFrom(bend.At, toward, centerDepth), radius, middle,
                stepFrom(bend.At, bend.Along[k], tangent));
    } else {
      arcs.push_back(bend.At);
    }
    rounding.HalfEnd[k] = arcs.size();
  }
  return rounding;
}

// Puts each straight ground line in lines, by the segment's index in the layout, keeping the lines
// it no longer needs in spare.
void straightLines(const PathLayout &layout, std::vector<std::vector<CarrierPoint>> &lines,
                   std::vector<std::vector<CarrierPoint>> &spare) {
  resizeKeeping(lines, layout.Segments.size(), spare);
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    straightLine(layout, layout.Segments[i].Intent, lines[i]);
  }
}

// Directions whose cross product is within this of zero run along one line.
constexpr double PARALLEL = 1e-9;

// A segment's end at a point, as pavingReaches sees it: the segment's index, its unit direction
// away from the point, and its half width.
struct ReachArm {
  size_t Segment = 0;
  ParkPoint Along;
  double Half = 0.0;
  // The edge radius held at the point, for the edge starting from the segment's left side.
  double Edge = 0.0;
};

// The lists finding the roundings reuses: each segment end, by its point's key and then by the
// segment's index, the bends, the paving reaches, the arms at one point, and the roundings.
struct RoundingLists {
  std::vector<std::pair<EntityKey, size_t>> Ends;
  std::vector<Bend> Bends;
  std::vector<std::tuple<EntityKey, size_t, double>> Reaches;
  std::vector<ReachArm> Arms;
  std::vector<Rounding> Roundings;
};

// Whether the outline would step straight across from the first arm's left side to the second's
// right side, the second lying counter-clockwise of the first, when the edge there has all the room
// it needs: where they run straight on, or where an outer edge can't touch both sides between the
// point's own corner and the wider width behind the point, as the paving fits it. start is how far
// from the point the point's own corner arc touches its segments.
bool stepsBetween(const ReachArm &a, const ReachArm &b, double start) {
  const double sine = ccwOf(a.Along, b.Along);
  const double cosine = a.Along.X * b.Along.X + a.Along.Z * b.Along.Z;
  if (std::abs(sine) <= PARALLEL) {
    return cosine < 0.0;
  }
  // An inner edge with all the room it needs always fits.
  if (sine > 0.0) {
    return false;
  }
  const double grow = -(1.0 + cosine);
  const auto radiusA = [&](double d) { return (d * sine - b.Half - a.Half * cosine) / grow; };
  const auto radiusB = [&](double d) { return (d * sine - a.Half - b.Half * cosine) / grow; };
  const double behind = -2.0 * std::max(a.Half, b.Half);
  const double least = std::max({0.0, radiusA(behind), radiusB(behind)});
  return least > std::min(radiusA(start), radiusB(start));
}

// At a point where exactly two segments of different widths meet, the outline steps between them
// on at least one side, and the point holds a taper length above 0, the index of the narrower one's
// arm, whose curve may run that far along it. None elsewhere. The arms are in counter-clockwise
// order.
std::optional<size_t> taperArm(const PathPointIntent &point, const std::vector<ReachArm> &arms) {
  const double taper = std::isfinite(point.Taper) && point.Taper > 0.0 ? point.Taper : 0.0;
  if (arms.size() != 2 || arms[0].Half == arms[1].Half || taper == 0.0) {
    return std::nullopt;
  }
  const double cosine = arms[0].Along.X * arms[1].Along.X + arms[0].Along.Z * arms[1].Along.Z;
  const double sine = std::abs(ccwOf(arms[0].Along, arms[1].Along));
  const double radius = std::isfinite(point.Radius) && point.Radius > 0.0 ? point.Radius : 0.0;
  const double start = sine > PARALLEL ? radius * sine / (1.0 - cosine) : 0.0;
  if (!stepsBetween(arms[0], arms[1], start) && !stepsBetween(arms[1], arms[0], start)) {
    return std::nullopt;
  }
  return arms[0].Half < arms[1].Half ? 0 : 1;
}

// How far along each segment from each of its points with two or more segments that point's paving
// reaches: the farther of where the inner edges on the segment's two sides touch it, each with the
// radius it is held at, or 0. At a point with two segments, the inner edge's radius is the point's
// radius less the narrower half width. Where the narrower of two segments may curve out to the
// wider one, its reach is the taper length the point holds farther. By the point's key,
// then the segment's index, in reaches. arms is a list it reuses.
void pavingReaches(const PathLayout &layout, const std::vector<std::pair<EntityKey, size_t>> &ends,
                   const std::vector<std::vector<CarrierPoint>> &straight,
                   std::vector<std::tuple<EntityKey, size_t, double>> &reaches,
                   std::vector<ReachArm> &arms) {
  reaches.clear();
  for (size_t first = 0; first < ends.size();) {
    size_t last = first;
    while (last < ends.size() && ends[last].first == ends[first].first) {
      ++last;
    }
    const EntityKey key = ends[first].first;
    const PathPointIntent *point = findPoint(layout, key);
    arms.clear();
    for (size_t i = first; i < last && point != nullptr; ++i) {
      const size_t segment = ends[i].second;
      const PathSegmentIntent &intent = layout.Segments[segment].Intent;
      if (straight[segment].empty()) {
        continue;
      }
      const PathPointIntent *far = findPoint(layout, intent.From == key ? intent.To : intent.From);
      const double edge = intent.From == key ? intent.FromEdge : intent.ToEdge;
      arms.push_back(ReachArm{segment, unitToward(point->At, far->At), intent.Width / 2.0,
                              std::isfinite(edge) && edge > 0.0 ? edge : 0.0});
    }
    first = last;
    if (arms.size() < 2) {
      continue;
    }
    std::ranges::sort(arms, [](const ReachArm &x, const ReachArm &y) {
      if (comesBefore(x.Along, y.Along) != comesBefore(y.Along, x.Along)) {
        return comesBefore(x.Along, y.Along);
      }
      return x.Segment < y.Segment;
    });
    const std::optional<size_t> tapered = taperArm(*point, arms);
    for (size_t i = 0; i < arms.size(); ++i) {
      const ReachArm &arm = arms[i];
      const ReachArm &next = arms[(i + 1) % arms.size()];
      const ReachArm &previous = arms[(i + arms.size() - 1) % arms.size()];
      // An inner edge of radius r between two segments touches the first
      // (b.Half + a.Half cos + (1 + cos) r) / sine from the point, and the second the same with the
      // halves swapped, when the second lies less than half a turn on.
      const auto touch = [&](const ReachArm &a, const ReachArm &b, bool onFirst) {
        const double sine = ccwOf(a.Along, b.Along);
        const double cosine = a.Along.X * b.Along.X + a.Along.Z * b.Along.Z;
        if (sine <= PARALLEL) {
          return 0.0;
        }
        const double held =
            arms.size() == 2 ? std::max(point->Radius - std::min(a.Half, b.Half), 0.0) : a.Edge;
        const double radius = std::isfinite(held) ? held : 0.0;
        const double grow = (1.0 + cosine) * radius;
        return onFirst ? (b.Half + a.Half * cosine + grow) / sine
                       : (a.Half + b.Half * cosine + grow) / sine;
      };
      const double reach = std::max({0.0, touch(arm, next, true), touch(previous, arm, false)});
      const bool curves = tapered.has_value() && tapered.value_or(0) == i;
      reaches.emplace_back(key, arm.Segment, curves ? reach + point->Taper : reach);
    }
  }
  std::ranges::sort(reaches);
}

// Each corner the layout's points make, in ascending key order of its point, with their halves in
// the arc buffer. Every bend is found first, so each corner knows what the points at its segments'
// far ends need: the tangent length a bend's own radius asks for, and the reach of its paving's
// inner edges. The roundings are put in lists.Roundings.
void roundingsOf(const PathLayout &layout, const std::vector<std::vector<CarrierPoint>> &straight,
                 std::vector<ParkPoint> &arcs, RoundingLists &lists) {
  std::vector<std::pair<EntityKey, size_t>> &ends = lists.Ends;
  ends.clear();
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    ends.emplace_back(layout.Segments[i].Intent.From, i);
    ends.emplace_back(layout.Segments[i].Intent.To, i);
  }
  std::ranges::sort(ends);
  std::vector<Bend> &bends = lists.Bends;
  bends.clear();
  for (size_t first = 0; first < ends.size();) {
    size_t last = first;
    while (last < ends.size() && ends[last].first == ends[first].first) {
      ++last;
    }
    const EntityKey key = ends[first].first;
    const PathPointIntent *point = findPoint(layout, key);
    if (last - first == 2 && point != nullptr && !straight[ends[first].second].empty() &&
        !straight[ends[first + 1].second].empty()) {
      if (std::optional<Bend> bend =
              bendOf(layout, key, *point, {ends[first].second, ends[first + 1].second})) {
        bends.push_back(*bend);
      }
    }
    first = last;
  }
  const std::vector<std::tuple<EntityKey, size_t, double>> &sharp = lists.Reaches;
  pavingReaches(layout, ends, straight, lists.Reaches, lists.Arms);
  const auto needed = [&bends, &sharp](EntityKey key, size_t segment) {
    const auto bend = std::ranges::lower_bound(bends, key, {}, &Bend::Key);
    const double asked =
        bend != bends.end() && bend->Key == key ? bend->tangentOf(bend->Asked) : 0.0;
    const auto reach = std::ranges::lower_bound(sharp, std::tuple{key, segment, 0.0});
    const bool found =
        reach != sharp.end() && std::get<0>(*reach) == key && std::get<1>(*reach) == segment;
    return std::max(asked, found ? std::get<2>(*reach) : 0.0);
  };
  lists.Roundings.clear();
  lists.Roundings.reserve(bends.size());
  for (const Bend &bend : bends) {
    lists.Roundings.push_back(roundingOf(bend, needed, arcs));
  }
}

} // namespace

PathLayout layoutOf(const World &world) {
  PathLayout layout;
  layoutOf(world, layout);
  return layout;
}

void layoutOf(const World &world, PathLayout &layout) {
  layout.Points.clear();
  layout.Segments.clear();
  world.forEachWith<PathPointIntent>([&layout](EntityKey key, const PathPointIntent &point) {
    layout.Points.push_back(LayoutPoint{key, point});
  });
  world.forEachWith<PathSegmentIntent>([&layout](EntityKey key, const PathSegmentIntent &segment) {
    layout.Segments.push_back(LayoutSegment{key, segment});
  });
}

namespace {

// Puts the ground line of the segment at index i in line, where a corner at either end is rounded:
// from the start's arc, or from the straight line's start, to the end's arc, or to its end.
void roundedLine(std::vector<CarrierPoint> &line, size_t i, ParkPoint from, ParkPoint to,
                 const Rounding *start, const Rounding *end, const std::vector<ParkPoint> &arcs) {
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

// Puts the layout's ground in lists.Ground. When the lists are kept, each segment's line is copied
// from its straight line, so both keep their memory. Otherwise the straight line moves into it.
void fillGround(const PathLayout &layout, GroundLists &lists, bool kept) {
  straightLines(layout, lists.Lines, lists.SpareLines);
  lists.Arcs.clear();
  RoundingLists &found = lists.Scratch.get<RoundingLists>();
  roundingsOf(layout, lists.Lines, lists.Arcs, found);
  const std::vector<Rounding> &roundings = found.Roundings;
  // The rounded corner at the point, or null.
  const auto roundingAt = [&roundings](EntityKey point) -> const Rounding * {
    const auto at = std::ranges::lower_bound(
        roundings, point, {}, [](const Rounding &rounding) { return rounding.Corner.Point; });
    return at != roundings.end() && at->Corner.Point == point ? &*at : nullptr;
  };
  PathGround &ground = lists.Ground;
  resizeKeeping(ground.Segments, layout.Segments.size(), lists.SpareSegments);
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const auto &[key, intent] = layout.Segments[i];
    std::vector<CarrierPoint> &straight = lists.Lines[i];
    ParkSegment &segment = ground.Segments[i];
    std::vector<CarrierPoint> &line = segment.Line;
    const Rounding *start = straight.empty() ? nullptr : roundingAt(intent.From);
    const Rounding *end = straight.empty() ? nullptr : roundingAt(intent.To);
    if (start == nullptr && end == nullptr) {
      if (kept) {
        copyGrowing(line, std::span<const CarrierPoint>(straight));
      } else {
        line = std::move(straight);
      }
    } else {
      const ParkPoint from{straight.front().X, straight.front().Z};
      const ParkPoint to{straight.back().X, straight.back().Z};
      if (!kept) {
        line = std::move(straight);
      }
      roundedLine(line, i, from, to, start, end, lists.Arcs);
    }
    segment.Key = key;
    segment.Kind = intent.Kind;
    segment.From = intent.From;
    segment.To = intent.To;
    segment.Width = intent.Width;
    segment.FromEdge = intent.FromEdge;
    segment.ToEdge = intent.ToEdge;
    segment.Furnished = {};
  }
  ground.Corners.clear();
  ground.Corners.reserve(roundings.size());
  for (const Rounding &rounding : roundings) {
    ground.Corners.push_back(rounding.Corner);
  }
}

} // namespace

PathGround groundOf(const PathLayout &layout) {
  GroundLists lists;
  fillGround(layout, lists, false);
  return std::move(lists.Ground);
}

void groundOf(const PathLayout &layout, GroundLists &lists) { fillGround(layout, lists, true); }

std::vector<ParkSegment> segmentsOf(const PathLayout &layout) { return groundOf(layout).Segments; }

std::vector<ParkCorner> cornersOf(const PathLayout &layout) { return groundOf(layout).Corners; }

} // namespace tpj
