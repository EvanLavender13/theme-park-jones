#include "sim/park/edits.h"

#include "sim/clock.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/curves.h"
#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/layout.h"
#include "sim/park/internal/meetings.h"
#include "sim/park/internal/plan.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

constexpr double HALF_PARK = PARK_SIZE / 2.0;

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

// Puts every entrance's and box's footprint in solids, skipping any with none. entrances and boxes
// are lists it reuses.
void solidsOf(const World &world, std::vector<ParkEntrance> &entrances, std::vector<ParkBox> &boxes,
              std::vector<Solid> &solids) {
  parkEntrances(world, entrances);
  parkBoxes(world, boxes);
  solids.clear();
  for (const ParkEntrance &entrance : entrances) {
    if (const std::optional<Solid> solid = solidOf(entrance.Key, entrance.At, ENTRANCE_SIZE)) {
      solids.push_back(*solid);
    }
  }
  for (const ParkBox &box : boxes) {
    if (const std::optional<Solid> solid = solidOf(box.Key, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
}

// Every entrance's and box's footprint, skipping any with none.
std::vector<Solid> solidsOf(const World &world) {
  std::vector<ParkEntrance> entrances;
  std::vector<ParkBox> boxes;
  std::vector<Solid> solids;
  solidsOf(world, entrances, boxes, solids);
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
  return world.tryGet<T>(key);
}

// Whether the kind is one of its enum's values.
template <typename Kind> bool isKnown(Kind kind) {
  return static_cast<size_t>(kind) < enumNames(kind).size();
}

// Gives the segment the furnishing: each side's intent holds that side's furnishing, and a side
// with none holds no intent, so it saves as one never furnished.
void setFurnishingOf(World &world, EntityKey segment, const Furnishing &furnishing) {
  if (furnishing.Left) {
    world.emplaceOrReplace<LeftFurnishingIntent>(segment, LeftFurnishingIntent{*furnishing.Left});
  } else {
    world.remove<LeftFurnishingIntent>(segment);
  }
  if (furnishing.Right) {
    world.emplaceOrReplace<RightFurnishingIntent>(segment,
                                                  RightFurnishingIntent{*furnishing.Right});
  } else {
    world.remove<RightFurnishingIntent>(segment);
  }
}

bool isFurnished(const Furnishing &furnishing) {
  return furnishing.Left.has_value() || furnishing.Right.has_value();
}

// The furnishing as a segment running the other way holds it: left and right swapped.
Furnishing turned(const Furnishing &furnishing) {
  return Furnishing{furnishing.Right, furnishing.Left};
}

// Whether one side's furnishing has a known kind and a spacing finite and above zero.
bool isSideValid(const std::optional<SideFurnishing> &side) {
  return !side || (isKnown(side->Kind) && std::isfinite(side->Spacing) && side->Spacing > 0.0);
}

// Whether a segment of the kind can hold the furnishing: none, or, on a guest segment, a valid
// furnishing on each side that has one.
bool isFurnishingValid(const Furnishing &furnishing, PathKind kind) {
  return !isFurnished(furnishing) ||
         (kind == PathKind::Guest && isSideValid(furnishing.Left) && isSideValid(furnishing.Right));
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

// Two segments in ascending key order.
using SegmentPair = std::array<const ParkSegment *, 2>;

// The two segments that end at the point, in ascending key order, leaving out each that ends at
// skip when skip isn't NULL_KEY, or none unless exactly two do.
std::optional<SegmentPair> twoAt(const std::vector<ParkSegment> &segments, EntityKey point,
                                 EntityKey skip = NULL_KEY) {
  SegmentPair two{};
  size_t count = 0;
  for (const ParkSegment &segment : segments) {
    if (!endsAt(segment, point) || (skip != NULL_KEY && endsAt(segment, skip))) {
      continue;
    }
    if (count == 2) {
      return std::nullopt;
    }
    two[count++] = &segment;
  }
  return count == 2 ? std::optional<SegmentPair>(two) : std::nullopt;
}

// The segment's edge radius at the point, one of its two.
double edgeAt(const ParkSegment &segment, EntityKey point) {
  return segment.From == point ? segment.FromEdge : segment.ToEdge;
}

// The corner the path point makes, when it is rounded with a radius above 0, or none. The corners
// are in ascending key order of their points, as parkCorners gives them.
std::optional<ParkCorner> roundedIn(std::span<const ParkCorner> corners, EntityKey point) {
  const auto found = std::ranges::lower_bound(corners, point, {}, &ParkCorner::Point);
  if (found == corners.end() || found->Point != point || !(found->Radius > 0.0)) {
    return std::nullopt;
  }
  return *found;
}

// The segment's end that is not the point.
EntityKey farEnd(const ParkSegment &segment, EntityKey point) {
  return segment.From == point ? segment.To : segment.From;
}

// The target as a stop, for stopPosition.
PathStop stopOf(const PointTarget &target) {
  return std::visit([](const auto &stop) { return PathStop{stop}; }, target);
}

// The stop's ground position, as stopPosition gives it, finding an OnBend's corner with the corner
// finder, which gives the rounded corner at a point or none.
template <typename FindCorner>
std::optional<ParkPoint> positionWith(const World &world, const PathStop &stop,
                                      FindCorner findCorner) {
  if (const auto *at = std::get_if<NewPoint>(&stop)) {
    return at->At;
  }
  if (const auto *on = std::get_if<OnPoint>(&stop)) {
    const PathPointIntent *point = findIntent<PathPointIntent>(world, on->Point);
    return point == nullptr ? std::nullopt : std::optional<ParkPoint>(point->At);
  }
  if (const auto *on = std::get_if<OnBend>(&stop)) {
    const std::optional<ParkCorner> corner = findCorner(on->Point);
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

// The stop's ground position, as stopPosition gives it, reading the corners from a list as
// parkCorners gives them.
std::optional<ParkPoint> positionIn(const World &world, std::span<const ParkCorner> corners,
                                    const PathStop &stop) {
  return positionWith(world, stop,
                      [corners](EntityKey point) { return roundedIn(corners, point); });
}

// The furnishing the segment the key holds has on each side.
Furnishing furnishingOf(const World &world, EntityKey key) {
  Furnishing furnishing;
  if (const auto *left = world.tryGet<LeftFurnishingIntent>(key)) {
    furnishing.Left = left->Held;
  }
  if (const auto *right = world.tryGet<RightFurnishingIntent>(key)) {
    furnishing.Right = right->Held;
  }
  return furnishing;
}

// The position of the path point the key holds, or none.
std::optional<ParkPoint> positionOf(const World &world, EntityKey key) {
  const PathPointIntent *point = findIntent<PathPointIntent>(world, key);
  return point == nullptr ? std::nullopt : std::optional<ParkPoint>(point->At);
}

// The two segments a deleted point merges, the lower key first: exactly two segments use the
// point, and their far ends are different points other than it.
std::optional<SegmentPair> mergingPair(const std::vector<ParkSegment> &segments, EntityKey point) {
  const std::optional<SegmentPair> two = twoAt(segments, point);
  if (!two) {
    return std::nullopt;
  }
  const EntityKey lowFar = farEnd(*(*two)[0], point);
  const EntityKey highFar = farEnd(*(*two)[1], point);
  if (lowFar == highFar || lowFar == point || highFar == point) {
    return std::nullopt;
  }
  return two;
}

// Whether the OnBend names a place on the arc of a corner with a radius above 0 whose two segments
// both have the kind, or one kind when kind is none.
bool isBendOfKind(const World &world, const std::vector<ParkSegment> &segments,
                  std::span<const ParkCorner> corners, const OnBend &on,
                  std::optional<PathKind> kind) {
  const std::optional<SegmentPair> two = twoAt(segments, on.Point);
  return positionIn(world, corners, on).has_value() && two && (*two)[0]->Kind == (*two)[1]->Kind &&
         (!kind || (*two)[0]->Kind == *kind);
}

// Whether the stop names what it lands on, of the kind.
bool isStopOfKind(const World &world, const std::vector<ParkSegment> &segments,
                  std::span<const ParkCorner> corners, const PathStop &stop, PathKind kind) {
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
    return isBendOfKind(world, segments, corners, *on, kind);
  }
  return true;
}

// Adds an element to one of a plan's lists, taking a spare one when there is one, so that its own
// lists keep their memory. The caller sets every field.
template <typename T> T &addKept(std::vector<T> &list, std::vector<T> &spare) {
  resizeKeeping(list, list.size() + 1, spare);
  return list.back();
}

// Empties the plan, keeping the memory of its lists and of their elements' lists.
void clearPlan(PlanLists &kept) {
  EditPlan &plan = kept.Plan;
  plan.Points.clear();
  plan.Stops.clear();
  resizeKeeping(plan.Splits, 0, kept.SpareSplits);
  resizeKeeping(plan.Lines, 0, kept.SpareLines);
  resizeKeeping(plan.BendSplits, 0, kept.SpareBends);
  plan.Crossings.clear();
  plan.Destroyed.clear();
  plan.Moves.clear();
  plan.Rounded = NULL_KEY;
  plan.RoundedTo = 0.0;
  plan.Edged = NULL_KEY;
  plan.EdgedAtFrom = false;
  plan.EdgedTo = 0.0;
  plan.Tapered = NULL_KEY;
  plan.TaperedTo = 0.0;
  plan.Curves.Radii.clear();
  plan.Curves.Edges.clear();
  plan.Inheritor = NULL_KEY;
  plan.Inherits = {};
}

// Adds a line to the plan, running through no points yet, with neither end added.
PlanLine &addLine(PlanLists &kept, EntityKey key, PathKind kind, double width, size_t from,
                  size_t to, double fromEdge = DEFAULT_EDGE_RADIUS,
                  double toEdge = DEFAULT_EDGE_RADIUS) {
  PlanLine &line = addKept(kept.Plan.Lines, kept.SpareLines);
  line.Key = key;
  line.Kind = kind;
  line.Width = width;
  line.From = from;
  line.To = to;
  line.Through.clear();
  line.FromEdge = fromEdge;
  line.ToEdge = toEdge;
  line.FromAdded = false;
  line.ToAdded = false;
  return line;
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
std::vector<PlacedPoint> &splitOf(PlanLists &kept, EntityKey segment) {
  EditPlan &plan = kept.Plan;
  const auto found = std::ranges::find(plan.Splits, segment, &Split::Segment);
  if (found != plan.Splits.end()) {
    return found->At;
  }
  Split &split = addKept(plan.Splits, kept.SpareSplits);
  split.Segment = segment;
  split.At.clear();
  return split.At;
}

// The places a plan splits the corner at the point at, the corner added to the plan's bend splits
// if it isn't there yet. The corner is rounded with a radius above 0, and exactly two of the
// segments end at its point, leaving out those that end at skip.
std::vector<PlacedPoint> &bendSplitOf(std::span<const ParkCorner> corners,
                                      const std::vector<ParkSegment> &segments, EntityKey skip,
                                      PlanLists &kept, EntityKey point) {
  EditPlan &plan = kept.Plan;
  const auto found = std::ranges::find_if(
      plan.BendSplits, [point](const BendSplit &bend) { return bend.Corner.Point == point; });
  if (found != plan.BendSplits.end()) {
    return found->At;
  }
  // The corner's first direction runs along its lower-keyed segment, the first of these. Callers
  // have checked that the corner is there.
  const SegmentPair two = twoAt(segments, point, skip).value_or(SegmentPair{});
  BendSplit &bend = addKept(plan.BendSplits, kept.SpareBends);
  bend.Corner = roundedIn(corners, point).value_or(ParkCorner{});
  bend.Segments = {two[0]->Key, two[1]->Key};
  bend.At.clear();
  bend.Points.clear();
  return bend.At;
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
// OnDrawnSegment stops on the segments they name. An OnDrawnSegment names a segment between two
// earlier stops, so its line is there before it.
void planStops(std::span<const ParkCorner> corners, const std::vector<ParkSegment> &segments,
               PlanLists &kept, const AddPath &command, const std::vector<ParkPoint> &positions) {
  EditPlan &plan = kept.Plan;
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
    if (i > 0) {
      PlanLine &line = addLine(kept, NULL_KEY, command.Kind, pathWidth(command.Kind),
                               plan.Stops[i - 1], plan.Stops[i]);
      line.FromAdded = plan.Points[line.From].Existing != NULL_KEY;
      line.ToAdded = plan.Points[line.To].Existing != NULL_KEY;
    }
    if (const auto *onSegment = std::get_if<OnSegment>(&stop)) {
      place(splitOf(kept, onSegment->Segment), plan.Stops[i], onSegment->Along);
    } else if (const auto *onDrawn = std::get_if<OnDrawnSegment>(&stop)) {
      place(plan.Lines[onDrawn->Stop].Through, plan.Stops[i], onDrawn->Along);
    } else if (const auto *onBend = std::get_if<OnBend>(&stop)) {
      place(bendSplitOf(corners, segments, NULL_KEY, kept, onBend->Point), plan.Stops[i],
            onBend->Along);
    }
  }
}

// The ends of the plan's line k.
std::pair<ParkPoint, ParkPoint> lineEnds(const EditPlan &plan, size_t k) {
  return {plan.Points[plan.Lines[k].From].At, plan.Points[plan.Lines[k].To].At};
}

// Whether every line of the plan has a ground line between its ends. line is a list it reuses.
bool linesHaveGround(const EditPlan &plan, std::vector<CarrierPoint> &line) {
  for (size_t k = 0; k < plan.Lines.size(); ++k) {
    const auto [from, to] = lineEnds(plan, k);
    segmentLine(from, to, line);
    if (line.empty()) {
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
void meetWorldSegment(const World &world, PlanLists &kept, size_t k, const ParkSegment &segment,
                      const std::vector<ParkCorner> &corners) {
  EditPlan &plan = kept.Plan;
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
    placed = place(splitOf(kept, segment.Key), point, alongB);
  } else if (meeting->Kind == MeetingKind::BEndOnA && tangents[meeting->End] == 0.0) {
    point = pointEntry(plan, meeting->End == 0 ? segment.From : segment.To, meeting->At);
    placed = place(plan.Lines[k].Through, point, meeting->AlongA);
  } else {
    // A crossing, or an end of the straight part where an arc touches it, which is no point.
    point = crossingPoint(plan, meeting->At);
    placed = place(plan.Lines[k].Through, point, meeting->AlongA);
    placed = place(splitOf(kept, segment.Key), point, alongB) || placed;
  }
  if (placed) {
    markCrossing(plan, point);
  }
}

// Adds the points where the plan's line k meets the arc of the world's corner, whose two segments
// the plan leaves in place. A place within MIN_SEGMENT_LENGTH of where the arc touches a segment
// splits that segment there instead.
// The corner's two segments are those that end at its point, leaving out each that ends at skip.
void meetCorner(const World &world, PlanLists &kept, size_t k, const ParkCorner &corner,
                const std::vector<ParkSegment> &segments, EntityKey skip,
                std::span<const ParkCorner> corners) {
  EditPlan &plan = kept.Plan;
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
  const SegmentPair at = twoAt(segments, corner.Point, skip).value_or(SegmentPair{});
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
      const ParkSegment &segment = *at[touched];
      const ParkPoint from = findIntent<PathPointIntent>(world, segment.From)->At;
      const ParkPoint to = findIntent<PathPointIntent>(world, segment.To)->At;
      const double along = distanceOf(from, where) / distanceOf(from, to);
      placed = place(splitOf(kept, segment.Key), point, along) || placed;
    } else {
      placed = place(bendSplitOf(corners, segments, skip, kept, corner.Point), point,
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
// place, and then each of the plan's earlier lines. The edit leaves in place each segment that
// doesn't end at skip, or every segment when skip is NULL_KEY. A segment of the world meets along
// the straight part of its ground line. meeting is a list it reuses.
void addMeetings(const World &world, PlanLists &kept, const std::vector<ParkSegment> &segments,
                 EntityKey skip, const std::vector<ParkCorner> &corners,
                 std::vector<ParkCorner> &meeting) {
  EditPlan &plan = kept.Plan;
  const auto leftInPlace = [skip](const ParkSegment &segment) {
    return skip == NULL_KEY || !endsAt(segment, skip);
  };
  // The corners that meet: rounded, with both of their segments kept, at a point the plan neither
  // joins nor moves.
  meeting.clear();
  for (const ParkCorner &corner : corners) {
    const bool joined = std::ranges::any_of(
        plan.Points, [&corner](const PlanPoint &point) { return point.Existing == corner.Point; });
    const bool moved = std::ranges::any_of(
        plan.Moves, [&corner](const auto &move) { return move.first == corner.Point; });
    if (corner.Radius > 0.0 && !joined && !moved && twoAt(segments, corner.Point, skip)) {
      meeting.push_back(corner);
    }
  }
  for (size_t k = 0; k < plan.Lines.size(); ++k) {
    for (const ParkSegment &segment : segments) {
      if (leftInPlace(segment) && segment.Kind == plan.Lines[k].Kind && !segment.Line.empty()) {
        meetWorldSegment(world, kept, k, segment, corners);
      }
    }
    for (const ParkCorner &corner : meeting) {
      const SegmentPair at = twoAt(segments, corner.Point, skip).value_or(SegmentPair{});
      if (at[0]->Kind == plan.Lines[k].Kind && at[1]->Kind == plan.Lines[k].Kind) {
        meetCorner(world, kept, k, corner, segments, skip, corners);
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

// Sorts the plan's meetings and places its bend splits' corner points. False when a corner it
// splits is also a point it joins or a segment it reshapes or destroys.
bool finishPlan(EditPlan &plan) {
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
      return false;
    }
    std::ranges::sort(bend.At, {}, &PlacedPoint::Along);
    placeBendPoints(plan, bend);
  }
  return true;
}

// The lists planning and checking an edit reuse, kept in the caller's TickBuffers:
// - the meeting corners, the stop positions, and a ground line;
// - the plan points' keys, the segment ends a plan adds, and the chain a bend split builds;
// - the layout withCurves applies the plan to before its curves are known;
// - the entrances, boxes, and solids;
// - the point lists of editedBetween, and the surfaces before and after an edit and of what it
//   changes;
// - the indexes of the edited segments in the after ground;
// - a restore's names, added keys, and extra points;
// - the focus and the surface of the pieces near a solid.
struct EditLists {
  std::vector<ParkCorner> Meeting;
  std::vector<ParkPoint> Positions;
  std::vector<CarrierPoint> Line;
  std::vector<EntityKey> Keys;
  std::vector<SegmentEnd> Added;
  std::vector<EntityKey> Chain;
  PathLayout Curved;
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkBox> Boxes;
  std::vector<Solid> Solids;
  std::vector<EntityKey> Points;
  std::vector<EntityKey> Changed;
  std::vector<EntityKey> Near;
  PavedSurface Before;
  PavedSurface After;
  PavedSurface Edited;
  std::vector<size_t> EditedSegments;
  std::vector<EntityKey> Names;
  std::vector<std::pair<EntityKey, EntityKey>> AddedKeys;
  std::vector<EntityKey> Extra;
  std::vector<EntityKey> Focus;
  PavedSurface NearSolid;
  std::vector<ParkSegment> SpareSegments;
};

// The plan of an AddPath that names what it acts on, in kept. False when it names nothing.
bool planPath(const World &world, PlanLists &kept, EditLists &lists, const AddPath &command) {
  if (!isKnown(command.Kind) || command.Stops.size() < 2) {
    return false;
  }
  const std::vector<ParkSegment> &segments = kept.World.Ground.Segments;
  const std::vector<ParkCorner> &corners = kept.World.Ground.Corners;
  std::vector<ParkPoint> &positions = lists.Positions;
  positions.clear();
  for (const PathStop &stop : command.Stops) {
    if (!appendStopPosition(world, corners, stop, positions)) {
      return false;
    }
  }
  if (!std::ranges::all_of(command.Stops, [&](const PathStop &stop) {
        return isStopOfKind(world, segments, corners, stop, command.Kind);
      })) {
    return false;
  }
  planStops(corners, segments, kept, command, positions);
  if (!linesHaveGround(kept.Plan, lists.Line)) {
    return false;
  }
  addMeetings(world, kept, segments, NULL_KEY, corners, lists.Meeting);
  return finishPlan(kept.Plan);
}

// Whether the MovePoint's target names what it lands on, of the kind, other than the moved point
// and the segments at it.
bool isTargetOfKind(const World &world, const std::vector<ParkSegment> &segments,
                    std::span<const ParkCorner> corners, const MovePoint &command, PathKind kind) {
  if (const auto *on = std::get_if<OnPoint>(&command.To)) {
    return on->Point != command.Point && isPointOfKind(world, segments, on->Point, kind);
  }
  if (const auto *on = std::get_if<OnSegment>(&command.To)) {
    return isStopOfKind(world, segments, corners, *on, kind) &&
           !endsAt(*findSegment(segments, on->Segment), command.Point);
  }
  if (const auto *on = std::get_if<OnBend>(&command.To)) {
    return isBendOfKind(world, segments, corners, *on, kind) &&
           std::ranges::none_of(segments, [&](const ParkSegment &segment) {
             return endsAt(segment, on->Point) && endsAt(segment, command.Point);
           });
  }
  return true;
}

// The kind of every segment at the point, or none when no segment ends there or two of them differ.
std::optional<PathKind> kindAt(const std::vector<ParkSegment> &segments, EntityKey point) {
  std::optional<PathKind> kind;
  for (const ParkSegment &segment : segments) {
    if (!endsAt(segment, point)) {
      continue;
    }
    if (kind && *kind != segment.Kind) {
      return std::nullopt;
    }
    kind = segment.Kind;
  }
  return kind;
}

// The plan of a MovePoint that names what it acts on, in kept. False when it names nothing.
bool planMove(const World &world, PlanLists &kept, EditLists &lists, const MovePoint &command) {
  const std::vector<ParkSegment> &segments = kept.World.Ground.Segments;
  const std::vector<ParkCorner> &corners = kept.World.Ground.Corners;
  const auto atPoint = [&command](const ParkSegment &segment) {
    return endsAt(segment, command.Point);
  };
  const std::optional<PathKind> kind = kindAt(segments, command.Point);
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr || !kind) {
    return false;
  }
  const std::optional<ParkPoint> position = positionIn(world, corners, stopOf(command.To));
  if (!isTargetOfKind(world, segments, corners, command, *kind) || !position) {
    return false;
  }
  const auto *onPoint = std::get_if<OnPoint>(&command.To);
  EditPlan &plan = kept.Plan;
  size_t moved = 0;
  if (onPoint != nullptr) {
    moved = pointEntry(plan, onPoint->Point, *position);
  } else {
    plan.Points.push_back(PlanPoint{*position, command.Point});
    plan.Moves.emplace_back(command.Point, *position);
  }
  for (const ParkSegment &segment : segments) {
    if (!atPoint(segment)) {
      continue;
    }
    const EntityKey far = farEnd(segment, command.Point);
    if (onPoint != nullptr && far == onPoint->Point) {
      plan.Destroyed.push_back(segment.Key);
      continue;
    }
    const std::optional<ParkPoint> farAt = positionOf(world, far);
    if (far == command.Point || !farAt) {
      return false;
    }
    const size_t farEntry = pointEntry(plan, far, *farAt);
    PlanLine &line =
        addLine(kept, segment.Key, segment.Kind, segment.Width,
                segment.From == command.Point ? moved : farEntry,
                segment.To == command.Point ? moved : farEntry, segment.FromEdge, segment.ToEdge);
    // Ends that arrive at the target point are added there.
    line.FromAdded = onPoint != nullptr && segment.From == command.Point;
    line.ToAdded = onPoint != nullptr && segment.To == command.Point;
  }
  if (onPoint != nullptr) {
    plan.Destroyed.push_back(command.Point);
    // A target left with no segment goes too.
    const bool targetUsed =
        !plan.Lines.empty() || std::ranges::any_of(segments, [&](const ParkSegment &segment) {
          return !atPoint(segment) && endsAt(segment, onPoint->Point);
        });
    if (!targetUsed) {
      plan.Destroyed.push_back(onPoint->Point);
    }
  }
  if (const auto *onSegment = std::get_if<OnSegment>(&command.To)) {
    place(splitOf(kept, onSegment->Segment), moved, onSegment->Along);
  } else if (const auto *onBend = std::get_if<OnBend>(&command.To)) {
    place(bendSplitOf(corners, segments, NULL_KEY, kept, onBend->Point), moved, onBend->Along);
  }
  if (!linesHaveGround(plan, lists.Line)) {
    return false;
  }
  addMeetings(world, kept, segments, command.Point, corners, lists.Meeting);
  return finishPlan(plan);
}

// The plan of an InsertPoint that names what it acts on, in kept. False when it names nothing. The
// parts lie along the segment, or along the arc and the lines touching it, which have met
// everything they meet already, so the plan finds no meetings.
bool planInsert(const World &world, PlanLists &kept, const InsertPoint &command) {
  const std::vector<ParkSegment> &segments = kept.World.Ground.Segments;
  const std::vector<ParkCorner> &corners = kept.World.Ground.Corners;
  EditPlan &plan = kept.Plan;
  if (const auto *onBend = std::get_if<OnBend>(&command.At)) {
    if (!isBendOfKind(world, segments, corners, *onBend, std::nullopt)) {
      return false;
    }
    const std::optional<ParkPoint> at = positionIn(world, corners, *onBend);
    if (!at) {
      return false;
    }
    plan.Points.push_back(PlanPoint{*at, NULL_KEY});
    place(bendSplitOf(corners, segments, NULL_KEY, kept, onBend->Point), 0, onBend->Along);
    return finishPlan(plan);
  }
  const auto *on = std::get_if<OnSegment>(&command.At);
  const ParkSegment *segment = findSegment(segments, on->Segment);
  if (segment == nullptr || segment->Line.empty() || !std::isfinite(on->Along) ||
      on->Along <= 0.0 || on->Along >= 1.0) {
    return false;
  }
  const std::optional<ParkPoint> at = positionIn(world, corners, *on);
  if (!at) {
    return false;
  }
  plan.Points.push_back(PlanPoint{*at, NULL_KEY});
  place(splitOf(kept, on->Segment), 0, on->Along);
  return finishPlan(plan);
}

// The plan of a RemovePoint whose point has two segments it can merge, in kept. False when it has
// none.
bool planRemove(const World &world, PlanLists &kept, EditLists &lists, const RemovePoint &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr) {
    return false;
  }
  const std::vector<ParkSegment> &segments = kept.World.Ground.Segments;
  const std::vector<ParkCorner> &corners = kept.World.Ground.Corners;
  const std::optional<SegmentPair> pair = mergingPair(segments, command.Point);
  if (!pair) {
    return false;
  }
  const ParkSegment &low = *(*pair)[0];
  const ParkSegment &high = *(*pair)[1];
  const EntityKey lowFar = farEnd(low, command.Point);
  const EntityKey highFar = farEnd(high, command.Point);
  const std::optional<ParkPoint> lowAt = positionOf(world, lowFar);
  const std::optional<ParkPoint> highAt = positionOf(world, highFar);
  if (!lowAt || !highAt) {
    return false;
  }
  EditPlan &plan = kept.Plan;
  const size_t lowEntry = pointEntry(plan, lowFar, *lowAt);
  const size_t highEntry = pointEntry(plan, highFar, *highAt);
  // The end at the point takes the high segment's far end, with its edge radius there.
  const double highEdge = edgeAt(high, highFar);
  addLine(kept, low.Key, low.Kind, std::max(low.Width, high.Width),
          low.From == command.Point ? highEntry : lowEntry,
          low.To == command.Point ? highEntry : lowEntry,
          low.From == command.Point ? highEdge : low.FromEdge,
          low.To == command.Point ? highEdge : low.ToEdge);
  // The merged segment runs as low does, so high's furnishing turns when high ran the other way.
  const Furnishing highFurnished = furnishingOf(world, high.Key);
  if (isFurnished(highFurnished)) {
    const bool sameWay = (low.To == command.Point) == (high.From == command.Point);
    plan.Inheritor = low.Key;
    plan.Inherits = sameWay ? highFurnished : turned(highFurnished);
  }
  plan.Destroyed.push_back(high.Key);
  plan.Destroyed.push_back(command.Point);
  if (!linesHaveGround(plan, lists.Line)) {
    return false;
  }
  addMeetings(world, kept, segments, command.Point, corners, lists.Meeting);
  return finishPlan(plan);
}

// The plan of a DeleteSegment that names a segment, in kept: the segment, and each of its points
// no other segment uses. False when it names none.
bool planDeleteSegment(const World &world, PlanLists &kept, const DeleteSegment &command) {
  if (findIntent<PathSegmentIntent>(world, command.Segment) == nullptr) {
    return false;
  }
  const std::vector<ParkSegment> &segments = kept.World.Ground.Segments;
  const ParkSegment &segment = *findSegment(segments, command.Segment);
  EditPlan &plan = kept.Plan;
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
  return true;
}

// The plan of a SetCornerRadius that names a path point with a radius finite and at least 0.
bool planRadius(const World &world, PlanLists &kept, const SetCornerRadius &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr ||
      !std::isfinite(command.Radius) || command.Radius < 0.0) {
    return false;
  }
  kept.Plan.Rounded = command.Point;
  kept.Plan.RoundedTo = command.Radius;
  return true;
}

// The plan of a SetTaperLength that names a path point with a length finite and at least 0.
bool planTaper(const World &world, PlanLists &kept, const SetTaperLength &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr ||
      !std::isfinite(command.Length) || command.Length < 0.0) {
    return false;
  }
  kept.Plan.Tapered = command.Point;
  kept.Plan.TaperedTo = command.Length;
  return true;
}

// The plan of a SetEdgeRadius that names a segment with a ground line, one of its two points, and a
// radius finite and at least 0.
bool planEdge(PlanLists &kept, const SetEdgeRadius &command) {
  const ParkSegment *segment = findSegment(kept.World.Ground.Segments, command.Segment);
  if (segment == nullptr || segment->Line.empty() || !endsAt(*segment, command.Point) ||
      !std::isfinite(command.Radius) || command.Radius < 0.0) {
    return false;
  }
  kept.Plan.Edged = segment->Key;
  kept.Plan.EdgedAtFrom = segment->From == command.Point;
  kept.Plan.EdgedTo = command.Radius;
  return true;
}

// The plan of a SetSegmentWidth that names a segment with a ground line and a width finite and
// above zero: the segment reshaped between its own points, at the width. It meets nothing it
// didn't meet before.
bool planWidth(const World &world, PlanLists &kept, const SetSegmentWidth &command) {
  const ParkSegment *segment = findSegment(kept.World.Ground.Segments, command.Segment);
  if (segment == nullptr || segment->Line.empty() || !std::isfinite(command.Width) ||
      !(command.Width > 0.0)) {
    return false;
  }
  const std::optional<ParkPoint> fromAt = positionOf(world, segment->From);
  const std::optional<ParkPoint> toAt = positionOf(world, segment->To);
  if (!fromAt || !toAt) {
    return false;
  }
  EditPlan &plan = kept.Plan;
  const size_t from = pointEntry(plan, segment->From, *fromAt);
  const size_t to = pointEntry(plan, segment->To, *toAt);
  addLine(kept, segment->Key, segment->Kind, command.Width, from, to, segment->FromEdge,
          segment->ToEdge);
  return true;
}

// Where applyPlan makes a plan's changes: a world.
class WorldStore {
public:
  explicit WorldStore(World &world) : Target(&world) {}
  void destroy(EntityKey key) { Target->destroyEntity(key); }
  EntityKey addPoint(const PathPointIntent &point) {
    const EntityKey key = Target->createEntity();
    Target->emplace<PathPointIntent>(key, point);
    return key;
  }
  EntityKey addSegment(const PathSegmentIntent &segment) {
    const EntityKey key = Target->createEntity();
    Target->emplace<PathSegmentIntent>(key, segment);
    return key;
  }
  PathPointIntent &point(EntityKey key) { return Target->get<PathPointIntent>(key); }
  PathSegmentIntent &segment(EntityKey key) { return Target->get<PathSegmentIntent>(key); }
  Furnishing furnished(EntityKey key) const {
    Furnishing furnishing;
    if (const auto *left = Target->tryGet<LeftFurnishingIntent>(key)) {
      furnishing.Left = left->Held;
    }
    if (const auto *right = Target->tryGet<RightFurnishingIntent>(key)) {
      furnishing.Right = right->Held;
    }
    return furnishing;
  }
  void setFurnished(EntityKey key, const Furnishing &furnishing) {
    setFurnishingOf(*Target, key, furnishing);
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
  // A layout holds no furnishing, since no path's shape depends on it.
  static Furnishing furnished(EntityKey /*key*/) { return {}; }
  void setFurnished(EntityKey /*key*/, const Furnishing & /*furnishing*/) {}

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
                    const AddedEnds &report, std::vector<EntityKey> &chain) {
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
  // The new parts run from the first segment toward the second, so a segment's furnishing turns
  // when it ran the other way: the first from the point, or the second into it.
  const Furnishing firstFurnished = first.From == point ? turned(store.furnished(bend.Segments[0]))
                                                        : store.furnished(bend.Segments[0]);
  const Furnishing secondFurnished = second.To == point ? turned(store.furnished(bend.Segments[1]))
                                                        : store.furnished(bend.Segments[1]);
  (second.From == point ? second.From : second.To) = keys[bend.Points.back()];
  const double width = second.Width;
  // The corner's point, then each place followed by the corner point after it.
  chain.clear();
  chain.push_back(point);
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
    store.setFurnished(part, j + 2 == chain.size() ? secondFurnished : firstFurnished);
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
  const Furnishing furnished = store.furnished(split.Segment);
  store.segment(split.Segment).To = keys[split.At.front().Point];
  store.segment(split.Segment).ToEdge = DEFAULT_EDGE_RADIUS;
  report.report(split.Segment, 1, report.existing(split.At.front().Point));
  for (size_t k = 0; k < split.At.size(); ++k) {
    const bool last = k + 1 == split.At.size();
    const EntityKey to = last ? was.To : keys[split.At[k + 1].Point];
    const EntityKey part = store.addSegment(
        PathSegmentIntent{was.Kind, keys[split.At[k].Point], to, was.Width, DEFAULT_EDGE_RADIUS,
                          last ? was.ToEdge : DEFAULT_EDGE_RADIUS});
    store.setFurnished(part, furnished);
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
      // Every later part of a reshaped segment keeps its furnishing.
      if (line.Key != NULL_KEY) {
        store.setFurnished(key, store.furnished(line.Key));
      }
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
// point. keys and chain are lists it reuses.
template <typename Store>
void applyPlan(Store &store, const EditPlan &plan, std::vector<EntityKey> &keys,
               std::vector<EntityKey> &chain, std::vector<SegmentEnd> *added = nullptr) {
  for (const EntityKey key : plan.Destroyed) {
    store.destroy(key);
  }
  keys.clear();
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
  if (plan.Tapered != NULL_KEY) {
    store.point(plan.Tapered).Taper = plan.TaperedTo;
  }
  if (plan.Edged != NULL_KEY) {
    PathSegmentIntent &edged = store.segment(plan.Edged);
    (plan.EdgedAtFrom ? edged.FromEdge : edged.ToEdge) = plan.EdgedTo;
  }
  for (const BendSplit &bend : plan.BendSplits) {
    applyBendSplit(store, bend, keys, report, chain);
  }
  for (const Split &split : plan.Splits) {
    applySplit(store, split, keys, report);
  }
  // Before the lines, so every part of a merged segment that splits takes what it inherits.
  if (plan.Inheritor != NULL_KEY) {
    Furnishing kept = store.furnished(plan.Inheritor);
    kept.Left = kept.Left ? kept.Left : plan.Inherits.Left;
    kept.Right = kept.Right ? kept.Right : plan.Inherits.Right;
    store.setFurnished(plan.Inheritor, kept);
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

// Sorts the keys and drops repeats.
void sortUnique(std::vector<EntityKey> &keys) {
  std::ranges::sort(keys);
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

// Empties the surface's lists, keeping their memory.
void clearSurface(PavedSurface &surface) {
  surface.Pieces.clear();
  surface.Edges.clear();
  surface.EdgePoints.clear();
  surface.Sides.clear();
  surface.SidePoints.clear();
}

// Puts in changed the pieces and edge lines of after whose owner's pieces or edge lines differ from
// before's. Both hold the surface of the same owners.
void changedOwners(const PavedSurface &before, const PavedSurface &after, PavedSurface &changed) {
  // Each owner's run of items in a list in ascending owner order.
  const auto runOf = [](const auto &items, EntityKey owner) {
    return std::ranges::equal_range(items, owner, {}, [](const auto &item) { return item.Owner; });
  };
  const auto sameEdges = [&before, &after](const auto &was, const auto &is) {
    return std::ranges::equal(was, is, [&before, &after](const EdgeLine &x, const EdgeLine &y) {
      return x.Kind == y.Kind && std::ranges::equal(edgePoints(before, x), edgePoints(after, y));
    });
  };
  clearSurface(changed);
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
}

// Whether the segment's record or line differs from the one before holds under its key, or before
// holds none.
bool differs(const std::vector<ParkSegment> &before, const ParkSegment &segment) {
  const ParkSegment *was = findSegment(before, segment.Key);
  return was == nullptr || was->From != segment.From || was->To != segment.To ||
         was->Line != segment.Line || was->Width != segment.Width ||
         was->FromEdge != segment.FromEdge || was->ToEdge != segment.ToEdge;
}

// Puts in points the points whose surface an edit can change: each end of each segment it adds,
// removes, or changes, before and after it.
void pointsChanged(const std::vector<ParkSegment> &before, const std::vector<ParkSegment> &after,
                   std::vector<EntityKey> &points) {
  points.clear();
  for (const ParkSegment &segment : after) {
    if (differs(before, segment)) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  for (const ParkSegment &segment : before) {
    if (differs(after, segment)) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  sortUnique(points);
}

// Puts in points the changed points, which are sorted, and the other end of each segment at one of
// them, before or after.
void withFarEnds(const std::vector<ParkSegment> &before, const std::vector<ParkSegment> &after,
                 const std::vector<EntityKey> &changed, std::vector<EntityKey> &points) {
  copyGrowing(points, std::span<const EntityKey>(changed));
  for (const std::vector<ParkSegment> *segments : {&before, &after}) {
    for (const ParkSegment &segment : *segments) {
      if (std::ranges::binary_search(changed, segment.From) ||
          std::ranges::binary_search(changed, segment.To)) {
        points.push_back(segment.From);
        points.push_back(segment.To);
      }
    }
  }
  sortUnique(points);
}

// What editedBetween finds: only the pieces of the surface it changes, which is all the validity
// check needs, or the pieces and edge lines, as the edit would be drawn.
enum class Wanted { Pieces, Drawing };

// Puts in lists.Edited what changes from before, the world's layout and ground, to after, whose
// ground is groundAfter, and in lists.EditedSegments the index in groundAfter of each segment the
// change adds or whose ground line or width it changes. Extra holds points whose paving can change
// though no segment at them does, as when a taper length changes.
void editedBetween(const GroundLists &before, const PathLayout &after,
                   const PathGround &groundAfter, std::span<const EntityKey> extra, Wanted wanted,
                   EditLists &lists, TickBuffers &buffers) {
  const PathGround &groundBefore = before.Ground;
  std::vector<EntityKey> &changed = lists.Changed;
  std::vector<EntityKey> &points = lists.Points;
  pointsChanged(groundBefore.Segments, groundAfter.Segments, changed);
  changed.insert(changed.end(), extra.begin(), extra.end());
  sortUnique(changed);
  // A segment's paving at one end depends on whether its other end is free, which can change
  // whenever a changed point loses or gains a segment.
  withFarEnds(groundBefore.Segments, groundAfter.Segments, changed, points);
  const bool drawing = wanted == Wanted::Drawing;
  if (drawing) {
    // Paving that changes can hide or show the edge lines of paving near it, before and after.
    copyGrowing(changed, std::span<const EntityKey>(points));
    pointsAffected(before.Layout, groundBefore, points, lists.Near, buffers);
    changed.insert(changed.end(), lists.Near.begin(), lists.Near.end());
    pointsAffected(after, groundAfter, points, lists.Near, buffers);
    changed.insert(changed.end(), lists.Near.begin(), lists.Near.end());
    sortUnique(changed);
    points.swap(changed);
  }
  surfaceAround(before.Layout, groundBefore, points, drawing, lists.Before, buffers);
  surfaceAround(after, groundAfter, points, drawing, lists.After, buffers);
  changedOwners(lists.Before, lists.After, lists.Edited);
  lists.EditedSegments.clear();
  for (size_t i = 0; i < groundAfter.Segments.size(); ++i) {
    const ParkSegment &segment = groundAfter.Segments[i];
    const ParkSegment *was = findSegment(groundBefore.Segments, segment.Key);
    if (was == nullptr || was->Line != segment.Line || was->Width != segment.Width) {
      lists.EditedSegments.push_back(i);
    }
  }
}

// Applies the plan to a copy of the world's layout in kept.After, derives its ground in
// kept.AfterGround, and finds what changes, as editedBetween does.
void editedBy(const World &world, PlanLists &kept, EditLists &lists, TickBuffers &buffers,
              Wanted wanted) {
  kept.After = kept.World.Layout;
  LayoutStore store(kept.After, world.nextKey());
  applyPlan(store, kept.Plan, lists.Keys, lists.Chain);
  groundOf(kept.After, kept.AfterGround);
  // A taper length changes no segment, but it changes the paving at its point.
  lists.Extra.clear();
  if (kept.Plan.Tapered != NULL_KEY) {
    lists.Extra.push_back(kept.Plan.Tapered);
  }
  editedBetween(kept.World, kept.After, kept.AfterGround.Ground, lists.Extra, wanted, lists,
                buffers);
}

// Whether the ground line, held by a path of the width, lies past the park's edge.
bool lineLeavesPark(std::span<const CarrierPoint> points, double width) {
  const double limit = (HALF_PARK - width / 2.0) + CONTACT_TOLERANCE;
  return std::ranges::any_of(points, [limit](const CarrierPoint &point) {
    return std::abs(point.X) > limit || std::abs(point.Z) > limit;
  });
}

// Whether what editedBetween found fits: every edited segment in the ground after has a ground
// line that stays in the park, and every piece of the surface it edits stays in the park and meets
// no solid in lists.Solids.
bool editedFits(const PathGround &groundAfter, const EditLists &lists) {
  return std::ranges::all_of(lists.EditedSegments,
                             [&groundAfter](size_t i) {
                               const ParkSegment &segment = groundAfter.Segments[i];
                               return !segment.Line.empty() &&
                                      !lineLeavesPark(segment.Line, segment.Width);
                             }) &&
         std::ranges::all_of(lists.Edited.Pieces, [&lists](const SurfacePiece &piece) {
           return !leavesPark(piece) && fits(piece, lists.Solids);
         });
}

// Whether the world accepts the plan kept holds: every segment it adds or changes has a ground line
// that stays in the park, and every piece of the surface it adds or changes stays in the park and
// meets no solid.
bool planFits(const World &world, PlanLists &kept, EditLists &lists, TickBuffers &buffers) {
  solidsOf(world, lists.Entrances, lists.Boxes, lists.Solids);
  editedBy(world, kept, lists, buffers, Wanted::Pieces);
  return editedFits(kept.AfterGround.Ground, lists);
}

// Adds to the plan kept holds the curve writes that keep every curve through its joins and splits,
// found by trying it on a copy of the world's layout.
void withCurves(const World &world, PlanLists &kept, EditLists &lists, TickBuffers &buffers) {
  lists.Curved = kept.World.Layout;
  LayoutStore store(lists.Curved, world.nextKey());
  lists.Added.clear();
  applyPlan(store, kept.Plan, lists.Keys, lists.Chain, &lists.Added);
  keptCurves(kept.World.Layout, kept.World.Ground, lists.Curved, lists.Added, kept.Plan.Curves,
             buffers);
}

// Each path command's plan in kept, with the world's layout and ground in kept.World. False when
// the command names nothing it acts on.
bool planOf(const World &world, const AddPath &command, PlanLists &kept, EditLists &lists,
            TickBuffers &buffers) {
  if (!planPath(world, kept, lists, command)) {
    return false;
  }
  withCurves(world, kept, lists, buffers);
  return true;
}
bool planOf(const World &world, const DeleteSegment &command, PlanLists &kept, EditLists &lists,
            TickBuffers &buffers) {
  if (!planDeleteSegment(world, kept, command)) {
    return false;
  }
  withCurves(world, kept, lists, buffers);
  return true;
}
bool planOf(const World &world, const MovePoint &command, PlanLists &kept, EditLists &lists,
            TickBuffers &buffers) {
  if (!planMove(world, kept, lists, command)) {
    return false;
  }
  withCurves(world, kept, lists, buffers);
  return true;
}
bool planOf(const World &world, const InsertPoint &command, PlanLists &kept, EditLists &lists,
            TickBuffers &buffers) {
  if (!planInsert(world, kept, command)) {
    return false;
  }
  withCurves(world, kept, lists, buffers);
  return true;
}
bool planOf(const World &world, const RemovePoint &command, PlanLists &kept, EditLists &lists,
            TickBuffers &buffers) {
  if (!planRemove(world, kept, lists, command)) {
    return false;
  }
  withCurves(world, kept, lists, buffers);
  return true;
}
bool planOf(const World &world, const SetCornerRadius &command, PlanLists &kept,
            EditLists & /*lists*/, TickBuffers & /*buffers*/) {
  return planRadius(world, kept, command);
}
bool planOf(const World &world, const SetSegmentWidth &command, PlanLists &kept,
            EditLists & /*lists*/, TickBuffers & /*buffers*/) {
  return planWidth(world, kept, command);
}
bool planOf(const World & /*world*/, const SetEdgeRadius &command, PlanLists &kept,
            EditLists & /*lists*/, TickBuffers & /*buffers*/) {
  return planEdge(kept, command);
}
bool planOf(const World &world, const SetTaperLength &command, PlanLists &kept,
            EditLists & /*lists*/, TickBuffers & /*buffers*/) {
  return planTaper(world, kept, command);
}

// The commands planned as a change to the path layout. Box commands, SetShopStock,
// SetOpeningHours, SetFurnishing, and RestoreIntent have no plan, and RestoreIntent shows no ghost.
template <typename T>
constexpr bool IS_PLANNED = std::is_same_v<T, AddPath> || std::is_same_v<T, DeleteSegment> ||
                            std::is_same_v<T, MovePoint> || std::is_same_v<T, InsertPoint> ||
                            std::is_same_v<T, RemovePoint> || std::is_same_v<T, SetCornerRadius> ||
                            std::is_same_v<T, SetSegmentWidth> ||
                            std::is_same_v<T, SetEdgeRadius> || std::is_same_v<T, SetTaperLength>;

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
  schema.addCommand<SetTaperLength>();
  schema.addCommand<SetShopStock>();
  schema.addCommand<SetOpeningHours>();
  schema.addCommand<SetFurnishing>();
  schema.addCommand<RestoreIntent>();
}

std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop) {
  return positionWith(world, stop,
                      [&world](EntityKey point) { return roundedIn(parkCorners(world), point); });
}

bool appendStopPosition(const World &world, std::span<const ParkCorner> corners,
                        const PathStop &stop, std::vector<ParkPoint> &positions) {
  if (const auto *repeat = std::get_if<RepeatStop>(&stop)) {
    if (repeat->Stop >= positions.size()) {
      return false;
    }
    positions.push_back(positions[repeat->Stop]);
    return true;
  }
  if (const auto *on = std::get_if<OnDrawnSegment>(&stop)) {
    if (on->Stop + 1 >= positions.size()) {
      return false;
    }
    const ParkPoint &from = positions[on->Stop];
    const ParkPoint &to = positions[on->Stop + 1];
    positions.push_back(
        ParkPoint{from.X + on->Along * (to.X - from.X), from.Z + on->Along * (to.Z - from.Z)});
    return true;
  }
  const std::optional<ParkPoint> position = positionIn(world, corners, stop);
  if (!position) {
    return false;
  }
  positions.push_back(*position);
  return true;
}

std::optional<std::vector<ParkPoint>> stopPositions(const World &world, const AddPath &path) {
  // The corners are derived only for a path with an OnBend stop.
  std::vector<ParkCorner> corners;
  if (std::ranges::any_of(
          path.Stops, [](const PathStop &stop) { return std::holds_alternative<OnBend>(stop); })) {
    corners = parkCorners(world);
  }
  std::vector<ParkPoint> positions;
  for (const PathStop &stop : path.Stops) {
    if (!appendStopPosition(world, corners, stop, positions)) {
      return std::nullopt;
    }
  }
  return positions;
}

namespace {

// The pieces of the surface of the layout, with its ground, that can reach the solid. Every piece
// of a point's or of a segment's lies within the longer of one of the point's segments' length and
// the widest width, plus the widest width, from the point, so only the points that near the solid's
// bounds and their segments are built. Puts them in lists.NearSolid's pieces.
void piecesNear(const PathLayout &layout, const PathGround &ground, const Solid &solid,
                EditLists &lists, TickBuffers &buffers) {
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
  std::vector<EntityKey> &focus = lists.Focus;
  focus.clear();
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
  sortUnique(focus);
  surfaceAround(layout, ground, focus, false, lists.NearSolid, buffers);
}

bool isListed(const std::vector<EntityKey> &keys, EntityKey key) {
  return std::ranges::find(keys, key) != keys.end();
}

bool isAtLeastZero(double value) { return std::isfinite(value) && value >= 0.0; }

bool isPointValid(const ParkPathPoint &point) {
  return std::isfinite(point.At.X) && std::isfinite(point.At.Z) && isAtLeastZero(point.Radius) &&
         isAtLeastZero(point.Taper);
}

bool isBoxValid(const ParkBox &box) {
  return isKnown(box.Kind) && isKnown(box.Stock) && std::isfinite(box.At.X) &&
         std::isfinite(box.At.Z) && std::isfinite(box.At.FacingX) && std::isfinite(box.At.FacingZ);
}

bool isTimeOfDay(uint64_t second) { return second < DAY_SECONDS; }

bool areHoursValid(const OpeningHours &hours) {
  return isTimeOfDay(hours.Opening) && isTimeOfDay(hours.Closing);
}

// Whether the restore names what it acts on, as the spec's RestoreIntent acceptance lists, apart
// from physical validity. Before is the world's layout.
bool namesValid(const World &world, const PathLayout &before, const RestoreIntent &command,
                std::vector<EntityKey> &names) {
  const auto removed = [&command](EntityKey key) { return isListed(command.Remove, key); };
  if (command.Add.Hours || (command.Set.Hours && !areHoursValid(*command.Set.Hours))) {
    return false;
  }
  if (!std::ranges::all_of(command.Remove, [&world](EntityKey key) {
        return findIntent<PathPointIntent>(world, key) != nullptr ||
               findIntent<PathSegmentIntent>(world, key) != nullptr ||
               findIntent<BoxIntent>(world, key) != nullptr;
      })) {
    return false;
  }
  const auto setsKept = [&removed](const auto &records, const auto &holds) {
    return std::ranges::all_of(
        records, [&](const auto &record) { return holds(record.Key) && !removed(record.Key); });
  };
  if (!setsKept(command.Set.Points,
                [&world](EntityKey key) { return findIntent<PathPointIntent>(world, key); }) ||
      !setsKept(command.Set.Segments,
                [&world](EntityKey key) { return findIntent<PathSegmentIntent>(world, key); }) ||
      !setsKept(command.Set.Boxes,
                [&world](EntityKey key) { return findIntent<BoxIntent>(world, key); })) {
    return false;
  }
  names.clear();
  for (const ParkPathPoint &point : command.Add.Points) {
    names.push_back(point.Key);
  }
  for (const SegmentRecord &segment : command.Add.Segments) {
    names.push_back(segment.Key);
  }
  for (const ParkBox &box : command.Add.Boxes) {
    names.push_back(box.Key);
  }
  std::ranges::sort(names);
  if (std::ranges::adjacent_find(names) != names.end() ||
      std::ranges::any_of(names, [&world](EntityKey key) { return world.isLive(key); })) {
    return false;
  }
  const auto pointLeft = [&](EntityKey key) {
    return std::ranges::find(command.Add.Points, key, &ParkPathPoint::Key) !=
               command.Add.Points.end() ||
           (findIntent<PathPointIntent>(world, key) != nullptr && !removed(key));
  };
  const auto segmentValid = [&pointLeft](const SegmentRecord &segment) {
    return isKnown(segment.Kind) && segment.From != segment.To && pointLeft(segment.From) &&
           pointLeft(segment.To) && std::isfinite(segment.Width) && segment.Width > 0.0 &&
           isAtLeastZero(segment.FromEdge) && isAtLeastZero(segment.ToEdge) &&
           isFurnishingValid(segment.Furnished, segment.Kind);
  };
  for (const IntentRecords *records : {&command.Set, &command.Add}) {
    if (!std::ranges::all_of(records->Points, isPointValid) ||
        !std::ranges::all_of(records->Segments, segmentValid) ||
        !std::ranges::all_of(records->Boxes, isBoxValid)) {
      return false;
    }
  }
  // A segment the restore leaves as it is must still end at path points.
  return std::ranges::all_of(before.Segments, [&](const LayoutSegment &segment) {
    const bool touched = removed(segment.Key) ||
                         std::ranges::find(command.Set.Segments, segment.Key,
                                           &SegmentRecord::Key) != command.Set.Segments.end();
    return touched || (!removed(segment.Intent.From) && !removed(segment.Intent.To));
  });
}

// Makes the restore's changes to path points and segments in the store: removes, then adds points
// and segments, which take keys in that order, then sets. Removing a box's key in a layout changes
// nothing. added is a list it reuses, of each added point's record key and its new key.
template <typename Store>
void applyRestore(Store &store, const RestoreIntent &command,
                  std::vector<std::pair<EntityKey, EntityKey>> &added) {
  for (const EntityKey key : command.Remove) {
    store.destroy(key);
  }
  added.clear();
  for (const ParkPathPoint &point : command.Add.Points) {
    added.emplace_back(point.Key,
                       store.addPoint(PathPointIntent{point.At, point.Radius, point.Taper}));
  }
  std::ranges::sort(added);
  const auto intentOf = [&added](const SegmentRecord &segment) {
    const auto keyOf = [&added](EntityKey key) {
      const auto found =
          std::ranges::lower_bound(added, key, {}, &std::pair<EntityKey, EntityKey>::first);
      return found != added.end() && found->first == key ? found->second : key;
    };
    return PathSegmentIntent{segment.Kind,  keyOf(segment.From), keyOf(segment.To),
                             segment.Width, segment.FromEdge,    segment.ToEdge};
  };
  for (const SegmentRecord &segment : command.Add.Segments) {
    store.setFurnished(store.addSegment(intentOf(segment)), segment.Furnished);
  }
  for (const ParkPathPoint &point : command.Set.Points) {
    store.point(point.Key) = PathPointIntent{point.At, point.Radius, point.Taper};
  }
  for (const SegmentRecord &segment : command.Set.Segments) {
    store.segment(segment.Key) = intentOf(segment);
    store.setFurnished(segment.Key, segment.Furnished);
  }
}

// Puts in solids the solids after the restore: the world's, less the boxes it removes, with the
// poses it sets, and with the boxes it adds, keyed from firstBoxKey on.
void restoredSolids(const World &world, const RestoreIntent &command, uint64_t firstBoxKey,
                    EditLists &lists) {
  std::vector<Solid> &solids = lists.Solids;
  solidsOf(world, lists.Entrances, lists.Boxes, solids);
  std::erase_if(solids, [&command](const Solid &solid) {
    const bool set =
        std::ranges::find(command.Set.Boxes, solid.Key, &ParkBox::Key) != command.Set.Boxes.end();
    return set || isListed(command.Remove, solid.Key);
  });
  for (const ParkBox &box : command.Set.Boxes) {
    if (const std::optional<Solid> solid = solidOf(box.Key, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
  for (size_t i = 0; i < command.Add.Boxes.size(); ++i) {
    const ParkBox &box = command.Add.Boxes[i];
    if (const std::optional<Solid> solid =
            solidOf(EntityKey{firstBoxKey + i}, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
}

// Whether the solid stays in the park and conflicts with no other solid in lists.Solids and no
// piece of the layout's surface, with its ground.
bool solidFits(const PathLayout &layout, const PathGround &ground, const Solid &solid,
               EditLists &lists, TickBuffers &buffers) {
  piecesNear(layout, ground, solid, lists, buffers);
  return fits(solid, lists.Solids, lists.NearSolid.Pieces);
}

// Whether the world accepts the restore: it names what it acts on, every segment it adds or
// changes has a ground line that stays in the park, every piece of the surface it adds or changes
// stays in the park and meets no solid, and every box it sets or adds stays in the park and
// conflicts with no other solid and no piece of the surface. Keeps the layouts and grounds before
// and after in kept.
bool isRestoreAccepted(const World &world, const RestoreIntent &command, PlanLists &kept,
                       EditLists &lists, TickBuffers &buffers) {
  worldGround(world, kept.World);
  const PathLayout &before = kept.World.Layout;
  if (!namesValid(world, before, command, lists.Names)) {
    return false;
  }
  kept.After = before;
  LayoutStore store(kept.After, world.nextKey());
  applyRestore(store, command, lists.AddedKeys);
  // A point's radius or taper can change the paving at it without changing a segment's record.
  lists.Extra.clear();
  for (const ParkPathPoint &point : command.Set.Points) {
    lists.Extra.push_back(point.Key);
  }
  for (size_t i = 0; i < command.Add.Points.size(); ++i) {
    lists.Extra.push_back(EntityKey{world.nextKey() + i});
  }
  const uint64_t firstBoxKey =
      world.nextKey() + command.Add.Points.size() + command.Add.Segments.size();
  restoredSolids(world, command, firstBoxKey, lists);
  groundOf(kept.After, kept.AfterGround);
  const PathGround &groundAfter = kept.AfterGround.Ground;
  editedBetween(kept.World, kept.After, groundAfter, lists.Extra, Wanted::Pieces, lists, buffers);
  if (!editedFits(groundAfter, lists)) {
    return false;
  }
  const auto boxFits = [&](EntityKey key, const ParkBox &box) {
    const std::optional<Solid> solid = solidOf(key, box.At, boxSize(box.Kind));
    return solid && solidFits(kept.After, groundAfter, *solid, lists, buffers);
  };
  for (const ParkBox &box : command.Set.Boxes) {
    if (!boxFits(box.Key, box)) {
      return false;
    }
  }
  for (size_t i = 0; i < command.Add.Boxes.size(); ++i) {
    if (!boxFits(EntityKey{firstBoxKey + i}, command.Add.Boxes[i])) {
      return false;
    }
  }
  return true;
}

// Whether the box of the size at the pose stays in the park and conflicts with no other solid and
// no piece of the surface. A box with the key is the one being moved, so it is skipped.
bool boxFits(const World &world, EntityKey key, const Pose &at, FootprintSize size, PlanLists &kept,
             EditLists &lists, TickBuffers &buffers) {
  const std::optional<Solid> solid = solidOf(key, at, size);
  if (!solid) {
    return false;
  }
  worldGround(world, kept.World);
  solidsOf(world, lists.Entrances, lists.Boxes, lists.Solids);
  return solidFits(kept.World.Layout, kept.World.Ground, *solid, lists, buffers);
}

// Gives the box the stock: removes its ShopStockIntent for food, so a shop at food saves as one
// never set, and holds the stock otherwise.
void setStock(World &world, EntityKey box, ShopStock stock) {
  if (stock == ShopStock::Food) {
    world.remove<ShopStockIntent>(box);
  } else {
    world.emplaceOrReplace<ShopStockIntent>(box, ShopStockIntent{stock});
  }
}

// Gives the park the hours: destroys the entity holding them for DEFAULT_HOURS, so a park at the
// default saves as one never set, and otherwise sets them on that entity, creating it if none holds
// them. holders is a list it reuses.
void setHours(World &world, const OpeningHours &hours, std::vector<EntityKey> &holders) {
  world.keysWith<OpeningHoursIntent>(holders);
  const std::optional<EntityKey> holder =
      holders.empty() ? std::nullopt : std::optional<EntityKey>(holders.front());
  if (hours == DEFAULT_HOURS) {
    if (holder) {
      world.destroyEntity(*holder);
    }
  } else if (holder) {
    world.get<OpeningHoursIntent>(*holder).Hours = hours;
  } else {
    const EntityKey key = world.createEntity();
    world.emplace<OpeningHoursIntent>(key, OpeningHoursIntent{hours});
  }
}

// Plans the command into kept: whether it has a plan, whether the world accepts it, and the
// positions of its crossings when it does.
template <typename T>
void planInto(const World &world, const T &command, PlanLists &kept, EditLists &lists,
              TickBuffers &buffers) {
  kept.HasPlan = false;
  kept.Accepted = false;
  kept.Crossings.clear();
  if constexpr (IS_PLANNED<T>) {
    clearPlan(kept);
    worldGround(world, kept.World);
    kept.HasPlan = planOf(world, command, kept, lists, buffers);
    kept.Accepted = kept.HasPlan && planFits(world, kept, lists, buffers);
    if (kept.Accepted) {
      for (const size_t point : kept.Plan.Crossings) {
        kept.Crossings.push_back(kept.Plan.Points[point].At);
      }
    }
  } else if constexpr (std::is_same_v<T, AddBox>) {
    kept.Accepted = isKnown(command.Kind) && boxFits(world, NULL_KEY, command.At,
                                                     boxSize(command.Kind), kept, lists, buffers);
  } else if constexpr (std::is_same_v<T, MoveBox>) {
    const BoxIntent *box = findIntent<BoxIntent>(world, command.Box);
    kept.Accepted = box != nullptr && boxFits(world, command.Box, command.At, boxSize(box->Kind),
                                              kept, lists, buffers);
  } else if constexpr (std::is_same_v<T, RestoreIntent>) {
    kept.Accepted = isRestoreAccepted(world, command, kept, lists, buffers);
  } else {
    kept.Accepted = isAccepted(world, command);
  }
}

// Makes the change applying the command makes, from its plan in kept, when the world accepted it.
template <typename T>
void applyInto(World &world, const T &command, const PlanLists &kept, EditLists &lists) {
  if (!kept.Accepted) {
    return;
  }
  if constexpr (IS_PLANNED<T>) {
    WorldStore store(world);
    applyPlan(store, kept.Plan, lists.Keys, lists.Chain);
  } else if constexpr (std::is_same_v<T, AddBox>) {
    const EntityKey key = world.createEntity();
    world.emplace<BoxIntent>(key, BoxIntent{command.Kind, command.At});
  } else if constexpr (std::is_same_v<T, MoveBox>) {
    world.get<BoxIntent>(command.Box).At = command.At;
  } else if constexpr (std::is_same_v<T, DeleteBox>) {
    world.destroyEntity(command.Box);
  } else if constexpr (std::is_same_v<T, SetShopStock>) {
    setStock(world, command.Shop, command.Stock);
  } else if constexpr (std::is_same_v<T, SetOpeningHours>) {
    setHours(world, command.Hours, lists.Names);
  } else if constexpr (std::is_same_v<T, SetFurnishing>) {
    setFurnishingOf(world, command.Segment, command.To);
  } else {
    static_assert(std::is_same_v<T, RestoreIntent>, "every command is applied");
    WorldStore store(world);
    applyRestore(store, command, lists.AddedKeys);
    for (const ParkBox &box : command.Set.Boxes) {
      world.get<BoxIntent>(box.Key) = BoxIntent{box.Kind, box.At};
      setStock(world, box.Key, box.Stock);
    }
    for (const ParkBox &box : command.Add.Boxes) {
      const EntityKey key = world.createEntity();
      world.emplace<BoxIntent>(key, BoxIntent{box.Kind, box.At});
      setStock(world, key, box.Stock);
    }
    // Last, so the records added above take the keys the history expects.
    if (command.Set.Hours) {
      setHours(world, *command.Set.Hours, lists.Names);
    }
  }
}

// Whether the world accepts the command, planned with lists of its own.
template <typename T> bool acceptsOnce(const World &world, const T &command) {
  PlannedEdit planned;
  TickBuffers buffers;
  planInto(world, command, *planned.Lists, buffers.get<EditLists>(), buffers);
  return planned.Lists->Accepted;
}

// Plans the command with lists kept in the buffers and applies it.
template <typename T> void applyWith(World &world, const T &command, TickBuffers &buffers) {
  PlanLists &kept = *buffers.get<PlannedEdit>().Lists;
  EditLists &lists = buffers.get<EditLists>();
  planInto(world, command, kept, lists, buffers);
  applyInto(world, command, kept, lists);
}

} // namespace

bool isAccepted(const World &world, const AddPath &command) { return acceptsOnce(world, command); }

bool isAccepted(const World &world, const AddBox &command) { return acceptsOnce(world, command); }

bool isAccepted(const World &world, const MoveBox &command) { return acceptsOnce(world, command); }

bool isAccepted(const World &world, const DeleteSegment &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const DeleteBox &command) {
  return findIntent<BoxIntent>(world, command.Box) != nullptr;
}

bool isAccepted(const World &world, const MovePoint &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const InsertPoint &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const RemovePoint &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const SetCornerRadius &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const SetSegmentWidth &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const SetEdgeRadius &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const SetTaperLength &command) {
  return acceptsOnce(world, command);
}

bool isAccepted(const World &world, const SetShopStock &command) {
  const BoxIntent *box = findIntent<BoxIntent>(world, command.Shop);
  return box != nullptr && box->Kind == BoxKind::Shop && isKnown(command.Stock);
}

bool isAccepted(const World & /*world*/, const SetOpeningHours &command) {
  return areHoursValid(command.Hours);
}

bool isAccepted(const World &world, const SetFurnishing &command) {
  const PathSegmentIntent *segment = findIntent<PathSegmentIntent>(world, command.Segment);
  return segment != nullptr && segment->Kind == PathKind::Guest &&
         isFurnishingValid(command.To, PathKind::Guest);
}

bool isAccepted(const World &world, const RestoreIntent &command) {
  return acceptsOnce(world, command);
}

void applyCommand(World &world, const AddPath &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const AddPath &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const AddBox &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const AddBox &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const MoveBox &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const MoveBox &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const DeleteSegment &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const DeleteSegment &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const DeleteBox &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const DeleteBox &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const MovePoint &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const MovePoint &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const InsertPoint &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const InsertPoint &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const RemovePoint &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const RemovePoint &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetCornerRadius &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetCornerRadius &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetSegmentWidth &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetSegmentWidth &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetEdgeRadius &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetEdgeRadius &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetTaperLength &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetTaperLength &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetShopStock &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetShopStock &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetOpeningHours &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetOpeningHours &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const SetFurnishing &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const SetFurnishing &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

void applyCommand(World &world, const RestoreIntent &command) {
  TickBuffers buffers;
  applyCommand(world, command, buffers);
}

void applyCommand(World &world, const RestoreIntent &command, TickBuffers &buffers) {
  applyWith(world, command, buffers);
}

namespace {

// A plan of the edit made with lists of its own, for callers that ask about an edit once.
struct OwnPlan {
  PlannedEdit Planned;
  TickBuffers Buffers;
};

OwnPlan planOnce(const World &world, const ParkEdit &edit) {
  OwnPlan own;
  planEdit(world, edit, own.Planned, own.Buffers);
  return own;
}

// Each segment the PlanLists' after ground holds at an index in the list, refilled into a list
// the caller keeps as parkSegments' kept form refills one, with no furnishing.
void editedSegmentsOf(const PathGround &groundAfter, const std::vector<size_t> &indexes,
                      std::vector<ParkSegment> &segments, std::vector<ParkSegment> &spare) {
  resizeKeeping(segments, indexes.size(), spare);
  for (size_t i = 0; i < indexes.size(); ++i) {
    const ParkSegment &from = groundAfter.Segments[indexes[i]];
    ParkSegment &segment = segments[i];
    segment.Key = from.Key;
    segment.Kind = from.Kind;
    segment.From = from.From;
    segment.To = from.To;
    segment.Width = from.Width;
    copyGrowing(segment.Line, std::span<const CarrierPoint>(from.Line));
    segment.FromEdge = from.FromEdge;
    segment.ToEdge = from.ToEdge;
    segment.Furnished = {};
  }
}

} // namespace

std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path) {
  const std::span<const ParkPoint> crossings = pathCrossings(planOnce(world, path).Planned);
  return {crossings.begin(), crossings.end()};
}

std::vector<ParkPoint> pathCrossings(const World &world, const MovePoint &move) {
  const std::span<const ParkPoint> crossings = pathCrossings(planOnce(world, move).Planned);
  return {crossings.begin(), crossings.end()};
}

std::vector<ParkPoint> pathCrossings(const World &world, const RemovePoint &remove) {
  const std::span<const ParkPoint> crossings = pathCrossings(planOnce(world, remove).Planned);
  return {crossings.begin(), crossings.end()};
}

std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit) {
  OwnPlan own = planOnce(world, edit);
  EditedParts parts;
  editedParts(world, own.Planned, parts, own.Buffers);
  return std::move(parts.Segments);
}

PavedSurface editedSurface(const World &world, const ParkEdit &edit) {
  OwnPlan own = planOnce(world, edit);
  EditedParts parts;
  editedParts(world, own.Planned, parts, own.Buffers);
  return std::move(parts.Surface);
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

bool changesPaths(const ParkEdit &edit) {
  return std::visit(
      []<typename T>(const T & /*command*/) {
        // A new kind of edit counts as changing paths until it is listed here.
        return !(std::is_same_v<T, AddBox> || std::is_same_v<T, MoveBox> ||
                 std::is_same_v<T, DeleteBox> || std::is_same_v<T, SetShopStock> ||
                 std::is_same_v<T, SetOpeningHours>);
      },
      edit);
}

void segmentRecords(const World &world, std::vector<SegmentRecord> &records) {
  records.clear();
  world.forEachWith<PathSegmentIntent>(
      [&world, &records](EntityKey key, const PathSegmentIntent &segment) {
        SegmentRecord record{key,           segment.Kind,     segment.From,   segment.To,
                             segment.Width, segment.FromEdge, segment.ToEdge, Furnishing{}};
        if (const auto *left = world.tryGet<LeftFurnishingIntent>(key)) {
          record.Furnished.Left = left->Held;
        }
        if (const auto *right = world.tryGet<RightFurnishingIntent>(key)) {
          record.Furnished.Right = right->Held;
        }
        records.push_back(record);
      });
}

PlannedEdit::PlannedEdit() : Lists(std::make_unique<PlanLists>()) {}
PlannedEdit::~PlannedEdit() = default;
PlannedEdit::PlannedEdit(PlannedEdit &&) noexcept = default;
PlannedEdit &PlannedEdit::operator=(PlannedEdit &&) noexcept = default;

void planEdit(const World &world, const ParkEdit &edit, PlannedEdit &planned,
              TickBuffers &buffers) {
  EditLists &lists = buffers.get<EditLists>();
  std::visit([&](const auto &command) { planInto(world, command, *planned.Lists, lists, buffers); },
             edit);
}

bool isAccepted(const PlannedEdit &planned) { return planned.Lists->Accepted; }

std::span<const ParkPoint> pathCrossings(const PlannedEdit &planned) {
  return planned.Lists->Crossings;
}

void applyPlanned(World &world, const ParkEdit &edit, const PlannedEdit &planned,
                  TickBuffers &buffers) {
  EditLists &lists = buffers.get<EditLists>();
  std::visit([&](const auto &command) { applyInto(world, command, *planned.Lists, lists); }, edit);
}

void editedParts(const World & /*world*/, const PlannedEdit &planned, EditedParts &parts,
                 TickBuffers &buffers) {
  const PlanLists &kept = *planned.Lists;
  EditLists &lists = buffers.get<EditLists>();
  clearSurface(parts.Surface);
  resizeKeeping(parts.Segments, 0, lists.SpareSegments);
  if (!kept.HasPlan) {
    return;
  }
  // A taper length changes no segment, but it changes the paving at its point.
  lists.Extra.clear();
  if (kept.Plan.Tapered != NULL_KEY) {
    lists.Extra.push_back(kept.Plan.Tapered);
  }
  const PathGround &groundAfter = kept.AfterGround.Ground;
  editedBetween(kept.World, kept.After, groundAfter, lists.Extra, Wanted::Drawing, lists, buffers);
  // An edit whose added or reshaped segment has no ground line shows nothing.
  if (std::ranges::any_of(lists.EditedSegments, [&groundAfter](size_t i) {
        return groundAfter.Segments[i].Line.empty();
      })) {
    return;
  }
  parts.Surface = lists.Edited;
  editedSegmentsOf(groundAfter, lists.EditedSegments, parts.Segments, lists.SpareSegments);
}

void queueEdit(CommandQueue &queue, const ParkEdit &edit) {
  std::visit([&queue](const auto &command) { queue.push(command); }, edit);
}

} // namespace tpj
