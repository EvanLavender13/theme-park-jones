#ifndef TPJ_SIM_PARK_INTERNAL_LAYOUT_H
#define TPJ_SIM_PARK_INTERNAL_LAYOUT_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/components.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

#include <array>
#include <span>
#include <stddef.h>
#include <vector>

namespace tpj {

struct LayoutPoint {
  EntityKey Key = NULL_KEY;
  PathPointIntent Intent;
};

struct LayoutSegment {
  EntityKey Key = NULL_KEY;
  PathSegmentIntent Intent;
};

// A park's path points and segments, each in ascending key order, apart from the rest of its
// world, so that an edit can be tried on a copy.
struct PathLayout {
  std::vector<LayoutPoint> Points;
  std::vector<LayoutSegment> Segments;
};

// Each segment with its ground line, rounded at its corners, in ascending key order, and each
// corner, in ascending key order of its point.
struct PathGround {
  std::vector<ParkSegment> Segments;
  std::vector<ParkCorner> Corners;
};

// How a point turns between its two segments, the lower-keyed first: each one's key, its far end,
// the unit direction toward it, and its straight length, and the dot and cross products of the two
// directions.
struct Bend {
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  double Asked = 0.0;
  std::array<EntityKey, 2> Segments{};
  std::array<EntityKey, 2> FarKeys{};
  std::array<ParkPoint, 2> Along{};
  std::array<double, 2> Lengths{};
  double Dot = 0.0;
  double Cross = 0.0;

  // The tangent length the radius gives.
  [[nodiscard]] double tangentOf(double radius) const { return radius * Cross / (1.0 - Dot); }
};

template <typename Visitor> void visitFields(Visitor &visitor, Bend &bend) {
  visitor.field("key", bend.Key);
  visitor.field("at", bend.At);
  visitor.field("asked", bend.Asked);
  for (size_t k = 0; k < 2; ++k) {
    visitor.field("segment", bend.Segments[k]);
    visitor.field("far-key", bend.FarKeys[k]);
    visitor.field("along", bend.Along[k]);
    visitor.field("length", bend.Lengths[k]);
  }
  visitor.field("dot", bend.Dot);
  visitor.field("cross", bend.Cross);
}

// One segment end at a point, which the point's arms are found from: its segment's key and intent,
// whether its straight ground line holds points, and the intent of the point at its far end, or
// null. A segment with both ends at the point gives two.
struct ArmInput {
  EntityKey Segment = NULL_KEY;
  const PathSegmentIntent *Intent = nullptr;
  bool Straight = false;
  const PathPointIntent *Far = nullptr;
};

// How far a point's paving reaches along one of its segments.
struct SegmentReach {
  EntityKey Segment = NULL_KEY;
  double Reach = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, SegmentReach &reach) {
  visitor.field("segment", reach.Segment);
  visitor.field("reach", reach.Reach);
}

// What a point's segment ends make there: whether it bends, and its bend when it does, and how far
// its paving reaches along each segment, by the segment's key. Without a bend, Bent holds no
// values.
struct PointArms {
  bool HasBend = false;
  Bend Bent;
  std::vector<SegmentReach> Reaches;
};

template <typename Visitor> void visitFields(Visitor &visitor, PointArms &arms) {
  visitor.field("has-bend", arms.HasBend);
  visitor.field("bend", arms.Bent);
  visitor.field("reaches", arms.Reaches);
}

// A corner, its two segments by key, and, for each segment, the points of its ground line from the
// arc's middle to where the arc touches it.
struct Rounding {
  ParkCorner Corner;
  std::array<EntityKey, 2> Segments{};
  std::array<std::vector<ParkPoint>, 2> Halves;
};

template <typename Visitor> void visitFields(Visitor &visitor, Rounding &rounding) {
  ParkCorner &corner = rounding.Corner;
  visitor.field("point", corner.Point);
  visitor.field("at", corner.At);
  visitor.field("along", corner.Along[0]);
  visitor.field("along", corner.Along[1]);
  visitor.field("toward", corner.Toward);
  visitor.field("radius", corner.Radius);
  visitor.field("max-radius", corner.MaxRadius);
  visitor.field("middle", corner.Middle);
  for (size_t k = 0; k < 2; ++k) {
    visitor.field("segment", rounding.Segments[k]);
    visitor.field("half", rounding.Halves[k]);
  }
}

// Puts the straight ground line between the segment's points in line: empty unless they are two
// different path points and its width is finite and above zero. from and to are the segment's
// points, or null where it has none.
void straightLineOf(const PathSegmentIntent &segment, const PathPointIntent *from,
                    const PathPointIntent *to, std::vector<CarrierPoint> &line);
// Puts in arms what the segment ends at the point make there, in the order of their segments'
// keys. Its bend needs exactly two ends, each with a straight ground line. scratch holds the lists
// it reuses.
void armsAt(EntityKey key, const PathPointIntent &point, std::span<const ArmInput> ends,
            TickBuffers &scratch, PointArms &arms);
// How far from a point along one of its segments that point's own arc and paving reach at least,
// given the point's arms.
double neededAt(const PointArms &far, EntityKey segment);
// Puts in rounding the corner the arms' bend makes, given what each of the bend's far ends needs
// along its segment, from neededAt. The arms must have a bend.
void cornerAt(const PointArms &arms, std::array<double, 2> needed, Rounding &rounding);
// Puts in out the segment with its ground line: its straight line, rounded at the corner at either
// end, or null where there is none.
void groundLineOf(EntityKey key, const PathSegmentIntent &segment,
                  std::span<const CarrierPoint> straight, const Rounding *start,
                  const Rounding *end, ParkSegment &out);

PathLayout layoutOf(const World &world);
// As the returning form, into a layout the caller keeps, clearing its lists first.
void layoutOf(const World &world, PathLayout &layout);

// The lists deriving a ground reuses, and the ground it gives: the layout read from the world, each
// segment's straight line, the scratch the corners are found with, and the ground. The straight
// lines, the ground's segments, and a caller's segments keep their extra values in spare lists.
struct GroundLists {
  PathLayout Layout;
  std::vector<std::vector<CarrierPoint>> Lines;
  std::vector<std::vector<CarrierPoint>> SpareLines;
  TickBuffers Scratch;
  PathGround Ground;
  std::vector<ParkSegment> SpareSegments;
  std::vector<ParkSegment> CallerSpare;
};

PathGround groundOf(const PathLayout &layout);
// As the returning form, into lists.Ground. Allocates nothing once the lists have grown.
void groundOf(const PathLayout &layout, GroundLists &lists);
// The paved surface the layout's segments make, given its ground.
PavedSurface surfaceOf(const PathLayout &layout, const PathGround &ground);
// The part of that surface that belongs to each point in focus, which is in ascending key order,
// and to each segment that ends at one of them. Unless hide is true, its edge lines are left
// whole, without hiding the parts that lie inside paving joined to them.
PavedSurface surfaceAround(const PathLayout &layout, const PathGround &ground,
                           const std::vector<EntityKey> &focus, bool hide);
// Each edge the layout's segments make, as parkEdges gives them, given its ground.
std::vector<ParkEdge> edgesOf(const PathLayout &layout, const PathGround &ground);
// The points, in ascending key order, whose part of the surface can change when the segments at
// the given points change: those points, and the points of each segment whose paving or edge
// lines can reach theirs.
std::vector<EntityKey> pointsAffected(const PathLayout &layout, const PathGround &ground,
                                      const std::vector<EntityKey> &points);

} // namespace tpj

#endif
