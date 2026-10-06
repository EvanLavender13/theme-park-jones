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

// Lists reused from one point to the next, so building a surface allocates only while they grow.
struct Scratch {
  std::vector<End> Group;
  std::vector<Pair> Pairs;
  std::vector<ParkPoint> Outline;
  std::vector<ParkPoint> Fan;
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

// The corner's tangent length: how far from its point its arc touches each of its segments.
double tangentOf(const ParkCorner &corner) {
  const double cosine = dot(corner.Along[0], corner.Along[1]);
  return corner.Radius * std::abs(ccw(corner.Along[0], corner.Along[1])) / (1.0 - cosine);
}

// A segment's end at a point, as the edges there see it: its outward direction, its half width,
// how far from the point it starts, and how far its trims may reach.
struct Arm {
  ParkPoint Along;
  double Half = 0.0;
  double Start = 0.0;
  double Reach = 0.0;
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
    most = std::min(radiusA(a.Reach), radiusB(b.Reach));
  } else {
    // A sharp outer edge reaches no farther behind the point than the wider width.
    const double behind = -2.0 * std::max(a.Half, b.Half);
    least = std::max({0.0, radiusA(behind), radiusB(behind)});
    most = std::min(radiusA(a.Start), radiusB(b.Start));
  }
  if (least > most) {
    return Pair{Turn::Step};
  }
  const double radius = std::clamp(asked, least, most);
  return Pair{Turn::Fillet, radius, touchA(radius), touchB(radius), inner, least, most};
}

// Whether a's direction comes before b's, counter-clockwise from +x seen from above.
bool before(ParkPoint a, ParkPoint b) {
  const auto halfOf = [](ParkPoint d) { return d.Z < 0.0 || (d.Z == 0.0 && d.X > 0.0) ? 0 : 1; };
  if (halfOf(a) != halfOf(b)) {
    return halfOf(a) < halfOf(b);
  }
  return ccw(a, b) > 0.0;
}

// The radius the edge from the end asks for: at a corner's inside, the corner's radius less the
// narrower half width, and elsewhere the end's edge radius. Never below 0.
double askedOf(const Shape &a, size_t whichA, const Shape &b, size_t whichB,
               const ParkCorner *corner) {
  if (corner != nullptr && ccw(a.outward(whichA), b.outward(whichB)) > 0.0) {
    return std::max(corner->Radius - std::min(a.Half, b.Half), 0.0);
  }
  const double held = a.Edge[whichA];
  return std::isfinite(held) && held > 0.0 ? held : 0.0;
}

// Sorts a point's segments counter-clockwise, finds how the outline turns between each and the
// next, and sets each segment's trims to where the edges on its two sides touch it. corner is the
// point's corner, or null.
void trimPoint(Scratch &scratch, std::vector<Shape> &shapes, const ParkCorner *corner) {
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
  const double start = corner != nullptr ? tangentOf(*corner) : 0.0;
  const auto armOf = [&](const End &end) {
    const Shape &shape = shapes[end.Shape];
    return Arm{shape.outward(end.Which), shape.Half, start, shape.reach(end.Which)};
  };
  scratch.Pairs.clear();
  for (size_t i = 0; i < count; ++i) {
    const End &a = ends[i];
    const End &b = ends[(i + 1) % count];
    const double asked = askedOf(shapes[a.Shape], a.Which, shapes[b.Shape], b.Which, corner);
    scratch.Pairs.push_back(pairOf(armOf(a), armOf(b), oneWay && i + 1 == count, asked));
  }
  for (size_t i = 0; i < count; ++i) {
    const Pair &next = scratch.Pairs[i];
    const Pair &previous = scratch.Pairs[(i + count - 1) % count];
    std::array<double, 2> &trim = shapes[ends[i].Shape].Trim[ends[i].Which];
    trim[LEFT] = next.Kind == Turn::Fillet ? std::max(next.TouchFirst, 0.0) : 0.0;
    trim[RIGHT] = previous.Kind == Turn::Fillet ? std::max(previous.TouchSecond, 0.0) : 0.0;
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
// point, to the next segment's trimmed right corner.
void outlinePair(std::vector<ParkPoint> &outline, ParkPoint at, const Pair &pair, const Shape &a,
                 size_t whichA, const Shape &b, size_t whichB) {
  const ParkPoint alongA = a.outward(whichA);
  const ParkPoint alongB = b.outward(whichB);
  outline.assign(1, a.trimmed(whichA, LEFT));
  if (pair.Kind == Turn::Fillet) {
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

// A point's paving and its edge lines, after trimPoint. At a corner, the paving belongs to the
// segment on its side of the line into the corner. Elsewhere, it belongs to the point and is
// fanned from it.
void buildPoint(Output &output, Scratch &scratch, EntityKey point, ParkPoint at,
                const std::vector<Shape> &shapes, const ParkCorner *corner) {
  const std::vector<End> &ends = scratch.Group;
  const size_t count = ends.size();
  const PathKind kind = shapes[ends[0].Shape].Kind;
  std::vector<ParkPoint> &ring = scratch.Fan;
  ring.clear();
  for (size_t i = 0; i < count; ++i) {
    const Shape &a = shapes[ends[i].Shape];
    const Shape &b = shapes[ends[(i + 1) % count].Shape];
    const size_t whichA = ends[i].Which;
    const size_t whichB = ends[(i + 1) % count].Which;
    outlinePair(scratch.Outline, at, scratch.Pairs[i], a, whichA, b, whichB);
    ring.push_back(a.trimmed(whichA, RIGHT));
    ring.insert(ring.end(), scratch.Outline.begin(), scratch.Outline.end() - 1);
    scratch.Outline.insert(scratch.Outline.begin(), a.middle(whichA, LEFT));
    scratch.Outline.push_back(b.middle(whichB, RIGHT));
    output.edge(kind, point, scratch.Outline);
  }
  if (corner == nullptr) {
    for (size_t i = 0; i < ring.size(); ++i) {
      output.piece(kind, point, {at, ring[i], ring[(i + 1) % ring.size()]});
    }
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
                           {},
                           {segment.FromEdge, segment.ToEdge}});
  }
  return shapes;
}

// Each segment end, by its point's key, then by its segment's key.
std::vector<std::tuple<EntityKey, EntityKey, End>> endsOf(const std::vector<Shape> &shapes) {
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
  const std::vector<std::tuple<EntityKey, EntityKey, End>> ends = endsOf(shapes);
  PavedSurface surface;
  Output output(surface, wanted, focus == nullptr, edges);
  Scratch scratch;
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
                   trimPoint(scratch, shapes, corner);
                   if (writing) {
                     buildPoint(output, scratch, key, findPoint(layout, key)->At, shapes, corner);
                   }
                 });
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

std::vector<ParkEdge> edgesOf(const PathLayout &layout, const PathGround &ground) {
  std::vector<Shape> shapes = shapesOf(layout, ground, nullptr);
  Scratch scratch;
  std::vector<ParkEdge> found;
  forEachPoint(
      endsOf(shapes), ground, scratch, nullptr, [&](EntityKey key, const ParkCorner *corner) {
        if (scratch.Group.size() < 2) {
          return;
        }
        trimPoint(scratch, shapes, corner);
        addEdges(found, scratch, shapes, key, findPoint(layout, key)->At, corner != nullptr);
      });
  std::ranges::sort(found, {},
                    [](const ParkEdge &edge) { return std::pair{edge.Point, edge.Segment}; });
  return found;
}

std::vector<ParkEdge> parkEdges(const World &world) {
  const PathLayout layout = layoutOf(world);
  return edgesOf(layout, groundOf(layout));
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
