#include "sim/park/surface.h"

#include "sim/park/geometry.h"
#include "sim/park/internal/layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
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
  // Whether no other segment uses the end's point.
  std::array<bool, 2> Free{};
  std::array<std::array<double, 2>, 2> Trim{};

  // The unit direction away from the end's point.
  ParkPoint outward(size_t end) const { return end == 0 ? Along : Along * -1.0; }
  // The point on the side's edge the distance from the end's point.
  ParkPoint edge(size_t end, Side side, double distance) const {
    const ParkPoint across = leftOf(outward(end)) * (side == LEFT ? Half : -Half);
    return Ends[end] + outward(end) * distance + across;
  }
  ParkPoint trimmed(size_t end, Side side) const { return edge(end, side, Trim[end][side]); }
  // How far from the end's point its trims may reach: the whole length when the other end is
  // free, so its trims there are 0, and otherwise half of it.
  double reach(size_t end) const { return Free[1 - end] ? Length : Length / 2.0; }
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

// How the outline turns from one segment at a junction to the next one counter-clockwise.
enum class Turn { Fillet, Around, Step };

struct Pair {
  Turn Kind = Turn::Step;
  // For a fillet: its radius, and how far from the point it touches each segment.
  double Radius = 0.0;
  double TouchFirst = 0.0;
  double TouchSecond = 0.0;
};

// Lists reused from one point to the next, so building a surface allocates only while they grow.
struct Scratch {
  std::vector<End> Group;
  std::vector<Pair> Pairs;
  std::vector<ParkPoint> Outline;
  std::vector<ParkPoint> Fan;
  std::vector<ParkPoint> Outer;
  std::vector<ParkPoint> Inner;
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

// The corner's tangent length, and its arc's center.
std::pair<double, ParkPoint> arcOf(const ParkCorner &corner, double sine, double cosine) {
  const double tangent = corner.Radius * sine / (1.0 - cosine);
  return {tangent,
          corner.At + corner.Toward * std::sqrt(tangent * tangent + corner.Radius * corner.Radius)};
}

// Where the inner sides of a sharp corner's two segments stop, measured along each from the
// point: the first runs into the corner and the second lies counter-clockwise of it. The inner
// edges run on until they meet. If the second's inner edge first meets the first's square end, it
// stops there and the first's inner side stops at the point, and the same the other way round. If
// neither fits within half of each segment's length, both stop at the point.
std::array<double, 2> innerTrims(const Shape &first, size_t a, const Shape &second, size_t b) {
  const ParkPoint alongA = first.outward(a);
  const ParkPoint alongB = second.outward(b);
  const double sine = ccw(alongA, alongB);
  const double cosine = dot(alongA, alongB);
  const double limitA = first.reach(a);
  const double limitB = second.reach(b);
  const double meetA = (second.Half + first.Half * cosine) / sine;
  const double meetB = (first.Half + second.Half * cosine) / sine;
  if (meetA >= 0.0 && meetB >= 0.0) {
    if (meetA <= limitA && meetB <= limitB) {
      return {meetA, meetB};
    }
    return {0.0, 0.0};
  }
  if (std::abs(cosine) <= PARALLEL) {
    return {0.0, 0.0};
  }
  if (meetA < 0.0 && meetB >= 0.0) {
    // The second's inner edge, at rightOf(alongB) * half, crosses the line of the first's end.
    const double along = -second.Half * dot(rightOf(alongB), alongA) / cosine;
    const double across =
        second.Half * dot(rightOf(alongB), leftOf(alongA)) + along * dot(alongB, leftOf(alongA));
    if (across >= 0.0 && across <= first.Half && along <= limitB) {
      return {0.0, along};
    }
  } else if (meetB < 0.0 && meetA >= 0.0) {
    const double along = -first.Half * dot(leftOf(alongA), alongB) / cosine;
    const double across =
        first.Half * dot(leftOf(alongA), rightOf(alongB)) + along * dot(alongA, rightOf(alongB));
    if (across >= 0.0 && across <= second.Half && along <= limitA) {
      return {along, 0.0};
    }
  }
  return {0.0, 0.0};
}

// The arc a corner's paving follows: its center and radius, the half width of the paving around
// it, which is the narrower of its two segments', and whether the corner is sharp inside.
struct CornerArc {
  ParkPoint Center;
  double Radius = 0.0;
  double Half = 0.0;
  bool Sharp = false;
};

// Sets the trims of a corner's two segments and gives its arc: the first runs into the corner and
// the second lies counter-clockwise of it. Each stops where the arc touches it. When the radius is
// less than the paving's half width, the corner is sharp inside, and the inner sides stop as
// innerTrims sets.
CornerArc trimCorner(const ParkCorner &corner, Shape &first, size_t a, Shape &second, size_t b) {
  const double sine = ccw(first.outward(a), second.outward(b));
  const double cosine = dot(first.outward(a), second.outward(b));
  const auto [tangent, center] = arcOf(corner, sine, cosine);
  const CornerArc arc{center, corner.Radius, std::min(first.Half, second.Half),
                      corner.Radius < std::min(first.Half, second.Half)};
  first.Trim[a] = {tangent, tangent};
  second.Trim[b] = {tangent, tangent};
  if (arc.Sharp) {
    const std::array<double, 2> inner = innerTrims(first, a, second, b);
    first.Trim[a][LEFT] = inner[0];
    second.Trim[b][RIGHT] = inner[1];
  }
  return arc;
}

// Writes a corner's outer edge, from the first segment's outer corner around the arc to the
// second's, the wider segment's end stepping in to the arc. Then writes its inner edge: the inner
// corners of both segments when the corner is sharp, and otherwise the inner arc between them,
// with one point for each point of the outer edge.
void outlineCorner(std::vector<ParkPoint> &outer, std::vector<ParkPoint> &inner,
                   const CornerArc &arc, const Shape &first, size_t a, const Shape &second,
                   size_t b) {
  outer.assign(1, first.trimmed(a, RIGHT));
  appendTurn(outer, arc.Center, rightOf(first.outward(a)), leftOf(second.outward(b)),
             Sense::Clockwise, arc.Radius + arc.Half, arc.Radius + arc.Half);
  outer.push_back(second.trimmed(b, LEFT));
  inner.assign(1, first.trimmed(a, LEFT));
  if (!arc.Sharp) {
    const double scale = (arc.Radius - arc.Half) / (arc.Radius + arc.Half);
    for (size_t i = 1; i + 1 < outer.size(); ++i) {
      inner.push_back(arc.Center + (outer[i] - arc.Center) * scale);
    }
  }
  inner.push_back(second.trimmed(b, RIGHT));
}

// A corner's paving and its inner and outer edge lines, and the trims of its segments. The pieces
// from the first segment's end to the arc's middle belong to the first, and the rest to the
// second.
void buildCorner(Output *output, Scratch &scratch, const ParkCorner &corner,
                 std::vector<Shape> &shapes, std::array<End, 2> ends) {
  if (ccw(shapes[ends[0].Shape].outward(ends[0].Which),
          shapes[ends[1].Shape].outward(ends[1].Which)) < 0.0) {
    std::swap(ends[0], ends[1]);
  }
  Shape &first = shapes[ends[0].Shape];
  Shape &second = shapes[ends[1].Shape];
  const size_t a = ends[0].Which;
  const size_t b = ends[1].Which;
  const CornerArc arc = trimCorner(corner, first, a, second, b);
  if (output == nullptr) {
    return;
  }
  std::vector<ParkPoint> &outer = scratch.Outer;
  std::vector<ParkPoint> &inner = scratch.Inner;
  outlineCorner(outer, inner, arc, first, a, second, b);
  const size_t pieces = outer.size() - 1;
  const size_t middle = (pieces + 1) / 2;
  const auto ownerOf = [&](size_t i) { return i < middle ? first.Key : second.Key; };
  if (arc.Sharp) {
    // The paving between both segments' ends, the outer edge, and the inner corners. The pieces
    // on the first segment's side of the line into the corner belong to it.
    std::vector<ParkPoint> &polygon = scratch.Fan;
    polygon.assign(outer.begin(), outer.end());
    polygon.push_back(inner.back());
    polygon.push_back(inner.front());
    triangulate(*output, first.Kind, polygon, [&](ParkPoint centroid) {
      return ccw(corner.Toward, centroid - corner.At) < 0.0 ? first.Key : second.Key;
    });
  } else {
    for (size_t i = 0; i < pieces; ++i) {
      output->piece(first.Kind, ownerOf(i), {inner[i], outer[i], outer[i + 1], inner[i + 1]});
    }
  }
  // The inner edge runs from the first segment to the second, and the outer edge back, so the
  // paving lies on the left of each.
  std::vector<ParkPoint> &line = scratch.Outline;
  line.assign(1, first.middle(a, LEFT));
  line.insert(line.end(), inner.begin(), inner.end());
  line.push_back(second.middle(b, RIGHT));
  output->edge(first.Kind, corner.Point, line);
  line.assign(1, second.middle(b, LEFT));
  line.insert(line.end(), outer.rbegin(), outer.rend());
  line.push_back(first.middle(a, RIGHT));
  output->edge(first.Kind, corner.Point, line);
}

// How the outline turns between two segments at a point: the first's outward direction, half
// width, and half its length, and the same for the second, counter-clockwise of it. full is true
// when the second lies a whole turn on, as when every segment at the point runs one way.
Pair pairOf(ParkPoint alongA, double halfA, double limitA, ParkPoint alongB, double halfB,
            double limitB, bool full) {
  const double sine = ccw(alongA, alongB);
  const double cosine = dot(alongA, alongB);
  if (sine <= PARALLEL) {
    const bool around = sine < -PARALLEL || cosine < 0.0 || full;
    return Pair{around ? Turn::Around : Turn::Step};
  }
  // A fillet of radius r touches the first segment (halfB + halfA cosine + r (1 + cosine)) / sine
  // from the point, and the second the same with the halves swapped.
  const auto touchA = [&](double r) {
    return (halfB + halfA * cosine + r * (1.0 + cosine)) / sine;
  };
  const auto touchB = [&](double r) {
    return (halfA + halfB * cosine + r * (1.0 + cosine)) / sine;
  };
  if (touchA(0.0) > limitA || touchB(0.0) > limitB) {
    return Pair{Turn::Step};
  }
  // A fillet reaches r (1 + cosine) / sine past where the edges meet. Between segments less than a
  // quarter turn apart, its radius shrinks so that it reaches no farther than its radius would.
  double radius = std::min(halfA, halfB);
  if (1.0 + cosine > sine) {
    radius *= sine / (1.0 + cosine);
  }
  if (touchA(radius) > limitA || touchB(radius) > limitB) {
    radius = std::max(std::min((limitA * sine - halfB - halfA * cosine) / (1.0 + cosine),
                               (limitB * sine - halfA - halfB * cosine) / (1.0 + cosine)),
                      0.0);
  }
  return Pair{Turn::Fillet, radius, touchA(radius), touchB(radius)};
}

// Whether a's direction comes before b's, counter-clockwise from +x seen from above.
bool before(ParkPoint a, ParkPoint b) {
  const auto halfOf = [](ParkPoint d) { return d.Z < 0.0 || (d.Z == 0.0 && d.X > 0.0) ? 0 : 1; };
  if (halfOf(a) != halfOf(b)) {
    return halfOf(a) < halfOf(b);
  }
  return ccw(a, b) > 0.0;
}

// Sorts a junction's segments counter-clockwise, finds how the outline turns between each and
// the next, and sets each segment's trims to the farther of its fillets.
void trimJunction(Scratch &scratch, std::vector<Shape> &shapes) {
  std::vector<End> &ends = scratch.Group;
  const auto along = [&shapes](const End &end) { return shapes[end.Shape].outward(end.Which); };
  std::ranges::sort(ends, [&along, &shapes](const End &x, const End &y) {
    if (before(along(x), along(y)) != before(along(y), along(x))) {
      return before(along(x), along(y));
    }
    return std::tie(shapes[x.Shape].Key, x.Which) < std::tie(shapes[y.Shape].Key, y.Which);
  });
  const size_t count = ends.size();
  const bool oneWay = std::ranges::all_of(ends, [&](const End &end) {
    return std::abs(ccw(along(ends[0]), along(end))) <= PARALLEL &&
           dot(along(ends[0]), along(end)) > 0.0;
  });
  scratch.Pairs.clear();
  for (size_t i = 0; i < count; ++i) {
    const Shape &a = shapes[ends[i].Shape];
    const Shape &b = shapes[ends[(i + 1) % count].Shape];
    scratch.Pairs.push_back(pairOf(along(ends[i]), a.Half, a.reach(ends[i].Which),
                                   along(ends[(i + 1) % count]), b.Half,
                                   b.reach(ends[(i + 1) % count].Which), oneWay && i + 1 == count));
  }
  for (size_t i = 0; i < count; ++i) {
    const Pair &next = scratch.Pairs[i];
    const Pair &previous = scratch.Pairs[(i + count - 1) % count];
    double trim = 0.0;
    if (next.Kind == Turn::Fillet) {
      trim = std::max(trim, next.TouchFirst);
    }
    if (previous.Kind == Turn::Fillet) {
      trim = std::max(trim, previous.TouchSecond);
    }
    shapes[ends[i].Shape].Trim[ends[i].Which] = {trim, trim};
  }
}

// Writes the outline from one segment's trimmed left corner, around the inside corner the pair
// makes at the point, to the next segment's trimmed right corner.
void outlinePair(std::vector<ParkPoint> &outline, ParkPoint at, const Pair &pair, const Shape &a,
                 size_t whichA, const Shape &b, size_t whichB) {
  const ParkPoint alongA = a.outward(whichA);
  const ParkPoint alongB = b.outward(whichB);
  outline.assign(1, a.trimmed(whichA, LEFT));
  if (pair.Kind == Turn::Fillet) {
    const ParkPoint center =
        at + alongA * pair.TouchFirst + leftOf(alongA) * (a.Half + pair.Radius);
    outline.push_back(a.edge(whichA, LEFT, pair.TouchFirst));
    appendTurn(outline, center, rightOf(alongA), leftOf(alongB), Sense::Clockwise, pair.Radius,
               pair.Radius);
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

// A junction's fan of pieces and its edge lines, after trimJunction.
void buildJunction(Output &output, Scratch &scratch, EntityKey point, ParkPoint at,
                   const std::vector<Shape> &shapes) {
  const std::vector<End> &ends = scratch.Group;
  const size_t count = ends.size();
  const PathKind kind = shapes[ends[0].Shape].Kind;
  std::vector<ParkPoint> &fan = scratch.Fan;
  fan.clear();
  for (size_t i = 0; i < count; ++i) {
    const Shape &a = shapes[ends[i].Shape];
    const Shape &b = shapes[ends[(i + 1) % count].Shape];
    const size_t whichA = ends[i].Which;
    const size_t whichB = ends[(i + 1) % count].Which;
    outlinePair(scratch.Outline, at, scratch.Pairs[i], a, whichA, b, whichB);
    fan.push_back(a.trimmed(whichA, RIGHT));
    fan.insert(fan.end(), scratch.Outline.begin(), scratch.Outline.end() - 1);
    scratch.Outline.insert(scratch.Outline.begin(), a.middle(whichA, LEFT));
    scratch.Outline.push_back(b.middle(whichB, RIGHT));
    output.edge(kind, point, scratch.Outline);
  }
  for (size_t i = 0; i < fan.size(); ++i) {
    output.piece(kind, point, {at, fan[i], fan[(i + 1) % fan.size()]});
  }
}

// The path point the key holds, or null.
const PathPointIntent *findPoint(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  return found != layout.Points.end() && found->Key == key ? &found->Intent : nullptr;
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

// The shape of each segment with a ground line, or when built is given, of each that ends at a
// point in it.
std::vector<Shape> shapesOf(const PathLayout &layout, const PathGround &ground,
                            const std::vector<EntityKey> *built) {
  // Each end of every segment with a ground line, by its point's key.
  std::vector<EntityKey> used;
  used.reserve(2 * ground.Segments.size());
  for (const ParkSegment &segment : ground.Segments) {
    if (!segment.Line.empty()) {
      used.push_back(segment.From);
      used.push_back(segment.To);
    }
  }
  std::ranges::sort(used);
  const auto free = [&used](EntityKey point) {
    return std::ranges::equal_range(used, point).size() == 1;
  };
  std::vector<Shape> shapes;
  for (const ParkSegment &segment : ground.Segments) {
    if (segment.Line.empty() || (built != nullptr && !touches(segment, *built))) {
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
                           {free(segment.From), free(segment.To)},
                           {}});
  }
  return shapes;
}

// The surface of the owners wanted: every owner when focus is null, and otherwise those focusOf
// gives. If edges is false, it has no edge lines.
PavedSurface buildSurface(const PathLayout &layout, const PathGround &ground,
                          const std::vector<EntityKey> *focus, bool edges) {
  std::vector<EntityKey> wanted;
  std::vector<EntityKey> built;
  if (focus != nullptr) {
    std::tie(wanted, built) = focusOf(ground, *focus);
  }
  std::vector<Shape> shapes = shapesOf(layout, ground, focus != nullptr ? &built : nullptr);
  // Each segment end, by its point's key, then by its segment's key.
  std::vector<std::tuple<EntityKey, EntityKey, End>> ends;
  ends.reserve(2 * shapes.size());
  for (size_t i = 0; i < shapes.size(); ++i) {
    ends.emplace_back(shapes[i].Points[0], shapes[i].Key, End{i, 0});
    ends.emplace_back(shapes[i].Points[1], shapes[i].Key, End{i, 1});
  }
  std::ranges::sort(ends, [](const auto &x, const auto &y) {
    return std::tie(std::get<0>(x), std::get<1>(x), std::get<2>(x).Which) <
           std::tie(std::get<0>(y), std::get<1>(y), std::get<2>(y).Which);
  });

  PavedSurface surface;
  Output output(surface, wanted, focus == nullptr, edges);
  Scratch scratch;
  // Every segment's trims are set before any edge line is written, since an edge line runs to the
  // middle between a side's trims at both ends. The second pass sets the same trims again.
  for (const bool writing : {false, true}) {
    for (size_t first = 0; first < ends.size();) {
      const EntityKey key = std::get<0>(ends[first]);
      scratch.Group.clear();
      for (; first < ends.size() && std::get<0>(ends[first]) == key; ++first) {
        scratch.Group.push_back(std::get<2>(ends[first]));
      }
      if (focus != nullptr && !std::ranges::binary_search(built, key)) {
        continue;
      }
      const auto corner = std::ranges::lower_bound(ground.Corners, key, {}, &ParkCorner::Point);
      if (scratch.Group.size() == 1) {
        if (writing) {
          buildFreeEnd(output, key, shapes[scratch.Group[0].Shape], scratch.Group[0].Which);
        }
      } else if (corner != ground.Corners.end() && corner->Point == key) {
        buildCorner(writing ? &output : nullptr, scratch, *corner, shapes,
                    {scratch.Group[0], scratch.Group[1]});
      } else {
        trimJunction(scratch, shapes);
        if (writing) {
          buildJunction(output, scratch, key, findPoint(layout, key)->At, shapes);
        }
      }
    }
  }
  for (const Shape &shape : shapes) {
    output.piece(shape.Kind, shape.Key,
                 {shape.trimmed(0, RIGHT), shape.trimmed(1, LEFT), shape.trimmed(1, RIGHT),
                  shape.trimmed(0, LEFT)});
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

// The points of each segment whose paving, or whose points' paving, can reach into any of the
// bounds, in ascending key order.
std::vector<EntityKey> pointsNear(const PathLayout &layout, const PathGround &ground,
                                  const std::vector<Bounds> &bounds);
// The rectangle that holds the segment's paving, its points' paving along it, and their edge
// lines: the segment's own rectangle, grown by its half width and the widest half width, which
// bounds a junction's rounding, and by the farther its corners' arcs touch it.
Bounds reachOf(const PathLayout &layout, const PathGround &ground, const ParkSegment &segment,
               double widest);

// The largest half width of any segment.
double widestHalf(const PathGround &ground) {
  double widest = 0.0;
  for (const ParkSegment &segment : ground.Segments) {
    if (!segment.Line.empty()) {
      widest = std::max(widest, segment.Width / 2.0);
    }
  }
  return widest;
}

Bounds reachOf(const PathLayout &layout, const PathGround &ground, const ParkSegment &segment,
               double widest) {
  const auto tangentAt = [&ground](EntityKey point) {
    const auto corner = std::ranges::lower_bound(ground.Corners, point, {}, &ParkCorner::Point);
    if (corner == ground.Corners.end() || corner->Point != point) {
      return 0.0;
    }
    const double cosine = dot(corner->Along[0], corner->Along[1]);
    return corner->Radius * std::abs(ccw(corner->Along[0], corner->Along[1])) / (1.0 - cosine);
  };
  Bounds reach = Bounds::around(findPoint(layout, segment.From)->At);
  reach.add(findPoint(layout, segment.To)->At);
  return reach.grown(segment.Width / 2.0 + widest +
                     std::max(tangentAt(segment.From), tangentAt(segment.To)));
}

std::vector<EntityKey> pointsNear(const PathLayout &layout, const PathGround &ground,
                                  const std::vector<Bounds> &bounds) {
  const double widest = widestHalf(ground);
  std::vector<EntityKey> points;
  for (const ParkSegment &segment : ground.Segments) {
    if (segment.Line.empty()) {
      continue;
    }
    const Bounds reach = reachOf(layout, ground, segment, widest);
    if (std::ranges::any_of(bounds, [&reach](const Bounds &box) { return reach.overlaps(box); })) {
      points.push_back(segment.From);
      points.push_back(segment.To);
    }
  }
  sortUnique(points);
  return points;
}

} // namespace

std::span<const ParkPoint> pieceCorners(const SurfacePiece &piece) {
  return {piece.Corners.data(), piece.Count};
}

std::span<const ParkPoint> edgePoints(const PavedSurface &surface, const EdgeLine &edge) {
  return std::span(surface.EdgePoints).subspan(edge.First, edge.Count);
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
  const std::vector<EntityKey> near = pointsNear(layout, ground, bounds);
  Joins joins(ground);
  hideJoined(surface, buildSurface(layout, ground, &near, false).Pieces, joins);
  return surface;
}

std::vector<EntityKey> pointsAffected(const PathLayout &layout, const PathGround &ground,
                                      const std::vector<EntityKey> &points) {
  const double widest = widestHalf(ground);
  std::vector<Bounds> reaches;
  for (const ParkSegment &segment : ground.Segments) {
    if (!segment.Line.empty() && touches(segment, points)) {
      reaches.push_back(reachOf(layout, ground, segment, widest));
    }
  }
  std::vector<EntityKey> affected = pointsNear(layout, ground, reaches);
  affected.insert(affected.end(), points.begin(), points.end());
  sortUnique(affected);
  return affected;
}

PavedSurface parkSurface(const World &world) {
  const PathLayout layout = layoutOf(world);
  return surfaceOf(layout, groundOf(layout));
}

} // namespace tpj
