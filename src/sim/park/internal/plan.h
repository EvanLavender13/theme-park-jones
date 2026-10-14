#ifndef TPJ_SIM_PARK_INTERNAL_PLAN_H
#define TPJ_SIM_PARK_INTERNAL_PLAN_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park/internal/curves.h"
#include "sim/park/internal/layout.h"

#include <array>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

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
// If Tapered is not NULL_KEY, that existing point's taper length becomes TaperedTo. If Inheritor
// is not NULL_KEY, each side of that existing segment that has no furnishing takes that side of
// Inherits, as a merged segment takes the other's.
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
  EntityKey Tapered = NULL_KEY;
  double TaperedTo = 0.0;
  CurveWrites Curves;
  EntityKey Inheritor = NULL_KEY;
  Furnishing Inherits;
};

// An edit's plan and what planning it found, kept by a PlannedEdit from call to call: whether
// planning gave a plan, whether the world accepts the edit, and the crossings' positions. The
// spare lists keep the elements resizeKeeping takes off the plan's lists. World holds the world's
// layout and ground, the before side. After holds the layout the plan makes, and AfterGround its
// ground, which editedParts reads so that the ghost derives no ground planning derived.
struct PlanLists {
  EditPlan Plan;
  bool HasPlan = false;
  bool Accepted = false;
  std::vector<ParkPoint> Crossings;
  std::vector<Split> SpareSplits;
  std::vector<PlanLine> SpareLines;
  std::vector<BendSplit> SpareBends;
  GroundLists World;
  PathLayout After;
  GroundLists AfterGround;
};

} // namespace tpj

#endif
