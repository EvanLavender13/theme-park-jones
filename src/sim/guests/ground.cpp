#include "sim/guests/internal/ground.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/mix.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace tpj {

namespace {

inline constexpr uint64_t GROUND_PURPOSE = hashName("guest-ground");

// The number of cells along each side of the grid, and where the grid starts on each axis.
constexpr size_t GRID = static_cast<size_t>(PARK_SIZE / WALL_CELL_SIZE);
constexpr double GRID_ORIGIN = -0.5 * PARK_SIZE;

// Rounding allowed where a line keeps its distance from a wall, and where a point fits.
constexpr double CLEAR_ROUNDING = 1e-9;
constexpr double FIT_ROUNDING = 1e-6;

// Only +, -, *, /, and std::sqrt below, so every build gives the same results (decision 0022).
GroundPoint minus(GroundPoint a, GroundPoint b) { return {a.X - b.X, a.Z - b.Z}; }
GroundPoint plus(GroundPoint a, GroundPoint b) { return {a.X + b.X, a.Z + b.Z}; }
GroundPoint scaled(GroundPoint a, double by) { return {a.X * by, a.Z * by}; }
double dot(GroundPoint a, GroundPoint b) { return (a.X * b.X) + (a.Z * b.Z); }
double squared(GroundPoint a) { return dot(a, a); }
// The direction a quarter turn from the given one toward its left seen from above, as sim/park
// turns a side toward the paving it bounds.
GroundPoint leftOf(GroundPoint a) { return {a.Z, -a.X}; }
// How far the point lies to the wall's left, in units of the wall's length: below 0 on its right.
double sideOf(const GroundWall &wall, GroundPoint point) {
  return dot(minus(point, wall.From), leftOf(minus(wall.To, wall.From)));
}

// The squared distance from the point to the segment from a to b.
double squaredToSegment(GroundPoint point, GroundPoint a, GroundPoint b) {
  const GroundPoint along = minus(b, a);
  const double length = squared(along);
  if (length == 0.0) {
    return squared(minus(point, a));
  }
  const double t = std::clamp(dot(minus(point, a), along) / length, 0.0, 1.0);
  return squared(minus(point, plus(a, scaled(along, t))));
}

// The point of the segment from a to b nearest the point.
GroundPoint nearestOnSegment(GroundPoint point, GroundPoint a, GroundPoint b) {
  const GroundPoint along = minus(b, a);
  const double length = squared(along);
  if (length == 0.0) {
    return a;
  }
  const double t = std::clamp(dot(minus(point, a), along) / length, 0.0, 1.0);
  return plus(a, scaled(along, t));
}

// The cross product of b - a and c - a: above 0 when c lies to the left of the line from a to b.
double turn(GroundPoint a, GroundPoint b, GroundPoint c) {
  const GroundPoint ab = minus(b, a);
  const GroundPoint ac = minus(c, a);
  return (ab.X * ac.Z) - (ab.Z * ac.X);
}

// Whether c, known to lie on the line through a and b, lies within their bounding box.
bool within(GroundPoint a, GroundPoint b, GroundPoint c) {
  return c.X >= std::min(a.X, b.X) && c.X <= std::max(a.X, b.X) && c.Z >= std::min(a.Z, b.Z) &&
         c.Z <= std::max(a.Z, b.Z);
}

// Whether the segments from a to b and from c to d meet, ends included.
bool segmentsMeet(GroundPoint a, GroundPoint b, GroundPoint c, GroundPoint d) {
  const double d1 = turn(c, d, a);
  const double d2 = turn(c, d, b);
  const double d3 = turn(a, b, c);
  const double d4 = turn(a, b, d);
  if (((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
      ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0))) {
    return true;
  }
  return (d1 == 0.0 && within(c, d, a)) || (d2 == 0.0 && within(c, d, b)) ||
         (d3 == 0.0 && within(a, b, c)) || (d4 == 0.0 && within(a, b, d));
}

// The squared distance between the segments from a to b and from c to d: 0 when they meet, and
// otherwise the least of the distances from one's ends to the other.
double squaredBetween(GroundPoint a, GroundPoint b, GroundPoint c, GroundPoint d) {
  if (segmentsMeet(a, b, c, d)) {
    return 0.0;
  }
  return std::min({squaredToSegment(a, c, d), squaredToSegment(b, c, d), squaredToSegment(c, a, b),
                   squaredToSegment(d, a, b)});
}

// The wall's copy shifted GUEST_CLEARANCE to its left, toward the paving, or none for a wall of no
// length.
std::optional<std::pair<GroundPoint, GroundPoint>> shiftedWall(const GroundWall &wall) {
  const GroundPoint along = minus(wall.To, wall.From);
  const double length = std::sqrt(squared(along));
  if (length == 0.0) {
    return std::nullopt;
  }
  const GroundPoint shift = scaled(leftOf(along), GUEST_CLEARANCE / length);
  return std::pair{plus(wall.From, shift), plus(wall.To, shift)};
}

// Where the lines through a to b and c to d cross, or none when they are parallel.
std::optional<GroundPoint> linesCross(GroundPoint a, GroundPoint b, GroundPoint c, GroundPoint d) {
  const GroundPoint r = minus(b, a);
  const GroundPoint s = minus(d, c);
  const double denominator = (r.X * s.Z) - (r.Z * s.X);
  if (denominator == 0.0) {
    return std::nullopt;
  }
  const GroundPoint ca = minus(c, a);
  const double t = ((ca.X * s.Z) - (ca.Z * s.X)) / denominator;
  return plus(a, scaled(r, t));
}

ParkPoint parkPoint(GroundPoint point) { return {point.X, point.Z}; }

// The column, or row, of the grid holding the coordinate, the edge one for a coordinate beyond it.
size_t cellOf(double coordinate) {
  const double cell = std::floor((coordinate - GRID_ORIGIN) / WALL_CELL_SIZE);
  return static_cast<size_t>(std::clamp(cell, 0.0, static_cast<double>(GRID - 1)));
}
GroundPoint groundPoint(ParkPoint point) { return {point.X, point.Z}; }

// Calls visit(cell) for each cell, row by row, within the distance of the segment from a to b,
// that is, each cell of the rows the segment's band crosses, between the columns within the
// distance of the part of the segment inside that row. A cell may come more than once only where
// rounding makes rows overlap, never in one row.
template <typename Visit>
void visitCellsNear(GroundPoint a, GroundPoint b, double distance, Visit visit) {
  const size_t firstRow = cellOf(std::min(a.Z, b.Z) - distance);
  const size_t lastRow = cellOf(std::max(a.Z, b.Z) + distance);
  for (size_t row = firstRow; row <= lastRow; ++row) {
    // The edge rows hold everything beyond the grid on their side.
    const double low = row == 0
                           ? -std::numeric_limits<double>::infinity()
                           : GRID_ORIGIN + (static_cast<double>(row) * WALL_CELL_SIZE) - distance;
    const double high =
        row == GRID - 1 ? std::numeric_limits<double>::infinity()
                        : GRID_ORIGIN + (static_cast<double>(row + 1) * WALL_CELL_SIZE) + distance;
    double fromX = 0.0;
    double toX = 0.0;
    if (a.Z == b.Z) {
      fromX = std::min(a.X, b.X);
      toX = std::max(a.X, b.X);
    } else {
      const auto xAt = [&](double z) {
        const double t = std::clamp((z - a.Z) / (b.Z - a.Z), 0.0, 1.0);
        return a.X + ((b.X - a.X) * t);
      };
      const double x1 = xAt(std::max(low, std::min(a.Z, b.Z)));
      const double x2 = xAt(std::min(high, std::max(a.Z, b.Z)));
      fromX = std::min(x1, x2);
      toX = std::max(x1, x2);
    }
    const size_t lastColumn = cellOf(toX + distance);
    for (size_t column = cellOf(fromX - distance); column <= lastColumn; ++column) {
      visit((row * GRID) + column);
    }
  }
}

// Lists each item in every cell its bounding box overlaps, in the items' order, as starts for each
// cell and the items' indices.
template <typename Bounds>
void listByCell(size_t count, Bounds bounds, std::vector<uint32_t> &starts,
                std::vector<uint32_t> &cells) {
  starts.assign((GRID * GRID) + 1, 0);
  const auto visitItem = [&](size_t item, auto visit) {
    const auto [low, high] = bounds(item);
    for (size_t row = cellOf(low.Z); row <= cellOf(high.Z); ++row) {
      for (size_t column = cellOf(low.X); column <= cellOf(high.X); ++column) {
        visit((row * GRID) + column);
      }
    }
  };
  for (size_t item = 0; item < count; ++item) {
    visitItem(item, [&](size_t cell) { ++starts[cell + 1]; });
  }
  for (size_t cell = 0; cell < GRID * GRID; ++cell) {
    starts[cell + 1] += starts[cell];
  }
  cells.assign(starts.back(), 0);
  std::vector<uint32_t> next(starts.begin(), starts.end() - 1);
  for (size_t item = 0; item < count; ++item) {
    visitItem(item, [&](size_t cell) { cells[next[cell]++] = static_cast<uint32_t>(item); });
  }
}

// The groups of joined guest paving: the group of each guest path with a ground line, and of each
// of its points, the lowest-keyed such path ending at each point, and each path's carrier with its
// group.
struct JoinedGroups {
  std::map<EntityKey, uint32_t> Groups;
  std::map<EntityKey, EntityKey> PointCarriers;
  std::vector<CarrierGroup> Carriers;
};

// Joins the guest paths that share points into groups, numbered in the order their first path
// comes, by key.
JoinedGroups joinGuestPaths(const std::vector<ParkSegment> &segments) {
  std::map<EntityKey, EntityKey> parents;
  const auto rootOf = [&parents](EntityKey key) {
    EntityKey root = key;
    while (parents.at(root) != root) {
      root = parents.at(root);
    }
    // Point each key on the way straight at the root, so later finds are short.
    while (parents.at(key) != root) {
      key = std::exchange(parents.at(key), root);
    }
    return root;
  };
  const auto isGuestPath = [](const ParkSegment &segment) {
    return segment.Kind == PathKind::Guest && !segment.Line.empty();
  };
  for (const ParkSegment &segment : segments) {
    if (!isGuestPath(segment)) {
      continue;
    }
    parents.try_emplace(segment.From, segment.From);
    parents.try_emplace(segment.To, segment.To);
    const EntityKey from = rootOf(segment.From);
    const EntityKey to = rootOf(segment.To);
    // The lower root wins, so the result doesn't depend on the order of joins.
    parents.at(std::max(from, to)) = std::min(from, to);
  }
  JoinedGroups joined;
  std::map<EntityKey, uint32_t> rootGroups;
  for (const ParkSegment &segment : segments) {
    if (!isGuestPath(segment)) {
      continue;
    }
    const uint32_t group =
        rootGroups.try_emplace(rootOf(segment.From), static_cast<uint32_t>(rootGroups.size()))
            .first->second;
    joined.Groups[segment.Key] = group;
    joined.Groups[segment.From] = group;
    joined.Groups[segment.To] = group;
    joined.PointCarriers.try_emplace(segment.From, segment.Key);
    joined.PointCarriers.try_emplace(segment.To, segment.Key);
    joined.Carriers.push_back({segment.Key, group});
  }
  return joined;
}

// Joins guest paving into groups and derives the ground guests walk on.
// The segments resolveGuestGround reads, kept from one resolution to the next.
struct GroundSegments {
  std::vector<ParkSegment> Segments;
};

void resolveGuestGround(World &world, TickBuffers &buffers) {
  std::vector<ParkSegment> &segments = buffers.get<GroundSegments>().Segments;
  parkSegments(world, segments, buffers);
  JoinedGroups joined = joinGuestPaths(segments);
  const std::map<EntityKey, uint32_t> &groups = joined.Groups;
  const std::map<EntityKey, EntityKey> &pointCarriers = joined.PointCarriers;
  std::vector<CarrierGroup> carriers = std::move(joined.Carriers);

  const std::vector<KeyedWalkway> keyed = keyedWalkways(world, PathKind::Guest);
  // Backstage paving never joins guest paving, so its walkways hide no guest edge.
  const PavedSurface &surface = walkedSurface(world);

  std::vector<GroundWall> walls;
  std::vector<GroundPiece> pieces;
  std::vector<SurfacePiece> walkwayPieces;
  for (const KeyedWalkway &walkway : keyed) {
    const auto group = groups.find(walkway.Way.Joins);
    const std::optional<SurfacePiece> piece = walkwayPiece(segments, walkway.Way);
    if (group == groups.end() || !piece) {
      continue;
    }
    walkwayPieces.push_back(*piece);
    pieces.push_back({*piece, group->second, walkway.Connector});
    walls.push_back(
        {groundPoint(piece->Corners[3]), groundPoint(piece->Corners[0]), group->second, true});
    carriers.push_back({walkway.Connector, group->second});
  }
  for (const SurfacePiece &piece : surface.Pieces) {
    const auto group = groups.find(piece.Owner);
    if (piece.Kind != PathKind::Guest || group == groups.end() ||
        std::ranges::find(walkwayPieces, piece) != walkwayPieces.end()) {
      continue;
    }
    const auto atPoint = pointCarriers.find(piece.Owner);
    pieces.push_back(
        {piece, group->second, atPoint == pointCarriers.end() ? piece.Owner : atPoint->second});
  }
  for (const EdgeLine &line : surface.Edges) {
    const auto group = groups.find(line.Owner);
    if (line.Kind != PathKind::Guest || group == groups.end()) {
      continue;
    }
    const std::span<const ParkPoint> points = edgePoints(surface, line);
    for (size_t i = 1; i < points.size(); ++i) {
      if (points[i - 1] != points[i]) {
        walls.push_back({groundPoint(points[i - 1]), groundPoint(points[i]), group->second, false});
      }
    }
  }

  const EntityKey key = world.createDerivedEntity(NULL_KEY, GROUND_PURPOSE, 0);
  world.emplaceOrReplace<GuestGround>(
      key, GuestGround(std::move(walls), std::move(pieces), std::move(carriers)));
}

} // namespace

GuestGround::GuestGround(std::vector<GroundWall> walls, std::vector<GroundPiece> pieces,
                         std::vector<CarrierGroup> carriers)
    : Walls(std::move(walls)), Pieces(std::move(pieces)), Carriers(std::move(carriers)) {
  std::ranges::sort(Carriers, {}, &CarrierGroup::Carrier);
  listByCell(
      Walls.size(),
      [this](size_t i) {
        const GroundWall &wall = Walls[i];
        return std::pair{
            GroundPoint{std::min(wall.From.X, wall.To.X), std::min(wall.From.Z, wall.To.Z)},
            GroundPoint{std::max(wall.From.X, wall.To.X), std::max(wall.From.Z, wall.To.Z)}};
      },
      WallStarts, WallCells);
  listByCell(
      Pieces.size(),
      [this](size_t i) {
        const SurfacePiece &piece = Pieces[i].Piece;
        GroundPoint low{std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity()};
        GroundPoint high{-std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()};
        for (const ParkPoint &corner : pieceCorners(piece)) {
          low = {std::min(low.X, corner.X), std::min(low.Z, corner.Z)};
          high = {std::max(high.X, corner.X), std::max(high.Z, corner.Z)};
        }
        return std::pair{low, high};
      },
      PieceStarts, PieceCells);
}

std::optional<uint32_t> GuestGround::groupOf(EntityKey carrier) const {
  const auto found = std::ranges::lower_bound(Carriers, carrier, {}, &CarrierGroup::Carrier);
  if (found == Carriers.end() || found->Carrier != carrier) {
    return std::nullopt;
  }
  return found->Group;
}

bool GuestGround::clearLine(uint32_t group, GroundPoint from, GroundPoint to) const {
  if (WallStarts.empty()) {
    return true;
  }
  bool clear = true;
  visitCellsNear(from, to, GUEST_CLEARANCE, [&](size_t cell) {
    for (uint32_t i = WallStarts[cell]; clear && i < WallStarts[cell + 1]; ++i) {
      const GroundWall &wall = Walls[WallCells[i]];
      if (wall.Group != group) {
        continue;
      }
      const double between = squaredBetween(from, to, wall.From, wall.To);
      if (between == 0.0 && sideOf(wall, to) < 0.0) {
        clear = false;
        break;
      }
      const double required =
          std::min(GUEST_CLEARANCE, std::sqrt(squaredToSegment(from, wall.From, wall.To))) -
          CLEAR_ROUNDING;
      if (required > 0.0 && between < required * required) {
        clear = false;
      }
    }
  });
  return clear;
}

bool GuestGround::clearOfWalls(uint32_t group, GroundPoint point, bool doorEnds) const {
  if (WallStarts.empty()) {
    return true;
  }
  const double least = GUEST_CLEARANCE - FIT_ROUNDING;
  bool clear = true;
  visitCellsNear(point, point, GUEST_CLEARANCE, [&](size_t cell) {
    for (uint32_t i = WallStarts[cell]; clear && i < WallStarts[cell + 1]; ++i) {
      const GroundWall &wall = Walls[WallCells[i]];
      if (wall.Group == group && (doorEnds || !wall.DoorEnd) &&
          squaredToSegment(point, wall.From, wall.To) < least * least) {
        clear = false;
      }
    }
  });
  return clear;
}

bool GuestGround::fits(uint32_t group, GroundPoint point, bool doorEnds) const {
  const GroundPiece *piece = pieceAt(point, group);
  return piece != nullptr && piece->Group == group && clearOfWalls(group, point, doorEnds);
}

const GroundPiece *GuestGround::pieceAt(GroundPoint point, std::optional<uint32_t> group) const {
  if (PieceStarts.empty()) {
    return nullptr;
  }
  const size_t cell = (cellOf(point.Z) * GRID) + cellOf(point.X);
  const GroundPiece *any = nullptr;
  for (uint32_t i = PieceStarts[cell]; i < PieceStarts[cell + 1]; ++i) {
    const GroundPiece &piece = Pieces[PieceCells[i]];
    if (pieceDistance(piece.Piece, parkPoint(point)) != 0.0) {
      continue;
    }
    if (!group || piece.Group == *group) {
      return &piece;
    }
    if (any == nullptr) {
      any = &piece;
    }
  }
  return any;
}

template <typename Consider>
void GuestGround::wallCandidates(GroundPoint point, size_t cell, uint32_t listed,
                                 Consider consider) const {
  const GroundWall &wall = Walls[WallCells[listed]];
  const auto shifted = shiftedWall(wall);
  if (!shifted) {
    return;
  }
  consider(nearestOnSegment(point, shifted->first, shifted->second), wall.Group);
  for (const GroundPoint end : {wall.From, wall.To}) {
    const double away = std::sqrt(squared(minus(point, end)));
    if (away > 0.0) {
      consider(plus(end, scaled(minus(point, end), GUEST_CLEARANCE / away)), wall.Group);
    }
  }
  for (uint32_t j = WallStarts[cell]; j < WallStarts[cell + 1]; ++j) {
    const GroundWall &other = Walls[WallCells[j]];
    if (j == listed || other.Group != wall.Group) {
      continue;
    }
    if (const auto otherShifted = shiftedWall(other)) {
      if (const auto cross = linesCross(shifted->first, shifted->second, otherShifted->first,
                                        otherShifted->second)) {
        consider(*cross, wall.Group);
      }
    }
  }
}

std::optional<GroundSpot> GuestGround::nearestFit(GroundPoint point) const {
  if (WallStarts.empty()) {
    return std::nullopt;
  }
  std::optional<GroundSpot> best;
  double bestSquared = std::numeric_limits<double>::infinity();
  const auto consider = [&](GroundPoint candidate, uint32_t group) {
    const double distance = squared(minus(candidate, point));
    if (distance < bestSquared && fits(group, candidate, true)) {
      best = GroundSpot{candidate, group};
      bestSquared = distance;
    }
  };
  const auto ownRow = static_cast<ptrdiff_t>(cellOf(point.Z));
  const auto ownColumn = static_cast<ptrdiff_t>(cellOf(point.X));
  const auto grid = static_cast<ptrdiff_t>(GRID);
  for (ptrdiff_t ring = 0; ring < grid; ++ring) {
    for (ptrdiff_t row = ownRow - ring; row <= ownRow + ring; ++row) {
      for (ptrdiff_t column = ownColumn - ring; column <= ownColumn + ring; ++column) {
        const bool onRing = row == ownRow - ring || row == ownRow + ring ||
                            column == ownColumn - ring || column == ownColumn + ring;
        if (!onRing || row < 0 || row >= grid || column < 0 || column >= grid) {
          continue;
        }
        const auto cell = static_cast<size_t>((row * grid) + column);
        for (uint32_t i = WallStarts[cell]; i < WallStarts[cell + 1]; ++i) {
          wallCandidates(point, cell, i, consider);
        }
      }
    }
    // A candidate lies at most GUEST_CLEARANCE nearer than the walls it comes from, and every wall
    // listed only in later rings lies at least this ring's width away.
    if (best &&
        std::sqrt(bestSquared) <= (static_cast<double>(ring) * WALL_CELL_SIZE) - GUEST_CLEARANCE) {
      break;
    }
  }
  return best;
}

const GuestGround &guestGround(const World &world) {
  static const GuestGround EMPTY;
  const GuestGround *ground = world.tryGet<GuestGround>(deriveKey(NULL_KEY, GROUND_PURPOSE, 0));
  return ground != nullptr ? *ground : EMPTY;
}

void addGuestGround(WorldSchema &schema) {
  schema.addComponent<GuestGround>("guest-ground", DataKind::Derived);
  schema.addResolver("guest-ground", &resolveGuestGround, {"walked-surface"});
}

} // namespace tpj
