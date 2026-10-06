#ifndef TPJ_TOOLS_INTERNAL_SNAPPING_H
#define TPJ_TOOLS_INTERNAL_SNAPPING_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <array>
#include <initializer_list>
#include <optional>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

// What to snap: the kind, the path being drawn so far, or the point being dragged, the pointer,
// the reach to catch a snap and to keep one in meters, and what the last snap held.
struct SnapQuery {
  PathKind Kind = PathKind::Guest;
  std::span<const PathStop> Drawn;
  EntityKey Dragged = NULL_KEY;
  ParkPoint Pointer;
  double CatchReach = 0.0;
  double ReleaseReach = 0.0;
  std::span<const SnapHold> Held;
};

// The snap for the query, as tools/SPEC.md's Snapping section states.
PointerSnap findSnap(const World &world, const SnapQuery &query);

// A stop a pointer snaps to, how far from the pointer it lies, where it lands, the first
// GuideCount guides it shows, and the first HoldCount things it holds to. No snap has more than two
// of either, so a candidate is built without allocating.
struct Snap {
  PathStop Stop;
  double Distance = 0.0;
  ParkPoint At;
  std::array<SnapGuide, 2> Guides{};
  size_t GuideCount = 0;
  std::array<SnapHold, 2> Holds{};
  size_t HoldCount = 0;
};

// A snap with the guides and holds given, keeping the first two of each.
Snap makeSnap(const PathStop &stop, double distance, ParkPoint at,
              std::initializer_list<SnapGuide> guides, std::initializer_list<SnapHold> holds);
// The things the snap holds to.
std::span<const SnapHold> holdsOf(const Snap &snap);

double distanceBetween(ParkPoint a, ParkPoint b);
// The distance from a point to the segment between two distinct ground line points.
double segmentDistance(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to);
// The distance from the point to the ground line, which has at least two points.
double lineDistance(ParkPoint point, const std::vector<CarrierPoint> &line);
// The rounded corner at the point, or none. The corners are in ascending key order of their points.
const ParkCorner *roundedAt(const std::vector<ParkCorner> &corners, EntityKey point);
// The nearest place on the straight part of the segment between two distinct positions, which runs
// between where the arcs at its ends touch it: the fraction of the way from the first position to
// the second, and its distance from the point. None when the straight part is shorter than
// MIN_SEGMENT_LENGTH.
std::optional<std::pair<double, double>> straightPlace(const std::vector<ParkCorner> &corners,
                                                       const ParkSegment &segment, ParkPoint from,
                                                       ParkPoint to, ParkPoint point);
// The place on the corner's arc nearest the point, as an OnBend with its distance, or none when it
// lies at an end of the arc.
std::optional<Snap> bendPlaceNear(const ParkCorner &corner, ParkPoint point);

} // namespace tpj

#endif
