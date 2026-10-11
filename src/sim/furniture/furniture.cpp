#include "sim/furniture/furniture.h"

#include "sim/mix.h"
#include "sim/park/surface.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// How far a piece's front stands from the paving, in meters.
constexpr double VERGE_GAP = 0.3;
// How far a stretch's nearest piece stands from its end, in meters.
constexpr double END_GAP = 1.0;
// The least room between neighbouring pieces, in meters.
constexpr double PIECE_GAP = 0.4;
// How far before a segment's From point a stretch's start can lie, in meters, so that every
// start keys as a whole number of centimeters at least 0.
constexpr double KEY_ORIGIN = 512.0;

constexpr uint64_t PIECE_PURPOSE = hashName("furniture-piece");

// A piece of furniture a resolution laid: its segment, the side it stands on, its kind, and its
// pose.
struct FurniturePieceState {
  EntityKey Segment = NULL_KEY;
  FurnishedSide Side = FurnishedSide::Left;
  FurnitureKind Kind = FurnitureKind::Bench;
  Pose At;

  bool operator==(const FurniturePieceState &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, FurniturePieceState &piece) {
  visitor.field("segment", piece.Segment);
  visitor.field("side", piece.Side);
  visitor.field("kind", piece.Kind);
  visitor.field("x", piece.At.X);
  visitor.field("z", piece.At.Z);
  visitor.field("facing-x", piece.At.FacingX);
  visitor.field("facing-z", piece.At.FacingZ);
}

double dotOf(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }

bool isFurnished(const ParkSegment &segment) {
  return segment.Kind == PathKind::Guest &&
         (segment.Furnished.Left.has_value() || segment.Furnished.Right.has_value()) &&
         !segment.Line.empty();
}

// The side of the segment's body the surface gives, or none.
const BodySide *sideOf(const PavedSurface &surface, EntityKey segment, bool left) {
  const auto first = std::ranges::lower_bound(surface.Sides, segment, {}, &BodySide::Segment);
  for (auto it = first; it != surface.Sides.end() && it->Segment == segment; ++it) {
    if (it->Left == left) {
      return &*it;
    }
  }
  return nullptr;
}

// Lays the pieces of one stretch, its side's body from start to end, evenly spread and clear of
// its ends, and adds each piece's key to laid. along is the segment's unit direction, and from its
// From point.
void layStretch(World &world, EntityKey segment, FurnishedSide side,
                const SideFurnishing &furnishing, const BodySide &stretch, ParkPoint from,
                ParkPoint along, std::vector<EntityKey> &laid) {
  const ParkPoint run{stretch.End.X - stretch.Start.X, stretch.End.Z - stretch.Start.Z};
  const double length = std::sqrt(dotOf(run, run));
  if (!(length > 0.0)) {
    return;
  }
  const ParkPoint d{run.X / length, run.Z / length};
  // Off the paving: a quarter turn counter-clockwise, seen from above, for the left side, and
  // clockwise for the right.
  const ParkPoint out = side == FurnishedSide::Left ? ParkPoint{d.Z, -d.X} : ParkPoint{-d.Z, d.X};
  const FootprintSize size = furnitureSize(furnishing.Kind);
  const double leastStep = size.Width + PIECE_GAP;
  const double step = std::max(furnishing.Spacing, leastStep);
  const double room = length - (2.0 * END_GAP) - size.Width;
  if (room < 0.0) {
    return;
  }
  // Of the two counts of gaps around room / step, the one whose spacing lies nearer the step, as
  // long as neighbours keep the least step apart. With no gap, the spacing is the whole room.
  const auto fewer = static_cast<uint64_t>(std::floor(room / step));
  const auto spacingOf = [room](uint64_t count) {
    return count == 0 ? room : room / static_cast<double>(count);
  };
  uint64_t gaps = fewer;
  if (spacingOf(fewer + 1) >= leastStep &&
      std::abs(spacingOf(fewer + 1) - step) < std::abs(spacingOf(fewer) - step)) {
    gaps = fewer + 1;
  }
  const double startAlong =
      dotOf(ParkPoint{stretch.Start.X - from.X, stretch.Start.Z - from.Z}, along) + KEY_ORIGIN;
  const auto stretchKey = static_cast<uint64_t>(std::floor(startAlong * 100.0));
  const uint64_t sideBit = side == FurnishedSide::Right ? (1ULL << 63) : 0ULL;
  const double outward = VERGE_GAP + (size.Depth / 2.0);
  for (uint64_t i = 0; i <= gaps; ++i) {
    const double at = gaps == 0 ? length / 2.0
                                : END_GAP + (size.Width / 2.0) +
                                      (static_cast<double>(i) * room / static_cast<double>(gaps));
    const Pose pose{stretch.Start.X + (d.X * at) + (out.X * outward),
                    stretch.Start.Z + (d.Z * at) + (out.Z * outward), -out.X, -out.Z};
    const uint64_t index = sideBit | ((stretchKey & ((1ULL << 47) - 1)) << 16) | (i & 0xFFFF);
    const EntityKey key = world.createDerivedEntity(segment, PIECE_PURPOSE, index);
    world.emplaceOrReplace<FurniturePieceState>(
        key, FurniturePieceState{segment, side, furnishing.Kind, pose});
    laid.push_back(key);
  }
}

// Lays the pieces each furnished guest segment asks for, and removes every piece laid before that
// this resolution doesn't lay.
void layFurniture(World &world) {
  const std::vector<EntityKey> before = world.keysWith<FurniturePieceState>();
  std::vector<EntityKey> laid;
  const std::vector<ParkSegment> segments = parkSegments(world);
  if (std::ranges::any_of(segments, isFurnished)) {
    const PavedSurface surface = parkSurface(world);
    const std::vector<ParkPathPoint> points = parkPathPoints(world);
    const auto positionOf = [&points](EntityKey key) -> std::optional<ParkPoint> {
      const auto found = std::ranges::lower_bound(points, key, {}, &ParkPathPoint::Key);
      return found != points.end() && found->Key == key ? std::optional<ParkPoint>(found->At)
                                                        : std::nullopt;
    };
    for (const ParkSegment &segment : segments) {
      if (!isFurnished(segment)) {
        continue;
      }
      const std::optional<ParkPoint> from = positionOf(segment.From);
      const std::optional<ParkPoint> to = positionOf(segment.To);
      if (!from || !to) {
        continue;
      }
      const ParkPoint span{to->X - from->X, to->Z - from->Z};
      const double spanLength = std::sqrt(dotOf(span, span));
      if (!(spanLength > 0.0)) {
        continue;
      }
      const ParkPoint along{span.X / spanLength, span.Z / spanLength};
      for (const FurnishedSide side : {FurnishedSide::Left, FurnishedSide::Right}) {
        const std::optional<SideFurnishing> &furnishing =
            side == FurnishedSide::Left ? segment.Furnished.Left : segment.Furnished.Right;
        if (!furnishing) {
          continue;
        }
        if (const BodySide *stretch = sideOf(surface, segment.Key, side == FurnishedSide::Left)) {
          layStretch(world, segment.Key, side, *furnishing, *stretch, *from, along, laid);
        }
      }
    }
  }
  std::ranges::sort(laid);
  for (const EntityKey key : before) {
    if (!std::ranges::binary_search(laid, key)) {
      world.destroyEntity(key);
    }
  }
}

} // namespace

void addFurniture(WorldSchema &schema) {
  schema.addComponent<FurniturePieceState>("furniture-piece", DataKind::Derived);
  schema.addResolver("furniture", layFurniture);
}

std::vector<FurniturePiece> parkFurniture(const World &world) {
  std::vector<FurniturePiece> pieces;
  for (const EntityKey key : world.keysWith<FurniturePieceState>()) {
    const FurniturePieceState &piece = world.get<FurniturePieceState>(key);
    pieces.push_back(FurniturePiece{key, piece.Segment, piece.Side, piece.Kind, piece.At});
  }
  return pieces;
}

} // namespace tpj
