#ifndef TPJ_SIM_PARK_INTERNAL_LAYOUT_H
#define TPJ_SIM_PARK_INTERNAL_LAYOUT_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/components.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

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

PathLayout layoutOf(const World &world);
// As the returning form, into a layout the caller keeps, clearing its lists first.
void layoutOf(const World &world, PathLayout &layout);

// The lists deriving a ground reuses, and the ground it gives: the layout read from the world, each
// segment's straight line, the arc points, the scratch the roundings are found with, and the
// ground. The straight lines, the ground's segments, and a caller's segments keep their extra
// values in spare lists.
struct GroundLists {
  PathLayout Layout;
  std::vector<std::vector<CarrierPoint>> Lines;
  std::vector<std::vector<CarrierPoint>> SpareLines;
  std::vector<ParkPoint> Arcs;
  TickBuffers Scratch;
  PathGround Ground;
  std::vector<ParkSegment> SpareSegments;
  std::vector<ParkSegment> CallerSpare;
};

PathGround groundOf(const PathLayout &layout);
// As the returning form, into lists.Ground. Allocates nothing once the lists have grown.
void groundOf(const PathLayout &layout, GroundLists &lists);
// Each segment with its ground line, rounded at its corners, in ascending key order.
std::vector<ParkSegment> segmentsOf(const PathLayout &layout);
// Each corner, in ascending key order of its point.
std::vector<ParkCorner> cornersOf(const PathLayout &layout);
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
