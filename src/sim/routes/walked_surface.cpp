#include "sim/routes/internal/walked_surface.h"

#include "sim/mix.h"
#include "sim/park/surface.h"
#include "sim/park/unit_reads.h"
#include "sim/routes/internal/connections.h"
#include "sim/routes/networks.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"
#include "sim/world.h"

#include <algorithm>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {

namespace {

// Derived, on its own entity: the park's paved surface with every connector's walkway.
struct WalkedSurface {
  PavedSurface Held;
};

template <typename Visitor> void visitFields(Visitor &visitor, WalkedSurface &walked) {
  visitor.field("surface", walked.Held);
}

inline constexpr uint64_t WALKED_SURFACE_PURPOSE = hashName("walked-surface");

// The lists the walked units reuse: the cells edge lines cover, the entities whose walkways are
// filed there, the walkway pieces found, and a segment's walkways in connector key order.
struct WalkedLists {
  std::vector<EntityKey> Cells;
  std::vector<EntityKey> Entities;
  std::vector<SurfacePiece> Pieces;
  std::vector<std::pair<EntityKey, SurfacePiece>> Joined;
};

// Puts in lists.Pieces, clearing it first, the pieces of the walkways filed in the cells the edge
// lines cover.
void walkwaysNear(UnitReads &reads, std::span<const EdgeLine> edges,
                  std::span<const ParkPoint> points, WalkedLists &lists) {
  lists.Cells.clear();
  for (const EdgeLine &edge : edges) {
    const std::span<const ParkPoint> line = points.subspan(edge.First, edge.Count);
    ParkPoint least = line.front();
    ParkPoint most = line.front();
    for (const ParkPoint &at : line) {
      least = {std::min(least.X, at.X), std::min(least.Z, at.Z)};
      most = {std::max(most.X, at.X), std::max(most.Z, at.Z)};
    }
    pavingCells(least, most, lists.Cells);
  }
  sortUnique(lists.Cells);
  lists.Entities.clear();
  for (const EntityKey cell : lists.Cells) {
    const std::span<const EntityKey> filed = reads.filed<WalkwayCells>(cell);
    lists.Entities.insert(lists.Entities.end(), filed.begin(), filed.end());
  }
  sortUnique(lists.Entities);
  lists.Pieces.clear();
  for (const EntityKey entity : lists.Entities) {
    for (const DoorWalkway &walkway : reads.unit<DoorWalkways>(entity)->Ways) {
      if (walkway.HasPiece) {
        lists.Pieces.push_back(walkway.Piece);
      }
    }
  }
}

// A point's edge lines in the walked surface, when a walkway piece lies near them: Own says
// whether one does, and so whether these replace its edge lines in the paved surface.
struct WalkedEdges {
  bool Own = false;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, WalkedEdges &walked) {
  visitor.field("own", walked.Own);
  visitor.field("edges", walked.Edges);
  visitor.field("edge-points", walked.EdgePoints);
}

// Hides each point's edge lines again, with the walkway pieces near them as well as the paving,
// when any walkway piece lies in the cells they cover. Elsewhere they are the paved surface's.
struct WalkedPointEdges {
  static constexpr std::string_view Name = "walked-point-edges";
  using Over = UnitsOf<PointPavingUnits>;
  using Value = WalkedEdges;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, WalkedEdges &walked) {
    walked.Own = false;
    walked.Edges.clear();
    walked.EdgePoints.clear();
    const EdgeRun run = pointEdgeLines(reads, key);
    if (run.Edges.empty()) {
      return;
    }
    WalkedLists &lists = reads.scratch().get<WalkedLists>();
    walkwaysNear(reads, run.Edges, run.Points, lists);
    if (lists.Pieces.empty()) {
      return;
    }
    walked.Own = true;
    hideJoined(reads, key, run.Edges, run.Points, lists.Pieces, walked.Edges, walked.EdgePoints);
  }
};

// A segment's walkways in the walked surface: their pieces, in connector key order, and their edge
// lines after hiding, with the positions those run through.
struct SegmentWalkways {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, SegmentWalkways &walkways) {
  visitor.field("pieces", walkways.Pieces);
  visitor.field("edges", walkways.Edges);
  visitor.field("edge-points", walkways.EdgePoints);
}

// The edge lines a segment's walkways make before hiding, and their positions.
struct RawWalkwayEdges {
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> Points;
};

// Gives each segment the pieces of the walkways that join it, and their edge lines along both
// sides and across the far end, hidden with the paving and the walkway pieces near them.
struct WalkedSegmentEdges {
  static constexpr std::string_view Name = "walked-segment-edges";
  using Over = UnitsOf<SegmentPavingUnits>;
  using Value = SegmentWalkways;
  using Reads = std::tuple<>;

  static void derive(UnitReads &reads, EntityKey key, SegmentWalkways &walkways) {
    walkways.Pieces.clear();
    walkways.Edges.clear();
    walkways.EdgePoints.clear();
    WalkedLists &lists = reads.scratch().get<WalkedLists>();
    lists.Joined.clear();
    for (const EntityKey entity : reads.filed<WalkwayJoins>(key)) {
      for (const DoorWalkway &walkway : reads.unit<DoorWalkways>(entity)->Ways) {
        if (walkway.HasPiece && walkway.Way.Joins == key) {
          lists.Joined.emplace_back(walkway.Connector, walkway.Piece);
        }
      }
    }
    if (lists.Joined.empty()) {
      return;
    }
    std::ranges::sort(lists.Joined, {}, &std::pair<EntityKey, SurfacePiece>::first);
    RawWalkwayEdges &raw = reads.scratch().get<RawWalkwayEdges>();
    raw.Edges.clear();
    raw.Points.clear();
    for (const auto &[connector, piece] : lists.Joined) {
      walkways.Pieces.push_back(piece);
      const std::span<const ParkPoint> corners = pieceCorners(piece);
      for (size_t i = 0; i + 1 < corners.size(); ++i) {
        raw.Edges.push_back(EdgeLine{piece.Kind, piece.Owner, raw.Points.size(), 2});
        raw.Points.push_back(corners[i]);
        raw.Points.push_back(corners[i + 1]);
      }
    }
    walkwaysNear(reads, raw.Edges, raw.Points, lists);
    hideJoined(reads, key, raw.Edges, raw.Points, lists.Pieces, walkways.Edges,
               walkways.EdgePoints);
  }
};

// The owners whose walked runs changed, which the walked-surface resolver reuses.
struct WalkedOwners {
  std::vector<EntityKey> Keys;
};

// Changes the walked surface only for the owners whose paved run or walked units the running
// resolution changed. An owner's walked run is its paved run, with its walked edge lines in place
// of its own when a walkway lies near them, and with the walkways that join it added.
void resolveWalkedSurface(World &world, TickBuffers &buffers) {
  PavedSurface &surface = world
                              .getOrEmplace<WalkedSurface>(
                                  world.createDerivedEntity(NULL_KEY, WALKED_SURFACE_PURPOSE, 0))
                              .Held;
  const PavedSurface &paved = resolvedSurface(world);
  std::vector<EntityKey> &owners = buffers.get<WalkedOwners>().Keys;
  owners.clear();
  changedPavingOwners(world, owners);
  changedUnits<WalkedPointEdges>(world, owners);
  changedUnits<WalkedSegmentEdges>(world, owners);
  for (const EntityKey owner : owners) {
    const OwnerPaving *from = pavingOf(paved, owner);
    const WalkedEdges *point = unitValue<WalkedPointEdges>(world, owner);
    const SegmentWalkways *segment = unitValue<WalkedSegmentEdges>(world, owner);
    const bool walkways = segment != nullptr && !segment->Pieces.empty();
    if (from == nullptr && !walkways) {
      surface.Owners.erase(owner);
      continue;
    }
    OwnerPaving &run = surface.Owners.getOrAdd(owner).first;
    if (from != nullptr) {
      copyGrowing(run.Pieces, std::span<const SurfacePiece>(from->Pieces));
      copyGrowing(run.Edges, std::span<const EdgeLine>(from->Edges));
      copyGrowing(run.EdgePoints, std::span<const ParkPoint>(from->EdgePoints));
      copyGrowing(run.Sides, std::span<const SegmentSide>(from->Sides));
      copyGrowing(run.SidePoints, std::span<const ParkPoint>(from->SidePoints));
    } else {
      run.Pieces.clear();
      run.Edges.clear();
      run.EdgePoints.clear();
      run.Sides.clear();
      run.SidePoints.clear();
    }
    if (point != nullptr && point->Own) {
      copyGrowing(run.Edges, std::span<const EdgeLine>(point->Edges));
      copyGrowing(run.EdgePoints, std::span<const ParkPoint>(point->EdgePoints));
    }
    if (walkways) {
      run.Pieces.insert(run.Pieces.end(), segment->Pieces.begin(), segment->Pieces.end());
      copyGrowing(run.Edges, std::span<const EdgeLine>(segment->Edges));
      copyGrowing(run.EdgePoints, std::span<const ParkPoint>(segment->EdgePoints));
    }
    if (run.Pieces.empty() && run.Edges.empty() && run.Sides.empty()) {
      surface.Owners.erase(owner);
    }
  }
}

} // namespace

const PavedSurface &walkedSurface(const World &world) {
  static const PavedSurface EMPTY;
  const WalkedSurface *walked =
      world.tryGet<WalkedSurface>(deriveKey(NULL_KEY, WALKED_SURFACE_PURPOSE, 0));
  return walked != nullptr ? walked->Held : EMPTY;
}

void addWalkedSurface(WorldSchema &schema) {
  schema.addComponent<WalkedSurface>("walked-surface", DataKind::Derived);
  addUnits<WalkedPointEdges>(schema);
  addUnits<WalkedSegmentEdges>(schema);
  schema.addResolver("walked-surface", &resolveWalkedSurface, {"path-networks"});
}

} // namespace tpj
