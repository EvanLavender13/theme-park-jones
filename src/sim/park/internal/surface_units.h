#ifndef TPJ_SIM_PARK_INTERNAL_SURFACE_UNITS_H
#define TPJ_SIM_PARK_INTERNAL_SURFACE_UNITS_H

#include "sim/entity_key.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/paving.h"
#include "sim/park/surface.h"
#include "sim/schema.h"
#include "sim/units.h"

#include <span>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

// Registers the units the paved surface derives in: each group's label, each point's asks, fit,
// and paving, each segment's paving, the cells their pieces cover, and each point's hidden edge
// lines, then the paved-surface resolver, which keeps the surface from them.
void addSurfaceUnits(WorldSchema &schema);

// The derived key of the entity that holds the paved surface.
EntityKey pavedSurfaceKey();

// The label of a point's group: the lowest key among the points that segments with ground lines
// join it to, itself included.
struct GroupLabel {
  EntityKey Label = NULL_KEY;
};

template <typename Visitor> void visitFields(Visitor &visitor, GroupLabel &label) {
  visitor.field("label", label.Label);
}

// Labels each point joined to another by a segment with a ground line. At the first update it
// takes every point, and after that each point whose segments or whose segments' ground lines
// changed. From each point taken that no earlier fill reached, it fills the point's group and gives
// every point in it the group's lowest key, setting only the labels that are new or differ. A point
// with no such segment has no label.
struct GroupLabels {
  static constexpr std::string_view Name = "group-labels";
  using Over = PathPointIntent;
  using Value = GroupLabel;

  static void update(UnitUpdate<GroupLabel> &update);
};

struct PointPavingUnits {
  static constexpr std::string_view Name = "point-paving";
  using Over = PathPointIntent;
  using Value = PointPaving;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, PointPaving &paving);
};

struct SegmentPavingUnits {
  static constexpr std::string_view Name = "segment-paving";
  using Over = PathSegmentIntent;
  using Value = SegmentPaving;
  using Reads = std::tuple<PathPointIntent>;

  static void derive(UnitReads &reads, EntityKey key, SegmentPaving &paving);
};

// Appends the key of every cell the bounds cover, from the cell of its least corner to the cell of
// its greatest.
void cellsOf(const Bounds &bounds, std::vector<EntityKey> &keys);

// Files each point under the cells its pieces and the corner pieces it gives cover.
struct PointCells {
  static constexpr std::string_view Name = "point-cells";
  using Of = PointPavingUnits;

  static void keysOf(const PointPaving &paving, std::vector<EntityKey> &keys) {
    keys.clear();
    for (const SurfacePiece &piece : paving.Pieces) {
      cellsOf(boundsOf(piece), keys);
    }
  }
};

// Files each segment under the cells its body covers.
struct SegmentCells {
  static constexpr std::string_view Name = "segment-cells";
  using Of = SegmentPavingUnits;

  static void keysOf(const SegmentPaving &paving, std::vector<EntityKey> &keys) {
    keys.clear();
    if (paving.HasBody) {
      cellsOf(boundsOf(paving.Pieces[paving.Body]), keys);
    }
  }
};

// A point's edge lines after hiding, and the positions they run through.
struct HiddenRun {
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, HiddenRun &run) {
  visitor.field("edges", run.Edges);
  visitor.field("edge-points", run.EdgePoints);
}

// Hides each point's edge lines with the pieces near them of the points and segments in its
// group: those filed in the cells its edge lines cover whose label, or whose From point's label,
// is the point's.
struct HiddenEdges {
  static constexpr std::string_view Name = "hidden-edges";
  using Over = PathPointIntent;
  using Value = HiddenRun;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, HiddenRun &run);
};

// The lists the surface's units reuse: a point's input, a segment's input, the paving's scratch,
// the hiders of a point's edge lines, the cells its edge lines cover, and the points and segments
// filed there.
struct SurfaceUnitLists {
  PointInput Point;
  SegmentInput Segment;
  PavingScratch Work;
  std::vector<Hider> Hiders;
  std::vector<EntityKey> Cells;
  std::vector<EntityKey> NearPoints;
  std::vector<EntityKey> NearSegments;
};

// The label of the owner's group: its own label when it is a point with one, the label of its From
// point when it is a segment with a ground line, or null when it has none.
const GroupLabel *labelOf(UnitReads &reads, EntityKey owner);

// Appends to the out lists the parts of the edge lines, whose First counts from the start of
// points, that no piece of the group with the label hides: none of the paving's pieces filed in
// the cells the edge lines cover, and none of the extra pieces.
void hideNearJoined(UnitReads &reads, EntityKey label, std::span<const EdgeLine> edges,
                    std::span<const ParkPoint> points, std::span<const SurfacePiece> extra,
                    std::vector<EdgeLine> &outEdges, std::vector<ParkPoint> &outPoints);

} // namespace tpj

#endif
