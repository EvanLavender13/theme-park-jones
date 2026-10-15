#include "sim/park/internal/surface_units.h"

#include "sim/mix.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/paving.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <stdint.h>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

// The label of a point's group: the lowest key among the points that segments with ground lines
// join it to, itself included.
struct GroupLabel {
  EntityKey Label = NULL_KEY;
};

template <typename Visitor> void visitFields(Visitor &visitor, GroupLabel &label) {
  visitor.field("label", label.Label);
}

// The lists labelling reuses: the points it takes, the ground lines that changed, the points a
// fill has reached, the group being filled, ascending, and the points still to visit.
struct LabelLists {
  std::vector<EntityKey> Taken;
  std::vector<EntityKey> Lines;
  std::vector<EntityKey> Reached;
  std::vector<EntityKey> Group;
  std::vector<EntityKey> Stack;
};

// Puts in taken each point whose segments or whose segments' ground lines changed, ascending.
void takeChanged(UnitUpdate<GroupLabel> &update, LabelLists &lists) {
  std::vector<EntityKey> &taken = lists.Taken;
  taken.clear();
  update.changed<SegmentEnds>(taken);
  lists.Lines.clear();
  update.changed<GroundLines>(lists.Lines);
  for (const EntityKey key : lists.Lines) {
    if (const GroundLine *line = update.unit<GroundLines>(key)) {
      taken.push_back(line->Segment.From);
      taken.push_back(line->Segment.To);
    }
  }
  sortUnique(taken);
}

// Puts in lists.Group the points joined to the point by segments with ground lines, the point
// included, ascending. Returns whether any such segment is at the point.
bool fillGroup(UnitUpdate<GroupLabel> &update, EntityKey point, LabelLists &lists) {
  std::vector<EntityKey> &group = lists.Group;
  group.assign(1, point);
  lists.Stack.assign(1, point);
  bool joined = false;
  while (!lists.Stack.empty()) {
    const EntityKey at = lists.Stack.back();
    lists.Stack.pop_back();
    for (const EntityKey segment : update.filed<SegmentEnds>(at)) {
      const GroundLine *line = update.unit<GroundLines>(segment);
      if (line == nullptr || line->Segment.Line.empty()) {
        continue;
      }
      joined = true;
      const EntityKey other = line->Segment.From == at ? line->Segment.To : line->Segment.From;
      if (!std::ranges::binary_search(group, other)) {
        insertSorted(group, other);
        lists.Stack.push_back(other);
      }
    }
  }
  return joined;
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

  static void update(UnitUpdate<GroupLabel> &update) {
    LabelLists &lists = update.scratch().get<LabelLists>();
    std::vector<EntityKey> &taken = lists.Taken;
    if (update.first()) {
      update.overKeys(taken);
    } else {
      takeChanged(update, lists);
    }
    lists.Reached.clear();
    for (const EntityKey point : taken) {
      if (std::ranges::binary_search(lists.Reached, point)) {
        continue;
      }
      if (!fillGroup(update, point, lists)) {
        if (update.held(point) != nullptr) {
          update.remove(point);
        }
        insertSorted(lists.Reached, point);
        continue;
      }
      const std::vector<EntityKey> &group = lists.Group;
      for (const EntityKey member : group) {
        const GroupLabel *was = update.held(member);
        if (was == nullptr || was->Label != group.front()) {
          update.set(member, GroupLabel{group.front()});
        }
      }
      lists.Reached.insert(lists.Reached.end(), group.begin(), group.end());
      sortUnique(lists.Reached);
    }
  }
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

// Which of a far end's values a point's input holds.
enum class FarValues { None, Asks, AsksAndTrims };

struct PointAsksUnits;
struct PointFitsUnits;

// Fills the input of the point the key holds through the reads, with each far end's values that
// far names.
void readInput(UnitReads &reads, EntityKey key, FarValues far, PointInput &input);

struct PointAsksUnits {
  static constexpr std::string_view Name = "point-asks";
  using Over = PathPointIntent;
  using Value = PointAsks;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, PointAsks &asks) {
    SurfaceUnitLists &lists = reads.scratch().get<SurfaceUnitLists>();
    readInput(reads, key, FarValues::None, lists.Point);
    askAt(lists.Point, lists.Work, asks);
  }
};

struct PointFitsUnits {
  static constexpr std::string_view Name = "point-fits";
  using Over = PathPointIntent;
  using Value = PointTrims;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, PointTrims &trims) {
    SurfaceUnitLists &lists = reads.scratch().get<SurfaceUnitLists>();
    readInput(reads, key, FarValues::Asks, lists.Point);
    fitAt(lists.Point, lists.Work, trims);
  }
};

struct PointPavingUnits {
  static constexpr std::string_view Name = "point-paving";
  using Over = PathPointIntent;
  using Value = PointPaving;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, PointPaving &paving) {
    SurfaceUnitLists &lists = reads.scratch().get<SurfaceUnitLists>();
    readInput(reads, key, FarValues::AsksAndTrims, lists.Point);
    paveAt(lists.Point, lists.Work, paving);
  }
};

void readInput(UnitReads &reads, EntityKey key, FarValues far, PointInput &input) {
  const PathPointIntent &point = *reads.component<PathPointIntent>(key);
  input.Key = key;
  input.At = point.At;
  input.Taper = heldTaper(point);
  const Corner *corner = reads.unit<Corners>(key);
  input.HasCorner = corner != nullptr && corner->Has;
  if (input.HasCorner) {
    input.Corner = corner->Round.Corner;
  }
  input.Ends.clear();
  for (const EntityKey segmentKey : reads.filed<SegmentEnds>(key)) {
    const GroundLine *line = reads.unit<GroundLines>(segmentKey);
    if (line == nullptr || line->Segment.Line.empty()) {
      continue;
    }
    const ParkSegment &segment = line->Segment;
    const ParkPoint from = reads.component<PathPointIntent>(segment.From)->At;
    const ParkPoint to = reads.component<PathPointIntent>(segment.To)->At;
    for (const size_t which : {size_t{0}, size_t{1}}) {
      if ((which == 0 ? segment.From : segment.To) != key) {
        continue;
      }
      PavingEnd end{pavingSegment(segment), from, to, which, {}, {}};
      const EntityKey farPoint = which == 0 ? segment.To : segment.From;
      if (far != FarValues::None) {
        if (const PointAsks *asks = reads.unit<PointAsksUnits>(farPoint)) {
          end.FarAsks = sidesOf(*asks, segmentKey, 1 - which);
        }
      }
      if (far == FarValues::AsksAndTrims) {
        if (const PointTrims *trims = reads.unit<PointFitsUnits>(farPoint)) {
          end.FarTrims = sidesOf(*trims, segmentKey, 1 - which);
        }
      }
      input.Ends.push_back(end);
    }
  }
}

struct SegmentPavingUnits {
  static constexpr std::string_view Name = "segment-paving";
  using Over = PathSegmentIntent;
  using Value = SegmentPaving;
  using Reads = std::tuple<PathPointIntent>;

  static void derive(UnitReads &reads, EntityKey key, SegmentPaving &paving) {
    const PathSegmentIntent &intent = *reads.component<PathSegmentIntent>(key);
    const GroundLine *line = reads.unit<GroundLines>(key);
    if (line == nullptr || line->Segment.Line.empty()) {
      paving.From = intent.From;
      paving.Pieces.clear();
      paving.HasBody = false;
      paving.Body = 0;
      paving.Sides.clear();
      paving.SidePoints.clear();
      return;
    }
    const ParkSegment &segment = line->Segment;
    SurfaceUnitLists &lists = reads.scratch().get<SurfaceUnitLists>();
    segmentInputOf(
        pavingSegment(segment), reads.component<PathPointIntent>(segment.From)->At,
        reads.component<PathPointIntent>(segment.To)->At,
        {reads.unit<PointFitsUnits>(segment.From), reads.unit<PointFitsUnits>(segment.To)},
        {reads.unit<PointPavingUnits>(segment.From), reads.unit<PointPavingUnits>(segment.To)},
        lists.Segment);
    paveSegment(lists.Segment, lists.Work, paving);
  }
};

// The side of a cell, in meters.
constexpr double SURFACE_CELL = 8.0;

// The key of the cell a coordinate pair lies in: the cell's column and row, each offset by 2^31,
// in the high and low 32 bits, so no cell's key is NULL_KEY.
EntityKey cellKey(int64_t column, int64_t row) {
  constexpr int64_t OFFSET = int64_t{1} << 31;
  return EntityKey{(static_cast<uint64_t>(column + OFFSET) << 32) |
                   static_cast<uint64_t>(row + OFFSET)};
}

int64_t cellOf(double coordinate) {
  return static_cast<int64_t>(std::floor(coordinate / SURFACE_CELL));
}

// Appends the key of every cell the bounds cover, from the cell of its least corner to the cell of
// its greatest.
void cellsOf(const Bounds &bounds, std::vector<EntityKey> &keys) {
  for (int64_t column = cellOf(bounds.MinX); column <= cellOf(bounds.MaxX); ++column) {
    for (int64_t row = cellOf(bounds.MinZ); row <= cellOf(bounds.MaxZ); ++row) {
      keys.push_back(cellKey(column, row));
    }
  }
}

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

  static void derive(UnitReads &reads, EntityKey key, HiddenRun &run) {
    run.Edges.clear();
    run.EdgePoints.clear();
    const PointPaving *paving = reads.unit<PointPavingUnits>(key);
    const GroupLabel *label = reads.unit<GroupLabels>(key);
    // A point with no label has no paving.
    if (paving == nullptr || label == nullptr || paving->Edges.empty()) {
      return;
    }
    SurfaceUnitLists &lists = reads.scratch().get<SurfaceUnitLists>();
    lists.Cells.clear();
    for (const EdgeLine &edge : paving->Edges) {
      cellsOf(
          boundsOf(std::span<const ParkPoint>(paving->EdgePoints).subspan(edge.First, edge.Count)),
          lists.Cells);
    }
    sortUnique(lists.Cells);
    lists.NearPoints.clear();
    lists.NearSegments.clear();
    for (const EntityKey cell : lists.Cells) {
      const std::span<const EntityKey> points = reads.filed<PointCells>(cell);
      lists.NearPoints.insert(lists.NearPoints.end(), points.begin(), points.end());
      const std::span<const EntityKey> segments = reads.filed<SegmentCells>(cell);
      lists.NearSegments.insert(lists.NearSegments.end(), segments.begin(), segments.end());
    }
    sortUnique(lists.NearPoints);
    sortUnique(lists.NearSegments);
    const auto joined = [&reads, label](EntityKey point) {
      const GroupLabel *other = reads.unit<GroupLabels>(point);
      return other != nullptr && other->Label == label->Label;
    };
    std::vector<Hider> &hiders = lists.Hiders;
    hiders.clear();
    for (const EntityKey point : lists.NearPoints) {
      if (!joined(point)) {
        continue;
      }
      for (const SurfacePiece &piece : reads.unit<PointPavingUnits>(point)->Pieces) {
        hiders.push_back(Hider{boundsOf(piece), &piece});
      }
    }
    for (const EntityKey segment : lists.NearSegments) {
      const SegmentPaving &near = *reads.unit<SegmentPavingUnits>(segment);
      if (near.HasBody && joined(near.From)) {
        const SurfacePiece &body = near.Pieces[near.Body];
        hiders.push_back(Hider{boundsOf(body), &body});
      }
    }
    hideOwner(paving->Edges, paving->EdgePoints, hiders, lists.Work, run.Edges, run.EdgePoints);
  }
};

inline constexpr uint64_t SURFACE_PURPOSE = hashName("paved-surface");

// The owners whose units changed, which the paved-surface resolver reuses.
struct ChangedOwners {
  std::vector<EntityKey> Keys;
};

// Changes the paved surface only for the owners whose paving, or whose hidden edge lines, the
// running resolution changed: a point's run is its own pieces and its hidden edge lines, and a
// segment's its pieces and sides.
void resolvePavedSurface(World &world, TickBuffers &buffers) {
  PavedSurface &surface =
      world.getOrEmplace<PavedSurface>(world.createDerivedEntity(NULL_KEY, SURFACE_PURPOSE, 0));
  std::vector<EntityKey> &owners = buffers.get<ChangedOwners>().Keys;
  owners.clear();
  changedUnits<PointPavingUnits>(world, owners);
  changedUnits<SegmentPavingUnits>(world, owners);
  changedUnits<HiddenEdges>(world, owners);
  for (const EntityKey owner : owners) {
    if (const SegmentPaving *segment = unitValue<SegmentPavingUnits>(world, owner)) {
      if (segment->Pieces.empty() && segment->Sides.empty()) {
        surface.Owners.erase(owner);
        continue;
      }
      OwnerPaving &run = pavingFor(surface, owner);
      copyGrowing(run.Pieces, std::span<const SurfacePiece>(segment->Pieces));
      run.Edges.clear();
      run.EdgePoints.clear();
      copyGrowing(run.Sides, std::span<const SegmentSide>(segment->Sides));
      copyGrowing(run.SidePoints, std::span<const ParkPoint>(segment->SidePoints));
      continue;
    }
    const PointPaving *paving = unitValue<PointPavingUnits>(world, owner);
    const HiddenRun *hidden = unitValue<HiddenEdges>(world, owner);
    const bool owns =
        paving != nullptr && std::ranges::any_of(paving->Pieces, [owner](const SurfacePiece &p) {
          return p.Owner == owner;
        });
    if (!owns && (hidden == nullptr || hidden->Edges.empty())) {
      surface.Owners.erase(owner);
      continue;
    }
    OwnerPaving &run = pavingFor(surface, owner);
    run.Pieces.clear();
    if (paving != nullptr) {
      for (const SurfacePiece &piece : paving->Pieces) {
        if (piece.Owner == owner) {
          run.Pieces.push_back(piece);
        }
      }
    }
    if (hidden != nullptr) {
      copyGrowing(run.Edges, std::span<const EdgeLine>(hidden->Edges));
      copyGrowing(run.EdgePoints, std::span<const ParkPoint>(hidden->EdgePoints));
    } else {
      run.Edges.clear();
      run.EdgePoints.clear();
    }
    run.Sides.clear();
    run.SidePoints.clear();
  }
}

} // namespace

void addSurfaceUnits(WorldSchema &schema) {
  addUnits<GroupLabels>(schema);
  addUnits<PointAsksUnits>(schema);
  addUnits<PointFitsUnits>(schema);
  addUnits<PointPavingUnits>(schema);
  addUnits<SegmentPavingUnits>(schema);
  addUnitIndex<PointCells>(schema);
  addUnitIndex<SegmentCells>(schema);
  addUnits<HiddenEdges>(schema);
  schema.addResolver("paved-surface", &resolvePavedSurface);
}

EntityKey pavedSurfaceKey() { return deriveKey(NULL_KEY, SURFACE_PURPOSE, 0); }

} // namespace tpj
