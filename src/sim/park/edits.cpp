#include "sim/park/edits.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/curves.h"
#include "sim/park/internal/layout.h"
#include "sim/park/internal/meetings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

constexpr double HALF_PARK = PARK_SIZE / 2.0;

// A footprint the check compares, with the pose and size it came from.
struct Solid {
  EntityKey Key = NULL_KEY;
  Pose At;
  FootprintSize Size;
  Footprint Shape;
};

// A path's ground line, with half the path's width.
struct Line {
  std::vector<CarrierPoint> Points;
  double HalfWidth = 0.0;
};

std::optional<Solid> solidOf(EntityKey key, const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> shape = footprintOf(pose, size);
  if (!shape) {
    return std::nullopt;
  }
  return Solid{key, pose, size, *shape};
}

// The segment's ground line with half its width, or none when the line is empty.
std::optional<Line> lineOf(const ParkSegment &segment) {
  if (segment.Line.empty()) {
    return std::nullopt;
  }
  return Line{segment.Line, segment.Width / 2.0};
}

// Every entrance's and box's footprint, skipping any with none.
std::vector<Solid> solidsOf(const World &world) {
  std::vector<Solid> solids;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (const std::optional<Solid> solid = solidOf(entrance.Key, entrance.At, ENTRANCE_SIZE)) {
      solids.push_back(*solid);
    }
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<Solid> solid = solidOf(box.Key, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
  return solids;
}

// Every segment's ground line, skipping any that is empty.
std::vector<Line> linesOf(const World &world) {
  std::vector<Line> lines;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (std::optional<Line> line = lineOf(segment)) {
      lines.push_back(std::move(*line));
    }
  }
  return lines;
}

// The intent T the key holds, or null.
template <typename T> const T *findIntent(const World &world, EntityKey key) {
  const entt::entity entity = world.findEntity(key);
  return entity == entt::null ? nullptr : world.Registry.try_get<T>(entity);
}

// Whether the kind is one of its enum's values.
template <typename Kind> bool isKnown(Kind kind) {
  return static_cast<size_t>(kind) < enumNames(kind).size();
}

bool leavesPark(const Solid &solid) {
  constexpr double LIMIT = HALF_PARK + CONTACT_TOLERANCE;
  return std::ranges::any_of(solid.Shape.Corners, [](const ParkPoint &corner) {
    return std::abs(corner.X) > LIMIT || std::abs(corner.Z) > LIMIT;
  });
}

bool leavesPark(const Line &line) {
  const double limit = (HALF_PARK - line.HalfWidth) + CONTACT_TOLERANCE;
  return std::ranges::any_of(line.Points, [limit](const CarrierPoint &point) {
    return std::abs(point.X) > limit || std::abs(point.Z) > limit;
  });
}

// Whether a corner of the piece lies past the park's edge by more than CONTACT_TOLERANCE.
bool leavesPark(const SurfacePiece &piece) {
  const double limit = HALF_PARK + CONTACT_TOLERANCE;
  return std::ranges::any_of(pieceCorners(piece), [limit](const ParkPoint &corner) {
    return std::abs(corner.X) > limit || std::abs(corner.Z) > limit;
  });
}

// The least and greatest projections of the corners onto the axis.
std::pair<double, double> extentOf(std::span<const ParkPoint> corners, const ParkPoint &axis) {
  const auto project = [&axis](const ParkPoint &corner) {
    return corner.X * axis.X + corner.Z * axis.Z;
  };
  double least = project(corners[0]);
  double greatest = least;
  for (const ParkPoint &corner : corners) {
    least = std::min(least, project(corner));
    greatest = std::max(greatest, project(corner));
  }
  return {least, greatest};
}

// The least and greatest projections of the footprint's corners onto the axis.
std::pair<double, double> extentAlong(const Footprint &footprint, const ParkPoint &axis) {
  return extentOf(footprint.Corners, axis);
}

// Whether the two footprints' extents along the axis share more than CONTACT_TOLERANCE.
bool overlapAlong(const Footprint &a, const Footprint &b, const ParkPoint &axis) {
  const auto [aLeast, aGreatest] = extentAlong(a, axis);
  const auto [bLeast, bGreatest] = extentAlong(b, axis);
  return std::min(aGreatest, bGreatest) - std::max(aLeast, bLeast) > CONTACT_TOLERANCE;
}

// The separating axis test: no axis of either footprint separates them.
bool overlaps(const Solid &a, const Solid &b) {
  return overlapAlong(a.Shape, b.Shape, a.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, a.Shape.Right) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Right);
}

// Edges shorter than this, in meters, give no axis to the separating axis test.
constexpr double LEAST_EDGE = 1e-9;

// Whether the piece and the solid's footprint overlap by more than CONTACT_TOLERANCE: the
// separating axis test over the footprint's axes and the normal of each of the piece's edges.
bool meets(const SurfacePiece &piece, const Solid &solid) {
  const std::span<const ParkPoint> corners = pieceCorners(piece);
  const auto overlapOn = [&corners, &solid](const ParkPoint &axis) {
    const auto [pieceLeast, pieceGreatest] = extentOf(corners, axis);
    const auto [solidLeast, solidGreatest] = extentOf(solid.Shape.Corners, axis);
    return std::min(pieceGreatest, solidGreatest) - std::max(pieceLeast, solidLeast) >
           CONTACT_TOLERANCE;
  };
  if (!overlapOn(solid.Shape.Forward) || !overlapOn(solid.Shape.Right)) {
    return false;
  }
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint &from = corners[i];
    const ParkPoint &to = corners[(i + 1) % corners.size()];
    const double dx = to.X - from.X;
    const double dz = to.Z - from.Z;
    const double length = std::sqrt(dx * dx + dz * dz);
    if (length >= LEAST_EDGE && !overlapOn(ParkPoint{-dz / length, dx / length})) {
      return false;
    }
  }
  return true;
}

// Whether the solid stays in the park and conflicts with no piece of paving and no other solid. A
// solid holding the same key is the one being moved, so it is skipped.
bool fits(const Solid &solid, const std::vector<Solid> &solids,
          const std::vector<SurfacePiece> &pieces) {
  if (leavesPark(solid)) {
    return false;
  }
  if (std::ranges::any_of(solids, [&solid](const Solid &other) {
        return other.Key != solid.Key && overlaps(solid, other);
      })) {
    return false;
  }
  return std::ranges::none_of(pieces,
                              [&solid](const SurfacePiece &piece) { return meets(piece, solid); });
}

// Whether the piece meets no solid.
bool fits(const SurfacePiece &piece, const std::vector<Solid> &solids) {
  return std::ranges::none_of(solids, [&piece](const Solid &solid) { return meets(piece, solid); });
}

// A point an edit's segments run through: an existing path point, or, when Existing is NULL_KEY, a
// point the edit adds.
struct PlanPoint {
  ParkPoint At;
  EntityKey Existing = NULL_KEY;
  // The radius a point the edit adds takes.
  double Radius = DEFAULT_CORNER_RADIUS;
};

// A plan point on a segment, Along of the way from the segment's first end to its second.
struct PlacedPoint {
  size_t Point = 0;
  double Along = 0.0;
};

// A segment of the world that an edit leaves in place but splits, and the plan points it splits
// at.
struct Split {
  EntityKey Segment = NULL_KEY;
  std::vector<PlacedPoint> At;
};

// A corner of the world that an edit splits at places on its arc: the corner, its segment along its
// first direction and then the other, and the plan points at the places, each with its fraction
// along the arc. Points holds the plan points finishPlan adds where the lines touching the arc
// meet, in order along it, the last on the second segment's line.
struct BendSplit {
  ParkCorner Corner;
  std::array<EntityKey, 2> Segments{};
  std::vector<PlacedPoint> At;
  std::vector<size_t> Points;
};

// A segment an edit adds or reshapes: the one Key holds, or, when Key is NULL_KEY, a new one. It
// runs from plan point From to plan point To and splits at the points it runs Through, in order of
// Along. FromEdge and ToEdge are its edge radii at From and To. FromAdded and ToAdded say whether
// the edit adds that end at a path point that already has segments, rather than keeping or moving
// an end that was there.
struct PlanLine {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  double Width = 0.0;
  size_t From = 0;
  size_t To = 0;
  std::vector<PlacedPoint> Through;
  double FromEdge = DEFAULT_EDGE_RADIUS;
  double ToEdge = DEFAULT_EDGE_RADIUS;
  bool FromAdded = false;
  bool ToAdded = false;
};

// What an accepted edit makes. Points holds the points its lines and splits run through: for
// AddPath, each stop's point in stop order, a RepeatStop sharing its earlier stop's, then each
// point a meeting adds, in the order found. Stops gives each AddPath stop's entry in Points.
// Splits holds the segments the edit leaves in place but splits, in ascending key order, each
// with its points in order of Along. Lines holds the segments it adds or reshapes: for AddPath,
// the one from stop i to stop i + 1 at index i. Crossings holds the entries in Points where a
// meeting splits a segment, in the order found. Destroyed holds the keys it destroys, segments
// first. Moves holds each existing point that moves, with where it moves to. BendSplits holds the
// corners the edit splits, in ascending key order of their points. If Rounded is not NULL_KEY,
// that existing point's radius becomes RoundedTo. Curves holds the radii written last, so that
// the edit keeps every curve. If Edged is not NULL_KEY, that existing
// segment's edge radius at its From end, when EdgedAtFrom, or else at its To end, becomes EdgedTo.
struct EditPlan {
  std::vector<PlanPoint> Points;
  std::vector<size_t> Stops;
  std::vector<Split> Splits;
  std::vector<PlanLine> Lines;
  std::vector<size_t> Crossings;
  std::vector<EntityKey> Destroyed;
  std::vector<std::pair<EntityKey, ParkPoint>> Moves;
  std::vector<BendSplit> BendSplits;
  EntityKey Rounded = NULL_KEY;
  double RoundedTo = 0.0;
  EntityKey Edged = NULL_KEY;
  bool EdgedAtFrom = false;
  double EdgedTo = 0.0;
  CurveWrites Curves;
};

// Whether the key holds a path point that some segment uses, every such segment of the kind.
bool isPointOfKind(const World &world, const std::vector<ParkSegment> &segments, EntityKey point,
                   PathKind kind) {
  if (findIntent<PathPointIntent>(world, point) == nullptr) {
    return false;
  }
  bool used = false;
  for (const ParkSegment &segment : segments) {
    if (segment.From == point || segment.To == point) {
      if (segment.Kind != kind) {
        return false;
      }
      used = true;
    }
  }
  return used;
}

// The segment the key holds, or null. The segments are in ascending key order.
const ParkSegment *findSegment(const std::vector<ParkSegment> &segments, EntityKey key) {
  const auto found = std::ranges::lower_bound(segments, key, {}, &ParkSegment::Key);
  return found != segments.end() && found->Key == key ? &*found : nullptr;
}

bool endsAt(const ParkSegment &segment, EntityKey point) {
  return segment.From == point || segment.To == point;
}

// The segments that end at the point, in ascending key order.
std::vector<ParkSegment> segmentsAt(const std::vector<ParkSegment> &segments, EntityKey point) {
  std::vector<ParkSegment> at;
  std::ranges::copy_if(segments, std::back_inserter(at),
                       [point](const ParkSegment &segment) { return endsAt(segment, point); });
  return at;
}

// The segments that don't end at the point, in ascending key order.
std::vector<ParkSegment> segmentsNotAt(const std::vector<ParkSegment> &segments, EntityKey point) {
  std::vector<ParkSegment> notAt;
  std::ranges::copy_if(segments, std::back_inserter(notAt),
                       [point](const ParkSegment &segment) { return !endsAt(segment, point); });
  return notAt;
}

// The segment's edge radius at the point, one of its two.
double edgeAt(const ParkSegment &segment, EntityKey point) {
  return segment.From == point ? segment.FromEdge : segment.ToEdge;
}

// The corner the path point makes, when it is rounded with a radius above 0, or none.
std::optional<ParkCorner> roundedCorner(const World &world, EntityKey point) {
  for (const ParkCorner &corner : parkCorners(world)) {
    if (corner.Point == point && corner.Radius > 0.0) {
      return corner;
    }
  }
  return std::nullopt;
}

// The segment's end that is not the point.
EntityKey farEnd(const ParkSegment &segment, EntityKey point) {
  return segment.From == point ? segment.To : segment.From;
}

// The target as a stop, for stopPosition.
PathStop stopOf(const PointTarget &target) {
  return std::visit([](const auto &stop) { return PathStop{stop}; }, target);
}

// The position of the path point the key holds, or none.
std::optional<ParkPoint> positionOf(const World &world, EntityKey key) {
  const PathPointIntent *point = findIntent<PathPointIntent>(world, key);
  return point == nullptr ? std::nullopt : std::optional<ParkPoint>(point->At);
}

// The two segments a deleted point merges, the lower key first: exactly two segments use the
// point, and their far ends are different points other than it.
std::optional<std::pair<ParkSegment, ParkSegment>> mergingPair(const std::vector<ParkSegment> &at,
                                                               EntityKey point) {
  if (at.size() != 2) {
    return std::nullopt;
  }
  const EntityKey lowFar = farEnd(at[0], point);
  const EntityKey highFar = farEnd(at[1], point);
  if (lowFar == highFar || lowFar == point || highFar == point) {
    return std::nullopt;
  }
  return std::pair{at[0], at[1]};
}

// Whether the OnBend names a place on the arc of a corner with a radius above 0 whose two segments
// both have the kind, or one kind when kind is none.
bool isBendOfKind(const World &world, const std::vector<ParkSegment> &segments, const OnBend &on,
                  std::optional<PathKind> kind) {
  const std::vector<ParkSegment> at = segmentsAt(segments, on.Point);
  return stopPosition(world, on).has_value() && at.size() == 2 && at[0].Kind == at[1].Kind &&
         (!kind || at[0].Kind == *kind);
}

// Whether the stop names what it lands on, of the kind.
bool isStopOfKind(const World &world, const std::vector<ParkSegment> &segments,
                  const PathStop &stop, PathKind kind) {
  if (const auto *on = std::get_if<OnPoint>(&stop)) {
    return isPointOfKind(world, segments, on->Point, kind);
  }
  if (const auto *on = std::get_if<OnSegment>(&stop)) {
    const ParkSegment *segment = findSegment(segments, on->Segment);
    return std::isfinite(on->Along) && on->Along > 0.0 && on->Along < 1.0 && segment != nullptr &&
           segment->Kind == kind && !segment->Line.empty();
  }
  if (const auto *on = std::get_if<OnDrawnSegment>(&stop)) {
    return std::isfinite(on->Along) && on->Along > 0.0 && on->Along < 1.0;
  }
  if (const auto *on = std::get_if<OnBend>(&stop)) {
    return isBendOfKind(world, segments, *on, kind);
  }
  return true;
}

// The entry in the plan's points for the existing path point, added at its position if there is
// none.
size_t pointEntry(EditPlan &plan, EntityKey key, ParkPoint at) {
  const auto found = std::ranges::find(plan.Points, key, &PlanPoint::Existing);
  if (found != plan.Points.end()) {
    return static_cast<size_t>(found - plan.Points.begin());
  }
  plan.Points.push_back(PlanPoint{at, key});
  return plan.Points.size() - 1;
}

// Places the point on a segment's list, unless the list holds it already. Whether it placed it.
bool place(std::vector<PlacedPoint> &on, size_t point, double along) {
  if (std::ranges::find(on, point, &PlacedPoint::Point) != on.end()) {
    return false;
  }
  on.push_back(PlacedPoint{point, along});
  return true;
}

// The points the world's segment splits at, starting none.
std::vector<PlacedPoint> &splitOf(EditPlan &plan, EntityKey segment) {
  const auto found = std::ranges::find(plan.Splits, segment, &Split::Segment);
  if (found != plan.Splits.end()) {
    return found->At;
  }
  plan.Splits.push_back(Split{segment, {}});
  return plan.Splits.back().At;
}

// The places a plan splits the corner at the point at, the corner added to the plan's bend splits
// if it isn't there yet. The corner is rounded with a radius above 0, and exactly two segments
// end at its point.
std::vector<PlacedPoint> &bendSplitOf(const World &world, const std::vector<ParkSegment> &segments,
                                      EditPlan &plan, EntityKey point) {
  const auto found = std::ranges::find_if(
      plan.BendSplits, [point](const BendSplit &bend) { return bend.Corner.Point == point; });
  if (found != plan.BendSplits.end()) {
    return found->At;
  }
  const std::vector<ParkSegment> at = segmentsAt(segments, point);
  // The corner's first direction runs along its lower-keyed segment, the first of these. Callers
  // have checked that the corner is there.
  plan.BendSplits.push_back(BendSplit{
      roundedCorner(world, point).value_or(ParkCorner{}), {at[0].Key, at[1].Key}, {}, {}});
  return plan.BendSplits.back().At;
}

void markCrossing(EditPlan &plan, size_t point) {
  if (std::ranges::find(plan.Crossings, point) == plan.Crossings.end()) {
    plan.Crossings.push_back(point);
  }
}

// The point for a crossing: a point the plan already has within MIN_SEGMENT_LENGTH of it, as where
// a path doubles back across a segment, or else a new one.
size_t crossingPoint(EditPlan &plan, ParkPoint at) {
  const auto near = std::ranges::find_if(plan.Points, [at](const PlanPoint &point) {
    const double dx = point.At.X - at.X;
    const double dz = point.At.Z - at.Z;
    return dx * dx + dz * dz <= MIN_SEGMENT_LENGTH * MIN_SEGMENT_LENGTH;
  });
  if (near != plan.Points.end()) {
    return static_cast<size_t>(near - plan.Points.begin());
  }
  plan.Points.push_back(PlanPoint{at, NULL_KEY});
  return plan.Points.size() - 1;
}

// The plan's stop points, a line between each two stops, and the points of OnSegment and
// OnDrawnSegment stops on the segments they name.
EditPlan planStops(const World &world, const std::vector<ParkSegment> &segments,
                   const AddPath &command, const std::vector<ParkPoint> &positions) {
  EditPlan plan;
  std::vector<std::vector<PlacedPoint>> drawn(command.Stops.size() - 1);
  for (size_t i = 0; i < command.Stops.size(); ++i) {
    const PathStop &stop = command.Stops[i];
    if (const auto *on = std::get_if<OnPoint>(&stop)) {
      plan.Stops.push_back(pointEntry(plan, on->Point, positions[i]));
    } else if (const auto *repeat = std::get_if<RepeatStop>(&stop)) {
      plan.Stops.push_back(plan.Stops[repeat->Stop]);
    } else {
      plan.Points.push_back(PlanPoint{positions[i], NULL_KEY});
      plan.Stops.push_back(plan.Points.size() - 1);
    }
    if (const auto *onSegment = std::get_if<OnSegment>(&stop)) {
      place(splitOf(plan, onSegment->Segment), plan.Stops[i], onSegment->Along);
    } else if (const auto *onDrawn = std::get_if<OnDrawnSegment>(&stop)) {
      place(drawn[onDrawn->Stop], plan.Stops[i], onDrawn->Along);
    } else if (const auto *onBend = std::get_if<OnBend>(&stop)) {
      place(bendSplitOf(world, segments, plan, onBend->Point), plan.Stops[i], onBend->Along);
    }
  }
  for (size_t i = 0; i + 1 < command.Stops.size(); ++i) {
    PlanLine line{NULL_KEY,      command.Kind,      pathWidth(command.Kind),
                  plan.Stops[i], plan.Stops[i + 1], std::move(drawn[i])};
    line.FromAdded = plan.Points[line.From].Existing != NULL_KEY;
    line.ToAdded = plan.Points[line.To].Existing != NULL_KEY;
    plan.Lines.push_back(std::move(line));
  }
  return plan;
}

// The ends of the plan's line k.
std::pair<ParkPoint, ParkPoint> lineEnds(const EditPlan &plan, size_t k) {
  return {plan.Points[plan.Lines[k].From].At, plan.Points[plan.Lines[k].To].At};
}

// Whether every line of the plan has a ground line between its ends.
bool linesHaveGround(const EditPlan &plan) {
  for (size_t k = 0; k < plan.Lines.size(); ++k) {
    const auto [from, to] = lineEnds(plan, k);
    if (segmentLine(from, to).empty()) {
      return false;
    }
  }
  return true;
}

double distanceOf(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// How far from the point its corner's arc touches each of its segments, or 0 where the point makes
// no corner rounded with a radius above 0. The corners are in ascending key order of their points.
double tangentAt(const std::vector<ParkCorner> &corners, EntityKey point) {
  const auto found = std::ranges::lower_bound(corners, point, {}, &ParkCorner::Point);
  if (found == corners.end() || found->Point != point || !(found->Radius > 0.0)) {
    return 0.0;
  }
  const ParkPoint touch = arcOf(*found).Touch[0];
  return distanceOf(touch, found->At);
}

// Adds the point where the plan's line k meets the straight part of the world's segment, if they
// meet. The straight part runs between where the arcs at its ends touch it.
void meetWorldSegment(const World &world, EditPlan &plan, size_t k, const ParkSegment &segment,
                      const std::vector<ParkCorner> &corners) {
  const auto [a0, a1] = lineEnds(plan, k);
  // The segment has a ground line, so both of its ends are path points.
  const ParkPoint b0 = findIntent<PathPointIntent>(world, segment.From)->At;
  const ParkPoint b1 = findIntent<PathPointIntent>(world, segment.To)->At;
  const double length = distanceOf(b0, b1);
  const std::array<double, 2> tangents{tangentAt(corners, segment.From),
                                       tangentAt(corners, segment.To)};
  const ParkPoint along{(b1.X - b0.X) / length, (b1.Z - b0.Z) / length};
  const ParkPoint s0{b0.X + along.X * tangents[0], b0.Z + along.Z * tangents[0]};
  const ParkPoint s1{b1.X - along.X * tangents[1], b1.Z - along.Z * tangents[1]};
  const double straight = length - tangents[0] - tangents[1];
  if (straight < MIN_SEGMENT_LENGTH) {
    return;
  }
  const std::optional<Meeting> meeting = meetingOf(a0, a1, s0, s1);
  if (!meeting) {
    return;
  }
  // Where the meeting lies along the whole line between the segment's points.
  const double alongB = (tangents[0] + meeting->AlongB * straight) / length;
  size_t point = 0;
  bool placed = false;
  if (meeting->Kind == MeetingKind::AEndOnB) {
    point = meeting->End == 0 ? plan.Lines[k].From : plan.Lines[k].To;
    placed = place(splitOf(plan, segment.Key), point, alongB);
  } else if (meeting->Kind == MeetingKind::BEndOnA && tangents[meeting->End] == 0.0) {
    point = pointEntry(plan, meeting->End == 0 ? segment.From : segment.To, meeting->At);
    placed = place(plan.Lines[k].Through, point, meeting->AlongA);
  } else {
    // A crossing, or an end of the straight part where an arc touches it, which is no point.
    point = crossingPoint(plan, meeting->At);
    placed = place(plan.Lines[k].Through, point, meeting->AlongA);
    placed = place(splitOf(plan, segment.Key), point, alongB) || placed;
  }
  if (placed) {
    markCrossing(plan, point);
  }
}

// Adds the points where the plan's line k meets the arc of the world's corner, whose two segments
// the plan leaves in place. A place within MIN_SEGMENT_LENGTH of where the arc touches a segment
// splits that segment there instead.
void meetCorner(const World &world, EditPlan &plan, size_t k, const ParkCorner &corner,
                const std::vector<ParkSegment> &segments) {
  const auto [a0, a1] = lineEnds(plan, k);
  const CornerArc arc = arcOf(corner);
  const ParkPoint d{a1.X - a0.X, a1.Z - a0.Z};
  const ParkPoint f{a0.X - arc.Center.X, a0.Z - arc.Center.Z};
  const double a = d.X * d.X + d.Z * d.Z;
  const double b = 2.0 * (f.X * d.X + f.Z * d.Z);
  const double c = f.X * f.X + f.Z * f.Z - arc.Radius * arc.Radius;
  const double disc = b * b - 4.0 * a * c;
  if (!(disc > 0.0)) {
    return;
  }
  const double length = std::sqrt(a);
  const double tolerance = MIN_SEGMENT_LENGTH / length;
  const ParkPoint middle{corner.Middle.X - arc.Center.X, corner.Middle.Z - arc.Center.Z};
  const auto towardMiddle = [&](ParkPoint at) {
    return (at.X - arc.Center.X) * middle.X + (at.Z - arc.Center.Z) * middle.Z;
  };
  const double spanToward = towardMiddle(arc.Touch[0]);
  const std::vector<ParkSegment> at = segmentsAt(segments, corner.Point);
  for (const double sign : {-1.0, 1.0}) {
    const double s = (-b + sign * std::sqrt(disc)) / (2.0 * a);
    if (s < -tolerance || s > 1.0 + tolerance) {
      continue;
    }
    const ParkPoint x{a0.X + d.X * s, a0.Z + d.Z * s};
    if (towardMiddle(x) < spanToward) {
      continue;
    }
    const bool atFrom = s * length <= MIN_SEGMENT_LENGTH;
    const bool atTo = (1.0 - s) * length <= MIN_SEGMENT_LENGTH;
    // The corner's segment whose touch the place lies at, or 2 for neither.
    size_t touched = 2;
    for (size_t t = 0; t < 2; ++t) {
      if (distanceOf(x, arc.Touch[t]) <= MIN_SEGMENT_LENGTH) {
        touched = t;
      }
    }
    const ParkPoint where = touched < 2 ? arc.Touch[touched] : x;
    size_t point = 0;
    bool placed = false;
    if (atFrom || atTo) {
      point = atFrom ? plan.Lines[k].From : plan.Lines[k].To;
    } else {
      point = crossingPoint(plan, where);
      placed = place(plan.Lines[k].Through, point, std::clamp(s, 0.0, 1.0));
    }
    if (touched < 2) {
      // The touched segment is the corner's first, the lower-keyed, or its second.
      const ParkSegment &segment = at[touched];
      const ParkPoint from = findIntent<PathPointIntent>(world, segment.From)->At;
      const ParkPoint to = findIntent<PathPointIntent>(world, segment.To)->At;
      const double along = distanceOf(from, where) / distanceOf(from, to);
      placed = place(splitOf(plan, segment.Key), point, along) || placed;
    } else {
      placed = place(bendSplitOf(world, segments, plan, corner.Point), point,
                     bendAlong(corner, where)) ||
               placed;
    }
    if (placed) {
      markCrossing(plan, point);
    }
  }
}

// Adds the point where the plan's earlier line m meets its line k, if they meet.
void meetLines(EditPlan &plan, size_t m, size_t k) {
  const auto [a0, a1] = lineEnds(plan, m);
  const auto [b0, b1] = lineEnds(plan, k);
  const std::optional<Meeting> meeting = meetingOf(a0, a1, b0, b1);
  if (!meeting) {
    return;
  }
  size_t point = 0;
  bool placed = false;
  if (meeting->Kind == MeetingKind::AEndOnB) {
    point = meeting->End == 0 ? plan.Lines[m].From : plan.Lines[m].To;
    placed = place(plan.Lines[k].Through, point, meeting->AlongB);
  } else if (meeting->Kind == MeetingKind::BEndOnA) {
    point = meeting->End == 0 ? plan.Lines[k].From : plan.Lines[k].To;
    placed = place(plan.Lines[m].Through, point, meeting->AlongA);
  } else {
    point = crossingPoint(plan, meeting->At);
    placed = place(plan.Lines[m].Through, point, meeting->AlongA);
    placed = place(plan.Lines[k].Through, point, meeting->AlongB) || placed;
  }
  if (placed) {
    markCrossing(plan, point);
  }
}

// Adds the points where each of the plan's lines meets a segment of its kind that the edit leaves
// in place, in ascending key order, then the arc of each corner of its kind the edit leaves in
// place, and then each of the plan's earlier lines. A segment of the world meets along the straight
// part of its ground line.
void addMeetings(const World &world, EditPlan &plan, const std::vector<ParkSegment> &kept) {
  const std::vector<ParkCorner> corners = parkCorners(world);
  // The corners that meet: rounded, with both of their segments kept, at a point the plan neither
  // joins nor moves.
  std::vector<ParkCorner> meeting;
  for (const ParkCorner &corner : corners) {
    const bool joined = std::ranges::any_of(
        plan.Points, [&corner](const PlanPoint &point) { return point.Existing == corner.Point; });
    const bool moved = std::ranges::any_of(
        plan.Moves, [&corner](const auto &move) { return move.first == corner.Point; });
    if (corner.Radius > 0.0 && !joined && !moved && segmentsAt(kept, corner.Point).size() == 2) {
      meeting.push_back(corner);
    }
  }
  for (size_t k = 0; k < plan.Lines.size(); ++k) {
    for (const ParkSegment &segment : kept) {
      if (segment.Kind == plan.Lines[k].Kind && !segment.Line.empty()) {
        meetWorldSegment(world, plan, k, segment, corners);
      }
    }
    for (const ParkCorner &corner : meeting) {
      const std::vector<ParkSegment> at = segmentsAt(kept, corner.Point);
      if (at[0].Kind == plan.Lines[k].Kind && at[1].Kind == plan.Lines[k].Kind) {
        meetCorner(world, plan, k, corner, kept);
      }
    }
    for (size_t m = 0; m < k; ++m) {
      meetLines(plan, m, k);
    }
  }
}

// Where the line through the point perpendicular to normal meets the line from from along the
// unit direction.
ParkPoint meetAlong(ParkPoint point, ParkPoint normal, ParkPoint from, ParkPoint direction) {
  const double u = ((point.X - from.X) * normal.X + (point.Z - from.Z) * normal.Z) /
                   (direction.X * normal.X + direction.Z * normal.Z);
  return {from.X + direction.X * u, from.Z + direction.Z * u};
}

// Where the lines through two points, each perpendicular to its normal, meet.
ParkPoint meetLines(ParkPoint a, ParkPoint normalA, ParkPoint b, ParkPoint normalB) {
  const double ca = a.X * normalA.X + a.Z * normalA.Z;
  const double cb = b.X * normalB.X + b.Z * normalB.Z;
  const double det = normalA.X * normalB.Z - normalA.Z * normalB.X;
  return {(ca * normalB.Z - cb * normalA.Z) / det, (normalA.X * cb - normalB.X * ca) / det};
}

// Adds the corner points of the bend split: the corner's own point moves to where the line touching
// the arc at the first place meets the first segment's line, a new point lies where the lines
// touching it at each two places in a row meet, and another where the line touching it at the last
// place meets the second segment's line. Each takes the radius the corner was rounded with.
void placeBendPoints(EditPlan &plan, BendSplit &bend) {
  const ParkCorner &corner = bend.Corner;
  const CornerArc arc = arcOf(corner);
  const auto normalAt = [&](size_t i) {
    const ParkPoint &at = plan.Points[bend.At[i].Point].At;
    return ParkPoint{at.X - arc.Center.X, at.Z - arc.Center.Z};
  };
  const auto placeAt = [&](size_t i) { return plan.Points[bend.At[i].Point].At; };
  plan.Moves.emplace_back(corner.Point,
                          meetAlong(placeAt(0), normalAt(0), corner.At, corner.Along[0]));
  const size_t last = bend.At.size() - 1;
  for (size_t i = 1; i <= last; ++i) {
    plan.Points.push_back(
        PlanPoint{meetLines(placeAt(i - 1), normalAt(i - 1), placeAt(i), normalAt(i)), NULL_KEY,
                  corner.Radius});
    bend.Points.push_back(plan.Points.size() - 1);
  }
  plan.Points.push_back(
      PlanPoint{meetAlong(placeAt(last), normalAt(last), corner.At, corner.Along[1]), NULL_KEY,
                corner.Radius});
  bend.Points.push_back(plan.Points.size() - 1);
}

// The plan with its meetings sorted and its bend splits' corner points placed, or none when a
// corner it splits is also a point it joins or a segment it reshapes or destroys.
std::optional<EditPlan> finishPlan(EditPlan plan) {
  for (Split &split : plan.Splits) {
    std::ranges::sort(split.At, {}, &PlacedPoint::Along);
  }
  for (PlanLine &line : plan.Lines) {
    std::ranges::sort(line.Through, {}, &PlacedPoint::Along);
  }
  std::ranges::sort(plan.Splits, {}, &Split::Segment);
  std::ranges::sort(plan.BendSplits, {}, [](const BendSplit &bend) { return bend.Corner.Point; });
  for (BendSplit &bend : plan.BendSplits) {
    const EntityKey point = bend.Corner.Point;
    const bool joined = std::ranges::any_of(
        plan.Points, [point](const PlanPoint &planned) { return planned.Existing == point; });
    const bool reshaped = std::ranges::any_of(plan.Lines, [&bend](const PlanLine &line) {
      return line.Key == bend.Segments[0] || line.Key == bend.Segments[1];
    });
    const bool destroyed = std::ranges::any_of(plan.Destroyed, [&bend, point](EntityKey key) {
      return key == point || key == bend.Segments[0] || key == bend.Segments[1];
    });
    if (joined || reshaped || destroyed) {
      return std::nullopt;
    }
    std::ranges::sort(bend.At, {}, &PlacedPoint::Along);
    placeBendPoints(plan, bend);
  }
  return plan;
}

// The plan of an AddPath that names what it acts on, or none.
std::optional<EditPlan> planPath(const World &world, const AddPath &command) {
  if (!isKnown(command.Kind) || command.Stops.size() < 2) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, command);
  if (!positions || !std::ranges::all_of(command.Stops, [&](const PathStop &stop) {
        return isStopOfKind(world, segments, stop, command.Kind);
      })) {
    return std::nullopt;
  }
  EditPlan plan = planStops(world, segments, command, *positions);
  if (!linesHaveGround(plan)) {
    return std::nullopt;
  }
  addMeetings(world, plan, segments);
  return finishPlan(std::move(plan));
}

// Whether the MovePoint's target names what it lands on, of the kind, other than the moved point
// and the segments at it.
bool isTargetOfKind(const World &world, const std::vector<ParkSegment> &segments,
                    const MovePoint &command, PathKind kind) {
  if (const auto *on = std::get_if<OnPoint>(&command.To)) {
    return on->Point != command.Point && isPointOfKind(world, segments, on->Point, kind);
  }
  if (const auto *on = std::get_if<OnSegment>(&command.To)) {
    return isStopOfKind(world, segments, *on, kind) &&
           !endsAt(*findSegment(segments, on->Segment), command.Point);
  }
  if (const auto *on = std::get_if<OnBend>(&command.To)) {
    return isBendOfKind(world, segments, *on, kind) &&
           std::ranges::none_of(segmentsAt(segments, on->Point), [&](const ParkSegment &segment) {
             return endsAt(segment, command.Point);
           });
  }
  return true;
}

// The plan of a MovePoint that names what it acts on, or none.
std::optional<EditPlan> planMove(const World &world, const MovePoint &command) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  const std::vector<ParkSegment> at = segmentsAt(segments, command.Point);
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr || at.empty() ||
      std::ranges::any_of(
          at, [&at](const ParkSegment &segment) { return segment.Kind != at.front().Kind; })) {
    return std::nullopt;
  }
  const std::optional<ParkPoint> position = stopPosition(world, stopOf(command.To));
  if (!isTargetOfKind(world, segments, command, at.front().Kind) || !position) {
    return std::nullopt;
  }
  const auto *onPoint = std::get_if<OnPoint>(&command.To);
  EditPlan plan;
  size_t moved = 0;
  if (onPoint != nullptr) {
    moved = pointEntry(plan, onPoint->Point, *position);
  } else {
    plan.Points.push_back(PlanPoint{*position, command.Point});
    plan.Moves.emplace_back(command.Point, *position);
  }
  for (const ParkSegment &segment : at) {
    const EntityKey far = farEnd(segment, command.Point);
    if (onPoint != nullptr && far == onPoint->Point) {
      plan.Destroyed.push_back(segment.Key);
      continue;
    }
    const std::optional<ParkPoint> farAt = positionOf(world, far);
    if (far == command.Point || !farAt) {
      return std::nullopt;
    }
    const size_t farEntry = pointEntry(plan, far, *farAt);
    plan.Lines.push_back(PlanLine{segment.Key,
                                  segment.Kind,
                                  segment.Width,
                                  segment.From == command.Point ? moved : farEntry,
                                  segment.To == command.Point ? moved : farEntry,
                                  {},
                                  segment.FromEdge,
                                  segment.ToEdge});
    // Ends that arrive at the target point are added there.
    plan.Lines.back().FromAdded = onPoint != nullptr && segment.From == command.Point;
    plan.Lines.back().ToAdded = onPoint != nullptr && segment.To == command.Point;
  }
  const std::vector<ParkSegment> kept = segmentsNotAt(segments, command.Point);
  if (onPoint != nullptr) {
    plan.Destroyed.push_back(command.Point);
    // A target left with no segment goes too.
    const bool targetUsed =
        !plan.Lines.empty() || std::ranges::any_of(kept, [onPoint](const ParkSegment &segment) {
          return endsAt(segment, onPoint->Point);
        });
    if (!targetUsed) {
      plan.Destroyed.push_back(onPoint->Point);
    }
  }
  if (const auto *onSegment = std::get_if<OnSegment>(&command.To)) {
    place(splitOf(plan, onSegment->Segment), moved, onSegment->Along);
  } else if (const auto *onBend = std::get_if<OnBend>(&command.To)) {
    place(bendSplitOf(world, segments, plan, onBend->Point), moved, onBend->Along);
  }
  if (!linesHaveGround(plan)) {
    return std::nullopt;
  }
  addMeetings(world, plan, kept);
  return finishPlan(std::move(plan));
}

// The plan of an InsertPoint that names what it acts on, or none. The parts lie along the segment,
// or along the arc and the lines touching it, which have met everything they meet already, so the
// plan finds no meetings.
std::optional<EditPlan> planInsert(const World &world, const InsertPoint &command) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  if (const auto *onBend = std::get_if<OnBend>(&command.At)) {
    if (!isBendOfKind(world, segments, *onBend, std::nullopt)) {
      return std::nullopt;
    }
    const std::optional<ParkPoint> at = stopPosition(world, *onBend);
    if (!at) {
      return std::nullopt;
    }
    EditPlan plan;
    plan.Points.push_back(PlanPoint{*at, NULL_KEY});
    place(bendSplitOf(world, segments, plan, onBend->Point), 0, onBend->Along);
    return finishPlan(std::move(plan));
  }
  const auto *on = std::get_if<OnSegment>(&command.At);
  const ParkSegment *segment = findSegment(segments, on->Segment);
  if (segment == nullptr || segment->Line.empty() || !std::isfinite(on->Along) ||
      on->Along <= 0.0 || on->Along >= 1.0) {
    return std::nullopt;
  }
  const std::optional<ParkPoint> at = stopPosition(world, *on);
  if (!at) {
    return std::nullopt;
  }
  EditPlan plan;
  plan.Points.push_back(PlanPoint{*at, NULL_KEY});
  place(splitOf(plan, on->Segment), 0, on->Along);
  return finishPlan(std::move(plan));
}

// The plan of a RemovePoint whose point has two segments it can merge, or none.
std::optional<EditPlan> planRemove(const World &world, const RemovePoint &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  const auto pair = mergingPair(segmentsAt(segments, command.Point), command.Point);
  if (!pair) {
    return std::nullopt;
  }
  const auto &[low, high] = *pair;
  const EntityKey lowFar = farEnd(low, command.Point);
  const EntityKey highFar = farEnd(high, command.Point);
  const std::optional<ParkPoint> lowAt = positionOf(world, lowFar);
  const std::optional<ParkPoint> highAt = positionOf(world, highFar);
  if (!lowAt || !highAt) {
    return std::nullopt;
  }
  EditPlan plan;
  const size_t lowEntry = pointEntry(plan, lowFar, *lowAt);
  const size_t highEntry = pointEntry(plan, highFar, *highAt);
  // The end at the point takes the high segment's far end, with its edge radius there.
  const double highEdge = edgeAt(high, highFar);
  plan.Lines.push_back(PlanLine{low.Key,
                                low.Kind,
                                std::max(low.Width, high.Width),
                                low.From == command.Point ? highEntry : lowEntry,
                                low.To == command.Point ? highEntry : lowEntry,
                                {},
                                low.From == command.Point ? highEdge : low.FromEdge,
                                low.To == command.Point ? highEdge : low.ToEdge});
  plan.Destroyed = {high.Key, command.Point};
  if (!linesHaveGround(plan)) {
    return std::nullopt;
  }
  addMeetings(world, plan, segmentsNotAt(segments, command.Point));
  return finishPlan(std::move(plan));
}

// The plan of a DeleteSegment that names a segment: the segment, and each of its points no other
// segment uses.
std::optional<EditPlan> planDeleteSegment(const World &world, const DeleteSegment &command) {
  if (findIntent<PathSegmentIntent>(world, command.Segment) == nullptr) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  const ParkSegment &segment = *findSegment(segments, command.Segment);
  EditPlan plan;
  plan.Destroyed.push_back(segment.Key);
  for (const EntityKey point : {segment.From, segment.To}) {
    const bool used = std::ranges::any_of(segments, [&](const ParkSegment &other) {
      return other.Key != segment.Key && endsAt(other, point);
    });
    if (!used && findIntent<PathPointIntent>(world, point) != nullptr &&
        std::ranges::find(plan.Destroyed, point) == plan.Destroyed.end()) {
      plan.Destroyed.push_back(point);
    }
  }
  return plan;
}

// The plan of a SetCornerRadius that names a path point with a radius finite and at least 0.
std::optional<EditPlan> planRadius(const World &world, const SetCornerRadius &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr ||
      !std::isfinite(command.Radius) || command.Radius < 0.0) {
    return std::nullopt;
  }
  EditPlan plan;
  plan.Rounded = command.Point;
  plan.RoundedTo = command.Radius;
  return plan;
}

// The plan of a SetEdgeRadius that names a segment with a ground line, one of its two points, and a
// radius finite and at least 0.
std::optional<EditPlan> planEdge(const World &world, const SetEdgeRadius &command) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  const ParkSegment *segment = findSegment(segments, command.Segment);
  if (segment == nullptr || segment->Line.empty() || !endsAt(*segment, command.Point) ||
      !std::isfinite(command.Radius) || command.Radius < 0.0) {
    return std::nullopt;
  }
  EditPlan plan;
  plan.Edged = segment->Key;
  plan.EdgedAtFrom = segment->From == command.Point;
  plan.EdgedTo = command.Radius;
  return plan;
}

// The plan of a SetSegmentWidth that names a segment with a ground line and a width finite and
// above zero: the segment reshaped between its own points, at the width. It meets nothing it
// didn't meet before.
std::optional<EditPlan> planWidth(const World &world, const SetSegmentWidth &command) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  const ParkSegment *segment = findSegment(segments, command.Segment);
  if (segment == nullptr || segment->Line.empty() || !std::isfinite(command.Width) ||
      !(command.Width > 0.0)) {
    return std::nullopt;
  }
  const std::optional<ParkPoint> fromAt = positionOf(world, segment->From);
  const std::optional<ParkPoint> toAt = positionOf(world, segment->To);
  if (!fromAt || !toAt) {
    return std::nullopt;
  }
  EditPlan plan;
  const size_t from = pointEntry(plan, segment->From, *fromAt);
  const size_t to = pointEntry(plan, segment->To, *toAt);
  plan.Lines.push_back(PlanLine{segment->Key,
                                segment->Kind,
                                command.Width,
                                from,
                                to,
                                {},
                                segment->FromEdge,
                                segment->ToEdge});
  return plan;
}

// Where applyPlan makes a plan's changes: a world.
class WorldStore {
public:
  explicit WorldStore(World &world) : Target(&world) {}
  void destroy(EntityKey key) { Target->destroyEntity(key); }
  EntityKey addPoint(const PathPointIntent &point) {
    const EntityKey key = Target->createEntity();
    Target->Registry.emplace<PathPointIntent>(Target->findEntity(key), point);
    return key;
  }
  EntityKey addSegment(const PathSegmentIntent &segment) {
    const EntityKey key = Target->createEntity();
    Target->Registry.emplace<PathSegmentIntent>(Target->findEntity(key), segment);
    return key;
  }
  PathPointIntent &point(EntityKey key) {
    return Target->Registry.get<PathPointIntent>(Target->findEntity(key));
  }
  PathSegmentIntent &segment(EntityKey key) {
    return Target->Registry.get<PathSegmentIntent>(Target->findEntity(key));
  }

private:
  World *Target;
};

// Where applyPlan makes a plan's changes: a copy of a world's layout. New keys come from the
// world's counter in the order the world would give them.
class LayoutStore {
public:
  LayoutStore(PathLayout &layout, uint64_t nextKey) : Target(&layout), NextKey(nextKey) {}
  void destroy(EntityKey key) {
    std::erase_if(Target->Points, [key](const LayoutPoint &point) { return point.Key == key; });
    std::erase_if(Target->Segments,
                  [key](const LayoutSegment &segment) { return segment.Key == key; });
  }
  EntityKey addPoint(const PathPointIntent &point) {
    const EntityKey key{NextKey++};
    Target->Points.push_back(LayoutPoint{key, point});
    return key;
  }
  EntityKey addSegment(const PathSegmentIntent &segment) {
    const EntityKey key{NextKey++};
    Target->Segments.push_back(LayoutSegment{key, segment});
    return key;
  }
  PathPointIntent &point(EntityKey key) {
    return std::ranges::lower_bound(Target->Points, key, {}, &LayoutPoint::Key)->Intent;
  }
  PathSegmentIntent &segment(EntityKey key) {
    return std::ranges::lower_bound(Target->Segments, key, {}, &LayoutSegment::Key)->Intent;
  }

private:
  PathLayout *Target;
  uint64_t NextKey;
};

// Where applyPlan records each segment end it makes at a plan point that was already a path point,
// when it is given a list for them.
class AddedEnds {
public:
  AddedEnds(const EditPlan &plan, std::vector<SegmentEnd> *added) : Plan(plan), Added(added) {}

  // Whether the plan point was already a path point.
  bool existing(size_t point) const { return Plan.Points[point].Existing != NULL_KEY; }
  // Records the segment end when it is added.
  void report(EntityKey segment, size_t end, bool isAdded) const {
    if (Added != nullptr && isAdded) {
      Added->emplace_back(segment, end);
    }
  }

private:
  const EditPlan &Plan;
  std::vector<SegmentEnd> *Added;
};

// Splits the corner in the store, after its point has moved: the second segment's end at the
// corner's point moves to the last corner point, and new segments run from the corner's point
// through each place and the corner point after it. Each new corner's outside edge takes the old
// corner's outside edge radius, held by whichever of its two ends starts it.
template <typename Store>
void applyBendSplit(Store &store, const BendSplit &bend, const std::vector<EntityKey> &keys,
                    const AddedEnds &report) {
  const EntityKey point = bend.Corner.Point;
  const PathSegmentIntent first = store.segment(bend.Segments[0]);
  PathSegmentIntent &second = store.segment(bend.Segments[1]);
  // The outline turns outward from the first segment to the second when the second lies clockwise
  // of it. Then each corner's outside edge starts from its end toward the first segment, and
  // otherwise from its end toward the second.
  const ParkPoint &a = bend.Corner.Along[0];
  const ParkPoint &b = bend.Corner.Along[1];
  const bool outerOnFirst = a.Z * b.X - a.X * b.Z < 0.0;
  const auto edgeAtPoint = [point](const PathSegmentIntent &segment) {
    return segment.From == point ? segment.FromEdge : segment.ToEdge;
  };
  const double outer = outerOnFirst ? edgeAtPoint(first) : edgeAtPoint(second);
  (second.From == point ? second.From : second.To) = keys[bend.Points.back()];
  const double width = second.Width;
  // The corner's point, then each place followed by the corner point after it.
  std::vector<EntityKey> chain{point};
  for (size_t i = 0; i < bend.At.size(); ++i) {
    chain.push_back(keys[bend.At[i].Point]);
    chain.push_back(keys[bend.Points[i]]);
  }
  for (size_t j = 0; j + 1 < chain.size(); ++j) {
    // Even parts start at a corner point and run toward the second segment, and odd parts end at
    // one and run back toward the first.
    const bool fromCorner = j % 2 == 0;
    const EntityKey part = store.addSegment(PathSegmentIntent{
        first.Kind, chain[j], chain[j + 1], j + 2 == chain.size() ? width : first.Width,
        fromCorner && !outerOnFirst ? outer : DEFAULT_EDGE_RADIUS,
        !fromCorner && outerOnFirst ? outer : DEFAULT_EDGE_RADIUS});
    // The end at a place is added there when the place's point was already a path point.
    const size_t place = bend.At[(fromCorner ? j : j - 1) / 2].Point;
    report.report(part, fromCorner ? 1 : 0, report.existing(place));
  }
  store.point(point).Radius = bend.Corner.Radius;
}

// Splits the segment in the store at its points: it ends at the first, and new parts run on
// through the rest to its old end.
template <typename Store>
void applySplit(Store &store, const Split &split, const std::vector<EntityKey> &keys,
                const AddedEnds &report) {
  const PathSegmentIntent was = store.segment(split.Segment);
  store.segment(split.Segment).To = keys[split.At.front().Point];
  store.segment(split.Segment).ToEdge = DEFAULT_EDGE_RADIUS;
  report.report(split.Segment, 1, report.existing(split.At.front().Point));
  for (size_t k = 0; k < split.At.size(); ++k) {
    const bool last = k + 1 == split.At.size();
    const EntityKey to = last ? was.To : keys[split.At[k + 1].Point];
    const EntityKey part = store.addSegment(
        PathSegmentIntent{was.Kind, keys[split.At[k].Point], to, was.Width, DEFAULT_EDGE_RADIUS,
                          last ? was.ToEdge : DEFAULT_EDGE_RADIUS});
    report.report(part, 0, report.existing(split.At[k].Point));
    report.report(part, 1, !last && report.existing(split.At[k + 1].Point));
  }
}

// Makes the line in the store as a run of segments through its points: a reshaped line keeps its
// key for its first part, and every other part takes a new key. The first part takes the line's
// edge radius at From, and the last its edge radius at To.
template <typename Store>
void applyLine(Store &store, const PlanLine &line, const std::vector<EntityKey> &keys,
               const AddedEnds &report) {
  size_t from = line.From;
  bool first = true;
  const auto addPart = [&](size_t to, bool last) {
    const PathSegmentIntent part{line.Kind,
                                 keys[from],
                                 keys[to],
                                 line.Width,
                                 first ? line.FromEdge : DEFAULT_EDGE_RADIUS,
                                 last ? line.ToEdge : DEFAULT_EDGE_RADIUS};
    EntityKey key = line.Key;
    if (first && line.Key != NULL_KEY) {
      store.segment(line.Key) = part;
    } else {
      key = store.addSegment(part);
    }
    report.report(key, 0, first ? line.FromAdded : report.existing(from));
    report.report(key, 1, last ? line.ToAdded : report.existing(to));
    first = false;
    from = to;
  };
  for (const PlacedPoint &placed : line.Through) {
    addPart(placed.Point, false);
  }
  addPart(line.To, true);
}

// Makes what the plan describes in the store. Destroyed keys go first. Each plan point's key is
// its existing point's, or a new point's, in the plan's order. A moved point moves, and a rounded
// point takes its radius. Each split segment ends at its first point, and new parts run on through
// the rest to its old end. Each line runs through its points: a reshaped line keeps its key for
// its first part, and every other part takes a new key. The plan's curve writes come last. When
// added is given, it receives each segment end the plan adds at a point that was already a path
// point.
template <typename Store>
void applyPlan(Store &store, const EditPlan &plan, std::vector<SegmentEnd> *added = nullptr) {
  for (const EntityKey key : plan.Destroyed) {
    store.destroy(key);
  }
  std::vector<EntityKey> keys;
  keys.reserve(plan.Points.size());
  for (const PlanPoint &point : plan.Points) {
    keys.push_back(point.Existing != NULL_KEY
                       ? point.Existing
                       : store.addPoint(PathPointIntent{point.At, point.Radius}));
  }
  const AddedEnds report{plan, added};
  for (const auto &[point, to] : plan.Moves) {
    store.point(point).At = to;
  }
  if (plan.Rounded != NULL_KEY) {
    store.point(plan.Rounded).Radius = plan.RoundedTo;
  }
  if (plan.Edged != NULL_KEY) {
    PathSegmentIntent &edged = store.segment(plan.Edged);
    (plan.EdgedAtFrom ? edged.FromEdge : edged.ToEdge) = plan.EdgedTo;
  }
  for (const BendSplit &bend : plan.BendSplits) {
    applyBendSplit(store, bend, keys, report);
  }
  for (const Split &split : plan.Splits) {
    applySplit(store, split, keys, report);
  }
  for (const PlanLine &line : plan.Lines) {
    applyLine(store, line, keys, report);
  }
  for (const auto &[point, radius] : plan.Curves.Radii) {
    store.point(point).Radius = radius;
  }
  for (const auto &[end, radius] : plan.Curves.Edges) {
    PathSegmentIntent &segment = store.segment(end.first);
    (end.second == 0 ? segment.FromEdge : segment.ToEdge) = radius;
  }
}

// What a plan changes, as it would lie after the plan: each segment it adds or whose ground line or
// width it changes, and the surface of each owner whose part of the surface it changes, each in
// ascending key order.
struct Edited {
  std::vector<ParkSegment> Segments;
  PavedSurface Surface;
};

// The pieces and edge lines of after whose owner's pieces or edge lines differ from before's. Both
// hold the surface of the same owners.
PavedSurface changedOwners(const PavedSurface &before, const PavedSurface &after) {
  // Each owner's run of items in a list in ascending owner order.
  const auto runOf = [](const auto &items, EntityKey owner) {
    return std::ranges::equal_range(items, owner, {}, [](const auto &item) { return item.Owner; });
  };
  const auto sameEdges = [&before, &after](const auto &was, const auto &is) {
    return std::ranges::equal(was, is, [&before, &after](const EdgeLine &x, const EdgeLine &y) {
      return x.Kind == y.Kind && std::ranges::equal(edgePoints(before, x), edgePoints(after, y));
    });
  };
  PavedSurface changed;
  for (auto first = after.Pieces.begin(); first != after.Pieces.end();) {
    const auto run = runOf(after.Pieces, first->Owner);
    if (!std::ranges::equal(runOf(before.Pieces, first->Owner), run)) {
      changed.Pieces.insert(changed.Pieces.end(), run.begin(), run.end());
    }
    first = run.end();
  }
  for (auto first = after.Edges.begin(); first != after.Edges.end();) {
    const auto run = runOf(after.Edges, first->Owner);
    if (!sameEdges(runOf(before.Edges, first->Owner), run)) {
      for (const EdgeLine &edge : run) {
        const std::span<const ParkPoint> points = edgePoints(after, edge);
        changed.Edges.push_back(
            EdgeLine{edge.Kind, edge.Owner, changed.EdgePoints.size(), points.size()});
        changed.EdgePoints.insert(changed.EdgePoints.end(), points.begin(), points.end());
      }
    }
    first = run.end();
  }
  return changed;
}

// The points whose surface an edit can change: each end of each segment it adds, removes, or
// changes, before and after it.
std::vector<EntityKey> pointsChanged(const std::vector<ParkSegment> &before,
                                     const std::vector<ParkSegment> &after) {
  std::vector<EntityKey> points;
  const auto differs = [](const ParkSegment *was, const ParkSegment &is) {
    return was == nullptr || was->From != is.From || was->To != is.To || was->Line != is.Line ||
           was->Width != is.Width || was->FromEdge != is.FromEdge || was->ToEdge != is.ToEdge;
  };
  for (const ParkSegment &segment : after) {
    if (differs(findSegment(before, segment.Key), segment)) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  for (const ParkSegment &segment : before) {
    if (differs(findSegment(after, segment.Key), segment)) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  std::ranges::sort(points);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  return points;
}

// What editedBy finds: only the pieces of the surface it changes, which is all the validity check
// needs, or the pieces and edge lines, as the edit would be drawn.
enum class Wanted { Pieces, Drawing };

// The points, and the other end of each segment at one of them, before or after.
std::vector<EntityKey> withFarEnds(const std::vector<ParkSegment> &before,
                                   const std::vector<ParkSegment> &after,
                                   std::vector<EntityKey> points) {
  const std::vector<EntityKey> changed = points;
  for (const std::vector<ParkSegment> *segments : {&before, &after}) {
    for (const ParkSegment &segment : *segments) {
      if (std::ranges::binary_search(changed, segment.From) ||
          std::ranges::binary_search(changed, segment.To)) {
        points.push_back(segment.From);
        points.push_back(segment.To);
      }
    }
  }
  std::ranges::sort(points);
  points.erase(std::unique(points.begin(), points.end()), points.end());
  return points;
}

Edited editedBy(const World &world, const EditPlan &plan, Wanted wanted) {
  const PathLayout before = layoutOf(world);
  const PathGround groundBefore = groundOf(before);
  PathLayout after = before;
  LayoutStore store(after, world.nextKey());
  applyPlan(store, plan);
  PathGround groundAfter = groundOf(after);
  std::vector<EntityKey> points = pointsChanged(groundBefore.Segments, groundAfter.Segments);
  // A segment's paving at one end depends on whether its other end is free, which can change
  // whenever a changed point loses or gains a segment.
  points = withFarEnds(groundBefore.Segments, groundAfter.Segments, points);
  const bool drawing = wanted == Wanted::Drawing;
  if (drawing) {
    // Paving that changes can hide or show the edge lines of paving near it, before and after.
    const std::vector<EntityKey> nearBefore = pointsAffected(before, groundBefore, points);
    const std::vector<EntityKey> nearAfter = pointsAffected(after, groundAfter, points);
    points.insert(points.end(), nearBefore.begin(), nearBefore.end());
    points.insert(points.end(), nearAfter.begin(), nearAfter.end());
    std::ranges::sort(points);
    points.erase(std::unique(points.begin(), points.end()), points.end());
  }
  Edited edited;
  edited.Surface = changedOwners(surfaceAround(before, groundBefore, points, drawing),
                                 surfaceAround(after, groundAfter, points, drawing));
  for (ParkSegment &segment : groundAfter.Segments) {
    const ParkSegment *was = findSegment(groundBefore.Segments, segment.Key);
    if (was == nullptr || was->Line != segment.Line || was->Width != segment.Width) {
      edited.Segments.push_back(std::move(segment));
    }
  }
  return edited;
}

// The plan when the world accepts it: every segment it adds or changes has a ground line that stays
// in the park, and every piece of the surface it adds or changes stays in the park and meets no
// solid.
std::optional<EditPlan> acceptedPlan(const World &world, std::optional<EditPlan> plan) {
  if (!plan) {
    return std::nullopt;
  }
  const std::vector<Solid> solids = solidsOf(world);
  const Edited edited = editedBy(world, *plan, Wanted::Pieces);
  const bool fitting =
      std::ranges::all_of(edited.Segments,
                          [](const ParkSegment &segment) {
                            const std::optional<Line> line = lineOf(segment);
                            return line && !leavesPark(*line);
                          }) &&
      std::ranges::all_of(edited.Surface.Pieces, [&solids](const SurfacePiece &piece) {
        return !leavesPark(piece) && fits(piece, solids);
      });
  return fitting ? std::move(plan) : std::nullopt;
}

// The plan with the curve writes that keep every curve through its joins and splits, found by
// trying it on a copy of the world's layout.
std::optional<EditPlan> withCurves(const World &world, std::optional<EditPlan> plan) {
  if (!plan) {
    return plan;
  }
  const PathLayout before = layoutOf(world);
  PathLayout after = before;
  LayoutStore store(after, world.nextKey());
  std::vector<SegmentEnd> added;
  applyPlan(store, *plan, &added);
  plan->Curves = keptCurves(before, after, added);
  return plan;
}

// Each path command's plan, and none for box commands, which have no plan.
std::optional<EditPlan> planOf(const World &world, const AddPath &command) {
  return withCurves(world, planPath(world, command));
}
std::optional<EditPlan> planOf(const World & /*world*/, const AddBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World & /*world*/, const MoveBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World &world, const DeleteSegment &command) {
  return withCurves(world, planDeleteSegment(world, command));
}
std::optional<EditPlan> planOf(const World & /*world*/, const DeleteBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World &world, const MovePoint &command) {
  return withCurves(world, planMove(world, command));
}
std::optional<EditPlan> planOf(const World &world, const InsertPoint &command) {
  return withCurves(world, planInsert(world, command));
}
std::optional<EditPlan> planOf(const World &world, const RemovePoint &command) {
  return withCurves(world, planRemove(world, command));
}
std::optional<EditPlan> planOf(const World &world, const SetCornerRadius &command) {
  return planRadius(world, command);
}
std::optional<EditPlan> planOf(const World &world, const SetSegmentWidth &command) {
  return planWidth(world, command);
}
std::optional<EditPlan> planOf(const World &world, const SetEdgeRadius &command) {
  return planEdge(world, command);
}

// The ground position of each of the plan's crossings, or none without a plan.
std::vector<ParkPoint> crossingsOf(const std::optional<EditPlan> &plan) {
  std::vector<ParkPoint> crossings;
  if (plan) {
    for (const size_t point : plan->Crossings) {
      crossings.push_back(plan->Points[point].At);
    }
  }
  return crossings;
}

} // namespace

void addParkEdits(WorldSchema &schema) {
  schema.addCommand<AddPath>();
  schema.addCommand<AddBox>();
  schema.addCommand<MoveBox>();
  schema.addCommand<DeleteSegment>();
  schema.addCommand<DeleteBox>();
  schema.addCommand<MovePoint>();
  schema.addCommand<InsertPoint>();
  schema.addCommand<RemovePoint>();
  schema.addCommand<SetCornerRadius>();
  schema.addCommand<SetSegmentWidth>();
  schema.addCommand<SetEdgeRadius>();
}

std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop) {
  if (const auto *at = std::get_if<NewPoint>(&stop)) {
    return at->At;
  }
  if (const auto *on = std::get_if<OnPoint>(&stop)) {
    const PathPointIntent *point = findIntent<PathPointIntent>(world, on->Point);
    return point == nullptr ? std::nullopt : std::optional<ParkPoint>(point->At);
  }
  if (const auto *on = std::get_if<OnBend>(&stop)) {
    const std::optional<ParkCorner> corner = roundedCorner(world, on->Point);
    if (!corner || !std::isfinite(on->Along) || on->Along <= 0.0 || on->Along >= 1.0) {
      return std::nullopt;
    }
    return bendPlace(*corner, on->Along);
  }
  if (std::holds_alternative<RepeatStop>(stop) || std::holds_alternative<OnDrawnSegment>(stop)) {
    return std::nullopt;
  }
  const auto &on = std::get<OnSegment>(stop);
  const PathSegmentIntent *segment = findIntent<PathSegmentIntent>(world, on.Segment);
  if (segment == nullptr) {
    return std::nullopt;
  }
  const PathPointIntent *from = findIntent<PathPointIntent>(world, segment->From);
  const PathPointIntent *to = findIntent<PathPointIntent>(world, segment->To);
  if (from == nullptr || to == nullptr) {
    return std::nullopt;
  }
  return ParkPoint{from->At.X + on.Along * (to->At.X - from->At.X),
                   from->At.Z + on.Along * (to->At.Z - from->At.Z)};
}

std::optional<std::vector<ParkPoint>> stopPositions(const World &world, const AddPath &path) {
  std::vector<ParkPoint> positions;
  for (const PathStop &stop : path.Stops) {
    if (const auto *repeat = std::get_if<RepeatStop>(&stop)) {
      if (repeat->Stop >= positions.size()) {
        return std::nullopt;
      }
      positions.push_back(positions[repeat->Stop]);
      continue;
    }
    if (const auto *on = std::get_if<OnDrawnSegment>(&stop)) {
      if (on->Stop + 1 >= positions.size()) {
        return std::nullopt;
      }
      const ParkPoint &from = positions[on->Stop];
      const ParkPoint &to = positions[on->Stop + 1];
      positions.push_back(
          ParkPoint{from.X + on->Along * (to.X - from.X), from.Z + on->Along * (to.Z - from.Z)});
      continue;
    }
    const std::optional<ParkPoint> position = stopPosition(world, stop);
    if (!position) {
      return std::nullopt;
    }
    positions.push_back(*position);
  }
  return positions;
}

std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path) {
  return crossingsOf(acceptedPlan(world, planOf(world, path)));
}

std::vector<ParkPoint> pathCrossings(const World &world, const MovePoint &move) {
  return crossingsOf(acceptedPlan(world, planOf(world, move)));
}

std::vector<ParkPoint> pathCrossings(const World &world, const RemovePoint &remove) {
  return crossingsOf(acceptedPlan(world, planOf(world, remove)));
}

namespace {

// The pieces of the park's surface that can reach the solid. Every piece of a point's or of a
// segment's lies within the longer of one of the point's segments' length and the widest width,
// plus the widest width, from the point, so only the points that near the solid's bounds and their
// segments are built.
std::vector<SurfacePiece> piecesNear(const World &world, const Solid &solid) {
  const PathLayout layout = layoutOf(world);
  const PathGround ground = groundOf(layout);
  const auto [leastX, greatestX] = extentOf(solid.Shape.Corners, ParkPoint{1.0, 0.0});
  const auto [leastZ, greatestZ] = extentOf(solid.Shape.Corners, ParkPoint{0.0, 1.0});
  const auto positionOf = [&layout](EntityKey key) {
    return std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key)->Intent.At;
  };
  double widest = 0.0;
  for (const ParkSegment &segment : ground.Segments) {
    if (!segment.Line.empty()) {
      widest = std::max(widest, segment.Width);
    }
  }
  std::vector<EntityKey> focus;
  for (const ParkSegment &segment : ground.Segments) {
    if (segment.Line.empty()) {
      continue;
    }
    const ParkPoint from = positionOf(segment.From);
    const ParkPoint to = positionOf(segment.To);
    const double dx = to.X - from.X;
    const double dz = to.Z - from.Z;
    const double reach =
        std::max(std::sqrt(dx * dx + dz * dz), widest) + widest + CONTACT_TOLERANCE;
    for (const auto &[key, at] : {std::pair{segment.From, from}, std::pair{segment.To, to}}) {
      const double outX = std::max({leastX - at.X, at.X - greatestX, 0.0});
      const double outZ = std::max({leastZ - at.Z, at.Z - greatestZ, 0.0});
      if (outX * outX + outZ * outZ <= reach * reach) {
        focus.push_back(key);
      }
    }
  }
  std::ranges::sort(focus);
  focus.erase(std::unique(focus.begin(), focus.end()), focus.end());
  return surfaceAround(layout, ground, focus, false).Pieces;
}

} // namespace

bool isAccepted(const World &world, const AddPath &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const AddBox &command) {
  if (!isKnown(command.Kind)) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(NULL_KEY, command.At, boxSize(command.Kind));
  return solid && fits(*solid, solidsOf(world), piecesNear(world, *solid));
}

bool isAccepted(const World &world, const MoveBox &command) {
  const BoxIntent *box = findIntent<BoxIntent>(world, command.Box);
  if (box == nullptr) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(command.Box, command.At, boxSize(box->Kind));
  return solid && fits(*solid, solidsOf(world), piecesNear(world, *solid));
}

bool isAccepted(const World &world, const DeleteSegment &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const DeleteBox &command) {
  return findIntent<BoxIntent>(world, command.Box) != nullptr;
}

bool isAccepted(const World &world, const MovePoint &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const InsertPoint &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const RemovePoint &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const SetCornerRadius &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const SetSegmentWidth &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

bool isAccepted(const World &world, const SetEdgeRadius &command) {
  return acceptedPlan(world, planOf(world, command)).has_value();
}

void applyCommand(World &world, const AddPath &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const AddBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const EntityKey key = world.createEntity();
  world.Registry.emplace<BoxIntent>(world.findEntity(key), BoxIntent{command.Kind, command.At});
}

void applyCommand(World &world, const MoveBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  world.Registry.get<BoxIntent>(world.findEntity(command.Box)).At = command.At;
}

void applyCommand(World &world, const DeleteSegment &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const DeleteBox &command) {
  if (isAccepted(world, command)) {
    world.destroyEntity(command.Box);
  }
}

void applyCommand(World &world, const MovePoint &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const InsertPoint &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const RemovePoint &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const SetCornerRadius &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const SetSegmentWidth &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

void applyCommand(World &world, const SetEdgeRadius &command) {
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
}

namespace {

// What the edit changes, or nothing when it names nothing it acts on, or a segment it adds or
// reshapes has no ground line, which leaves it nothing it can show.
Edited editedOf(const World &world, const ParkEdit &edit) {
  const std::optional<EditPlan> plan =
      std::visit([&world](const auto &command) { return planOf(world, command); }, edit);
  if (!plan) {
    return {};
  }
  Edited edited = editedBy(world, *plan, Wanted::Drawing);
  if (std::ranges::any_of(edited.Segments,
                          [](const ParkSegment &segment) { return segment.Line.empty(); })) {
    return {};
  }
  return edited;
}

} // namespace

std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit) {
  return editedOf(world, edit).Segments;
}

PavedSurface editedSurface(const World &world, const ParkEdit &edit) {
  return editedOf(world, edit).Surface;
}

bool isPhysicallyValid(const World &world) {
  const std::vector<Solid> solids = solidsOf(world);
  const std::vector<Line> lines = linesOf(world);
  // An entrance or box with no footprint, or a segment with an empty ground line, is not valid.
  if (solids.size() != parkEntrances(world).size() + parkBoxes(world).size() ||
      lines.size() != parkSegments(world).size()) {
    return false;
  }
  for (size_t i = 0; i < solids.size(); ++i) {
    if (leavesPark(solids[i])) {
      return false;
    }
    for (size_t j = i + 1; j < solids.size(); ++j) {
      if (overlaps(solids[i], solids[j])) {
        return false;
      }
    }
  }
  if (std::ranges::any_of(lines, [](const Line &line) { return leavesPark(line); })) {
    return false;
  }
  return std::ranges::all_of(parkSurface(world).Pieces, [&solids](const SurfacePiece &piece) {
    return !leavesPark(piece) && fits(piece, solids);
  });
}

bool isAccepted(const World &world, const ParkEdit &edit) {
  return std::visit([&world](const auto &command) { return isAccepted(world, command); }, edit);
}

void queueEdit(CommandQueue &queue, const ParkEdit &edit) {
  std::visit([&queue](const auto &command) { queue.push(command); }, edit);
}

} // namespace tpj
