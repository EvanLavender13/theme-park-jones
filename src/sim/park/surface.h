#ifndef TPJ_SIM_PARK_SURFACE_H
#define TPJ_SIM_PARK_SURFACE_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/tick_buffers.h"
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

template <typename Visitor> void visitFields(Visitor &visitor, SurfacePiece &piece) {
  visitor.field("kind", piece.Kind);
  visitor.field("owner", piece.Owner);
  visitor.field("corner-0", piece.Corners[0]);
  visitor.field("corner-1", piece.Corners[1]);
  visitor.field("corner-2", piece.Corners[2]);
  visitor.field("corner-3", piece.Corners[3]);
  visitor.field("count", piece.Count);
}

// A run of positions along the paving's outline, with the paving on its left seen from above: the
// Count positions of its surface's EdgePoints from First. Owner is the point it runs around.
struct EdgeLine {
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;
  size_t First = 0;
  size_t Count = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, EdgeLine &edge) {
  visitor.field("kind", edge.Kind);
  visitor.field("owner", edge.Owner);
  visitor.field("first", edge.First);
  visitor.field("count", edge.Count);
}

// The side of a segment that another side's line runs on into: that segment, whether it is its
// left side, seen walking from its From point to its To point, and whether the line meets it at
// its From end. A Segment of NULL_KEY is none.
struct SideLink {
  EntityKey Segment = NULL_KEY;
  bool Left = false;
  bool AtFrom = false;

  bool operator==(const SideLink &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, SideLink &link) {
  visitor.field("segment", link.Segment);
  visitor.field("left", link.Left);
  visitor.field("at-from", link.AtFrom);
}

// One side of a segment, seen walking from its From point to its To point: its segment, whether it
// is the left side, the Count positions of its surface's SidePoints from First that it runs
// through, from its From end to its To end, the index among them of where its body starts, and,
// at its From end and at its To end, the side it runs on into there, or none where it stops.
struct SegmentSide {
  EntityKey Segment = NULL_KEY;
  bool Left = false;
  size_t First = 0;
  size_t Count = 0;
  size_t Body = 0;
  std::array<SideLink, 2> RunsOn{};

  bool operator==(const SegmentSide &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, SegmentSide &side) {
  visitor.field("segment", side.Segment);
  visitor.field("left", side.Left);
  visitor.field("first", side.First);
  visitor.field("count", side.Count);
  visitor.field("body", side.Body);
  visitor.field("runs-on-from", side.RunsOn[0]);
  visitor.field("runs-on-to", side.RunsOn[1]);
}

// The paving the park's segments make: its pieces and its edge lines, each list in ascending key
// order of its owner, the positions the edge lines run through, the sides of each segment, in
// ascending key order of the segment, the left side first, and the positions the sides run
// through.
struct PavedSurface {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
  std::vector<SegmentSide> Sides;
  std::vector<ParkPoint> SidePoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, PavedSurface &surface) {
  visitor.field("pieces", surface.Pieces);
  visitor.field("edges", surface.Edges);
  visitor.field("edge-points", surface.EdgePoints);
  visitor.field("sides", surface.Sides);
  visitor.field("side-points", surface.SidePoints);
}

// The piece's corners.
std::span<const ParkPoint> pieceCorners(const SurfacePiece &piece);
// The positions the edge line of the surface runs through.
std::span<const ParkPoint> edgePoints(const PavedSurface &surface, const EdgeLine &edge);
// The positions the side of the surface runs through.
std::span<const ParkPoint> sidePoints(const PavedSurface &surface, const SegmentSide &side);
// How far the point lies from the piece, in meters: 0 when it lies on the piece.
double pieceDistance(const SurfacePiece &piece, ParkPoint point);

// The park's paved surface. A segment with an empty ground line adds nothing.
PavedSurface parkSurface(const World &world);

// The park's paved surface as the last resolution derived it with parkSurface, or an empty surface
// when the world holds none, as before its first resolution. Read it outside resolution, and in
// resolvers that depend on paved-surface; read parkSurface where intent may have changed since.
const PavedSurface &resolvedSurface(const World &world);

// Registers the derived surface and the paved-surface resolver that keeps it for resolvedSurface.
void addPavedSurface(WorldSchema &schema);

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
// As the returning forms of parkEdges and parkTapers, into a list the caller keeps, which they
// clear first, keeping the lists they work with in the buffers. Allocate nothing once the lists
// have grown.
void parkEdges(const World &world, std::vector<ParkEdge> &edges, TickBuffers &buffers);
void parkTapers(const World &world, std::vector<ParkTaper> &tapers, TickBuffers &buffers);

} // namespace tpj

#endif
