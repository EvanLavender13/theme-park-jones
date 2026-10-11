#ifndef TPJ_SIM_FURNITURE_FURNITURE_H
#define TPJ_SIM_FURNITURE_FURNITURE_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

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

// Registers the derived type furniture-piece and the resolver furniture.
void addFurniture(WorldSchema &schema);
// Every piece, in ascending key order.
std::vector<FurniturePiece> parkFurniture(const World &world);

} // namespace tpj

#endif
