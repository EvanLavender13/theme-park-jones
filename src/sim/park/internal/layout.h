#ifndef TPJ_SIM_PARK_INTERNAL_LAYOUT_H
#define TPJ_SIM_PARK_INTERNAL_LAYOUT_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/components.h"
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

PathLayout layoutOf(const World &world);
// Each segment with its ground line, rounded at its corners, in ascending key order.
std::vector<ParkSegment> segmentsOf(const PathLayout &layout);
// Each corner, in ascending key order of its point.
std::vector<ParkCorner> cornersOf(const PathLayout &layout);

} // namespace tpj

#endif
