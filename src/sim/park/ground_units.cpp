#include "sim/park/internal/ground_units.h"

#include "sim/park/internal/components.h"
#include "sim/park/internal/layout.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"

#include <array>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

void addPathGround(WorldSchema &schema) {
  addUnitIndex<SegmentEnds>(schema);
  addUnits<StraightLines>(schema);
  addUnits<PointArmsUnits>(schema);
  addUnits<Corners>(schema);
  addUnits<GroundLines>(schema);
}

void worldGround(const World &world, GroundLists &lists) {
  layoutOf(world, lists.Layout);
  if (!unitsCurrent<GroundLines>(world) || !unitsCurrent<Corners>(world)) {
    groundOf(lists.Layout, lists);
    return;
  }
  const PathLayout &layout = lists.Layout;
  PathGround &ground = lists.Ground;
  resizeKeeping(ground.Segments, layout.Segments.size(), lists.SpareSegments);
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const GroundLine *line = unitValue<GroundLines>(world, layout.Segments[i].Key);
    if (line == nullptr) {
      throw std::logic_error("a current segment has no ground line unit");
    }
    ParkSegment &segment = ground.Segments[i];
    segment.Key = line->Segment.Key;
    segment.Kind = line->Segment.Kind;
    segment.From = line->Segment.From;
    segment.To = line->Segment.To;
    segment.Width = line->Segment.Width;
    copyGrowing(segment.Line, std::span<const CarrierPoint>(line->Segment.Line));
    segment.FromEdge = line->Segment.FromEdge;
    segment.ToEdge = line->Segment.ToEdge;
    segment.Furnished = {};
  }
  ground.Corners.clear();
  for (const LayoutPoint &point : layout.Points) {
    const Corner *corner = unitValue<Corners>(world, point.Key);
    if (corner != nullptr && corner->Has) {
      ground.Corners.push_back(corner->Round.Corner);
    }
  }
}

} // namespace tpj
