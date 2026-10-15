#ifndef TPJ_SIM_PARK_INTERNAL_PAVING_H
#define TPJ_SIM_PARK_INTERNAL_PAVING_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/layout.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <stddef.h>
#include <tuple>
#include <utility>
#include <vector>

namespace tpj {

// Outline points nearer together than this, in meters, are one point.
inline constexpr double SAME_POINT = 1e-9;

// A segment end's side, seen walking away from its point.
enum Side : size_t { LEFT = 0, RIGHT = 1 };

inline ParkPoint operator+(ParkPoint a, ParkPoint b) { return {a.X + b.X, a.Z + b.Z}; }
inline ParkPoint operator-(ParkPoint a, ParkPoint b) { return {a.X - b.X, a.Z - b.Z}; }
inline ParkPoint operator*(ParkPoint a, double scale) { return {a.X * scale, a.Z * scale}; }

inline double dot(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }
// Positive when b lies counter-clockwise of a, seen from above.
inline double ccw(ParkPoint a, ParkPoint b) { return a.Z * b.X - a.X * b.Z; }
inline double lengthOf(ParkPoint a) { return std::sqrt(dot(a, a)); }
inline ParkPoint unit(ParkPoint a) { return a * (1.0 / lengthOf(a)); }
// The direction a quarter turn counter-clockwise of the given one, seen from above: its left.
inline ParkPoint leftOf(ParkPoint a) { return {a.Z, -a.X}; }
inline ParkPoint rightOf(ParkPoint a) { return {-a.Z, a.X}; }

inline bool samePoint(ParkPoint a, ParkPoint b) { return lengthOf(b - a) < SAME_POINT; }

// How far an arc of the circle bulges from its chord of the length.
inline double sagittaOf(double radius, double chord) {
  const double half = chord / 2.0;
  return radius - std::sqrt(std::max(radius * radius - half * half, 0.0));
}

// A segment with a ground line: where it runs, how wide it is, and, at each end and on each side,
// how far from that end's point its body stops.
struct Shape {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  std::array<EntityKey, 2> Points{};
  std::array<ParkPoint, 2> Ends{};
  ParkPoint Along;
  double Half = 0.0;
  double Length = 0.0;
  std::array<std::array<double, 2>, 2> Trim{};
  // At each end and on each side, how far from that end's point the edge there would touch it if
  // its reach were the whole length.
  std::array<std::array<double, 2>, 2> Asked{};
  // The edge radius it holds at each end.
  std::array<double, 2> Edge{};

  // The unit direction away from the end's point.
  ParkPoint outward(size_t end) const { return end == 0 ? Along : Along * -1.0; }
  // The point on the side's edge the distance from the end's point.
  ParkPoint edge(size_t end, Side side, double distance) const {
    const ParkPoint across = leftOf(outward(end)) * (side == LEFT ? Half : -Half);
    return Ends[end] + outward(end) * distance + across;
  }
  ParkPoint trimmed(size_t end, Side side) const { return edge(end, side, Trim[end][side]); }
  // How far from the end's point its trim on the side may reach: the length less what the other end
  // asks for on the same side, which is that end's other side, or half the length if that is more.
  double reach(size_t end, Side side) const {
    return std::max(Length - Asked[1 - end][side == LEFT ? RIGHT : LEFT], Length / 2.0);
  }
  // The point of the side's edge halfway between where it stops at either end.
  ParkPoint middle(size_t end, Side side) const {
    const double far = Length - Trim[1 - end][side == LEFT ? RIGHT : LEFT];
    return edge(end, side, (Trim[end][side] + far) / 2.0);
  }
};

// A segment end at a point: the shape's index and which end.
struct End {
  size_t Shape = 0;
  size_t Which = 0;
};

// How the outline turns from one segment at a point to the next one counter-clockwise: with a
// fillet, around the point, or straight across it.
enum class Turn { Fillet, Around, Step };

struct Pair {
  Turn Kind = Turn::Step;
  // For a fillet: its radius, how far from the point it touches each segment, whether it is an
  // inner edge, and the least and greatest radii that fit.
  double Radius = 0.0;
  double TouchFirst = 0.0;
  double TouchSecond = 0.0;
  bool Inner = false;
  double Least = 0.0;
  double Most = 0.0;
};

// Whether a point is a taper: exactly two segments of different widths use it, and on at least one
// side the outline would step from one segment's side to the other's. If so, Narrow is the index in
// the point's group of the narrower segment's end. For each pair, Curves says whether its step is
// replaced by a curve, which leaves the narrower segment's side Lengths[pair] farther from the
// point than its side would stop, and runs to where the wider segment's side begins. Start is how
// far from the point the narrower segment's sides would stop, on the sides that curve, at the
// farther of them. Length is the longest curve, and MaxLength the longest any curve can be.
struct TaperFit {
  bool Tapers = false;
  size_t Narrow = 0;
  std::array<bool, 2> Curves{};
  std::array<double, 2> Lengths{};
  double Start = 0.0;
  double Length = 0.0;
  double MaxLength = 0.0;
};

// A segment end's half of a turn: the run of positions in a list where it lies, where it starts
// and how many it holds, and the side whose half it meets. A Count of 0 is no turn.
struct TurnSpan {
  size_t First = 0;
  size_t Count = 0;
  SideLink Meets;
};

// Lists reused from one point to the next, so building a surface allocates only while they grow,
// and the point's taper, after trimPoint. Turns holds, for each shape, at each end and on each
// side, its half of the outline's turn there, in TurnPoints, starting from its trimmed corner. Only
// paveAt sizes Turns.
struct Scratch {
  std::vector<End> Group;
  std::vector<Pair> Pairs;
  std::vector<ParkPoint> Outline;
  std::vector<ParkPoint> Fan;
  TaperFit Taper;
  std::vector<ParkPoint> TurnPoints;
  std::vector<std::array<std::array<TurnSpan, 2>, 2>> Turns;
};

// A rectangle on the ground, its sides along the axes.
struct Bounds {
  double MinX = 0.0;
  double MinZ = 0.0;
  double MaxX = 0.0;
  double MaxZ = 0.0;

  static Bounds around(ParkPoint at) { return {at.X, at.Z, at.X, at.Z}; }
  void add(ParkPoint at) {
    MinX = std::min(MinX, at.X);
    MinZ = std::min(MinZ, at.Z);
    MaxX = std::max(MaxX, at.X);
    MaxZ = std::max(MaxZ, at.Z);
  }
  void add(const Bounds &other) {
    add(ParkPoint{other.MinX, other.MinZ});
    add(ParkPoint{other.MaxX, other.MaxZ});
  }
  Bounds grown(double by) const { return {MinX - by, MinZ - by, MaxX + by, MaxZ + by}; }
  bool overlaps(const Bounds &other) const {
    return MinX <= other.MaxX && other.MinX <= MaxX && MinZ <= other.MaxZ && other.MinZ <= MaxZ;
  }
};

// The rectangle that holds the corners of the piece.
Bounds boundsOf(const SurfacePiece &piece);
// The rectangle that holds the positions, of which there is at least one.
Bounds boundsOf(std::span<const ParkPoint> points);

// A segment as the paving at its ends reads it: its key, kind, points, width, and the edge radius
// it holds at each end.
struct PavingSegment {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  double FromEdge = 0.0;
  double ToEdge = 0.0;
};

// The taper length the point holds, or 0 when that is not finite or is below zero.
double heldTaper(const PathPointIntent &point);

// The paving reading of a segment with a ground line, as the ground gives it.
PavingSegment pavingSegment(const ParkSegment &segment);

// A segment end at a point: its segment, the positions of its From and To points, which end is at
// the point, 0 for its From end, and at its far end the asks and the trims, each left and right as
// seen walking away from that end's point.
struct PavingEnd {
  PavingSegment Segment;
  ParkPoint FromAt;
  ParkPoint ToAt;
  size_t Which = 0;
  std::array<double, 2> FarAsks{};
  std::array<double, 2> FarTrims{};
};

// What one point's paving reads: its key, position, and taper length, at least 0; its corner, if
// it has one; and its ends, ascending by segment key and then by end, only of segments whose
// ground line isn't empty.
struct PointInput {
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  double Taper = 0.0;
  bool HasCorner = false;
  ParkCorner Corner;
  std::vector<PavingEnd> Ends;
};

// A value on each side of a segment end at a point, left and right as seen walking away from the
// point.
struct EndSides {
  EntityKey Segment = NULL_KEY;
  size_t Which = 0;
  double Left = 0.0;
  double Right = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, EndSides &sides) {
  visitor.field("segment", sides.Segment);
  visitor.field("which", sides.Which);
  visitor.field("left", sides.Left);
  visitor.field("right", sides.Right);
}

// A value on each side of each end at a point, in the order of the point's input ends.
struct PointSides {
  std::vector<EndSides> Ends;
};

template <typename Visitor> void visitFields(Visitor &visitor, PointSides &sides) {
  visitor.field("ends", sides.Ends);
}

// What each end at a point asks for, and how far it is trimmed. Zeros when the point has one end.
using PointAsks = PointSides;
using PointTrims = PointSides;

// A segment end's half of the turn at a point: its segment, which end, which side, the side it
// meets, and the Count positions of the point's TurnPoints from First that it runs through,
// starting from the end's trimmed corner.
struct PointTurn {
  EntityKey Segment = NULL_KEY;
  size_t Which = 0;
  size_t On = 0;
  SideLink Meets;
  size_t First = 0;
  size_t Count = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, PointTurn &turn) {
  visitor.field("segment", turn.Segment);
  visitor.field("which", turn.Which);
  visitor.field("on", turn.On);
  visitor.field("meets", turn.Meets);
  visitor.field("first", turn.First);
  visitor.field("count", turn.Count);
}

// One point's paving: the pieces it owns and the corner pieces it gives its segments, in the order
// they are made, its edge lines before hiding and their positions, and its ends' turn halves and
// their positions.
struct PointPaving {
  std::vector<SurfacePiece> Pieces;
  std::vector<EdgeLine> Edges;
  std::vector<ParkPoint> EdgePoints;
  std::vector<PointTurn> Turns;
  std::vector<ParkPoint> TurnPoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, PointPaving &paving) {
  visitor.field("pieces", paving.Pieces);
  visitor.field("edges", paving.Edges);
  visitor.field("edge-points", paving.EdgePoints);
  visitor.field("turns", paving.Turns);
  visitor.field("turn-points", paving.TurnPoints);
}

// What one segment's paving reads: the segment, its points' positions, its trims at each end and
// on each side, its turn halves at each end and on each side, in TurnPoints, and the corner pieces
// its points give it, the lower-keyed point's first.
struct SegmentInput {
  PavingSegment Segment;
  ParkPoint FromAt;
  ParkPoint ToAt;
  std::array<std::array<double, 2>, 2> Trims{};
  std::array<std::array<TurnSpan, 2>, 2> Turns{};
  std::vector<ParkPoint> TurnPoints;
  std::vector<SurfacePiece> CornerPieces;
};

// One segment's paving: its From point, its pieces, the index of its body among them when it has
// one, its sides, and the positions they run through.
struct SegmentPaving {
  EntityKey From = NULL_KEY;
  std::vector<SurfacePiece> Pieces;
  bool HasBody = false;
  size_t Body = 0;
  std::vector<SegmentSide> Sides;
  std::vector<ParkPoint> SidePoints;
};

template <typename Visitor> void visitFields(Visitor &visitor, SegmentPaving &paving) {
  visitor.field("from", paving.From);
  visitor.field("pieces", paving.Pieces);
  visitor.field("has-body", paving.HasBody);
  visitor.field("body", paving.Body);
  visitor.field("sides", paving.Sides);
  visitor.field("side-points", paving.SidePoints);
}

// The values of the segment's end in the sides, or zeros when they hold none for it.
std::array<double, 2> sidesOf(const PointSides &sides, EntityKey segment, size_t which);

// Fills the segment's input from the trims and pavings of its From and To points, each null when
// that point's isn't known, and their positions.
void segmentInputOf(const PavingSegment &segment, ParkPoint fromAt, ParkPoint toAt,
                    std::array<const PointTrims *, 2> trims,
                    std::array<const PointPaving *, 2> pavings, SegmentInput &input);

// A piece that can hide edge lines, and its bounds.
struct Hider {
  Bounds Box;
  const SurfacePiece *Piece = nullptr;
};

// The lists the per-point and per-segment paving reuse, cleared before each use.
struct PavingScratch {
  Scratch Work;
  std::vector<Shape> Shapes;
  std::vector<size_t> Near;
  std::vector<std::pair<double, double>> Hidden;
};

// What each end at the point asks for: how far from the point the edges on its sides would touch it
// if its trims could reach its whole length.
[[gnu::noinline]] void askAt(const PointInput &input, PavingScratch &scratch, PointAsks &out);
// How far each end at the point is trimmed, given what its far end asks for.
[[gnu::noinline]] void fitAt(const PointInput &input, PavingScratch &scratch, PointTrims &out);
// The point's paving, given what each end's far end asks for and how far it is trimmed there.
[[gnu::noinline]] void paveAt(const PointInput &input, PavingScratch &scratch, PointPaving &out);
// The segment's paving.
[[gnu::noinline]] void paveSegment(const SegmentInput &input, PavingScratch &scratch,
                                   SegmentPaving &out);
// Appends to the out lists the parts of the edge lines, whose First counts from the start of
// points, that no piece of the hiders hides, each as an edge line of its own. The caller gives only
// hiders joined to the edge lines.
[[gnu::noinline]] void hideOwner(std::span<const EdgeLine> edges, std::span<const ParkPoint> points,
                                 std::span<const Hider> hiders, PavingScratch &scratch,
                                 std::vector<EdgeLine> &outEdges,
                                 std::vector<ParkPoint> &outPoints);

// Refills surface with the paved surface of the layout with its ground, with the walkways added,
// keeping the lists it works with in the scratch.
void walkedSurfaceOf(const PathLayout &layout, const PathGround &ground,
                     std::span<const Walkway> walkways, PavedSurface &surface,
                     TickBuffers &scratch);

// The lists fitting edges and tapers reuses: the shapes, their ends, the fitting's scratch, the
// points whose asks are found, the taper candidates, and the segment ends they are found from.
struct EdgeLists {
  std::vector<Shape> Shapes;
  std::vector<std::tuple<EntityKey, EntityKey, End>> Ends;
  Scratch Fit;
  std::vector<EntityKey> Asked;
  std::vector<EntityKey> Candidates;
  std::vector<std::pair<EntityKey, double>> CandidateEnds;
};

// Each edge the layout's segments make, as parkEdges gives them, given its ground, in found.
void fitEdges(const PathLayout &layout, const PathGround &ground, EdgeLists &lists,
              std::vector<ParkEdge> &found);
// The points, in ascending key order, that exactly two segments of different widths use, as their
// intent gives them, in candidates. Every taper is one of them. ends is a list it reuses.
void taperCandidates(const World &world, std::vector<std::pair<EntityKey, double>> &ends,
                     std::vector<EntityKey> &candidates);
// Each taper among the candidates, which are in ascending key order, in found. Only the candidates
// and the far ends of their segments are fitted.
void fitTapers(const PathLayout &layout, const PathGround &ground,
               const std::vector<EntityKey> &candidates, EdgeLists &lists,
               std::vector<ParkTaper> &found);

} // namespace tpj

#endif
