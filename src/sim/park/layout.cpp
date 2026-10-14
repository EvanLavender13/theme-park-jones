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
#include <stdexcept>
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

// The index in the layout's points of the point the key holds. Throws std::logic_error when the
// layout holds no such point.
size_t pointIndex(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  if (found == layout.Points.end() || found->Key != key) {
    throw std::logic_error("a bend's far point isn't in its layout");
  }
  return static_cast<size_t>(found - layout.Points.begin());
}

// The bend the point makes with the two ends, each with a straight ground line. None where their
// far ends are one point or too close, or the segments run straight on or fold straight back.
std::optional<Bend> bendOf(EntityKey key, const PathPointIntent &point,
                           std::span<const ArmInput> ends) {
  Bend bend;
  bend.Key = key;
  bend.At = point.At;
  bend.Asked = std::isfinite(point.Radius) && point.Radius >= 0.0 ? point.Radius : 0.0;
  std::array<ParkPoint, 2> far{};
  for (size_t k = 0; k < 2; ++k) {
    const PathSegmentIntent &segment = *ends[k].Intent;
    bend.Segments[k] = ends[k].Segment;
    bend.FarKeys[k] = segment.From == key ? segment.To : segment.From;
    far[k] = ends[k].Far->At;
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

// Directions whose cross product is within this of zero run along one line.
constexpr double PARALLEL = 1e-9;

// A segment's end at a point, as the paving reaches see it: the segment's key, its unit direction
// away from the point, and its half width.
struct ReachArm {
  EntityKey Segment = NULL_KEY;
  ParkPoint Along;
  double Half = 0.0;
  // The edge radius held at the point, for the edge starting from the segment's left side.
  double Edge = 0.0;
};

// The arms at one point, which armsAt reuses.
struct ReachArms {
  std::vector<ReachArm> Arms;
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

// How far along each of the point's segments with a straight ground line its paving reaches, when
// it has two or more: the farther of where the inner edges on the segment's two sides touch it,
// each with the radius it is held at, or 0. At a point with two segments, the inner edge's radius
// is the point's radius less the narrower half width. Where the narrower of two segments may curve
// out to the wider one, its reach is the taper length the point holds farther. In reaches, by the
// segment's key. arms is a list it reuses.
void pavingReaches(EntityKey key, const PathPointIntent &point, std::span<const ArmInput> ends,
                   std::vector<SegmentReach> &reaches, std::vector<ReachArm> &arms) {
  reaches.clear();
  arms.clear();
  for (const ArmInput &end : ends) {
    if (!end.Straight) {
      continue;
    }
    const PathSegmentIntent &intent = *end.Intent;
    const double edge = intent.From == key ? intent.FromEdge : intent.ToEdge;
    arms.push_back(ReachArm{end.Segment, unitToward(point.At, end.Far->At), intent.Width / 2.0,
                            std::isfinite(edge) && edge > 0.0 ? edge : 0.0});
  }
  if (arms.size() < 2) {
    return;
  }
  std::ranges::sort(arms, [](const ReachArm &x, const ReachArm &y) {
    if (comesBefore(x.Along, y.Along) != comesBefore(y.Along, x.Along)) {
      return comesBefore(x.Along, y.Along);
    }
    return x.Segment < y.Segment;
  });
  const std::optional<size_t> tapered = taperArm(point, arms);
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
          arms.size() == 2 ? std::max(point.Radius - std::min(a.Half, b.Half), 0.0) : a.Edge;
      const double radius = std::isfinite(held) ? held : 0.0;
      const double grow = (1.0 + cosine) * radius;
      return onFirst ? (b.Half + a.Half * cosine + grow) / sine
                     : (a.Half + b.Half * cosine + grow) / sine;
    };
    const double reach = std::max({0.0, touch(arm, next, true), touch(previous, arm, false)});
    const bool curves = tapered.has_value() && tapered.value_or(0) == i;
    reaches.push_back(SegmentReach{arm.Segment, curves ? reach + point.Taper : reach});
  }
  std::ranges::sort(reaches, [](const SegmentReach &x, const SegmentReach &y) {
    return std::tuple{x.Segment, x.Reach} < std::tuple{y.Segment, y.Reach};
  });
}

// What a point needs from its far ends' corners: the tangent length its own bend asks for, and
// where its reaches lie in a list of them.
struct PointNeeds {
  double Asked = 0.0;
  size_t First = 0;
  size_t Count = 0;
};

// A corner found for a layout: its corner, its two segments by key, and, for each segment, where
// the half of its arc that belongs to it lies in a list of arc points.
struct FlatRounding {
  ParkCorner Corner;
  std::array<EntityKey, 2> Segments{};
  std::array<size_t, 2> First{};
  std::array<size_t, 2> Count{};
};

// The lists deriving the corners reuses, all flat so that they grow only while the layout does:
// each segment end, by its point's key and then by the segment's index, one point's ends and arms,
// what each point needs, by the point's index, the reaches it refers to, the bends, in ascending
// key order of their points, one corner's rounding, and the corners, in the same order, with the
// halves of their arcs.
struct RoundingLists {
  std::vector<std::pair<EntityKey, size_t>> Ends;
  std::vector<ArmInput> Inputs;
  PointArms Arms;
  std::vector<PointNeeds> Needs;
  std::vector<SegmentReach> Reaches;
  std::vector<Bend> Bends;
  Rounding Round;
  std::vector<FlatRounding> Roundings;
  std::vector<ParkPoint> Halves;
};

// Puts each corner the layout's points make in lists.Roundings, in ascending key order of its
// point. Every point's arms are found first, so each corner knows what the points at its segments'
// far ends need: the tangent length a bend's own radius asks for, and the reach of its paving's
// inner edges.
void roundingsOf(const PathLayout &layout, const std::vector<std::vector<CarrierPoint>> &straight,
                 TickBuffers &scratch, RoundingLists &lists) {
  std::vector<std::pair<EntityKey, size_t>> &ends = lists.Ends;
  ends.clear();
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    ends.emplace_back(layout.Segments[i].Intent.From, i);
    ends.emplace_back(layout.Segments[i].Intent.To, i);
  }
  std::ranges::sort(ends);
  lists.Needs.clear();
  lists.Reaches.clear();
  lists.Bends.clear();
  size_t first = 0;
  for (size_t p = 0; p < layout.Points.size(); ++p) {
    const auto &[key, point] = layout.Points[p];
    while (first < ends.size() && ends[first].first < key) {
      ++first;
    }
    lists.Inputs.clear();
    for (; first < ends.size() && ends[first].first == key; ++first) {
      const LayoutSegment &segment = layout.Segments[ends[first].second];
      const EntityKey far = segment.Intent.From == key ? segment.Intent.To : segment.Intent.From;
      lists.Inputs.push_back(ArmInput{segment.Key, &segment.Intent,
                                      !straight[ends[first].second].empty(),
                                      findPoint(layout, far)});
    }
    const PointArms &arms = lists.Arms;
    armsAt(key, point, lists.Inputs, scratch, lists.Arms);
    lists.Needs.push_back(PointNeeds{askedOf(arms), lists.Reaches.size(), arms.Reaches.size()});
    lists.Reaches.insert(lists.Reaches.end(), arms.Reaches.begin(), arms.Reaches.end());
    if (arms.HasBend) {
      lists.Bends.push_back(arms.Bent);
    }
  }
  lists.Roundings.clear();
  lists.Halves.clear();
  const std::span<const SegmentReach> reaches(lists.Reaches);
  for (const Bend &bend : lists.Bends) {
    std::array<double, 2> needed{};
    for (size_t k = 0; k < 2; ++k) {
      const PointNeeds &far = lists.Needs[pointIndex(layout, bend.FarKeys[k])];
      needed[k] = neededAt(far.Asked, reaches.subspan(far.First, far.Count), bend.Segments[k]);
    }
    cornerAt(bend, needed, lists.Round);
    FlatRounding &flat = lists.Roundings.emplace_back();
    flat.Corner = lists.Round.Corner;
    flat.Segments = lists.Round.Segments;
    for (size_t k = 0; k < 2; ++k) {
      const std::vector<ParkPoint> &half = lists.Round.Halves[k];
      flat.First[k] = lists.Halves.size();
      flat.Count[k] = half.size();
      lists.Halves.insert(lists.Halves.end(), half.begin(), half.end());
    }
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

// Puts the ground line of the segment in line, where a corner at either end is rounded: from the
// start's arc, or from the straight line's start, to the end's arc, or to its end. start and end
// are the halves of the arcs that belong to the segment, or empty where there is no corner.
void roundedLine(std::vector<CarrierPoint> &line, ParkPoint from, ParkPoint to,
                 std::span<const ParkPoint> start, std::span<const ParkPoint> end) {
  line.clear();
  if (!start.empty()) {
    // From the arc's middle to where it touches this segment.
    for (const ParkPoint &point : start) {
      extendLine(line, point);
    }
  } else {
    extendLine(line, from);
  }
  if (!end.empty()) {
    // From where the arc touches this segment back to its middle.
    for (size_t j = end.size() - 1; j > 0; --j) {
      extendLine(line, end[j]);
    }
    endLine(line, end[0]);
  } else {
    endLine(line, to);
  }
}

// Puts the layout's ground in lists.Ground.
void fillGround(const PathLayout &layout, GroundLists &lists) {
  resizeKeeping(lists.Lines, layout.Segments.size(), lists.SpareLines);
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const PathSegmentIntent &segment = layout.Segments[i].Intent;
    straightLineOf(segment, findPoint(layout, segment.From), findPoint(layout, segment.To),
                   lists.Lines[i]);
  }
  RoundingLists &found = lists.Scratch.get<RoundingLists>();
  roundingsOf(layout, lists.Lines, lists.Scratch, found);
  const std::vector<FlatRounding> &roundings = found.Roundings;
  const std::span<const ParkPoint> halves(found.Halves);
  // The half of the arc of the rounded corner at the point that belongs to the segment, or empty
  // when the point has no corner.
  const auto halfAt = [&roundings, halves](EntityKey point,
                                           EntityKey segment) -> std::span<const ParkPoint> {
    const auto at = std::ranges::lower_bound(
        roundings, point, {}, [](const FlatRounding &rounding) { return rounding.Corner.Point; });
    if (at == roundings.end() || at->Corner.Point != point) {
      return {};
    }
    const size_t k = at->Segments[0] == segment ? 0 : 1;
    return halves.subspan(at->First[k], at->Count[k]);
  };
  PathGround &ground = lists.Ground;
  resizeKeeping(ground.Segments, layout.Segments.size(), lists.SpareSegments);
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const auto &[key, intent] = layout.Segments[i];
    const std::vector<CarrierPoint> &straight = lists.Lines[i];
    // A segment without a straight line rounds no corner.
    const std::span<const ParkPoint> start =
        straight.empty() ? std::span<const ParkPoint>() : halfAt(intent.From, key);
    const std::span<const ParkPoint> end =
        straight.empty() ? std::span<const ParkPoint>() : halfAt(intent.To, key);
    groundLineOf(key, intent, straight, start, end, ground.Segments[i]);
  }
  ground.Corners.clear();
  for (const FlatRounding &rounding : roundings) {
    ground.Corners.push_back(rounding.Corner);
  }
}

} // namespace

[[gnu::noinline]] void straightLineOf(const PathSegmentIntent &segment, const PathPointIntent *from,
                                      const PathPointIntent *to, std::vector<CarrierPoint> &line) {
  line.clear();
  if (from == nullptr || to == nullptr || segment.From == segment.To ||
      !std::isfinite(segment.Width) || !(segment.Width > 0.0)) {
    return;
  }
  segmentLine(from->At, to->At, line);
}

[[gnu::noinline]] void armsAt(EntityKey key, const PathPointIntent &point,
                              std::span<const ArmInput> ends, TickBuffers &scratch,
                              PointArms &arms) {
  arms.HasBend = false;
  arms.Bent = Bend{};
  if (ends.size() == 2 && ends[0].Straight && ends[1].Straight) {
    if (const std::optional<Bend> bend = bendOf(key, point, ends)) {
      arms.HasBend = true;
      arms.Bent = *bend;
    }
  }
  pavingReaches(key, point, ends, arms.Reaches, scratch.get<ReachArms>().Arms);
}

double askedOf(const PointArms &arms) {
  return arms.HasBend ? arms.Bent.tangentOf(arms.Bent.Asked) : 0.0;
}

double neededAt(double asked, std::span<const SegmentReach> reaches, EntityKey segment) {
  const auto reach =
      std::ranges::lower_bound(reaches, std::tuple{segment, 0.0}, {}, [](const SegmentReach &each) {
        return std::tuple{each.Segment, each.Reach};
      });
  const bool found = reach != reaches.end() && reach->Segment == segment;
  return std::max(asked, found ? reach->Reach : 0.0);
}

double neededAt(const PointArms &far, EntityKey segment) {
  return neededAt(askedOf(far), far.Reaches, segment);
}

// Each segment allows the arc its length less what its far end needs, or half its length if that
// is more.
[[gnu::noinline]] void cornerAt(const Bend &bend, std::array<double, 2> needed,
                                Rounding &rounding) {
  double allowed = 0.0;
  for (size_t k = 0; k < 2; ++k) {
    const double length = bend.Lengths[k];
    const double allowance = std::max(length - needed[k], length / 2.0);
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
  rounding.Corner = ParkCorner{bend.Key, bend.At, bend.Along, toward, radius, maxRadius, middle};
  rounding.Segments = bend.Segments;
  for (size_t k = 0; k < 2; ++k) {
    std::vector<ParkPoint> &half = rounding.Halves[k];
    half.clear();
    if (radius > 0.0) {
      appendArc(half, stepFrom(bend.At, toward, centerDepth), radius, middle,
                stepFrom(bend.At, bend.Along[k], tangent));
    } else {
      half.push_back(bend.At);
    }
  }
}

std::span<const ParkPoint> halfOf(const Rounding &rounding, EntityKey segment) {
  return rounding.Halves[rounding.Segments[0] == segment ? 0 : 1];
}

[[gnu::noinline]] void groundLineOf(EntityKey key, const PathSegmentIntent &segment,
                                    std::span<const CarrierPoint> straight,
                                    std::span<const ParkPoint> start,
                                    std::span<const ParkPoint> end, ParkSegment &out) {
  if (start.empty() && end.empty()) {
    copyGrowing(out.Line, straight);
  } else {
    const ParkPoint from{straight.front().X, straight.front().Z};
    const ParkPoint to{straight.back().X, straight.back().Z};
    roundedLine(out.Line, from, to, start, end);
  }
  out.Key = key;
  out.Kind = segment.Kind;
  out.From = segment.From;
  out.To = segment.To;
  out.Width = segment.Width;
  out.FromEdge = segment.FromEdge;
  out.ToEdge = segment.ToEdge;
  out.Furnished = {};
}

PathGround groundOf(const PathLayout &layout) {
  GroundLists lists;
  fillGround(layout, lists);
  return std::move(lists.Ground);
}

void groundOf(const PathLayout &layout, GroundLists &lists) { fillGround(layout, lists); }

} // namespace tpj
