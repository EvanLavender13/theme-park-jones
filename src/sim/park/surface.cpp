#include "sim/park/surface.h"

#include "sim/mix.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/around.h"
#include "sim/park/internal/layout.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <stddef.h>
#include <tuple>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// A turn is halved into equal pieces at most this many times.
constexpr int MAX_ARC_LEVELS = 6;
// Outline points nearer together than this, in meters, are one point.
constexpr double SAME_POINT = 1e-9;
// Pieces with less area than this, in square meters, are left out.
constexpr double LEAST_AREA = 1e-12;
// Directions whose cross product is within this of zero run along one line.
constexpr double PARALLEL = 1e-9;
// Where a taper's curve has no taper length to take, it runs this many times as far along the
// narrower segment as it moves out.
constexpr double TAPER_SLOPE = 2.0;
// A taper's curve is cut into at most this many pieces.
constexpr size_t MAX_CURVE_PIECES = 64;

// A segment end's side, seen walking away from its point.
enum Side : size_t { LEFT = 0, RIGHT = 1 };

ParkPoint operator+(ParkPoint a, ParkPoint b) { return {a.X + b.X, a.Z + b.Z}; }
ParkPoint operator-(ParkPoint a, ParkPoint b) { return {a.X - b.X, a.Z - b.Z}; }
ParkPoint operator*(ParkPoint a, double scale) { return {a.X * scale, a.Z * scale}; }

double dot(ParkPoint a, ParkPoint b) { return a.X * b.X + a.Z * b.Z; }
// Positive when b lies counter-clockwise of a, seen from above.
double ccw(ParkPoint a, ParkPoint b) { return a.Z * b.X - a.X * b.Z; }
double lengthOf(ParkPoint a) { return std::sqrt(dot(a, a)); }
ParkPoint unit(ParkPoint a) { return a * (1.0 / lengthOf(a)); }
// The direction a quarter turn counter-clockwise of the given one, seen from above: its left.
ParkPoint leftOf(ParkPoint a) { return {a.Z, -a.X}; }
ParkPoint rightOf(ParkPoint a) { return {-a.Z, a.X}; }

bool samePoint(ParkPoint a, ParkPoint b) { return lengthOf(b - a) < SAME_POINT; }

// How far an arc of the circle bulges from its chord of the length.
double sagittaOf(double radius, double chord) {
  const double half = chord / 2.0;
  return radius - std::sqrt(std::max(radius * radius - half * half, 0.0));
}

// Which way a turn runs, seen from above.
enum class Sense { Clockwise, CounterClockwise };

// The unit direction halfway along the turn from one unit direction to another.
ParkPoint halfway(ParkPoint from, ParkPoint to, Sense sense) {
  if (dot(from, to) > 0.0) {
    return unit(from + to);
  }
  // The chord from the first to the second lies a quarter turn past the halfway direction.
  return unit(sense == Sense::CounterClockwise ? rightOf(to - from) : leftOf(to - from));
}

// Appends the points of the curve around the center from one unit direction to another, both
// included, its distance from the center changing evenly from one radius to the other. The turn
// is halved into equal pieces until each lies within ARC_TOLERANCE of a circle of the larger
// radius, at most MAX_ARC_LEVELS times. The directions are found in place, then scaled.
void appendTurn(std::vector<ParkPoint> &out, ParkPoint center, ParkPoint from, ParkPoint to,
                Sense sense, double fromRadius, double toRadius) {
  const double radius = std::max(fromRadius, toRadius);
  size_t pieces = 1;
  double chord = lengthOf(to - from);
  if (dot(from, to) <= 0.0) {
    // The chord of half a turn of a quarter or more.
    chord = lengthOf(halfway(from, to, sense) - from);
    pieces = 2;
  }
  for (int level = 0; level < MAX_ARC_LEVELS && sagittaOf(radius, radius * chord) > ARC_TOLERANCE;
       ++level) {
    chord = std::sqrt(2.0 * sagittaOf(1.0, chord));
    pieces *= 2;
  }
  const size_t first = out.size();
  out.resize(first + pieces + 1);
  out[first] = from;
  out[first + pieces] = to;
  for (size_t step = pieces; step > 1; step /= 2) {
    for (size_t i = first; i < first + pieces; i += step) {
      out[i + step / 2] = halfway(out[i], out[i + step], sense);
    }
  }
  for (size_t i = 0; i <= pieces; ++i) {
    const double fraction = static_cast<double>(i) / static_cast<double>(pieces);
    out[first + i] = center + out[first + i] * (fromRadius + (toRadius - fromRadius) * fraction);
  }
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
// buildSurface sizes Turns.
struct Scratch {
  std::vector<End> Group;
  std::vector<Pair> Pairs;
  std::vector<ParkPoint> Outline;
  std::vector<ParkPoint> Fan;
  TaperFit Taper;
  std::vector<ParkPoint> TurnPoints;
  std::vector<std::array<std::array<TurnSpan, 2>, 2>> Turns;
};

// Writes the pieces and edge lines of the owners it wants into a surface.
class Output {
public:
  // wanted is in ascending key order. If all is true, every owner is wanted. If edges is false,
  // edge lines are left out.
  Output(PavedSurface &surface, const std::vector<EntityKey> &wanted, bool all, bool edges)
      : Surface(surface), Wanted(wanted), All(all), Edges(edges) {}

  bool wants(EntityKey owner) const { return All || std::ranges::binary_search(Wanted, owner); }

  // Adds the polygon of at most four corners as a piece, its corners made counter-clockwise,
  // unless it has no area.
  void piece(PathKind kind, EntityKey owner, std::initializer_list<ParkPoint> corners) {
    if (!wants(owner)) {
      return;
    }
    SurfacePiece added{kind, owner, {}, 0};
    for (const ParkPoint &corner : corners) {
      if (added.Count == 0 || !samePoint(added.Corners[added.Count - 1], corner)) {
        added.Corners[added.Count++] = corner;
      }
    }
    while (added.Count > 1 && samePoint(added.Corners[0], added.Corners[added.Count - 1])) {
      --added.Count;
    }
    double area = 0.0;
    for (size_t i = 0; i < added.Count; ++i) {
      area += ccw(added.Corners[i], added.Corners[(i + 1) % added.Count]);
    }
    if (std::abs(area) / 2.0 < LEAST_AREA) {
      return;
    }
    if (area < 0.0) {
      std::reverse(added.Corners.begin(), added.Corners.begin() + added.Count);
    }
    Surface.Pieces.push_back(added);
  }

  // Adds one side of the shape, the left or the right seen from its From end: its turn at the From
  // end, reversed, which ends where its body starts, and then its turn at the To end, or the
  // trimmed corner alone at an end with no turn on that side. Positions that repeat the one before
  // are left out. The side is left out when its body ends behind where it starts, or when fewer
  // than two positions remain.
  void side(const Shape &shape, bool left, const std::array<std::array<TurnSpan, 2>, 2> &turns,
            const std::vector<ParkPoint> &turnPoints) {
    if (!wants(shape.Key)) {
      return;
    }
    // Seen from the From end, the left side ends at the To end's right, and the right at its left.
    const Side fromSide = left ? LEFT : RIGHT;
    const Side toSide = left ? RIGHT : LEFT;
    const ParkPoint start = shape.trimmed(0, fromSide);
    const ParkPoint end = shape.trimmed(1, toSide);
    if (dot(end - start, shape.Along) < -SAME_POINT) {
      return;
    }
    const size_t first = Surface.SidePoints.size();
    const auto add = [this, first](ParkPoint at) {
      if (Surface.SidePoints.size() == first || !samePoint(Surface.SidePoints.back(), at)) {
        Surface.SidePoints.push_back(at);
      }
    };
    const TurnSpan &before = turns[0][fromSide];
    const TurnSpan &after = turns[1][toSide];
    for (size_t i = before.Count; i > 0; --i) {
      add(turnPoints[before.First + i - 1]);
    }
    if (before.Count == 0) {
      add(start);
    }
    const size_t body = Surface.SidePoints.size() - 1 - first;
    for (size_t i = 0; i < after.Count; ++i) {
      add(turnPoints[after.First + i]);
    }
    if (after.Count == 0) {
      add(end);
    }
    const size_t count = Surface.SidePoints.size() - first;
    if (count < 2) {
      Surface.SidePoints.resize(first);
      return;
    }
    Surface.Sides.push_back(
        SegmentSide{shape.Key, left, first, count, body, {before.Meets, after.Meets}});
  }

  // Adds the run of points as an edge line, leaving out each point that repeats the one before.
  template <typename Points> void edge(PathKind kind, EntityKey owner, const Points &points) {
    if (!Edges || !wants(owner)) {
      return;
    }
    const size_t first = Surface.EdgePoints.size();
    for (const ParkPoint &point : points) {
      if (Surface.EdgePoints.size() == first || !samePoint(Surface.EdgePoints.back(), point)) {
        Surface.EdgePoints.push_back(point);
      }
    }
    const size_t count = Surface.EdgePoints.size() - first;
    if (count >= 2) {
      Surface.Edges.push_back(EdgeLine{kind, owner, first, count});
    } else {
      Surface.EdgePoints.resize(first);
    }
  }

private:
  PavedSurface &Surface;
  const std::vector<EntityKey> &Wanted;
  bool All;
  bool Edges;
};

// How far the way from a through b to c turns left, seen from above, scaled by the two steps.
double turnAt(ParkPoint a, ParkPoint b, ParkPoint c) { return ccw(b - a, c - b); }

// Leaves out each point of the polygon that repeats the one before or lies on the line between its
// neighbours, and makes the rest run counter-clockwise.
void simplify(std::vector<ParkPoint> &polygon) {
  for (size_t i = 0; polygon.size() >= 3 && i < polygon.size();) {
    const size_t n = polygon.size();
    const ParkPoint previous = polygon[(i + n - 1) % n];
    if (samePoint(previous, polygon[i]) ||
        std::abs(turnAt(previous, polygon[i], polygon[(i + 1) % n])) <= LEAST_AREA) {
      polygon.erase(polygon.begin() + static_cast<std::ptrdiff_t>(i));
      i = 0;
    } else {
      ++i;
    }
  }
  double area = 0.0;
  for (size_t i = 0; i < polygon.size(); ++i) {
    area += ccw(polygon[i], polygon[(i + 1) % polygon.size()]);
  }
  if (area < 0.0) {
    std::ranges::reverse(polygon);
  }
}

// The index of a corner of the counter-clockwise polygon that turns left and whose triangle with
// its neighbours holds no other corner, or the polygon's size when there is none.
size_t earOf(const std::vector<ParkPoint> &polygon) {
  const size_t n = polygon.size();
  const auto inside = [](ParkPoint p, ParkPoint a, ParkPoint b, ParkPoint c) {
    return ccw(b - a, p - a) > 0.0 && ccw(c - b, p - b) > 0.0 && ccw(a - c, p - c) > 0.0;
  };
  for (size_t i = 0; i < n; ++i) {
    const ParkPoint a = polygon[(i + n - 1) % n];
    const ParkPoint b = polygon[i];
    const ParkPoint c = polygon[(i + 1) % n];
    if (turnAt(a, b, c) <= 0.0) {
      continue;
    }
    bool empty = true;
    for (size_t j = (i + 2) % n; j != (i + n - 1) % n && empty; j = (j + 1) % n) {
      empty = !inside(polygon[j], a, b, c);
    }
    if (empty) {
      return i;
    }
  }
  return n;
}

// Adds a simple polygon as triangles, by ear clipping, each owned as ownerOf gives for its
// centroid. The polygon is used up.
template <typename OwnerOf>
void triangulate(Output &output, PathKind kind, std::vector<ParkPoint> &polygon, OwnerOf ownerOf) {
  simplify(polygon);
  while (polygon.size() >= 3) {
    const size_t n = polygon.size();
    const size_t ear = earOf(polygon);
    // A polygon that rounding has made not quite simple is closed off as a fan.
    if (ear == n) {
      for (size_t i = 1; i + 1 < n; ++i) {
        const ParkPoint centroid = (polygon[0] + polygon[i] + polygon[i + 1]) * (1.0 / 3.0);
        output.piece(kind, ownerOf(centroid), {polygon[0], polygon[i], polygon[i + 1]});
      }
      return;
    }
    const ParkPoint a = polygon[(ear + n - 1) % n];
    const ParkPoint b = polygon[ear];
    const ParkPoint c = polygon[(ear + 1) % n];
    output.piece(kind, ownerOf((a + b + c) * (1.0 / 3.0)), {a, b, c});
    polygon.erase(polygon.begin() + static_cast<std::ptrdiff_t>(ear));
  }
}

// The square end of a segment at a point no other segment uses: its edge line across the end.
void buildFreeEnd(Output &output, EntityKey point, const Shape &shape, size_t which) {
  output.edge(shape.Kind, point,
              std::array{shape.middle(which, LEFT), shape.edge(which, LEFT, 0.0),
                         shape.edge(which, RIGHT, 0.0), shape.middle(which, RIGHT)});
}

// The corner's tangent length: how far from its point its arc touches each of its segments.
double tangentOf(const ParkCorner &corner) {
  const double cosine = dot(corner.Along[0], corner.Along[1]);
  return corner.Radius * std::abs(ccw(corner.Along[0], corner.Along[1])) / (1.0 - cosine);
}

// A segment's end at a point, as the edges there see it: its outward direction, its half width,
// how far from the point it starts, and how far its trims may reach on each side.
struct Arm {
  ParkPoint Along;
  double Half = 0.0;
  double Start = 0.0;
  std::array<double, 2> Reach{};
};

// How the outline turns from the first segment's left side to the right side of the second,
// counter-clockwise of it, rounded with the radius asked for, made to fit. full is true when the
// second lies a whole turn on, as when every segment at the point runs one way.
Pair pairOf(const Arm &a, const Arm &b, bool full, double asked) {
  const double sine = ccw(a.Along, b.Along);
  const double cosine = dot(a.Along, b.Along);
  if (std::abs(sine) <= PARALLEL) {
    return Pair{cosine < 0.0 || full ? Turn::Around : Turn::Step};
  }
  const bool inner = sine > 0.0;
  // An inner fillet of radius r has its center r outside both sides, and an outer one r inside
  // them. It touches the first segment (halfB + halfA cosine + grow r) / sine from the point, and
  // the second the same with the halves swapped. Both grow with r.
  const double grow = (inner ? 1.0 : -1.0) * (1.0 + cosine);
  const auto touchA = [&](double r) { return (b.Half + a.Half * cosine + grow * r) / sine; };
  const auto touchB = [&](double r) { return (a.Half + b.Half * cosine + grow * r) / sine; };
  const auto radiusA = [&](double d) { return (d * sine - b.Half - a.Half * cosine) / grow; };
  const auto radiusB = [&](double d) { return (d * sine - a.Half - b.Half * cosine) / grow; };
  double least = 0.0;
  double most = 0.0;
  if (inner) {
    least = std::max({0.0, radiusA(a.Start), radiusB(b.Start)});
    most = std::min(radiusA(a.Reach[LEFT]), radiusB(b.Reach[RIGHT]));
  } else {
    // A sharp outer edge reaches no farther behind the point than the wider width.
    const double behind = -2.0 * std::max(a.Half, b.Half);
    least = std::max({0.0, radiusA(behind), radiusB(behind)});
    // An outer edge touches each segment no farther than its start, and no farther than its reach,
    // so it never runs into the paving at the segment's other end.
    most = std::min(
        {radiusA(std::min(a.Start, a.Reach[LEFT])), radiusB(std::min(b.Start, b.Reach[RIGHT]))});
  }
  // An inner edge with too little room to touch past its start fits the room it has. A sharper
  // inner edge leaves more paving, so it still covers the line guests walk.
  if (inner && least > most && most >= 0.0) {
    least = most;
  }
  if (least > most) {
    return Pair{Turn::Step};
  }
  const double radius = std::clamp(asked, least, most);
  return Pair{Turn::Fillet, radius, touchA(radius), touchB(radius), inner, least, most};
}

// The radius the edge from the first arm's end asks for: at a corner's inside, the corner's radius
// less the narrower of the arms' half widths, and elsewhere the end's edge radius. Never below 0.
double askedOf(const Shape &a, size_t whichA, const Arm &armA, const Arm &armB,
               const ParkCorner *corner) {
  if (corner != nullptr && ccw(armA.Along, armB.Along) > 0.0) {
    return std::max(corner->Radius - std::min(armA.Half, armB.Half), 0.0);
  }
  const double held = a.Edge[whichA];
  return std::isfinite(held) && held > 0.0 ? held : 0.0;
}

// Finds how the outline turns from each of the point's segments, sorted counter-clockwise, to the
// next, and sets each segment's trims to where the edges on its two sides touch it.
void fitPoint(Scratch &scratch, std::vector<Shape> &shapes, const ParkCorner *corner, bool asking) {
  const std::vector<End> &ends = scratch.Group;
  const auto along = [&shapes](const End &end) { return shapes[end.Shape].outward(end.Which); };
  const size_t count = ends.size();
  const bool oneWay = std::ranges::all_of(ends, [&](const End &end) {
    return std::abs(ccw(along(ends[0]), along(end))) <= PARALLEL &&
           dot(along(ends[0]), along(end)) > 0.0;
  });
  const double start = corner != nullptr ? tangentOf(*corner) : 0.0;
  const auto armOf = [&](size_t index) {
    const End &end = ends[index];
    const Shape &shape = shapes[end.Shape];
    if (asking) {
      return Arm{shape.outward(end.Which), shape.Half, start, {shape.Length, shape.Length}};
    }
    return Arm{shape.outward(end.Which),
               shape.Half,
               start,
               {shape.reach(end.Which, LEFT), shape.reach(end.Which, RIGHT)}};
  };
  scratch.Pairs.clear();
  for (size_t i = 0; i < count; ++i) {
    const End &a = ends[i];
    const Arm armA = armOf(i);
    const Arm armB = armOf((i + 1) % count);
    const double asked = askedOf(shapes[a.Shape], a.Which, armA, armB, corner);
    scratch.Pairs.push_back(pairOf(armA, armB, oneWay && i + 1 == count, asked));
  }
  for (size_t i = 0; i < count; ++i) {
    const Pair &next = scratch.Pairs[i];
    const Pair &previous = scratch.Pairs[(i + count - 1) % count];
    std::array<double, 2> &trim = shapes[ends[i].Shape].Trim[ends[i].Which];
    trim[LEFT] = next.Kind == Turn::Fillet ? std::max(next.TouchFirst, 0.0) : 0.0;
    trim[RIGHT] = previous.Kind == Turn::Fillet ? std::max(previous.TouchSecond, 0.0) : 0.0;
    if (asking) {
      shapes[ends[i].Shape].Asked[ends[i].Which] = trim;
    }
  }
}

// Sorts a point's segments counter-clockwise, finds how the outline turns between each and the
// next, and sets each segment's trims to where the edges on its two sides touch it. corner is the
// point's corner, or null, and taper the taper length it holds, at least 0. Where the point is a
// taper, the narrower segment's trim on each side that curves lies the curve's length farther out.
// When asking, each trim may reach the whole length, and the trims are kept as what the end asks
// for.
void trimPoint(Scratch &scratch, std::vector<Shape> &shapes, const ParkCorner *corner, bool asking,
               double taper) {
  std::vector<End> &ends = scratch.Group;
  const auto along = [&shapes](const End &end) { return shapes[end.Shape].outward(end.Which); };
  std::ranges::sort(ends, [&along, &shapes](const End &x, const End &y) {
    if (comesBefore(along(x), along(y)) != comesBefore(along(y), along(x))) {
      return comesBefore(along(x), along(y));
    }
    return std::tie(shapes[x.Shape].Key, x.Which) < std::tie(shapes[y.Shape].Key, y.Which);
  });
  scratch.Taper = TaperFit{};
  fitPoint(scratch, shapes, corner, asking);
  if (ends.size() != 2 || shapes[ends[0].Shape].Half == shapes[ends[1].Shape].Half) {
    return;
  }
  TaperFit found;
  found.Narrow = shapes[ends[0].Shape].Half < shapes[ends[1].Shape].Half ? 0 : 1;
  Shape &narrow = shapes[ends[found.Narrow].Shape];
  const Shape &wide = shapes[ends[1 - found.Narrow].Shape];
  const size_t which = ends[found.Narrow].Which;
  const size_t wideWhich = ends[1 - found.Narrow].Which;
  for (size_t i = 0; i < 2; ++i) {
    // Each pair runs from its first end's left side to its second end's right side. A fillet joins
    // them; anything else steps.
    if (scratch.Pairs[i].Kind == Turn::Fillet) {
      continue;
    }
    const Side side = i == found.Narrow ? LEFT : RIGHT;
    const Side wideSide = side == LEFT ? RIGHT : LEFT;
    const double before = narrow.Trim[which][side];
    const double room = (asking ? narrow.Length : narrow.reach(which, side)) - before;
    // How far the wider segment's side, where it begins, lies out from the narrower one's.
    const ParkPoint across =
        side == LEFT ? leftOf(narrow.outward(which)) : rightOf(narrow.outward(which));
    const double out =
        std::abs(dot(wide.trimmed(wideWhich, wideSide) - narrow.Ends[which], across) - narrow.Half);
    const double length = std::max(std::min(taper > 0.0 ? taper : TAPER_SLOPE * out, room), 0.0);
    found.MaxLength = found.Tapers ? std::max(found.MaxLength, room) : room;
    found.Tapers = true;
    found.Curves[i] = true;
    found.Lengths[i] = length;
    found.Start = std::max(found.Start, before);
    found.Length = std::max(found.Length, length);
    narrow.Trim[which][side] = before + length;
    if (asking) {
      narrow.Asked[which][side] = narrow.Trim[which][side];
    }
  }
  found.MaxLength = std::max(found.MaxLength, 0.0);
  scratch.Taper = found;
}

// Appends the points of the cubic curve from one place to another, leaving the first along one
// unit direction and arriving at the second along another, each scaled by how far the second place
// lies beyond the first along it, so that the curve never doubles back: the places between, cut so
// that each piece lies within ARC_TOLERANCE of the curve. A cut into n pieces lies within
// M / (8n²) of it, where M is the largest second derivative, which is at one of its ends.
void appendCurve(std::vector<ParkPoint> &out, ParkPoint from, ParkPoint leaving, ParkPoint to,
                 ParkPoint arriving) {
  const ParkPoint chord = to - from;
  const ParkPoint m0 = leaving * std::max(dot(chord, leaving), 0.0);
  const ParkPoint m1 = arriving * std::max(dot(chord, arriving), 0.0);
  const double bend = std::max(lengthOf(chord * 6.0 - m0 * 4.0 - m1 * 2.0),
                               lengthOf(chord * -6.0 + m0 * 2.0 + m1 * 4.0));
  const double wanted = std::ceil(std::sqrt(bend / (8.0 * ARC_TOLERANCE)));
  const size_t pieces = std::clamp<size_t>(static_cast<size_t>(wanted), 1, MAX_CURVE_PIECES);
  for (size_t k = 1; k < pieces; ++k) {
    const double t = static_cast<double>(k) / static_cast<double>(pieces);
    const double t2 = t * t;
    const double t3 = t2 * t;
    out.push_back(from * (2.0 * t3 - 3.0 * t2 + 1.0) + m0 * (t3 - 2.0 * t2 + t) +
                  to * (3.0 * t2 - 2.0 * t3) + m1 * (t3 - t2));
  }
}

// The center of the pair's fillet, from the first segment, and the unit direction from it to the
// fillet's middle.
std::pair<ParkPoint, ParkPoint> filletOf(ParkPoint at, const Pair &pair, ParkPoint alongA,
                                         double halfA, ParkPoint alongB) {
  const double side = pair.Inner ? pair.Radius : -pair.Radius;
  const ParkPoint center = at + alongA * pair.TouchFirst + leftOf(alongA) * (halfA + side);
  const ParkPoint middle = pair.Inner
                               ? halfway(rightOf(alongA), leftOf(alongB), Sense::Clockwise)
                               : halfway(leftOf(alongA), rightOf(alongB), Sense::CounterClockwise);
  return {center, middle};
}

// Writes the outline from one segment's trimmed left corner, around the edge the pair makes at the
// point, to the next segment's trimmed right corner. Where the pair curves, a curve runs straight
// from the one to the other, leaving and meeting each side with no corner.
void outlinePair(std::vector<ParkPoint> &outline, ParkPoint at, const Pair &pair, const Shape &a,
                 size_t whichA, const Shape &b, size_t whichB, bool curves) {
  const ParkPoint alongA = a.outward(whichA);
  const ParkPoint alongB = b.outward(whichB);
  outline.assign(1, a.trimmed(whichA, LEFT));
  if (curves) {
    appendCurve(outline, a.trimmed(whichA, LEFT), alongA * -1.0, b.trimmed(whichB, RIGHT), alongB);
  } else if (pair.Kind == Turn::Fillet) {
    const ParkPoint center = filletOf(at, pair, alongA, a.Half, alongB).first;
    outline.push_back(a.edge(whichA, LEFT, pair.TouchFirst));
    if (pair.Inner) {
      appendTurn(outline, center, rightOf(alongA), leftOf(alongB), Sense::Clockwise, pair.Radius,
                 pair.Radius);
    } else {
      appendTurn(outline, center, leftOf(alongA), rightOf(alongB), Sense::CounterClockwise,
                 pair.Radius, pair.Radius);
    }
    outline.push_back(b.edge(whichB, RIGHT, pair.TouchSecond));
  } else {
    // Around the point, the outline keeps the narrower half width, and the wider segment's end
    // steps in to it.
    outline.push_back(a.edge(whichA, LEFT, 0.0));
    if (pair.Kind == Turn::Around) {
      const double half = std::min(a.Half, b.Half);
      appendTurn(outline, at, leftOf(alongA), rightOf(alongB), Sense::CounterClockwise, half, half);
    }
    outline.push_back(b.edge(whichB, RIGHT, 0.0));
  }
  outline.push_back(b.trimmed(whichB, RIGHT));
}

// The side of the shape's segment that is the end's side, seen walking from its From point: the
// left side at the From end is its left, and at the To end its right.
SideLink sideAt(const Shape &shape, const End &end, Side side) {
  return SideLink{shape.Key, (end.Which == 0) == (side == LEFT), end.Which == 0};
}

// Splits the outline of the turn from the first end's left side to the second end's right side
// halfway along its length, and keeps each half as its end's turn on that side, each starting from
// that end's trimmed corner and meeting the other's.
void appendHalves(Scratch &scratch, const std::vector<ParkPoint> &outline,
                  const std::vector<Shape> &shapes, const End &first, const End &second) {
  double total = 0.0;
  for (size_t i = 0; i + 1 < outline.size(); ++i) {
    total += lengthOf(outline[i + 1] - outline[i]);
  }
  const double half = total / 2.0;
  const size_t n = outline.size();
  const auto appendHalf = [&](bool forward) {
    const auto at = [&](size_t i) { return forward ? outline[i] : outline[n - 1 - i]; };
    const size_t start = scratch.TurnPoints.size();
    scratch.TurnPoints.push_back(at(0));
    double walked = 0.0;
    for (size_t i = 0; i + 1 < n && walked < half; ++i) {
      const double step = lengthOf(at(i + 1) - at(i));
      if (walked + step >= half) {
        scratch.TurnPoints.push_back(at(i) + (at(i + 1) - at(i)) * ((half - walked) / step));
      } else {
        scratch.TurnPoints.push_back(at(i + 1));
      }
      walked += step;
    }
    return std::pair{start, scratch.TurnPoints.size() - start};
  };
  const auto [firstStart, firstCount] = appendHalf(true);
  scratch.Turns[first.Shape][first.Which][LEFT] =
      TurnSpan{firstStart, firstCount, sideAt(shapes[second.Shape], second, RIGHT)};
  const auto [secondStart, secondCount] = appendHalf(false);
  scratch.Turns[second.Shape][second.Which][RIGHT] =
      TurnSpan{secondStart, secondCount, sideAt(shapes[first.Shape], first, LEFT)};
}

// A point's paving and its edge lines, after trimPoint. At a corner, the paving belongs to the
// segment on its side of the line into the corner. Elsewhere, it belongs to the point, fanned from
// it, or cut into triangles where its outline curves.
void buildPoint(Output &output, Scratch &scratch, EntityKey point, ParkPoint at,
                const std::vector<Shape> &shapes, const ParkCorner *corner) {
  const std::vector<End> &ends = scratch.Group;
  const size_t count = ends.size();
  const PathKind kind = shapes[ends[0].Shape].Kind;
  std::vector<ParkPoint> &ring = scratch.Fan;
  ring.clear();
  const auto away = [&](size_t index) {
    return shapes[ends[index].Shape].outward(ends[index].Which);
  };
  for (size_t i = 0; i < count; ++i) {
    const size_t j = (i + 1) % count;
    const Shape &a = shapes[ends[i].Shape];
    const Shape &b = shapes[ends[j].Shape];
    const size_t whichA = ends[i].Which;
    const size_t whichB = ends[j].Which;
    const bool curves = scratch.Taper.Tapers && scratch.Taper.Curves[i];
    outlinePair(scratch.Outline, at, scratch.Pairs[i], a, whichA, b, whichB, curves);
    // Unless the two segments leave the point in the same direction, each one's side runs on into
    // the other's along half the turn between them.
    if (!scratch.Turns.empty() &&
        (std::abs(ccw(away(i), away(j))) > PARALLEL || dot(away(i), away(j)) <= 0.0)) {
      appendHalves(scratch, scratch.Outline, shapes, ends[i], ends[j]);
    }
    ring.push_back(a.trimmed(whichA, RIGHT));
    ring.insert(ring.end(), scratch.Outline.begin(), scratch.Outline.end() - 1);
    scratch.Outline.insert(scratch.Outline.begin(), a.middle(whichA, LEFT));
    scratch.Outline.push_back(b.middle(whichB, RIGHT));
    output.edge(kind, point, scratch.Outline);
  }
  if (corner == nullptr && !scratch.Taper.Tapers) {
    for (size_t i = 0; i < ring.size(); ++i) {
      output.piece(kind, point, {at, ring[i], ring[(i + 1) % ring.size()]});
    }
    return;
  }
  if (corner == nullptr) {
    triangulate(output, kind, ring, [point](ParkPoint /*centroid*/) { return point; });
    return;
  }
  // The first segment is the one the other lies counter-clockwise of.
  const bool firstIsZero = ccw(shapes[ends[0].Shape].outward(ends[0].Which),
                               shapes[ends[1].Shape].outward(ends[1].Which)) > 0.0;
  const EntityKey first = shapes[ends[firstIsZero ? 0 : 1].Shape].Key;
  const EntityKey second = shapes[ends[firstIsZero ? 1 : 0].Shape].Key;
  triangulate(output, kind, ring, [&](ParkPoint centroid) {
    return ccw(corner->Toward, centroid - corner->At) < 0.0 ? first : second;
  });
}

// Adds each edge the point's pairs make, other than a corner's inside edge, after trimPoint.
void addEdges(std::vector<ParkEdge> &found, const Scratch &scratch,
              const std::vector<Shape> &shapes, EntityKey point, ParkPoint at, bool corner) {
  const std::vector<End> &ends = scratch.Group;
  for (size_t i = 0; i < ends.size(); ++i) {
    const Pair &pair = scratch.Pairs[i];
    if (pair.Kind != Turn::Fillet || (corner && pair.Inner)) {
      continue;
    }
    const Shape &a = shapes[ends[i].Shape];
    const Shape &b = shapes[ends[(i + 1) % ends.size()].Shape];
    const ParkPoint alongA = a.outward(ends[i].Which);
    const ParkPoint alongB = b.outward(ends[(i + 1) % ends.size()].Which);
    const double sine = ccw(alongA, alongB);
    const double meet = (b.Half + a.Half * dot(alongA, alongB)) / sine;
    const auto [center, middle] = filletOf(at, pair, alongA, a.Half, alongB);
    found.push_back(ParkEdge{point,
                             a.Key,
                             {alongA, alongB},
                             pair.Inner,
                             pair.Radius,
                             pair.Least,
                             pair.Most,
                             center + middle * pair.Radius,
                             at + alongA * meet + leftOf(alongA) * a.Half,
                             middle * -1.0});
  }
}

// The path point the key holds, or null.
const PathPointIntent *findPoint(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  return found != layout.Points.end() && found->Key == key ? &found->Intent : nullptr;
}

// The taper length the point holds, or 0 when that is not finite or is below zero.
double heldTaper(const PathPointIntent &point) {
  return std::isfinite(point.Taper) && point.Taper > 0.0 ? point.Taper : 0.0;
}

bool touches(const ParkSegment &segment, const std::vector<EntityKey> &points) {
  return std::ranges::binary_search(points, segment.From) ||
         std::ranges::binary_search(points, segment.To);
}

void sortUnique(std::vector<EntityKey> &keys) {
  std::ranges::sort(keys);
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
}

// The owners wanted for a focus: each point in it and each segment that ends at one. And the points
// to build so that each wanted owner's trims are known: those and each wanted segment's other end.
std::pair<std::vector<EntityKey>, std::vector<EntityKey>>
focusOf(const PathGround &ground, const std::vector<EntityKey> &focus) {
  std::vector<EntityKey> wanted = focus;
  std::vector<EntityKey> built = focus;
  for (const ParkSegment &segment : ground.Segments) {
    if (touches(segment, focus)) {
      wanted.push_back(segment.Key);
      built.push_back(segment.From);
      built.push_back(segment.To);
    }
  }
  sortUnique(wanted);
  sortUnique(built);
  return {std::move(wanted), std::move(built)};
}

// The shape of each segment with a ground line, or when loaded is given, of each that ends at a
// point in it.
void shapesOf(const PathLayout &layout, const PathGround &ground,
              const std::vector<EntityKey> *loaded, std::vector<Shape> &shapes) {
  shapes.clear();
  for (const ParkSegment &segment : ground.Segments) {
    if (segment.Line.empty() || (loaded != nullptr && !touches(segment, *loaded))) {
      continue;
    }
    const ParkPoint from = findPoint(layout, segment.From)->At;
    const ParkPoint to = findPoint(layout, segment.To)->At;
    const double length = lengthOf(to - from);
    shapes.push_back(Shape{segment.Key,
                           segment.Kind,
                           {segment.From, segment.To},
                           {from, to},
                           (to - from) * (1.0 / length),
                           segment.Width / 2.0,
                           length,
                           {},
                           {},
                           {segment.FromEdge, segment.ToEdge}});
  }
}

std::vector<Shape> shapesOf(const PathLayout &layout, const PathGround &ground,
                            const std::vector<EntityKey> *loaded) {
  std::vector<Shape> shapes;
  shapesOf(layout, ground, loaded, shapes);
  return shapes;
}

// Each segment end, by its point's key, then by its segment's key, in ends.
void endsOf(const std::vector<Shape> &shapes,
            std::vector<std::tuple<EntityKey, EntityKey, End>> &ends) {
  ends.clear();
  ends.reserve(2 * shapes.size());
  for (size_t i = 0; i < shapes.size(); ++i) {
    ends.emplace_back(shapes[i].Points[0], shapes[i].Key, End{i, 0});
    ends.emplace_back(shapes[i].Points[1], shapes[i].Key, End{i, 1});
  }
  std::ranges::sort(ends, [](const auto &x, const auto &y) {
    return std::tie(std::get<0>(x), std::get<1>(x), std::get<2>(x).Which) <
           std::tie(std::get<0>(y), std::get<1>(y), std::get<2>(y).Which);
  });
}

std::vector<std::tuple<EntityKey, EntityKey, End>> endsOf(const std::vector<Shape> &shapes) {
  std::vector<std::tuple<EntityKey, EntityKey, End>> ends;
  endsOf(shapes, ends);
  return ends;
}

// Calls visit(key, corner) for each point in ascending key order, with its ends in scratch.Group,
// leaving out each point not in built when built is given. corner is the point's corner, or null.
template <typename Visit>
void forEachPoint(const std::vector<std::tuple<EntityKey, EntityKey, End>> &ends,
                  const PathGround &ground, Scratch &scratch, const std::vector<EntityKey> *built,
                  Visit visit) {
  for (size_t first = 0; first < ends.size();) {
    const EntityKey key = std::get<0>(ends[first]);
    scratch.Group.clear();
    for (; first < ends.size() && std::get<0>(ends[first]) == key; ++first) {
      scratch.Group.push_back(std::get<2>(ends[first]));
    }
    if (built != nullptr && !std::ranges::binary_search(*built, key)) {
      continue;
    }
    const auto corner = std::ranges::lower_bound(ground.Corners, key, {}, &ParkCorner::Point);
    visit(key, corner != ground.Corners.end() && corner->Point == key ? &*corner : nullptr);
  }
}

// The points, in ascending key order, and the far end of each segment at one of them.
void farEndsOf(const PathGround &ground, const std::vector<EntityKey> &points,
               std::vector<EntityKey> &reached) {
  copyGrowing(reached, std::span<const EntityKey>(points));
  for (const ParkSegment &segment : ground.Segments) {
    if (touches(segment, points)) {
      reached.push_back(segment.From);
      reached.push_back(segment.To);
    }
  }
  sortUnique(reached);
}

std::vector<EntityKey> farEndsOf(const PathGround &ground, const std::vector<EntityKey> &points) {
  std::vector<EntityKey> reached;
  farEndsOf(ground, points, reached);
  return reached;
}

// Sets what each segment end at the points asks for: every point when points is null. A point's
// trims depend on what the far ends of its segments ask for, so those are found first.
void askPoints(const PathLayout &layout,
               const std::vector<std::tuple<EntityKey, EntityKey, End>> &ends,
               const PathGround &ground, Scratch &scratch, std::vector<Shape> &shapes,
               const std::vector<EntityKey> *points) {
  forEachPoint(ends, ground, scratch, points, [&](EntityKey key, const ParkCorner *corner) {
    if (scratch.Group.size() > 1) {
      trimPoint(scratch, shapes, corner, true, heldTaper(*findPoint(layout, key)));
    }
  });
}

// The surface of the owners wanted: every owner when focus is null, and otherwise those focusOf
// gives. If edges is false, it has no edge lines.
PavedSurface buildSurface(const PathLayout &layout, const PathGround &ground,
                          const std::vector<EntityKey> *focus, bool edges) {
  std::vector<EntityKey> wanted;
  std::vector<EntityKey> built;
  std::vector<EntityKey> asked;
  if (focus != nullptr) {
    std::tie(wanted, built) = focusOf(ground, *focus);
    asked = farEndsOf(ground, built);
  }
  std::vector<Shape> shapes = shapesOf(layout, ground, focus != nullptr ? &asked : nullptr);
  const std::vector<std::tuple<EntityKey, EntityKey, End>> ends = endsOf(shapes);
  PavedSurface surface;
  Output output(surface, wanted, focus == nullptr, edges);
  Scratch scratch;
  scratch.Turns.assign(shapes.size(), {});
  askPoints(layout, ends, ground, scratch, shapes, focus != nullptr ? &asked : nullptr);
  // Every segment's trims are set before any edge line is written, since an edge line runs to the
  // middle between a side's trims at both ends. The second pass sets the same trims again.
  for (const bool writing : {false, true}) {
    forEachPoint(ends, ground, scratch, focus != nullptr ? &built : nullptr,
                 [&](EntityKey key, const ParkCorner *corner) {
                   if (scratch.Group.size() == 1) {
                     if (writing) {
                       buildFreeEnd(output, key, shapes[scratch.Group[0].Shape],
                                    scratch.Group[0].Which);
                     }
                     return;
                   }
                   const PathPointIntent &point = *findPoint(layout, key);
                   trimPoint(scratch, shapes, corner, false, heldTaper(point));
                   if (writing) {
                     buildPoint(output, scratch, key, point.At, shapes, corner);
                   }
                 });
  }
  for (size_t i = 0; i < shapes.size(); ++i) {
    const Shape &shape = shapes[i];
    output.piece(shape.Kind, shape.Key,
                 {shape.trimmed(0, RIGHT), shape.trimmed(1, LEFT), shape.trimmed(1, RIGHT),
                  shape.trimmed(0, LEFT)});
    output.side(shape, true, scratch.Turns[i], scratch.TurnPoints);
    output.side(shape, false, scratch.Turns[i], scratch.TurnPoints);
  }
  std::ranges::stable_sort(surface.Pieces, {}, &SurfacePiece::Owner);
  std::ranges::stable_sort(surface.Edges, {}, &EdgeLine::Owner);
  return surface;
}

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

// Edge lines lie on the boundary of their own paving. A point of one is hidden only when it lies
// more than this many meters inside a piece.
constexpr double HIDDEN_DEPTH = 1e-6;
// Visible parts of an edge line shorter than this, in meters, are left out.
constexpr double LEAST_EDGE = 1e-3;

// Which segments are joined to which through shared points: a group for each set of them, and for
// each of their points.
class Joins {
public:
  explicit Joins(const PathGround &ground) {
    for (const ParkSegment &segment : ground.Segments) {
      if (!segment.Line.empty()) {
        Points.push_back(segment.From);
        Points.push_back(segment.To);
      }
    }
    sortUnique(Points);
    Parent.resize(Points.size());
    for (size_t i = 0; i < Parent.size(); ++i) {
      Parent[i] = i;
    }
    for (const ParkSegment &segment : ground.Segments) {
      if (!segment.Line.empty()) {
        Parent[root(indexOf(segment.From))] = root(indexOf(segment.To));
        Segments.emplace_back(segment.Key, indexOf(segment.From));
      }
    }
  }

  // The group of the point or segment the key holds.
  size_t groupOf(EntityKey owner) {
    const auto segment =
        std::ranges::lower_bound(Segments, owner, {}, &std::pair<EntityKey, size_t>::first);
    if (segment != Segments.end() && segment->first == owner) {
      return root(segment->second);
    }
    return root(indexOf(owner));
  }

private:
  size_t indexOf(EntityKey point) const {
    return static_cast<size_t>(std::ranges::lower_bound(Points, point) - Points.begin());
  }
  size_t root(size_t i) {
    while (Parent[i] != i) {
      Parent[i] = Parent[Parent[i]];
      i = Parent[i];
    }
    return i;
  }

  std::vector<EntityKey> Points;
  std::vector<size_t> Parent;
  std::vector<std::pair<EntityKey, size_t>> Segments;
};

// The stretch of the step from p to q, as fractions of it, that lies more than HIDDEN_DEPTH inside
// the convex piece, or none.
std::optional<std::pair<double, double>> insideOf(ParkPoint p, ParkPoint q,
                                                  std::span<const ParkPoint> corners) {
  double enter = 0.0;
  double leave = 1.0;
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint side = corners[(i + 1) % corners.size()] - corners[i];
    const double length = lengthOf(side);
    if (length < SAME_POINT) {
      continue;
    }
    // The inside lies to the left of each side of a counter-clockwise piece.
    const ParkPoint inward = leftOf(side) * (1.0 / length);
    const double start = dot(p - corners[i], inward) - HIDDEN_DEPTH;
    const double change = dot(q - p, inward);
    if (change == 0.0) {
      if (start <= 0.0) {
        return std::nullopt;
      }
    } else if (change > 0.0) {
      enter = std::max(enter, -start / change);
    } else {
      leave = std::min(leave, -start / change);
    }
    if (enter >= leave) {
      return std::nullopt;
    }
  }
  return std::pair{enter, leave};
}

// Writes the parts of an edge line that no piece of the hiders hides, each as an edge line of its
// own.
class Visible {
public:
  explicit Visible(PavedSurface &out) : Out(out) {}

  void start(PathKind kind, EntityKey owner) {
    Kind = kind;
    Owner = owner;
    First = Out.EdgePoints.size();
  }
  void to(ParkPoint at) {
    if (Out.EdgePoints.size() == First || !samePoint(Out.EdgePoints.back(), at)) {
      Out.EdgePoints.push_back(at);
    }
  }
  void close() {
    double length = 0.0;
    for (size_t i = First + 1; i < Out.EdgePoints.size(); ++i) {
      length += lengthOf(Out.EdgePoints[i] - Out.EdgePoints[i - 1]);
    }
    if (length >= LEAST_EDGE) {
      Out.Edges.push_back(EdgeLine{Kind, Owner, First, Out.EdgePoints.size() - First});
    } else {
      Out.EdgePoints.resize(First);
    }
    First = Out.EdgePoints.size();
  }
  bool open() const { return Out.EdgePoints.size() > First; }

private:
  PavedSurface &Out;
  PathKind Kind = PathKind::Guest;
  EntityKey Owner = NULL_KEY;
  size_t First = 0;
};

// Writes the stretches of the step from p to q between the hidden ones, which it sorts. A stretch
// that starts the step continues the edge line written so far.
void writeStep(Visible &writer, ParkPoint p, ParkPoint q,
               std::vector<std::pair<double, double>> &hidden) {
  std::ranges::sort(hidden);
  double from = 0.0;
  const auto show = [&](double until) {
    if (until <= from) {
      return;
    }
    if (from > 0.0 || !writer.open()) {
      writer.close();
      writer.to(p + (q - p) * from);
    }
    writer.to(p + (q - p) * until);
    if (until < 1.0) {
      writer.close();
    }
  };
  for (const auto &[enter, leave] : hidden) {
    show(enter);
    from = std::max(from, leave);
  }
  show(1.0);
}

// Hides each part of the surface's edge lines that lies inside a piece of the hiders joined to it:
// one whose owner is in the edge line's group.
void hideJoined(PavedSurface &surface, const std::vector<SurfacePiece> &hiders, Joins &joins) {
  std::vector<std::pair<Bounds, size_t>> hiderBounds;
  hiderBounds.reserve(hiders.size());
  for (const SurfacePiece &piece : hiders) {
    Bounds bounds = Bounds::around(piece.Corners[0]);
    for (const ParkPoint &corner : pieceCorners(piece)) {
      bounds.add(corner);
    }
    hiderBounds.emplace_back(bounds, joins.groupOf(piece.Owner));
  }
  PavedSurface visible;
  visible.Pieces = std::move(surface.Pieces);
  visible.Sides = std::move(surface.Sides);
  visible.SidePoints = std::move(surface.SidePoints);
  Visible writer(visible);
  std::vector<size_t> near;
  std::vector<std::pair<double, double>> hidden;
  for (const EdgeLine &edge : surface.Edges) {
    const std::span<const ParkPoint> points = edgePoints(surface, edge);
    Bounds bounds = Bounds::around(points[0]);
    for (const ParkPoint &point : points) {
      bounds.add(point);
    }
    const size_t group = joins.groupOf(edge.Owner);
    near.clear();
    for (size_t i = 0; i < hiders.size(); ++i) {
      if (hiderBounds[i].second == group && hiderBounds[i].first.overlaps(bounds)) {
        near.push_back(i);
      }
    }
    writer.start(edge.Kind, edge.Owner);
    for (size_t k = 0; k + 1 < points.size(); ++k) {
      hidden.clear();
      for (const size_t i : near) {
        if (const auto stretch = insideOf(points[k], points[k + 1], pieceCorners(hiders[i]))) {
          hidden.push_back(*stretch);
        }
      }
      writeStep(writer, points[k], points[k + 1], hidden);
    }
    writer.close();
  }
  surface = std::move(visible);
}

// How far from each point its paving and edge lines can reach, in ascending key order of the
// point. An inner edge touches each segment no farther than its length, and an outer edge no
// farther behind the point than twice the widest half width, and the paving lies within the
// widest half width of where they touch.
class PointReaches {
public:
  PointReaches(const PathLayout &layout, const PathGround &ground) {
    double widest = 0.0;
    for (const ParkSegment &segment : ground.Segments) {
      if (segment.Line.empty()) {
        continue;
      }
      widest = std::max(widest, segment.Width / 2.0);
      const double length =
          lengthOf(findPoint(layout, segment.To)->At - findPoint(layout, segment.From)->At);
      Reaches.emplace_back(segment.From, length);
      Reaches.emplace_back(segment.To, length);
    }
    std::ranges::sort(Reaches);
    Widest = widest;
  }

  double at(EntityKey point) const {
    const auto run =
        std::ranges::equal_range(Reaches, point, {}, &std::pair<EntityKey, double>::first);
    double longest = 2.0 * Widest;
    for (const auto &[key, length] : run) {
      longest = std::max(longest, length);
    }
    return longest + Widest;
  }

private:
  std::vector<std::pair<EntityKey, double>> Reaches;
  double Widest = 0.0;
};

// The rectangle that holds the segment's paving, its points' paving, and their edge lines: the
// segment's own rectangle grown by its half width, and the square each of its points can reach.
Bounds reachOf(const PathLayout &layout, const PointReaches &reaches, const ParkSegment &segment) {
  const ParkPoint from = findPoint(layout, segment.From)->At;
  const ParkPoint to = findPoint(layout, segment.To)->At;
  Bounds reach = Bounds::around(from);
  reach.add(to);
  reach = reach.grown(segment.Width / 2.0);
  reach.add(Bounds::around(from).grown(reaches.at(segment.From)));
  reach.add(Bounds::around(to).grown(reaches.at(segment.To)));
  return reach;
}

// The points of each segment whose paving, or whose points' paving, can reach into any of the
// bounds, in ascending key order.
std::vector<EntityKey> pointsNear(const PathLayout &layout, const PathGround &ground,
                                  const PointReaches &reaches, const std::vector<Bounds> &bounds) {
  std::vector<EntityKey> points;
  for (const ParkSegment &segment : ground.Segments) {
    if (segment.Line.empty()) {
      continue;
    }
    const Bounds reach = reachOf(layout, reaches, segment);
    if (std::ranges::any_of(bounds, [&reach](const Bounds &box) { return reach.overlaps(box); })) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  sortUnique(points);
  return points;
}

// A walkway's door end leans along its door's face only while its line leaves the face at least
// this steeply, as the cosine between them.
constexpr double LEAST_WALKWAY_LEAN = 0.1;

// Adds the walkway's piece and its edge lines along both sides and across its far end, owned by the
// segment it joins. Nothing when that segment has no ground line, its width is not above 0, or its
// door and the place it meets lie closer together than MIN_SEGMENT_LENGTH.
void addWalkway(PavedSurface &surface, const PathGround &ground, const Walkway &walkway) {
  const std::optional<SurfacePiece> piece = walkwayPiece(ground.Segments, walkway);
  if (!piece) {
    return;
  }
  surface.Pieces.push_back(*piece);
  const std::span<const ParkPoint> corners = pieceCorners(*piece);
  for (size_t i = 0; i + 1 < corners.size(); ++i) {
    surface.Edges.push_back(EdgeLine{piece->Kind, piece->Owner, surface.EdgePoints.size(), 2});
    surface.EdgePoints.push_back(corners[i]);
    surface.EdgePoints.push_back(corners[i + 1]);
  }
}

} // namespace

std::optional<SurfacePiece> walkwayPiece(std::span<const ParkSegment> segments,
                                         const Walkway &walkway) {
  const bool joins = std::ranges::any_of(segments, [&](const ParkSegment &segment) {
    return segment.Key == walkway.Joins && segment.Kind == walkway.Kind && !segment.Line.empty();
  });
  const double length = lengthOf(walkway.Meets - walkway.Door);
  if (!joins || !(walkway.Width > 0.0) || !(length >= MIN_SEGMENT_LENGTH)) {
    return std::nullopt;
  }
  const ParkPoint along = (walkway.Meets - walkway.Door) * (1.0 / length);
  const ParkPoint left = leftOf(along);
  const double half = walkway.Width / 2.0;
  const double lean = dot(along, walkway.Out);
  // The door corner on the side s, on the face's line through the door when the walkway leans.
  const auto doorCorner = [&](double s) {
    const ParkPoint side = walkway.Door + left * (half * s);
    double t = lean >= LEAST_WALKWAY_LEAN ? -half * s * dot(left, walkway.Out) / lean : 0.0;
    if (!(std::abs(t) < length)) {
      t = 0.0;
    }
    return side + along * t;
  };
  const ParkPoint end = walkway.Meets + along * WALKWAY_OVERLAP;
  const ParkPoint right = doorCorner(-1.0);
  const ParkPoint farRight = end - left * half;
  const ParkPoint farLeft = end + left * half;
  const ParkPoint leftDoor = doorCorner(1.0);
  return SurfacePiece{walkway.Kind, walkway.Joins, {right, farRight, farLeft, leftDoor}, 4};
}

std::span<const ParkPoint> pieceCorners(const SurfacePiece &piece) {
  return {piece.Corners.data(), piece.Count};
}

std::span<const ParkPoint> edgePoints(const PavedSurface &surface, const EdgeLine &edge) {
  return std::span(surface.EdgePoints).subspan(edge.First, edge.Count);
}

std::span<const ParkPoint> sidePoints(const PavedSurface &surface, const SegmentSide &side) {
  return std::span(surface.SidePoints).subspan(side.First, side.Count);
}

double pieceDistance(const SurfacePiece &piece, ParkPoint point) {
  const std::span<const ParkPoint> corners = pieceCorners(piece);
  if (corners.empty()) {
    return std::numeric_limits<double>::infinity();
  }
  bool inside = true;
  double nearest = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < corners.size(); ++i) {
    const ParkPoint from = corners[i];
    const ParkPoint side = corners[(i + 1) % corners.size()] - from;
    const double length = lengthOf(side);
    if (length < SAME_POINT) {
      nearest = std::min(nearest, lengthOf(point - from));
      continue;
    }
    // The inside of a counter-clockwise piece lies to the left of each side.
    if (dot(point - from, leftOf(side)) < 0.0) {
      inside = false;
    }
    const double along = std::clamp(dot(point - from, side) / (length * length), 0.0, 1.0);
    nearest = std::min(nearest, lengthOf(point - (from + side * along)));
  }
  return inside ? 0.0 : nearest;
}

PavedSurface surfaceOf(const PathLayout &layout, const PathGround &ground) {
  PavedSurface surface = buildSurface(layout, ground, nullptr, true);
  const std::vector<SurfacePiece> pieces = surface.Pieces;
  Joins joins(ground);
  hideJoined(surface, pieces, joins);
  return surface;
}

PavedSurface surfaceAround(const PathLayout &layout, const PathGround &ground,
                           const std::vector<EntityKey> &focus, bool hide) {
  PavedSurface surface = buildSurface(layout, ground, &focus, true);
  if (!hide || surface.EdgePoints.empty()) {
    return surface;
  }
  // The pieces that can hide these edge lines are those of the segments near them, and of the
  // junctions at their points.
  std::vector<Bounds> bounds;
  bounds.reserve(surface.Edges.size());
  for (const EdgeLine &edge : surface.Edges) {
    const std::span<const ParkPoint> points = edgePoints(surface, edge);
    Bounds box = Bounds::around(points[0]);
    for (const ParkPoint &point : points) {
      box.add(point);
    }
    bounds.push_back(box);
  }
  const std::vector<EntityKey> near =
      pointsNear(layout, ground, PointReaches(layout, ground), bounds);
  Joins joins(ground);
  hideJoined(surface, buildSurface(layout, ground, &near, false).Pieces, joins);
  return surface;
}

std::vector<EntityKey> pointsAffected(const PathLayout &layout, const PathGround &ground,
                                      const std::vector<EntityKey> &points) {
  const PointReaches reaches(layout, ground);
  std::vector<Bounds> bounds;
  for (const ParkSegment &segment : ground.Segments) {
    if (!segment.Line.empty() && touches(segment, points)) {
      bounds.push_back(reachOf(layout, reaches, segment));
    }
  }
  std::vector<EntityKey> affected = pointsNear(layout, ground, reaches, bounds);
  affected.insert(affected.end(), points.begin(), points.end());
  sortUnique(affected);
  return affected;
}

PavedSurface parkSurface(const World &world) {
  const PathLayout layout = layoutOf(world);
  return surfaceOf(layout, groundOf(layout));
}

namespace {

inline constexpr uint64_t SURFACE_PURPOSE = hashName("paved-surface");

void resolvePavedSurface(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, SURFACE_PURPOSE, 0);
  world.emplaceOrReplace<PavedSurface>(key, parkSurface(world));
}

} // namespace

const PavedSurface &resolvedSurface(const World &world) {
  static const PavedSurface EMPTY;
  const PavedSurface *surface = world.tryGet<PavedSurface>(deriveKey(NULL_KEY, SURFACE_PURPOSE, 0));
  return surface != nullptr ? *surface : EMPTY;
}

void addPavedSurface(WorldSchema &schema) {
  schema.addComponent<PavedSurface>("paved-surface", DataKind::Derived);
  schema.addResolver("paved-surface", &resolvePavedSurface);
}

PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways) {
  const PathLayout layout = layoutOf(world);
  const PathGround ground = groundOf(layout);
  PavedSurface surface = buildSurface(layout, ground, nullptr, true);
  for (const Walkway &walkway : walkways) {
    addWalkway(surface, ground, walkway);
  }
  std::ranges::stable_sort(surface.Pieces, {}, &SurfacePiece::Owner);
  std::ranges::stable_sort(surface.Edges, {}, &EdgeLine::Owner);
  const std::vector<SurfacePiece> pieces = surface.Pieces;
  Joins joins(ground);
  hideJoined(surface, pieces, joins);
  return surface;
}

namespace {

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
              std::vector<ParkEdge> &found) {
  std::vector<Shape> &shapes = lists.Shapes;
  shapesOf(layout, ground, nullptr, shapes);
  endsOf(shapes, lists.Ends);
  Scratch &scratch = lists.Fit;
  askPoints(layout, lists.Ends, ground, scratch, shapes, nullptr);
  found.clear();
  forEachPoint(lists.Ends, ground, scratch, nullptr, [&](EntityKey key, const ParkCorner *corner) {
    if (scratch.Group.size() < 2) {
      return;
    }
    const PathPointIntent &point = *findPoint(layout, key);
    trimPoint(scratch, shapes, corner, false, heldTaper(point));
    addEdges(found, scratch, shapes, key, point.At, corner != nullptr);
  });
  std::ranges::sort(found, {},
                    [](const ParkEdge &edge) { return std::pair{edge.Point, edge.Segment}; });
}

// The points, in ascending key order, that exactly two segments of different widths use, as their
// intent gives them, in candidates. Every taper is one of them. ends is a list it reuses.
void taperCandidates(const World &world, std::vector<std::pair<EntityKey, double>> &ends,
                     std::vector<EntityKey> &candidates) {
  ends.clear();
  world.forEachWith<PathSegmentIntent>(
      [&ends](EntityKey /*key*/, const PathSegmentIntent &segment) {
        ends.emplace_back(segment.From, segment.Width);
        ends.emplace_back(segment.To, segment.Width);
      });
  std::ranges::sort(ends);
  candidates.clear();
  for (size_t first = 0; first < ends.size();) {
    size_t last = first;
    while (last < ends.size() && ends[last].first == ends[first].first) {
      ++last;
    }
    if (last - first == 2 && ends[first].second != ends[first + 1].second) {
      candidates.push_back(ends[first].first);
    }
    first = last;
  }
}

// Each taper among the candidates, which are in ascending key order, in found. Only the candidates
// and the far ends of their segments are fitted.
void fitTapers(const PathLayout &layout, const PathGround &ground,
               const std::vector<EntityKey> &candidates, EdgeLists &lists,
               std::vector<ParkTaper> &found) {
  farEndsOf(ground, candidates, lists.Asked);
  std::vector<Shape> &shapes = lists.Shapes;
  shapesOf(layout, ground, &lists.Asked, shapes);
  endsOf(shapes, lists.Ends);
  Scratch &scratch = lists.Fit;
  askPoints(layout, lists.Ends, ground, scratch, shapes, &lists.Asked);
  found.clear();
  forEachPoint(
      lists.Ends, ground, scratch, &candidates, [&](EntityKey key, const ParkCorner *corner) {
        if (scratch.Group.size() != 2) {
          return;
        }
        const PathPointIntent &point = *findPoint(layout, key);
        trimPoint(scratch, shapes, corner, false, heldTaper(point));
        if (!scratch.Taper.Tapers) {
          return;
        }
        const TaperFit &taper = scratch.Taper;
        const End &narrow = scratch.Group[taper.Narrow];
        const ParkPoint along = shapes[narrow.Shape].outward(narrow.Which);
        found.push_back(ParkTaper{key, point.At + along * taper.Start, shapes[narrow.Shape].Key,
                                  along, taper.Length, taper.MaxLength});
      });
}

} // namespace

std::vector<ParkEdge> edgesOf(const PathLayout &layout, const PathGround &ground) {
  EdgeLists lists;
  std::vector<ParkEdge> found;
  fitEdges(layout, ground, lists, found);
  return found;
}

std::vector<ParkTaper> parkTapers(const World &world) {
  EdgeLists lists;
  taperCandidates(world, lists.CandidateEnds, lists.Candidates);
  std::vector<ParkTaper> tapers;
  if (lists.Candidates.empty()) {
    return tapers;
  }
  const PathLayout layout = layoutOf(world);
  fitTapers(layout, groundOf(layout), lists.Candidates, lists, tapers);
  return tapers;
}

void parkTapers(const World &world, std::vector<ParkTaper> &tapers, TickBuffers &buffers) {
  EdgeLists &lists = buffers.get<EdgeLists>();
  taperCandidates(world, lists.CandidateEnds, lists.Candidates);
  tapers.clear();
  if (lists.Candidates.empty()) {
    return;
  }
  GroundLists &ground = buffers.get<GroundLists>();
  layoutOf(world, ground.Layout);
  groundOf(ground.Layout, ground);
  fitTapers(ground.Layout, ground.Ground, lists.Candidates, lists, tapers);
}

std::vector<ParkEdge> parkEdges(const World &world) {
  const PathLayout layout = layoutOf(world);
  return edgesOf(layout, groundOf(layout));
}

void parkEdges(const World &world, std::vector<ParkEdge> &edges, TickBuffers &buffers) {
  GroundLists &ground = buffers.get<GroundLists>();
  layoutOf(world, ground.Layout);
  groundOf(ground.Layout, ground);
  fitEdges(ground.Layout, ground.Ground, buffers.get<EdgeLists>(), edges);
}

double edgeRadiusFor(const ParkEdge &edge, double distance) {
  // The middle lies 1 / sin(g / 2) - 1 farther from where the sides meet for each meter of radius,
  // where g is the turn from one segment to the other.
  const double halfSine = std::sqrt((1.0 - dot(edge.Along[0], edge.Along[1])) / 2.0);
  const double perRadius = halfSine > 0.0 ? 1.0 / halfSine - 1.0 : 0.0;
  if (!(distance > 0.0) || !(perRadius > 0.0)) {
    return 0.0;
  }
  return distance / perRadius;
}

} // namespace tpj
