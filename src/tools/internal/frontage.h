#ifndef TPJ_TOOLS_INTERNAL_FRONTAGE_H
#define TPJ_TOOLS_INTERNAL_FRONTAGE_H

#include "sim/park/intent.h"
#include "sim/park/surface.h"

#include <stddef.h>
#include <vector>

namespace tpj {

// A step of an edge line: where it starts, its unit direction, its length, and the unit direction
// away from the paving, which lies on its left.
struct EdgeStep {
  ParkPoint From;
  ParkPoint Along;
  double Length = 0.0;
  ParkPoint Out;
};

// A rectangle on the ground, its sides along the axes.
struct Bounds {
  double MinX = 0.0;
  double MinZ = 0.0;
  double MaxX = 0.0;
  double MaxZ = 0.0;

  [[nodiscard]] bool overlaps(const Bounds &other) const {
    return MinX <= other.MaxX && other.MinX <= MaxX && MinZ <= other.MaxZ && other.MinZ <= MaxZ;
  }
};

// A piece of paving and the rectangle that bounds it.
struct NearPiece {
  const SurfacePiece *Piece = nullptr;
  Bounds Around;
};

// A place a box might front: where it lies, the outward direction there, its distance from the
// pointer, and the order it was found in.
struct Candidate {
  ParkPoint At;
  ParkPoint Out;
  double Distance = 0.0;
  size_t Order = 0;
  size_t Step = 0;
};

// The steps of the surface's edge lines of the kind, in the surface's order, leaving out each
// shorter than MIN_SEGMENT_LENGTH, into a list it clears first.
void edgeSteps(const PavedSurface &surface, PathKind kind, std::vector<EdgeStep> &steps);

// The lists the frontage search reads and fills: each kind's edge steps, filled with the park, and,
// kept from call to call, the pieces near the box and the places it tries.
struct FrontageLists {
  std::vector<EdgeStep> GuestSteps;
  std::vector<EdgeStep> BackstageSteps;
  std::vector<NearPiece> Near;
  std::vector<Candidate> Candidates;
};

} // namespace tpj

#endif
