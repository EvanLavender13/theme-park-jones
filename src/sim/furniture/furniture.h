#ifndef TPJ_SIM_FURNITURE_FURNITURE_H
#define TPJ_SIM_FURNITURE_FURNITURE_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// A kind's footprint: its width along the paving and its depth across it, in meters.
constexpr FootprintSize furnitureSize(FurnitureKind kind) {
  switch (kind) {
  case FurnitureKind::Bench:
    return {1.8, 0.6};
  case FurnitureKind::Table:
    return {1.8, 1.6};
  }
  return {1.8, 0.6};
}

// A kind's number of seats.
constexpr uint32_t furnitureSeats(FurnitureKind kind) {
  switch (kind) {
  case FurnitureKind::Bench:
    return 2;
  case FurnitureKind::Table:
    return 4;
  }
  return 2;
}

// The pose of the seat at the index of a piece of the kind standing at the pose. A bench's two
// seats lie side by side along it, 0.45 m either side of its middle, facing as the bench faces. A
// table's four seats lie in pairs on its two long sides, 0.45 m either side of its middle along it
// and 0.5 m from its middle across it, each facing the middle. Seats are numbered from the left,
// seen from the paving, and a table's paving side comes first. Throws std::out_of_range for a seat
// not below furnitureSeats(kind).
Pose seatPose(FurnitureKind kind, const Pose &piece, uint32_t seat);

// A piece of furniture as other modules read it: its key, its segment, the side it stands on, Left
// or Right, its kind, and its pose, facing the paving.
struct FurniturePiece {
  EntityKey Key = NULL_KEY;
  EntityKey Segment = NULL_KEY;
  FurnishedSide Side = FurnishedSide::Left;
  FurnitureKind Kind = FurnitureKind::Bench;
  Pose At;

  bool operator==(const FurniturePiece &) const = default;
};

// A piece's seats as the seat places field gives them: the piece's key, its pose, its kind, its
// number of seats, and its segment.
struct SeatPlace {
  EntityKey Piece = NULL_KEY;
  Pose At;
  FurnitureKind Kind = FurnitureKind::Bench;
  uint32_t Seats = 0;
  EntityKey Segment = NULL_KEY;

  bool operator==(const SeatPlace &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, SeatPlace &place) {
  visitor.field("piece", place.Piece);
  visitor.field("x", place.At.X);
  visitor.field("z", place.At.Z);
  visitor.field("facing-x", place.At.FacingX);
  visitor.field("facing-z", place.At.FacingZ);
  visitor.field("kind", place.Kind);
  visitor.field("seats", place.Seats);
  visitor.field("segment", place.Segment);
}

// The seat places field: each furnished segment's pieces, at the places on its ground line nearest
// them.
struct SeatPlaces {
  using Entry = SeatPlace;
  static constexpr std::string_view Name = "seat-places";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// Registers the derived types furniture-piece and laid-furniture, the field seat places, and the
// resolver furniture.
void addFurniture(WorldSchema &schema);
// Every piece, in ascending key order.
std::vector<FurniturePiece> parkFurniture(const World &world);

} // namespace tpj

#endif
