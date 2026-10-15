#include "sim/park/unit_reads.h"

#include "sim/park/internal/components.h"
#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/paving.h"
#include "sim/park/internal/surface_units.h"

#include <string_view>
#include <tuple>

namespace tpj {

namespace {

// A box as parkBox gives it.
struct BoxUnit {
  ParkBox Box;
};

template <typename Visitor> void visitFields(Visitor &visitor, BoxUnit &unit) {
  ParkBox &box = unit.Box;
  visitor.field("key", box.Key);
  visitor.field("kind", box.Kind);
  visitor.field("x", box.At.X);
  visitor.field("z", box.At.Z);
  visitor.field("facing-x", box.At.FacingX);
  visitor.field("facing-z", box.At.FacingZ);
  visitor.field("stock", box.Stock);
}

// An entrance as parkEntrances gives it.
struct EntranceUnit {
  ParkEntrance Entrance;
};

template <typename Visitor> void visitFields(Visitor &visitor, EntranceUnit &unit) {
  ParkEntrance &entrance = unit.Entrance;
  visitor.field("key", entrance.Key);
  visitor.field("x", entrance.At.X);
  visitor.field("z", entrance.At.Z);
  visitor.field("facing-x", entrance.At.FacingX);
  visitor.field("facing-z", entrance.At.FacingZ);
}

} // namespace

struct BoxUnits {
  static constexpr std::string_view Name = "park-boxes";
  using Over = BoxIntent;
  using Value = BoxUnit;
  using Reads = std::tuple<ShopStockIntent>;

  static void derive(UnitReads &reads, EntityKey key, BoxUnit &unit) {
    const BoxIntent &box = *reads.component<BoxIntent>(key);
    const ShopStockIntent *stock = reads.component<ShopStockIntent>(key);
    unit.Box = ParkBox{key, box.Kind, box.At, stock != nullptr ? stock->Stock : ShopStock::Food};
  }
};

struct EntranceUnits {
  static constexpr std::string_view Name = "park-entrances";
  using Over = EntranceIntent;
  using Value = EntranceUnit;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, EntranceUnit &unit) {
    unit.Entrance = ParkEntrance{key, reads.component<EntranceIntent>(key)->At};
  }
};

void addParkUnits(WorldSchema &schema) {
  addUnits<BoxUnits>(schema);
  addUnits<EntranceUnits>(schema);
}

const ParkBox *boxOf(UnitReads &reads, EntityKey key) {
  const BoxUnit *unit = reads.unit<BoxUnits>(key);
  return unit != nullptr ? &unit->Box : nullptr;
}

const ParkEntrance *entranceOf(UnitReads &reads, EntityKey key) {
  const EntranceUnit *unit = reads.unit<EntranceUnits>(key);
  return unit != nullptr ? &unit->Entrance : nullptr;
}

const ParkSegment *groundSegmentOf(UnitReads &reads, EntityKey key) {
  const GroundLine *line = reads.unit<GroundLines>(key);
  return line != nullptr ? &line->Segment : nullptr;
}

std::span<const EntityKey> segmentsAt(UnitReads &reads, EntityKey point) {
  return reads.filed<SegmentEnds>(point);
}

namespace {

// The lists pavingNear reuses: the cells, and the points and segments filed there.
struct NearLists {
  std::vector<EntityKey> Cells;
  std::vector<EntityKey> Points;
  std::vector<EntityKey> Segments;
};

} // namespace

void pavingNear(UnitReads &reads, ParkPoint least, ParkPoint most,
                std::vector<const SurfacePiece *> &pieces) {
  pieces.clear();
  NearLists &lists = reads.scratch().get<NearLists>();
  lists.Cells.clear();
  cellsOf(Bounds{least.X, least.Z, most.X, most.Z}, lists.Cells);
  lists.Points.clear();
  lists.Segments.clear();
  for (const EntityKey cell : lists.Cells) {
    const std::span<const EntityKey> points = reads.filed<PointCells>(cell);
    lists.Points.insert(lists.Points.end(), points.begin(), points.end());
    const std::span<const EntityKey> segments = reads.filed<SegmentCells>(cell);
    lists.Segments.insert(lists.Segments.end(), segments.begin(), segments.end());
  }
  sortUnique(lists.Points);
  sortUnique(lists.Segments);
  // A point's pieces include the corner pieces it gives its segments, so a segment's body is the
  // only piece of its own to add.
  for (const EntityKey point : lists.Points) {
    for (const SurfacePiece &piece : reads.unit<PointPavingUnits>(point)->Pieces) {
      pieces.push_back(&piece);
    }
  }
  for (const EntityKey segment : lists.Segments) {
    const SegmentPaving &paving = *reads.unit<SegmentPavingUnits>(segment);
    if (paving.HasBody) {
      pieces.push_back(&paving.Pieces[paving.Body]);
    }
  }
}

EdgeRun pointEdgeLines(UnitReads &reads, EntityKey point) {
  const PointPaving *paving = reads.unit<PointPavingUnits>(point);
  if (paving == nullptr) {
    return {};
  }
  return EdgeRun{paving->Edges, paving->EdgePoints};
}

void hideJoined(UnitReads &reads, EntityKey owner, std::span<const EdgeLine> edges,
                std::span<const ParkPoint> points, std::span<const SurfacePiece> extra,
                std::vector<EdgeLine> &outEdges, std::vector<ParkPoint> &outPoints) {
  const GroupLabel *label = labelOf(reads, owner);
  if (label == nullptr || edges.empty()) {
    return;
  }
  hideNearJoined(reads, label->Label, edges, points, extra, outEdges, outPoints);
}

void pavingCells(ParkPoint least, ParkPoint most, std::vector<EntityKey> &cells) {
  cellsOf(Bounds{least.X, least.Z, most.X, most.Z}, cells);
}

} // namespace tpj
