#ifndef TPJ_SIM_GUESTS_INTERNAL_GROUND_H
#define TPJ_SIM_GUESTS_INTERNAL_GROUND_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/surface.h"

#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// A line a guest may not cross, with the paving on its left seen from above: a run of a visible
// guest edge line, or a walkway's end at its door. Group is the group of joined paving it bounds.
struct GroundWall {
  GroundPoint From;
  GroundPoint To;
  uint32_t Group = 0;
  bool DoorEnd = false;
};

template <typename Visitor> void visitFields(Visitor &visitor, GroundWall &wall) {
  visitor.field("from-x", wall.From.X);
  visitor.field("from-z", wall.From.Z);
  visitor.field("to-x", wall.To.X);
  visitor.field("to-z", wall.To.Z);
  visitor.field("group", wall.Group);
  visitor.field("door-end", wall.DoorEnd);
}

// A piece of guest paving, the group of joined paving it belongs to, and the carrier a guest
// standing on it takes its place from after an edit.
struct GroundPiece {
  SurfacePiece Piece;
  uint32_t Group = 0;
  EntityKey Carrier = NULL_KEY;
};

template <typename Visitor> void visitFields(Visitor &visitor, GroundPiece &piece) {
  // The schema's fields don't include std::array, so the corners are listed one by one.
  visitor.field("kind", piece.Piece.Kind);
  visitor.field("owner", piece.Piece.Owner);
  visitor.field("c0", piece.Piece.Corners[0]);
  visitor.field("c1", piece.Piece.Corners[1]);
  visitor.field("c2", piece.Piece.Corners[2]);
  visitor.field("c3", piece.Piece.Corners[3]);
  visitor.field("count", piece.Piece.Count);
  visitor.field("group", piece.Group);
  visitor.field("carrier", piece.Carrier);
}

// A carrier of the guest network and the group of joined paving it runs on.
struct CarrierGroup {
  EntityKey Carrier = NULL_KEY;
  uint32_t Group = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, CarrierGroup &carrier) {
  visitor.field("carrier", carrier.Carrier);
  visitor.field("group", carrier.Group);
}

// A point and the group of joined paving it is on.
struct GroundSpot {
  GroundPoint At;
  uint32_t Group = 0;
};

// The side of the square cells the ground's lookup groups walls and pieces by, in meters.
inline constexpr double WALL_CELL_SIZE = 4.0;

// The guest paving as guests walk on it. Derived: built in resolution, never saved.
class GuestGround {
public:
  GuestGround() = default;
  // Builds the lookup by cell from the walls and pieces. Carriers are sorted by key.
  GuestGround(std::vector<GroundWall> walls, std::vector<GroundPiece> pieces,
              std::vector<CarrierGroup> carriers);

  // The group of the paving the carrier runs on, or none for a carrier the ground doesn't know.
  [[nodiscard]] std::optional<uint32_t> groupOf(EntityKey carrier) const;
  // Whether a straight line from one point to another stays on the group's paving with a guest's
  // box on it: it crosses none of the group's walls, and keeps GUEST_CLEARANCE from each, or, from
  // a wall nearer than that to where it starts, comes no nearer. Allocates nothing.
  [[nodiscard]] bool clearLine(uint32_t group, GroundPoint from, GroundPoint to) const;
  // Whether a guest's box fits at the point on the group's paving: the point is on a piece of the
  // group and at least GUEST_CLEARANCE from each of its walls, or from each other than door ends
  // when doorEnds is false.
  [[nodiscard]] bool fits(uint32_t group, GroundPoint point, bool doorEnds) const;
  // The first piece of the group the point stands on, else the first piece of any group it stands
  // on, else none. Pieces are in the order the ground was built with.
  [[nodiscard]] const GroundPiece *pieceAt(GroundPoint point, std::optional<uint32_t> group) const;
  // The nearest point to the point where a guest's box fits, with its group, or none when no point
  // fits.
  [[nodiscard]] std::optional<GroundSpot> nearestFit(GroundPoint point) const;

  // Lists the inputs. The lookup is rebuilt from them only by the constructor, so GuestGround is
  // never loaded: it is derived, and saves never hold it.
  template <typename Visitor> friend void visitFields(Visitor &visitor, GuestGround &ground) {
    visitor.field("walls", ground.Walls);
    visitor.field("pieces", ground.Pieces);
    visitor.field("carriers", ground.Carriers);
  }

private:
  // Whether no wall of the group listed within GUEST_CLEARANCE of the point lies nearer than
  // GUEST_CLEARANCE to it, door ends counted only when doorEnds is true.
  [[nodiscard]] bool clearOfWalls(uint32_t group, GroundPoint point, bool doorEnds) const;
  // Hands consider(candidate, group) the nearest-fit candidates the wall listed at that position
  // in WallCells gives toward the point: the nearest point of its copy shifted GUEST_CLEARANCE
  // toward its paving, the points GUEST_CLEARANCE from its ends toward the point, and where its
  // shifted copy crosses those of the cell's other walls of its group.
  template <typename Consider>
  void wallCandidates(GroundPoint point, size_t cell, uint32_t listed, Consider consider) const;

  std::vector<GroundWall> Walls;
  std::vector<GroundPiece> Pieces;
  std::vector<CarrierGroup> Carriers;

  // Built by the constructor from the members above: for cell c, row by row, the walls listed in
  // it run from WallStarts[c] up to WallStarts[c + 1] in WallCells, and likewise its pieces.
  std::vector<uint32_t> WallStarts;
  std::vector<uint32_t> WallCells;
  std::vector<uint32_t> PieceStarts;
  std::vector<uint32_t> PieceCells;
};

// The ground the last resolution derived, or an empty ground before the first.
const GuestGround &guestGround(const World &world);

// Registers the derived type guest-ground and its resolver, guest-ground, after path-networks.
void addGuestGround(WorldSchema &schema);

} // namespace tpj

#endif
