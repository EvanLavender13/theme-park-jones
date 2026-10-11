#ifndef TPJ_SIM_PARK_SURFACE_H
#define TPJ_SIM_PARK_SURFACE_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <array>
#include <optional>
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

// Where one side of a segment's body runs, seen walking from its From point to its To point: its
// segment, whether it is the left side, and where it starts, nearer the From point, and ends.
struct BodySide {
  EntityKey Segment = NULL_KEY;
  bool Left = false;
  ParkPoint Start;
  ParkPoint End;

  bool operator==(const BodySide &) const = default;
};

// The paving the park's segments make: its pieces and its edge lines, each list in ascending key
// order of its owner, the positions the edge lines run through, and the sides of each segment's
// body, in ascending key order of the segment, the left side first.
struct PavedSurface {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
  std::vector<BodySide> Sides;
};

// The piece's corners.
std::span<const ParkPoint> pieceCorners(const SurfacePiece &piece);
// The positions the edge line of the surface runs through.
std::span<const ParkPoint> edgePoints(const PavedSurface &surface, const EdgeLine &edge);
// How far the point lies from the piece, in meters: 0 when it lies on the piece.
double pieceDistance(const SurfacePiece &piece, ParkPoint point);

// The park's paved surface. A segment with an empty ground line adds nothing.
PavedSurface parkSurface(const World &world);

// A strip of paving from a door to a segment, joined to it: its kind, the segment it joins, the
// door, the unit direction out of the door's face, where it ends inside the segment's paving, and
// its width.
struct Walkway {
  PathKind Kind = PathKind::Guest;
  EntityKey Joins = NULL_KEY;
  ParkPoint Door;
  ParkPoint Out;
  ParkPoint Meets;
  double Width = 0.0;

  bool operator==(const Walkway &) const = default;
};

// How far a walkway's far end lies past the place it meets its segment, in meters.
inline constexpr double WALKWAY_OVERLAP = 0.01;

// The piece parkSurface adds for the walkway, owned by the segment it joins, given the park's
// segments as parkSegments gives them, or none when it adds nothing for it.
std::optional<SurfacePiece> walkwayPiece(std::span<const ParkSegment> segments,
                                         const Walkway &walkway);

// The park's paved surface with the walkways added, each joined to its segment.
PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways);

// An edge of the paving at a path point, where its outline turns from one segment's left side, seen
// walking away from the point, to the right side of the next segment counter-clockwise: the point,
// that segment, the unit direction along each of the two away from the point, that segment's
// first, and whether the next lies less than half a turn around. Radius is the radius it is rounded
// with, between the smallest and largest that fit. Middle is its curve's middle, Sharp is where the
// two sides meet, and Toward is the unit direction from Sharp in which Middle moves as the radius
// grows.
struct ParkEdge {
  EntityKey Point = NULL_KEY;
  EntityKey Segment = NULL_KEY;
  std::array<ParkPoint, 2> Along{};
  bool Inner = false;
  double Radius = 0.0;
  double MinRadius = 0.0;
  double MaxRadius = 0.0;
  ParkPoint Middle;
  ParkPoint Sharp;
  ParkPoint Toward;

  bool operator==(const ParkEdge &) const = default;
};

// Each edge with a radius, other than a corner's inside edge, in ascending key order of its point,
// then of its segment.
std::vector<ParkEdge> parkEdges(const World &world);
// The radius whose curve's middle lies the distance from the edge's Sharp along its Toward. 0 for a
// distance of 0 or less.
double edgeRadiusFor(const ParkEdge &edge, double distance);

// Where exactly two segments with ground lines and different widths meet at a path point, and the
// outline curves from one's side to the other's instead of stepping: the point's key, the place on
// the narrower segment's line where its sides would stop without curves, the narrower segment's
// key, the unit direction along it away from the point, the length of its longest curve, and the
// longest any of its curves can be.
struct ParkTaper {
  EntityKey Point = NULL_KEY;
  ParkPoint At;
  EntityKey Segment = NULL_KEY;
  ParkPoint Along;
  double Length = 0.0;
  double MaxLength = 0.0;

  bool operator==(const ParkTaper &) const = default;
};

// Each taper, in ascending key order of its point.
std::vector<ParkTaper> parkTapers(const World &world);

} // namespace tpj

#endif
