#include "tools/internal/snapping.h"

#include "sim/park/geometry.h"
#include "sim/routes/networks.h"
#include "sim/tick_buffers.h"
#include "tools/internal/tool_lists.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <variant>

namespace tpj {
namespace {

ParkPoint offset(ParkPoint point, ParkPoint direction, double distance) {
  return {point.X + direction.X * distance, point.Z + direction.Z * distance};
}

ParkPoint difference(ParkPoint a, ParkPoint b) { return {a.X - b.X, a.Z - b.Z}; }

double dot(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }

double cross(ParkPoint a, ParkPoint b) { return a.X * b.Z - a.Z * b.X; }

// The unit direction of the vector, or none when it is shorter than MIN_SEGMENT_LENGTH.
std::optional<ParkPoint> unitOf(ParkPoint vector) {
  const double length = sqrt(dot(vector, vector));
  if (!(length > MIN_SEGMENT_LENGTH)) {
    return std::nullopt;
  }
  return ParkPoint{vector.X / length, vector.Z / length};
}

// How far along the line between two distinct positions the point's nearest place on it lies, as
// a fraction from 0 at the first to 1 at the second.
double alongSegment(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::clamp(((point.X - from.X) * dx + (point.Z - from.Z) * dz) / (dx * dx + dz * dz), 0.0,
                    1.0);
}

ParkPoint lerp(ParkPoint from, ParkPoint to, double t) {
  return {from.X + t * (to.X - from.X), from.Z + t * (to.Z - from.Z)};
}

// The distance from the point to the line between two positions, which may coincide.
double distanceToLine(ParkPoint point, ParkPoint from, ParkPoint to) {
  if (from == to) {
    return distanceBetween(point, from);
  }
  return distanceBetween(point, lerp(from, to, alongSegment(point, from, to)));
}

// How far from the point the arc of its rounded corner touches each of its segments, or 0.
double tangentAt(const std::vector<ParkCorner> &corners, EntityKey point) {
  const ParkCorner *corner = roundedAt(corners, point);
  return corner == nullptr ? 0.0 : distanceBetween(corner->At, arcOf(*corner).Touch[0]);
}

// The straight part of a segment between two distinct positions: where it starts and ends, its
// length, how far from the first position it starts, and the whole segment's length.
struct StraightPart {
  ParkPoint From;
  ParkPoint To;
  double Length = 0.0;
  double Start = 0.0;
  double Whole = 0.0;
};

// The segment's straight part, between where the arcs at its ends touch it, or none when it is
// shorter than MIN_SEGMENT_LENGTH.
std::optional<StraightPart> straightPart(const std::vector<ParkCorner> &corners,
                                         const ParkSegment &segment, ParkPoint from, ParkPoint to) {
  const double length = distanceBetween(from, to);
  const double t0 = tangentAt(corners, segment.From);
  const double t1 = tangentAt(corners, segment.To);
  const double straight = length - t0 - t1;
  if (straight < MIN_SEGMENT_LENGTH) {
    return std::nullopt;
  }
  const ParkPoint along{(to.X - from.X) / length, (to.Z - from.Z) / length};
  return StraightPart{offset(from, along, t0), offset(to, along, -t1), straight, t0, length};
}

// Whether the point is the kind's: some segment uses it, and every one that does has the kind.
bool isKindsPoint(std::span<const ParkSegment> segments, EntityKey point, PathKind kind) {
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

bool uses(const ParkSegment &segment, EntityKey point) {
  return segment.From == point || segment.To == point;
}

// The stop that names the point a drawn stop makes: the stop itself for an OnPoint, its earlier
// stop for a RepeatStop, and a RepeatStop of it otherwise.
PathStop pointOfDrawn(std::span<const PathStop> drawn, size_t stop) {
  if (std::holds_alternative<OnPoint>(drawn[stop])) {
    return drawn[stop];
  }
  if (const auto *repeat = std::get_if<RepeatStop>(&drawn[stop])) {
    return pointOfDrawn(drawn, repeat->Stop);
  }
  return RepeatStop{stop};
}

// What one findSnap call reads: the query, the park's points, segments, corners, and the doors
// serving the query's kind, and the lists it fills: the drawn stops' positions, which segments and
// drawn segments are left out, the anchors, and the guides.
struct SnapScene {
  SnapQuery Query;
  std::span<const ParkPathPoint> Points;
  std::span<const ParkSegment> Segments;
  const std::vector<ParkCorner> &Corners;
  std::span<const ParkDoor> Doors;
  SnapLists &Lists;
};

std::optional<ParkPoint> positionOf(const SnapScene &scene, EntityKey key) {
  const auto found = std::ranges::lower_bound(scene.Points, key, {}, &ParkPathPoint::Key);
  return found != scene.Points.end() && found->Key == key ? std::optional<ParkPoint>(found->At)
                                                          : std::nullopt;
}

// The reach a candidate with the holds catches within: the release reach when every one of them
// was held, and the catch reach otherwise.
double reachFor(const SnapScene &scene, std::span<const SnapHold> holds) {
  const bool held = !holds.empty() && std::ranges::all_of(holds, [&scene](const SnapHold &hold) {
    return std::ranges::find(scene.Query.Held, hold) != scene.Query.Held.end();
  });
  return held ? scene.Query.ReleaseReach : scene.Query.CatchReach;
}

// The scene for the query, filling the park's snapping lists: the drawn stops' positions, or none
// when a stop has no position, and which segments and drawn segments are left out.
SnapScene makeScene(const World &world, ToolPark &park, const SnapQuery &query) {
  SnapLists &lists = park.Lists->Snapping;
  const std::vector<ParkDoor> &doors =
      query.Kind == PathKind::Guest ? park.GuestDoors : park.BackstageDoors;
  SnapScene scene{query, park.Points, park.Segments, park.Corners, doors, lists};
  std::vector<ParkPoint> &drawn = lists.Drawn;
  drawn.clear();
  for (const PathStop &stop : query.Drawn) {
    if (!appendStopPosition(world, park.Corners, stop, drawn)) {
      drawn.clear();
      break;
    }
  }
  fillGrowing(lists.LeftOut, scene.Segments.size(), false);
  for (size_t s = 0; s < scene.Segments.size(); ++s) {
    const ParkSegment &segment = scene.Segments[s];
    if (query.Dragged != NULL_KEY) {
      lists.LeftOut[s] = uses(segment, query.Dragged);
    } else if (!drawn.empty() && segment.Line.size() >= 2) {
      lists.LeftOut[s] = lineDistance(drawn.back(), segment.Line) <= MIN_SEGMENT_LENGTH;
    }
  }
  const size_t drawnSegments = drawn.empty() ? 0 : drawn.size() - 1;
  fillGrowing(lists.DrawnLeftOut, drawnSegments, false);
  for (size_t k = 0; k < drawnSegments; ++k) {
    lists.DrawnLeftOut[k] =
        distanceToLine(drawn.back(), drawn[k], drawn[k + 1]) <= MIN_SEGMENT_LENGTH;
  }
  return scene;
}

// Keeps the candidate when it lies within its reach and nearer than the nearest so far.
void consider(const SnapScene &scene, std::optional<Snap> &nearest, Snap candidate) {
  if (candidate.Distance <= reachFor(scene, holdsOf(candidate)) &&
      (!nearest || candidate.Distance < nearest->Distance)) {
    nearest = candidate;
  }
}

// Half the width a door's target leaves room for: the kind's new width while drawing, or the
// widest segment at the dragged point.
double doorHalfWidth(const SnapScene &scene) {
  if (scene.Query.Dragged == NULL_KEY) {
    return 0.5 * pathWidth(scene.Query.Kind);
  }
  double widest = 0.0;
  for (const ParkSegment &segment : scene.Segments) {
    if (uses(segment, scene.Query.Dragged)) {
      widest = std::max(widest, segment.Width);
    }
  }
  return 0.5 * widest;
}

// Whether the place lies on the paving of a segment of the kind that doesn't move with the
// dragged point.
bool onPaving(const SnapScene &scene, ParkPoint place) {
  return std::ranges::any_of(scene.Segments, [&](const ParkSegment &segment) {
    return segment.Kind == scene.Query.Kind && segment.Line.size() >= 2 &&
           (scene.Query.Dragged == NULL_KEY || !uses(segment, scene.Query.Dragged)) &&
           lineDistance(place, segment.Line) <= 0.5 * segment.Width;
  });
}

// Whether any segment at the point is left out.
bool anyLeftOutAt(const SnapScene &scene, EntityKey point) {
  for (size_t s = 0; s < scene.Segments.size(); ++s) {
    if (scene.Lists.LeftOut[s] && uses(scene.Segments[s], point)) {
      return true;
    }
  }
  return false;
}

// Where a path point is caught, and the stop it gives. If its corner is rounded and no segment at
// it is left out, that is the middle of its arc, where the paving is. Otherwise it is the point.
std::pair<ParkPoint, PathStop> pointTarget(const SnapScene &scene, const ParkPathPoint &point) {
  if (const ParkCorner *corner = roundedAt(scene.Corners, point.Key)) {
    const ParkPoint middle = bendPlace(*corner, 0.5);
    if (distanceBetween(middle, point.At) > MIN_SEGMENT_LENGTH && !anyLeftOutAt(scene, point.Key)) {
      return {middle, OnBend{point.Key, 0.5}};
    }
  }
  return {point.At, OnPoint{point.Key}};
}

// The nearest point target within reach: a path point of the kind, a point a drawn stop makes
// other than the last, or a door's target. An earlier one wins a tie.
std::optional<Snap> pointSnap(const SnapScene &scene) {
  const SnapQuery &query = scene.Query;
  std::optional<Snap> nearest;
  // A rounded corner's arc lies between its point and its center, so a corner within the release
  // reach of the pointer has its point within that reach plus the corner's depth. Only those pay
  // for the costlier tests.
  for (const ParkPathPoint &point : scene.Points) {
    if (point.Key == query.Dragged) {
      continue;
    }
    const ParkCorner *corner = roundedAt(scene.Corners, point.Key);
    const double depth = corner == nullptr ? 0.0 : distanceBetween(point.At, corner->Middle);
    if (distanceBetween(query.Pointer, point.At) > query.ReleaseReach + depth ||
        !isKindsPoint(scene.Segments, point.Key, query.Kind)) {
      continue;
    }
    const auto [at, stop] = pointTarget(scene, point);
    consider(scene, nearest,
             makeSnap(stop, distanceBetween(query.Pointer, at), at, {{GuideKind::Point, at, at}},
                      {{HoldKind::Point, point.Key, 0}}));
  }
  for (size_t i = 0; i + 1 < scene.Lists.Drawn.size(); ++i) {
    const PathStop &stop = query.Drawn[i];
    const bool made =
        !std::holds_alternative<OnPoint>(stop) && !std::holds_alternative<RepeatStop>(stop);
    if (made) {
      const ParkPoint at = scene.Lists.Drawn[i];
      consider(scene, nearest,
               makeSnap(RepeatStop{i}, distanceBetween(query.Pointer, at), at,
                        {{GuideKind::Point, at, at}}, {{HoldKind::DrawnStop, NULL_KEY, i}}));
    }
  }
  const double clearance = doorHalfWidth(scene) + DOOR_CLEARANCE;
  for (const ParkDoor &door : scene.Doors) {
    const ParkPoint target = offset(door.At, door.Out, clearance);
    const double distance = distanceToLine(query.Pointer, door.At, target);
    if (distance > query.ReleaseReach || onPaving(scene, target)) {
      continue;
    }
    consider(scene, nearest,
             makeSnap(NewPoint{target}, distance, target, {{GuideKind::Door, door.At, target}},
                      {{HoldKind::Door, door.Entity, static_cast<uint64_t>(door.Side)}}));
  }
  return nearest;
}

// An anchor being built: its arrivals are the end of the list, from its First.
Anchor startAnchor(SnapLists &lists, ParkPoint at) { return {at, lists.Arrivals.size(), 0}; }

void addArrival(SnapLists &lists, Anchor &anchor, ParkPoint vector) {
  const std::optional<ParkPoint> direction = unitOf(vector);
  if (!direction) {
    return;
  }
  const std::span<const ParkPoint> arrivals =
      std::span(lists.Arrivals).subspan(anchor.First, anchor.Count);
  const bool listed = std::ranges::any_of(arrivals, [&](ParkPoint arrival) {
    return fabs(arrival.X - direction->X) < 1e-9 && fabs(arrival.Z - direction->Z) < 1e-9;
  });
  if (!listed) {
    lists.Arrivals.push_back(*direction);
    ++anchor.Count;
  }
}

// Adds both ways along the direction.
void addBothWays(SnapLists &lists, Anchor &anchor, ParkPoint vector) {
  addArrival(lists, anchor, vector);
  addArrival(lists, anchor, {-vector.X, -vector.Z});
}

// Lists the anchor, giving it the world's axes when no segment arrives at it.
void finishAnchor(SnapLists &lists, Anchor anchor) {
  if (anchor.Count == 0) {
    lists.Arrivals.push_back({1.0, 0.0});
    lists.Arrivals.push_back({0.0, 1.0});
    anchor.Count = 2;
  }
  lists.Anchors.push_back(anchor);
}

// Adds the arrivals of the place the drawn stop names: each segment of the kind at its point, both
// ways along the segment, arc, or drawn segment it lies on.
void addPlaceArrivals(const SnapScene &scene, size_t stop, Anchor &anchor) {
  SnapLists &lists = scene.Lists;
  size_t named = stop;
  while (const auto *repeat = std::get_if<RepeatStop>(&scene.Query.Drawn[named])) {
    if (repeat->Stop >= named) {
      return;
    }
    named = repeat->Stop;
  }
  const PathStop &place = scene.Query.Drawn[named];
  if (const auto *onPoint = std::get_if<OnPoint>(&place)) {
    for (const ParkSegment &segment : scene.Segments) {
      if (segment.Kind == scene.Query.Kind && uses(segment, onPoint->Point)) {
        const EntityKey other = segment.From == onPoint->Point ? segment.To : segment.From;
        if (const std::optional<ParkPoint> at = positionOf(scene, other)) {
          addArrival(lists, anchor, difference(anchor.At, *at));
        }
      }
    }
  } else if (const auto *onSegment = std::get_if<OnSegment>(&place)) {
    const auto found = std::ranges::find(scene.Segments, onSegment->Segment, &ParkSegment::Key);
    if (found != scene.Segments.end()) {
      const std::optional<ParkPoint> from = positionOf(scene, found->From);
      const std::optional<ParkPoint> to = positionOf(scene, found->To);
      if (from && to) {
        addBothWays(lists, anchor, difference(*to, *from));
      }
    }
  } else if (const auto *onBend = std::get_if<OnBend>(&place)) {
    if (const ParkCorner *corner = roundedAt(scene.Corners, onBend->Point)) {
      const ParkPoint radial = difference(anchor.At, arcOf(*corner).Center);
      addBothWays(lists, anchor, {-radial.Z, radial.X});
    }
  } else if (const auto *onDrawn = std::get_if<OnDrawnSegment>(&place)) {
    if (onDrawn->Stop + 1 < scene.Lists.Drawn.size()) {
      addBothWays(
          lists, anchor,
          difference(scene.Lists.Drawn[onDrawn->Stop + 1], scene.Lists.Drawn[onDrawn->Stop]));
    }
  }
}

// Lists the anchor a drawn stop makes: its position, the drawn segments at it, and the place it
// names.
void addDrawnAnchor(const SnapScene &scene, size_t stop) {
  SnapLists &lists = scene.Lists;
  Anchor anchor = startAnchor(lists, lists.Drawn[stop]);
  if (stop > 0) {
    addArrival(lists, anchor, difference(anchor.At, lists.Drawn[stop - 1]));
  }
  if (stop + 1 < lists.Drawn.size()) {
    addArrival(lists, anchor, difference(anchor.At, lists.Drawn[stop + 1]));
  }
  addPlaceArrivals(scene, stop, anchor);
  finishAnchor(lists, anchor);
}

// The anchors: the last stop and, once two are drawn, the first, while drawing; the other end of
// each segment at the dragged point, in key order of the segment, while dragging. An anchor no
// segment arrives at takes the world's axes. Fills the scene's anchors and arrivals.
void addAnchors(const SnapScene &scene) {
  SnapLists &lists = scene.Lists;
  lists.Anchors.clear();
  lists.Arrivals.clear();
  if (scene.Query.Dragged != NULL_KEY) {
    for (const ParkSegment &segment : scene.Segments) {
      if (!uses(segment, scene.Query.Dragged)) {
        continue;
      }
      const EntityKey end = segment.From == scene.Query.Dragged ? segment.To : segment.From;
      const std::optional<ParkPoint> found = positionOf(scene, end);
      if (!found) {
        continue;
      }
      const ParkPoint at = *found;
      Anchor anchor = startAnchor(lists, at);
      for (const ParkSegment &other : scene.Segments) {
        if (uses(other, end) && !uses(other, scene.Query.Dragged)) {
          const EntityKey far = other.From == end ? other.To : other.From;
          if (const std::optional<ParkPoint> from = positionOf(scene, far)) {
            addArrival(lists, anchor, difference(at, *from));
          }
        }
      }
      finishAnchor(lists, anchor);
    }
  } else if (!lists.Drawn.empty()) {
    addDrawnAnchor(scene, lists.Drawn.size() - 1);
    if (lists.Drawn.size() >= 2) {
      addDrawnAnchor(scene, 0);
    }
  }
}

// From each anchor, the rays straight on, at 45°, 90°, and 135° either way from each arrival,
// leaving out a direction already listed for the anchor. Fills the scene's guides from its anchors.
void addGuides(const SnapScene &scene) {
  const std::vector<Anchor> &anchors = scene.Lists.Anchors;
  const double half = sqrt(0.5);
  const std::array<ParkPoint, 7> turns{{{1.0, 0.0},
                                        {half, half},
                                        {half, -half},
                                        {0.0, 1.0},
                                        {0.0, -1.0},
                                        {-half, half},
                                        {-half, -half}}};
  std::vector<Guide> &guides = scene.Lists.Guides;
  guides.clear();
  for (size_t a = 0; a < anchors.size(); ++a) {
    const size_t first = guides.size();
    for (const ParkPoint arrival :
         std::span(scene.Lists.Arrivals).subspan(anchors[a].First, anchors[a].Count)) {
      for (const ParkPoint turn : turns) {
        const ParkPoint direction{arrival.X * turn.X - arrival.Z * turn.Z,
                                  arrival.X * turn.Z + arrival.Z * turn.X};
        const bool listed = std::any_of(guides.begin() + static_cast<ptrdiff_t>(first),
                                        guides.end(), [&](const Guide &guide) {
                                          return fabs(guide.Direction.X - direction.X) < 1e-9 &&
                                                 fabs(guide.Direction.Z - direction.Z) < 1e-9;
                                        });
        if (!listed) {
          guides.push_back({a, anchors[a].At, direction});
        }
      }
    }
  }
}

SnapHold guideHold(size_t guide) { return {HoldKind::Guide, NULL_KEY, guide}; }

// The nearest crossing within reach of two guides from different anchors, lying ahead of both.
std::optional<Snap> guideCrossing(const SnapScene &scene, const std::vector<Guide> &guides) {
  std::optional<Snap> nearest;
  for (size_t i = 0; i < guides.size(); ++i) {
    for (size_t j = i + 1; j < guides.size(); ++j) {
      const Guide &a = guides[i];
      const Guide &b = guides[j];
      const double turn = cross(a.Direction, b.Direction);
      if (a.Anchor == b.Anchor || !(fabs(turn) > 1e-12)) {
        continue;
      }
      const ParkPoint between = difference(b.From, a.From);
      const double alongA = cross(between, b.Direction) / turn;
      const double alongB = cross(between, a.Direction) / turn;
      if (!(alongA > MIN_SEGMENT_LENGTH) || !(alongB > MIN_SEGMENT_LENGTH)) {
        continue;
      }
      const ParkPoint at = offset(a.From, a.Direction, alongA);
      consider(scene, nearest,
               makeSnap(NewPoint{at}, distanceBetween(scene.Query.Pointer, at), at,
                        {{GuideKind::Direction, a.From, at}, {GuideKind::Direction, b.From, at}},
                        {guideHold(i), guideHold(j)}));
    }
  }
  return nearest;
}

// The nearest crossing within reach of a guide with the straight part of a segment of the kind
// that isn't left out, ahead of the guide's anchor and clear of the straight part's ends.
std::optional<Snap> segmentCrossing(const SnapScene &scene, const std::vector<Guide> &guides) {
  std::optional<Snap> nearest;
  for (size_t s = 0; s < scene.Segments.size(); ++s) {
    const ParkSegment &segment = scene.Segments[s];
    const std::optional<ParkPoint> from = positionOf(scene, segment.From);
    const std::optional<ParkPoint> to = positionOf(scene, segment.To);
    if (segment.Kind != scene.Query.Kind || segment.Line.empty() || scene.Lists.LeftOut[s] ||
        !from || !to) {
      continue;
    }
    const std::optional<StraightPart> found = straightPart(scene.Corners, segment, *from, *to);
    if (!found) {
      continue;
    }
    const StraightPart part = *found;
    const ParkPoint span = difference(part.To, part.From);
    for (size_t g = 0; g < guides.size(); ++g) {
      const Guide &guide = guides[g];
      const double turn = cross(guide.Direction, span);
      if (!(fabs(turn) > 1e-12)) {
        continue;
      }
      const ParkPoint between = difference(part.From, guide.From);
      const double ahead = cross(between, span) / turn;
      const double fraction = cross(between, guide.Direction) / turn;
      if (!(ahead > MIN_SEGMENT_LENGTH) || !(fraction * part.Length > MIN_SEGMENT_LENGTH) ||
          !((1.0 - fraction) * part.Length > MIN_SEGMENT_LENGTH)) {
        continue;
      }
      const ParkPoint at = offset(guide.From, guide.Direction, ahead);
      consider(scene, nearest,
               makeSnap(OnSegment{segment.Key, (part.Start + fraction * part.Length) / part.Whole},
                        distanceBetween(scene.Query.Pointer, at), at,
                        {{GuideKind::Direction, guide.From, at},
                         {GuideKind::Segment, part.From, part.To}},
                        {guideHold(g), {HoldKind::Segment, segment.Key, 0}}));
    }
  }
  return nearest;
}

// Keeps the candidate when it lies within the catch distance and nearer than the nearest so far.
void considerWithin(std::optional<Snap> &nearest, Snap candidate, double within) {
  if (candidate.Distance <= within && (!nearest || candidate.Distance < nearest->Distance)) {
    nearest = candidate;
  }
}

// The nearest place on the straight part of a segment of the kind that isn't left out, within half
// its width or its reach, or the segment's end point when that place lies within
// MIN_SEGMENT_LENGTH of it.
std::optional<Snap> straightSnap(const SnapScene &scene) {
  std::optional<Snap> nearest;
  for (size_t s = 0; s < scene.Segments.size(); ++s) {
    const ParkSegment &segment = scene.Segments[s];
    const std::optional<ParkPoint> from = positionOf(scene, segment.From);
    const std::optional<ParkPoint> to = positionOf(scene, segment.To);
    if (segment.Kind != scene.Query.Kind || segment.Line.empty() || scene.Lists.LeftOut[s] ||
        !from || !to) {
      continue;
    }
    const std::optional<StraightPart> part = straightPart(scene.Corners, segment, *from, *to);
    if (!part) {
      continue;
    }
    const double v = alongSegment(scene.Query.Pointer, part->From, part->To);
    const ParkPoint place = lerp(part->From, part->To, v);
    const double t = (part->Start + v * part->Length) / part->Whole;
    Snap snap = makeSnap(OnSegment{segment.Key, t}, distanceBetween(scene.Query.Pointer, place),
                         place, {{GuideKind::Segment, part->From, part->To}},
                         {{HoldKind::Segment, segment.Key, 0}});
    if (t * part->Whole < MIN_SEGMENT_LENGTH) {
      snap.Stop = OnPoint{segment.From};
      snap.At = *from;
    } else if ((1.0 - t) * part->Whole < MIN_SEGMENT_LENGTH) {
      snap.Stop = OnPoint{segment.To};
      snap.At = *to;
    }
    const double within = std::max(reachFor(scene, holdsOf(snap)), 0.5 * segment.Width);
    considerWithin(nearest, snap, within);
  }
  return nearest;
}

// The nearest place on the arc of a rounded corner whose segments all have the kind and aren't
// left out, within half the narrower one's width or the reach.
std::optional<Snap> bendSnap(const SnapScene &scene) {
  std::optional<Snap> nearest;
  for (const ParkCorner &corner : scene.Corners) {
    if (!(corner.Radius > 0.0)) {
      continue;
    }
    bool fits = true;
    double narrowest = 0.0;
    bool any = false;
    for (size_t s = 0; s < scene.Segments.size(); ++s) {
      const ParkSegment &segment = scene.Segments[s];
      if (!uses(segment, corner.Point)) {
        continue;
      }
      fits = fits && segment.Kind == scene.Query.Kind && !scene.Lists.LeftOut[s];
      narrowest = any ? std::min(narrowest, segment.Width) : segment.Width;
      any = true;
    }
    if (!fits || corner.Point == scene.Query.Dragged) {
      continue;
    }
    std::optional<Snap> place = bendPlaceNear(corner, scene.Query.Pointer);
    if (!place) {
      continue;
    }
    place->Guides[0] = {GuideKind::Point, place->At, place->At};
    place->GuideCount = 1;
    place->Holds[0] = {HoldKind::Bend, corner.Point, 0};
    place->HoldCount = 1;
    const double within = std::max(reachFor(scene, holdsOf(*place)), 0.5 * narrowest);
    considerWithin(nearest, *place, within);
  }
  return nearest;
}

// The nearest place on a drawn segment that isn't left out, within half the kind's width or the
// reach, or the point at its end when that place lies within MIN_SEGMENT_LENGTH of it.
std::optional<Snap> drawnSegmentSnap(const SnapScene &scene) {
  std::optional<Snap> nearest;
  for (size_t k = 0; k + 1 < scene.Lists.Drawn.size(); ++k) {
    const ParkPoint from = scene.Lists.Drawn[k];
    const ParkPoint to = scene.Lists.Drawn[k + 1];
    const double length = distanceBetween(from, to);
    if (scene.Lists.DrawnLeftOut[k] || length < MIN_SEGMENT_LENGTH) {
      continue;
    }
    const double t = alongSegment(scene.Query.Pointer, from, to);
    const ParkPoint place = lerp(from, to, t);
    Snap snap = makeSnap(OnDrawnSegment{k, t}, distanceBetween(scene.Query.Pointer, place), place,
                         {{GuideKind::Segment, from, to}}, {{HoldKind::DrawnSegment, NULL_KEY, k}});
    if (t * length < MIN_SEGMENT_LENGTH) {
      snap.Stop = pointOfDrawn(scene.Query.Drawn, k);
      snap.At = from;
    } else if ((1.0 - t) * length < MIN_SEGMENT_LENGTH) {
      snap.Stop = pointOfDrawn(scene.Query.Drawn, k + 1);
      snap.At = to;
    }
    const double within =
        std::max(reachFor(scene, holdsOf(snap)), 0.5 * pathWidth(scene.Query.Kind));
    considerWithin(nearest, snap, within);
  }
  return nearest;
}

// The nearest segment place: an existing segment's straight part or arc, the straight part
// winning a tie, or else a drawn segment, which wins only when nearer.
std::optional<Snap> segmentSnap(const SnapScene &scene) {
  std::optional<Snap> existing = straightSnap(scene);
  std::optional<Snap> bend = bendSnap(scene);
  if (bend && (!existing || bend->Distance < existing->Distance)) {
    existing = bend;
  }
  std::optional<Snap> drawn = drawnSegmentSnap(scene);
  if (drawn && (!existing || drawn->Distance < existing->Distance)) {
    return drawn;
  }
  return existing;
}

// The place nearest the pointer on the nearest guide within reach, ahead of its anchor, moved to a
// whole number of meters from the anchor when that lies within reach.
std::optional<Snap> guideSnap(const SnapScene &scene, const std::vector<Guide> &guides) {
  std::optional<size_t> best;
  double bestAhead = 0.0;
  double bestDistance = 0.0;
  for (size_t g = 0; g < guides.size(); ++g) {
    const Guide &guide = guides[g];
    const double ahead = dot(difference(scene.Query.Pointer, guide.From), guide.Direction);
    if (!(ahead > MIN_SEGMENT_LENGTH)) {
      continue;
    }
    const double distance =
        distanceBetween(scene.Query.Pointer, offset(guide.From, guide.Direction, ahead));
    const std::array<SnapHold, 1> holds{guideHold(g)};
    if (distance <= reachFor(scene, holds) && (!best || distance < bestDistance)) {
      best = g;
      bestAhead = ahead;
      bestDistance = distance;
    }
  }
  if (!best) {
    return std::nullopt;
  }
  const Guide &guide = guides[*best];
  const ParkPoint at = offset(guide.From, guide.Direction, bestAhead);
  const double meters = floor(bestAhead + 0.5);
  const std::array<SnapHold, 2> lengthHolds{
      guideHold(*best), {HoldKind::Length, NULL_KEY, static_cast<uint64_t>(meters)}};
  if (meters >= 1.0 && fabs(bestAhead - meters) <= reachFor(scene, lengthHolds)) {
    const ParkPoint whole = offset(guide.From, guide.Direction, meters);
    return makeSnap(
        NewPoint{whole}, bestDistance, whole,
        {{GuideKind::Direction, guide.From, whole}, {GuideKind::Length, guide.From, whole}},
        {lengthHolds[0], lengthHolds[1]});
  }
  return makeSnap(NewPoint{at}, bestDistance, at, {{GuideKind::Direction, guide.From, at}},
                  {guideHold(*best)});
}

} // namespace

Snap makeSnap(const PathStop &stop, double distance, ParkPoint at,
              std::initializer_list<SnapGuide> guides, std::initializer_list<SnapHold> holds) {
  Snap snap{stop, distance, at, {}, 0, {}, 0};
  for (const SnapGuide &guide : guides) {
    if (snap.GuideCount < snap.Guides.size()) {
      snap.Guides.at(snap.GuideCount++) = guide;
    }
  }
  for (const SnapHold &hold : holds) {
    if (snap.HoldCount < snap.Holds.size()) {
      snap.Holds.at(snap.HoldCount++) = hold;
    }
  }
  return snap;
}

std::span<const SnapHold> holdsOf(const Snap &snap) {
  return std::span(snap.Holds).first(snap.HoldCount);
}

double distanceBetween(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return sqrt(dx * dx + dz * dz);
}

double segmentDistance(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double px = point.X - from.X;
  const double pz = point.Z - from.Z;
  const double t = std::clamp((px * dx + pz * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  const double ex = px - dx * t;
  const double ez = pz - dz * t;
  return sqrt(ex * ex + ez * ez);
}

double lineDistance(ParkPoint point, const std::vector<CarrierPoint> &line) {
  double nearest = segmentDistance(point, line[0], line[1]);
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    nearest = std::min(nearest, segmentDistance(point, line[i], line[i + 1]));
  }
  return nearest;
}

const ParkCorner *roundedAt(const std::vector<ParkCorner> &corners, EntityKey point) {
  const auto found = std::ranges::lower_bound(corners, point, {}, &ParkCorner::Point);
  return found != corners.end() && found->Point == point && found->Radius > 0.0 ? &*found : nullptr;
}

std::optional<std::pair<double, double>> straightPlace(const std::vector<ParkCorner> &corners,
                                                       const ParkSegment &segment, ParkPoint from,
                                                       ParkPoint to, ParkPoint point) {
  const std::optional<StraightPart> part = straightPart(corners, segment, from, to);
  if (!part) {
    return std::nullopt;
  }
  const double t = alongSegment(point, part->From, part->To);
  const ParkPoint place = lerp(part->From, part->To, t);
  return std::pair{(part->Start + t * part->Length) / part->Whole, distanceBetween(point, place)};
}

std::optional<Snap> bendPlaceNear(const ParkCorner &corner, ParkPoint point) {
  const double along = bendAlong(corner, point);
  if (!(along > 0.0) || !(along < 1.0)) {
    return std::nullopt;
  }
  const ParkPoint place = bendPlace(corner, along);
  return makeSnap(OnBend{corner.Point, along}, distanceBetween(point, place), place, {}, {});
}

PointerSnap findSnap(const World &world, ToolPark &park, const SnapQuery &query) {
  const SnapScene scene = makeScene(world, park, query);
  std::optional<Snap> snap = pointSnap(scene);
  if (!snap) {
    addAnchors(scene);
    addGuides(scene);
    const std::vector<Guide> &guides = scene.Lists.Guides;
    snap = guideCrossing(scene, guides);
    const std::optional<Snap> onSegment = segmentCrossing(scene, guides);
    if (onSegment && (!snap || onSegment->Distance < snap->Distance)) {
      snap = onSegment;
    }
    if (!snap) {
      snap = segmentSnap(scene);
    }
    if (!snap) {
      snap = guideSnap(scene, guides);
    }
  }
  if (!snap) {
    return PointerSnap{query.Pointer, NewPoint{query.Pointer}};
  }
  return PointerSnap{snap->At,         snap->Stop,  snap->Guides,
                     snap->GuideCount, snap->Holds, snap->HoldCount};
}

} // namespace tpj
