#ifndef TPJ_SIM_PARK_SURFACE_H
#define TPJ_SIM_PARK_SURFACE_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <array>
#include <span>
#include <stddef.h>
#include <vector>

namespace tpj {

// A convex polygon of paving: its first Count corners, counter-clockwise seen from above. Owner is
// the segment it belongs to, or the point of the junction it fills.
struct SurfacePiece {
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;
  std::array<ParkPoint, 4> Corners{};
  size_t Count = 0;

  bool operator==(const SurfacePiece &) const = default;
};

// A run of positions along the paving's outline, with the paving on its left seen from above: the
// Count positions of its surface's EdgePoints from First. Owner is the point it runs around.
struct EdgeLine {
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;
  size_t First = 0;
  size_t Count = 0;
};

// The paving the park's segments make: its pieces and its edge lines, each list in ascending key
// order of its owner, and the positions the edge lines run through.
struct PavedSurface {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
};

// The piece's corners.
std::span<const ParkPoint> pieceCorners(const SurfacePiece &piece);
// The positions the edge line of the surface runs through.
std::span<const ParkPoint> edgePoints(const PavedSurface &surface, const EdgeLine &edge);

// The park's paved surface. A segment with an empty ground line adds nothing.
PavedSurface parkSurface(const World &world);

} // namespace tpj

#endif
