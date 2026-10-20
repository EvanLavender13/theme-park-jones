#include "sim/park/unit_reads.h"

#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/paving.h"
#include "sim/park/internal/surface_units.h"

#include <optional>
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

namespace {

// Puts in keys, clearing it first, the cells the bounds of the footprint at the pose cover, or
// none when the pose has no footprint.
void footprintCells(const Pose &at, FootprintSize size, std::vector<EntityKey> &keys) {
  keys.clear();
  if (const std::optional<Footprint> footprint = footprintOf(at, size)) {
    cellsOf(boundsOf(std::span<const ParkPoint>(footprint->Corners)), keys);
  }
}

// Files each box under the cells its footprint's bounds cover.
struct BoxCells {
  static constexpr std::string_view Name = "box-cells";
  using Of = BoxUnits;

  static void keysOf(const BoxUnit &unit, std::vector<EntityKey> &keys) {
    footprintCells(unit.Box.At, boxSize(unit.Box.Kind), keys);
  }
};

// Files each entrance under the cells its footprint's bounds cover.
struct EntranceCells {
  static constexpr std::string_view Name = "entrance-cells";
  using Of = EntranceUnits;

  static void keysOf(const EntranceUnit &unit, std::vector<EntityKey> &keys) {
    footprintCells(unit.Entrance.At, ENTRANCE_SIZE, keys);
  }
};

// A segment's furnishing: whether each side has one, and that side's, or a default one when it has
// none. The walk takes no std::optional, so each side is a flag and a value.
struct SegmentFurnishingUnit {
  bool HasLeft = false;
  SideFurnishing Left;
  bool HasRight = false;
  SideFurnishing Right;
};

template <typename Visitor> void visitFields(Visitor &visitor, SegmentFurnishingUnit &unit) {
  visitor.field("has-left", unit.HasLeft);
  visitor.field("left-kind", unit.Left.Kind);
  visitor.field("left-spacing", unit.Left.Spacing);
  visitor.field("has-right", unit.HasRight);
  visitor.field("right-kind", unit.Right.Kind);
  visitor.field("right-spacing", unit.Right.Spacing);
}

// Each segment's furnishing. Its Reads name the furnishing intents, so a command that changes only
// a segment's furnishing derives its unit again.
struct SegmentFurnishings {
  static constexpr std::string_view Name = "segment-furnishings";
  using Over = PathSegmentIntent;
  using Value = SegmentFurnishingUnit;
  using Reads = std::tuple<LeftFurnishingIntent, RightFurnishingIntent>;

  static void derive(UnitReads &reads, EntityKey key, SegmentFurnishingUnit &unit) {
    const LeftFurnishingIntent *left = reads.component<LeftFurnishingIntent>(key);
    const RightFurnishingIntent *right = reads.component<RightFurnishingIntent>(key);
    unit.HasLeft = left != nullptr;
    unit.Left = left != nullptr ? left->Held : SideFurnishing{};
    unit.HasRight = right != nullptr;
    unit.Right = right != nullptr ? right->Held : SideFurnishing{};
  }
};

} // namespace

void addParkUnits(WorldSchema &schema) {
  addUnits<BoxUnits>(schema);
  addUnits<EntranceUnits>(schema);
  addUnitIndex<BoxCells>(schema);
  addUnitIndex<EntranceCells>(schema);
  addUnits<SegmentFurnishings>(schema);
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

bool parkSegment(const World &world, EntityKey key, ParkSegment &segment) {
  const GroundLine *line = unitValue<GroundLines>(world, key);
  if (line == nullptr) {
    return false;
  }
  const ParkSegment &from = line->Segment;
  segment.Key = from.Key;
  segment.Kind = from.Kind;
  segment.From = from.From;
  segment.To = from.To;
  segment.Width = from.Width;
  copyGrowing(segment.Line, std::span<const CarrierPoint>(from.Line));
  segment.FromEdge = from.FromEdge;
  segment.ToEdge = from.ToEdge;
  segment.Furnished = {};
  if (const SegmentFurnishingUnit *furnishing = unitValue<SegmentFurnishings>(world, key)) {
    if (furnishing->HasLeft) {
      segment.Furnished.Left = furnishing->Left;
    }
    if (furnishing->HasRight) {
      segment.Furnished.Right = furnishing->Right;
    }
  }
  return true;
}

std::optional<ParkPoint> pathPointAt(const World &world, EntityKey point) {
  const PathPointIntent *intent = world.tryGet<PathPointIntent>(point);
  return intent != nullptr ? std::optional<ParkPoint>(intent->At) : std::nullopt;
}

namespace {

// Puts in the kept list, clearing it first, the keys the index files in the rectangle's cells,
// ascending, once each.
template <typename I>
void filedNear(const World &world, ParkPoint least, ParkPoint most, NearLists &lists,
               std::vector<EntityKey> &keys) {
  lists.Cells.clear();
  cellsOf(Bounds{least.X, least.Z, most.X, most.Z}, lists.Cells);
  keys.clear();
  for (const EntityKey cell : lists.Cells) {
    const std::span<const EntityKey> filed = filedIn<I>(world, cell);
    keys.insert(keys.end(), filed.begin(), filed.end());
  }
  sortUnique(keys);
}

// The lists changedParkCells keeps: the keys of the units that changed.
struct ChangedLists {
  std::vector<EntityKey> UnitKeys;
};

// Adds to the cells those the index files each changed unit of K under.
template <typename K, typename I>
void cellsOfChanged(const World &world, std::vector<EntityKey> &cells, ChangedLists &lists) {
  lists.UnitKeys.clear();
  changedUnits<K>(world, lists.UnitKeys);
  for (const EntityKey key : lists.UnitKeys) {
    const std::span<const EntityKey> filed = filedUnder<I>(world, key);
    cells.insert(cells.end(), filed.begin(), filed.end());
  }
}

} // namespace

void pavingNear(const World &world, ParkPoint least, ParkPoint most,
                std::vector<const SurfacePiece *> &pieces, TickBuffers &buffers) {
  pieces.clear();
  NearLists &lists = buffers.get<NearLists>();
  filedNear<PointCells>(world, least, most, lists, lists.Points);
  filedNear<SegmentCells>(world, least, most, lists, lists.Segments);
  // A point's pieces include the corner pieces it gives its segments, so a segment's body is the
  // only piece of its own to add.
  for (const EntityKey point : lists.Points) {
    for (const SurfacePiece &piece : unitValue<PointPavingUnits>(world, point)->Pieces) {
      pieces.push_back(&piece);
    }
  }
  for (const EntityKey segment : lists.Segments) {
    const SegmentPaving &paving = *unitValue<SegmentPavingUnits>(world, segment);
    if (paving.HasBody) {
      pieces.push_back(&paving.Pieces[paving.Body]);
    }
  }
}

void boxesNear(const World &world, ParkPoint least, ParkPoint most,
               std::vector<const ParkBox *> &boxes, TickBuffers &buffers) {
  boxes.clear();
  NearLists &lists = buffers.get<NearLists>();
  filedNear<BoxCells>(world, least, most, lists, lists.Points);
  for (const EntityKey key : lists.Points) {
    boxes.push_back(&unitValue<BoxUnits>(world, key)->Box);
  }
}

void entrancesNear(const World &world, ParkPoint least, ParkPoint most,
                   std::vector<const ParkEntrance *> &entrances, TickBuffers &buffers) {
  entrances.clear();
  NearLists &lists = buffers.get<NearLists>();
  filedNear<EntranceCells>(world, least, most, lists, lists.Points);
  for (const EntityKey key : lists.Points) {
    entrances.push_back(&unitValue<EntranceUnits>(world, key)->Entrance);
  }
}

void changedSegments(const World &world, std::vector<EntityKey> &segments) {
  changedUnits<SegmentFurnishings>(world, segments);
  changedUnits<SegmentPavingUnits>(world, segments);
  changedUnits<GroundLines>(world, segments);
}

void changedParkCells(const World &world, std::vector<EntityKey> &cells, TickBuffers &buffers) {
  ChangedLists &lists = buffers.get<ChangedLists>();
  changedUnits<PointCells>(world, cells);
  changedUnits<SegmentCells>(world, cells);
  changedUnits<BoxCells>(world, cells);
  changedUnits<EntranceCells>(world, cells);
  cellsOfChanged<PointPavingUnits, PointCells>(world, cells, lists);
  cellsOfChanged<SegmentPavingUnits, SegmentCells>(world, cells, lists);
  cellsOfChanged<BoxUnits, BoxCells>(world, cells, lists);
  cellsOfChanged<EntranceUnits, EntranceCells>(world, cells, lists);
  sortUnique(cells);
}

} // namespace tpj
