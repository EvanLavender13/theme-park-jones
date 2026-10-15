#include "render/park_mesh.h"

#include "sim/command_queue.h"
#include "sim/furniture/furniture.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <math.h>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace tpj {
namespace {

void addVertex(ParkMesh &mesh, double x, float y, double z, ParkPoint normal, float normalY,
               Rgba color) {
  ParkVertex vertex;
  vertex.Position[0] = static_cast<float>(x);
  vertex.Position[1] = y;
  vertex.Position[2] = static_cast<float>(z);
  vertex.Normal[0] = static_cast<float>(normal.X);
  vertex.Normal[1] = normalY;
  vertex.Normal[2] = static_cast<float>(normal.Z);
  vertex.Color = color;
  mesh.Vertices.push_back(vertex);
}

// The unit direction from one ground line point to the next. Ground line points are distinct.
ParkPoint unitStep(const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double length = sqrt(dx * dx + dz * dz);
  return {dx / length, dz / length};
}

// The ribbon's direction at point i: its segment's at the ends, and the two segments' averaged
// between them.
ParkPoint tangentAt(const std::vector<CarrierPoint> &line, size_t i) {
  if (i == 0) {
    return unitStep(line[0], line[1]);
  }
  const ParkPoint before = unitStep(line[i - 1], line[i]);
  if (i + 1 == line.size()) {
    return before;
  }
  const ParkPoint after = unitStep(line[i], line[i + 1]);
  const ParkPoint sum{before.X + after.X, before.Z + after.Z};
  const double length = sqrt(sum.X * sum.X + sum.Z * sum.Z);
  if (length > 0.0) {
    return {sum.X / length, sum.Z / length};
  }
  return before;
}

// Adds one face: four vertices given counter-clockwise seen from the normal's side, and two
// triangles.
void addQuad(ParkMesh &mesh, const std::array<ParkPoint, 4> &ground,
             const std::array<float, 4> &heights, ParkPoint normal, float normalY, Rgba color) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (size_t k = 0; k < 4; ++k) {
    addVertex(mesh, ground[k].X, heights[k], ground[k].Z, normal, normalY, color);
  }
  mesh.Indices.insert(mesh.Indices.end(),
                      {first, first + 1, first + 2, first, first + 2, first + 3});
}

// Adds a block over the footprint from the base up to the top: its top, then its four sides, the
// front one lightened, and, when closed, its bottom.
void appendBlock(ParkMesh &mesh, const Footprint &footprint, float base, float top, bool closed,
                 Rgba color) {
  const auto &[frontLeft, frontRight, backRight, backLeft] = footprint.Corners;
  const ParkPoint forward = footprint.Forward;
  const ParkPoint right = footprint.Right;
  const std::array<float, 4> sideHeights{base, base, top, top};

  addQuad(mesh, {frontLeft, backLeft, backRight, frontRight}, {top, top, top, top}, {}, 1.0f,
          color);
  // Each side runs along its edge counter-clockwise seen from above, then rises back over it.
  addQuad(mesh, {frontLeft, backLeft, backLeft, frontLeft}, sideHeights, {-right.X, -right.Z}, 0.0f,
          color);
  addQuad(mesh, {backLeft, backRight, backRight, backLeft}, sideHeights, {-forward.X, -forward.Z},
          0.0f, color);
  addQuad(mesh, {backRight, frontRight, frontRight, backRight}, sideHeights, right, 0.0f, color);
  addQuad(mesh, {frontRight, frontLeft, frontLeft, frontRight}, sideHeights, forward, 0.0f,
          lightened(color));
  if (closed) {
    // The top's corners in reverse, counter-clockwise seen from below.
    addQuad(mesh, {frontLeft, frontRight, backRight, backLeft}, {base, base, base, base}, {}, -1.0f,
            color);
  }
}

// An edit's ghost color for a kind's color: translucent when accepted, the invalid tint when not.
Rgba ghostColor(bool accepted, Rgba color) {
  return accepted ? Rgba{color.R, color.G, color.B, GHOST_ALPHA} : INVALID_TINT;
}

// Adds a flat band along the line, from one offset across it to another, each measured to the
// right of the line's direction, the lift above the ground. The line has at least two points, each
// distinct from the next.
void appendBand(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double from, double to,
                float lift, Rgba color) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (size_t i = 0; i < line.size(); ++i) {
    const ParkPoint tangent = tangentAt(line, i);
    const ParkPoint right{-tangent.Z, tangent.X};
    addVertex(mesh, line[i].X + right.X * from, lift, line[i].Z + right.Z * from, {}, 1.0f, color);
    addVertex(mesh, line[i].X + right.X * to, lift, line[i].Z + right.Z * to, {}, 1.0f, color);
  }
  for (uint32_t i = 0; i + 1 < line.size(); ++i) {
    const uint32_t left0 = first + 2 * i;
    const uint32_t right0 = left0 + 1;
    const uint32_t left1 = left0 + 2;
    const uint32_t right1 = left0 + 3;
    mesh.Indices.insert(mesh.Indices.end(), {left0, right0, left1, left1, right0, right1});
  }
}

// Adds the flat ribbon of the width along the line, the lift above the ground. The line has at
// least two points, each distinct from the next.
void appendRibbon(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width, float lift,
                  Rgba color) {
  appendBand(mesh, line, -0.5 * width, 0.5 * width, lift, color);
}

// Adds a flat half disc of the radius beyond a ribbon's end at the point, the lift above the
// ground, for the unit direction pointing away from the ribbon there: its center, then
// JOINT_SEGMENTS + 1 vertices on its rim, from the ribbon's corner on the direction's right around
// to the one on its left, and a triangle facing up from the center to each rim vertex and the next.
void appendJoint(ParkMesh &mesh, const CarrierPoint &point, ParkPoint direction, double radius,
                 float lift, Rgba color) {
  const ParkPoint right{-direction.Z, direction.X};
  const auto center = static_cast<uint32_t>(mesh.Vertices.size());
  addVertex(mesh, point.X, lift, point.Z, {}, 1.0f, color);
  for (uint32_t k = 0; k <= JOINT_SEGMENTS; ++k) {
    const double angle = std::numbers::pi * static_cast<double>(k) / JOINT_SEGMENTS;
    const double across = radius * cos(angle);
    const double along = radius * sin(angle);
    addVertex(mesh, point.X + across * right.X + along * direction.X, lift,
              point.Z + across * right.Z + along * direction.Z, {}, 1.0f, color);
  }
  for (uint32_t k = 0; k < JOINT_SEGMENTS; ++k) {
    mesh.Indices.insert(mesh.Indices.end(), {center, center + 1 + k, center + 2 + k});
  }
}

// Slides a ribbon's first left and right vertices, at first and first + 1, along its first segment
// onto the line through its first point across the face's normal, so the ribbon starts flush with
// the face. Leaves them where they are when the segment runs along the face, or when the slide
// would reach the segment's end and fold the ribbon.
void startFlush(ParkMesh &mesh, size_t first, const std::vector<CarrierPoint> &line, double half,
                ParkPoint normal) {
  const ParkPoint tangent = unitStep(line[0], line[1]);
  const ParkPoint right{-tangent.Z, tangent.X};
  const double along = tangent.X * normal.X + tangent.Z * normal.Z;
  if (along == 0.0) {
    return;
  }
  const double move = half * (right.X * normal.X + right.Z * normal.Z) / along;
  const double length = hypot(line[1].X - line[0].X, line[1].Z - line[0].Z);
  if (!(fabs(move) < length)) {
    return;
  }
  for (const auto &[index, sign] : {std::pair{first, 1.0}, std::pair{first + 1, -1.0}}) {
    ParkVertex &vertex = mesh.Vertices[index];
    vertex.Position[0] =
        static_cast<float>(line[0].X - sign * right.X * half + sign * tangent.X * move);
    vertex.Position[2] =
        static_cast<float>(line[0].Z - sign * right.Z * half + sign * tangent.Z * move);
  }
}

// Adds a segment's ribbon and round joints along the line, the width wide, the lift above the
// ground. The line has at least two points.
void appendSegmentAt(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width,
                     float lift, Rgba color) {
  appendRibbon(mesh, line, width, lift, color);
  appendJoint(mesh, line.front(), unitStep(line[1], line[0]), 0.5 * width, lift, color);
  appendJoint(mesh, line.back(), unitStep(line[line.size() - 2], line.back()), 0.5 * width, lift,
              color);
}

// Adds a flat disc of the radius centered on the point, the lift above the ground.
void appendDisc(ParkMesh &mesh, ParkPoint at, double radius, float lift, Rgba color) {
  const CarrierPoint center{at.X, at.Z, 0.0};
  appendJoint(mesh, center, {1.0, 0.0}, radius, lift, color);
  appendJoint(mesh, center, {-1.0, 0.0}, radius, lift, color);
}

// Adds a flat triangle facing up, the lift above the ground.
void addFlatTriangle(ParkMesh &mesh, ParkPoint a, ParkPoint b, ParkPoint c, float lift,
                     Rgba color) {
  // Counter-clockwise seen from above when b lies to the left of the way from a to c.
  const double turn = (b.Z - a.Z) * (c.X - a.X) - (b.X - a.X) * (c.Z - a.Z);
  if (turn < 0.0) {
    std::swap(b, c);
  }
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (const ParkPoint &corner : {a, b, c}) {
    addVertex(mesh, corner.X, lift, corner.Z, {}, 1.0f, color);
  }
  mesh.Indices.insert(mesh.Indices.end(), {first, first + 1, first + 2});
}

// Adds a strip EDGE_LINE_WIDTH wide along the left of the edge line, the lift above the ground: a
// quad along each step, and where the line turns right, a triangle that closes the gap between two
// quads.
void appendEdgeLine(ParkMesh &mesh, std::span<const ParkPoint> points, float lift, Rgba color) {
  std::optional<ParkPoint> previous;
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    const double dx = points[i + 1].X - points[i].X;
    const double dz = points[i + 1].Z - points[i].Z;
    const double length = std::sqrt(dx * dx + dz * dz);
    if (length == 0.0) {
      continue;
    }
    const ParkPoint left{dz / length * EDGE_LINE_WIDTH, -dx / length * EDGE_LINE_WIDTH};
    const ParkPoint from = points[i];
    const ParkPoint to = points[i + 1];
    if (previous && previous->Z * left.X - previous->X * left.Z < 0.0) {
      addFlatTriangle(mesh, from, {from.X + previous->X, from.Z + previous->Z},
                      {from.X + left.X, from.Z + left.Z}, lift, color);
    }
    addQuad(mesh,
            {from, to, ParkPoint{to.X + left.X, to.Z + left.Z},
             ParkPoint{from.X + left.X, from.Z + left.Z}},
            {lift, lift, lift, lift}, {}, 1.0f, color);
    previous = left;
  }
}

// Adds the piece, the lift above its kind's paths, in the color.
void appendPiece(ParkMesh &mesh, const SurfacePiece &piece, float lift, Rgba color) {
  const float height = pathLift(piece.Kind) + lift;
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (const ParkPoint &corner : pieceCorners(piece)) {
    addVertex(mesh, corner.X, height, corner.Z, {}, 1.0f, color);
  }
  for (uint32_t k = 1; k + 1 < piece.Count; ++k) {
    mesh.Indices.insert(mesh.Indices.end(), {first, first + k, first + k + 1});
  }
}

// Adds each piece of the surface, the lift above its kind's paths, in the color colorOf gives for
// its kind, and then each edge line EDGE_LIFT higher in that color's edgeColor.
template <typename ColorOf>
void appendSurface(ParkMesh &mesh, const PavedSurface &surface, float lift, ColorOf colorOf) {
  for (const auto &[owner, paving] : surface.Owners) {
    for (const SurfacePiece &piece : paving.Pieces) {
      appendPiece(mesh, piece, lift, colorOf(piece.Kind));
    }
  }
  for (const auto &[owner, paving] : surface.Owners) {
    for (const EdgeLine &edge : paving.Edges) {
      appendEdgeLine(mesh, edgePoints(paving, edge), pathLift(edge.Kind) + lift + EDGE_LIFT,
                     edgeColor(colorOf(edge.Kind)));
    }
  }
}

// Adds a segment's ribbon and round joints along the line, GHOST_LIFT above its kind's paths.
void appendGhostSegment(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &line,
                        double width, Rgba color) {
  if (line.size() >= 2) {
    appendSegmentAt(mesh, line, width, pathLift(kind) + GHOST_LIFT, color);
  }
}

// Puts the path's stops' positions, as stopPositions gives them, in lists.Positions, reading the
// corners only for a path with an OnBend stop. False where stopPositions gives none.
bool keptStopPositions(const World &world, const AddPath &path, GhostLists &lists) {
  lists.Corners.clear();
  if (std::ranges::any_of(
          path.Stops, [](const PathStop &stop) { return std::holds_alternative<OnBend>(stop); })) {
    parkCorners(world, lists.Corners, lists.Buffers);
  }
  lists.Positions.clear();
  return std::ranges::all_of(path.Stops, [&](const PathStop &stop) {
    return appendStopPosition(world, lists.Corners, stop, lists.Positions);
  });
}

// Adds an added path's straight segments between its stops, for a path the world refuses so that
// editedSegments gives none. Nothing when a stop has no position.
void appendStraightPath(ParkMesh &mesh, const World &world, const AddPath &path, Rgba color,
                        GhostLists &lists) {
  if (!keptStopPositions(world, path, lists)) {
    return;
  }
  for (size_t i = 0; i + 1 < lists.Positions.size(); ++i) {
    segmentLine(lists.Positions[i], lists.Positions[i + 1], lists.Line);
    appendGhostSegment(mesh, path.Kind, lists.Line, pathWidth(path.Kind), color);
  }
}

// Whether the piece the key holds is missing from the other furniture, or differs there in its kind
// or its pose.
bool isChangedIn(const std::vector<FurniturePiece> &other, const FurniturePiece &piece) {
  const auto found = std::ranges::lower_bound(other, piece.Key, {}, &FurniturePiece::Key);
  return found == other.end() || found->Key != piece.Key || found->Kind != piece.Kind ||
         !(found->At == piece.At);
}

// Adds to the ghost each piece of the candidate's furniture that the world lacks, or holds with
// another kind or pose, translucent in its kind's color, and each piece of the world's furniture
// that the candidate lacks, or holds with another kind or pose, in the delete tint.
void appendFurnitureChanges(GhostMesh &ghost, const World &world, const World &candidate,
                            GhostLists &lists) {
  std::vector<FurniturePiece> &was = lists.Was;
  std::vector<FurniturePiece> &is = lists.Is;
  parkFurniture(world, was);
  parkFurniture(candidate, is);
  for (const FurniturePiece &piece : is) {
    if (isChangedIn(was, piece)) {
      appendBox(ghost.Lit, piece.At, furnitureSize(piece.Kind), furnitureHeight(piece.Kind),
                ghostColor(true, furnitureColor(piece.Kind)));
    }
  }
  for (const FurniturePiece &piece : was) {
    if (isChangedIn(is, piece)) {
      appendBox(ghost.Tints, piece.At, furnitureSize(piece.Kind), furnitureHeight(piece.Kind),
                DELETE_TINT);
    }
  }
}

// Adds, in the delete tint, the box or segment a deletion removes, or both segments at a point
// RemovePoint merges.
void appendDeleted(ParkMesh &mesh, const World &world, const ParkEdit &edit, GhostLists &lists) {
  parkBoxes(world, lists.Boxes);
  parkPathPoints(world, lists.Points);
  if (const auto *remove = std::get_if<RemovePoint>(&edit)) {
    parkSegments(world, lists.Segments, lists.Buffers);
    for (const ParkSegment &segment : lists.Segments) {
      if (segment.From == remove->Point || segment.To == remove->Point) {
        appendEntity(mesh, world, lists.Boxes, lists.Points, segment.Key, DELETE_TINT);
      }
    }
  } else if (const auto *segment = std::get_if<DeleteSegment>(&edit)) {
    appendEntity(mesh, world, lists.Boxes, lists.Points, segment->Segment, DELETE_TINT);
  } else if (const auto *box = std::get_if<DeleteBox>(&edit)) {
    appendEntity(mesh, world, lists.Boxes, lists.Points, box->Box, DELETE_TINT);
  }
}

// The position of the place a moved point's target or an inserted point names, as stopPosition
// gives it, reading the corners only for an OnBend into lists.Corners. None where it gives none.
std::optional<ParkPoint> keptPlace(const World &world, const PathStop &place, GhostLists &lists) {
  lists.Corners.clear();
  if (std::holds_alternative<OnBend>(place)) {
    parkCorners(world, lists.Corners, lists.Buffers);
  }
  lists.Positions.clear();
  if (!appendStopPosition(world, lists.Corners, place, lists.Positions)) {
    return std::nullopt;
  }
  return lists.Positions.back();
}

// Adds a join mark at each stop where an added path joins a point, a segment, or an earlier stop,
// at a moved point's target when it is a point or segment, at an inserted point's place, and at
// each place pathCrossings gives for the plan.
void appendJoinMarks(ParkMesh &mesh, const World &world, const ParkEdit &edit,
                     const PlannedEdit &planned, GhostLists &lists) {
  if (const auto *path = std::get_if<AddPath>(&edit)) {
    if (keptStopPositions(world, *path, lists)) {
      for (size_t i = 0; i < lists.Positions.size(); ++i) {
        if (!std::holds_alternative<NewPoint>(path->Stops[i])) {
          appendJoinMark(mesh, lists.Positions[i]);
        }
      }
    }
    for (const ParkPoint &at : pathCrossings(planned)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *move = std::get_if<MovePoint>(&edit)) {
    if (!std::holds_alternative<NewPoint>(move->To)) {
      const PathStop target = std::visit([](const auto &stop) { return PathStop{stop}; }, move->To);
      if (const std::optional<ParkPoint> at = keptPlace(world, target, lists)) {
        appendJoinMark(mesh, *at);
      }
    }
    for (const ParkPoint &at : pathCrossings(planned)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *insert = std::get_if<InsertPoint>(&edit)) {
    const PathStop place = std::visit([](const auto &at) { return PathStop{at}; }, insert->At);
    if (const std::optional<ParkPoint> at = keptPlace(world, place, lists)) {
      appendJoinMark(mesh, *at);
    }
  } else if (std::holds_alternative<RemovePoint>(edit)) {
    for (const ParkPoint &at : pathCrossings(planned)) {
      appendJoinMark(mesh, at);
    }
  }
}

} // namespace

Rgba lightened(Rgba color) {
  return {color.R + (1.0f - color.R) * 0.4f, color.G + (1.0f - color.G) * 0.4f,
          color.B + (1.0f - color.B) * 0.4f, color.A};
}

Rgba edgeColor(Rgba color) { return {color.R * 0.6f, color.G * 0.6f, color.B * 0.6f, color.A}; }

void appendPathStart(ParkMesh &mesh, PathKind kind, ParkPoint at, bool joins) {
  const CarrierPoint center{at.X, at.Z, 0.0};
  const Rgba base = pathColor(kind);
  const Rgba color{base.R, base.G, base.B, GHOST_ALPHA};
  const double radius = 0.5 * pathWidth(kind);
  const float lift = pathLift(kind) + GHOST_LIFT;
  appendJoint(mesh, center, {1.0, 0.0}, radius, lift, color);
  appendJoint(mesh, center, {-1.0, 0.0}, radius, lift, color);
  if (joins) {
    appendJoinMark(mesh, at);
  }
}

void appendJoinMark(ParkMesh &mesh, ParkPoint at) {
  appendDisc(mesh, at, JOIN_MARK_RADIUS, JOIN_MARK_LIFT, JOIN_MARK_COLOR);
}

void appendFinishMark(ParkMesh &mesh, ParkPoint at) {
  appendDisc(mesh, at, FINISH_MARK_RADIUS, FINISH_MARK_LIFT, FINISH_MARK_COLOR);
}

void appendHandle(ParkMesh &mesh, ParkPoint at, Rgba color) {
  appendDisc(mesh, at, RADIUS_HANDLE_SIZE, HANDLE_LIFT, color);
}

void appendPointMark(ParkMesh &mesh, ParkPoint at, Rgba color) {
  appendDisc(mesh, at, POINT_MARK_SIZE, POINT_MARK_LIFT, color);
}

void appendSides(ParkMesh &mesh, const ParkSegment &segment, Rgba color) {
  if (segment.Line.size() < 2) {
    return;
  }
  const double half = 0.5 * segment.Width;
  const double inset = std::min(SIDE_MARK_WIDTH, half);
  const float lift = pathLift(segment.Kind) + 2.0f * GHOST_LIFT;
  appendBand(mesh, segment.Line, -half, -half + inset, lift, color);
  appendBand(mesh, segment.Line, half - inset, half, lift, color);
}

void appendSides(ParkMesh &mesh, std::span<const ParkSegment> segments, EntityKey segment,
                 Rgba color) {
  for (const ParkSegment &candidate : segments) {
    if (candidate.Key == segment) {
      appendSides(mesh, candidate, color);
    }
  }
}

void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   double width, Rgba color) {
  appendWalkway(mesh, kind, points, width, std::nullopt, color);
}

void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   double width, std::optional<ParkPoint> faceNormal, Rgba color) {
  if (points.size() < 2) {
    return;
  }
  const auto first = mesh.Vertices.size();
  appendRibbon(mesh, points, width, pathLift(kind), color);
  if (faceNormal) {
    startFlush(mesh, first, points, 0.5 * width, *faceNormal);
  }
  appendJoint(mesh, points.back(), unitStep(points[points.size() - 2], points.back()), 0.5 * width,
              pathLift(kind), color);
}

std::vector<Walkway> parkWalkways(const World &world) {
  std::vector<Walkway> walkways = connectorWalkways(world, PathKind::Guest);
  for (const Walkway &walkway : connectorWalkways(world, PathKind::Backstage)) {
    walkways.push_back(walkway);
  }
  return walkways;
}

void appendWalkways(ParkMesh &mesh, const World &world, float alpha, std::vector<Walkway> &walkways,
                    std::vector<CarrierPoint> &strip, TickBuffers &buffers) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const Rgba base = pathColor(kind);
    const Rgba color{base.R, base.G, base.B, alpha};
    walkways.clear();
    connectorWalkways(world, kind, walkways, buffers);
    // Each strip runs where the park draws the walkway, not along the connector's turn inside
    // the paving.
    for (const Walkway &walkway : walkways) {
      const double length =
          std::hypot(walkway.Meets.X - walkway.Door.X, walkway.Meets.Z - walkway.Door.Z);
      strip.clear();
      strip.push_back({.X = walkway.Door.X, .Z = walkway.Door.Z, .Distance = 0.0});
      strip.push_back({.X = walkway.Meets.X, .Z = walkway.Meets.Z, .Distance = length});
      const size_t first = mesh.Vertices.size();
      appendWalkway(mesh, kind, strip, walkway.Width, walkway.Out, color);
      // Lifted as ghost segments are, so the strip never flickers against the paving it meets.
      for (size_t i = first; i < mesh.Vertices.size(); ++i) {
        mesh.Vertices[i].Position[1] += GHOST_LIFT;
      }
    }
  }
}

void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color) {
  if (const std::optional<Footprint> footprint = footprintOf(pose, size)) {
    appendBlock(mesh, *footprint, 0.0f, height, false, color);
  }
}

void appendStarvedMark(ParkMesh &mesh, const Pose &pose, Rgba color) {
  if (const std::optional<Footprint> footprint =
          footprintOf(pose, FootprintSize{STARVED_MARK_SIZE, STARVED_MARK_SIZE})) {
    appendBlock(mesh, *footprint, STARVED_MARK_BASE, STARVED_MARK_BASE + STARVED_MARK_SIZE, true,
                color);
  }
}

void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha,
                        std::vector<ParkBox> &boxes, TickBuffers &buffers) {
  parkBoxes(world, boxes);
  for (const ParkBox &box : boxes) {
    // A shop is starved when it has no supply route, as its record says.
    if (box.Kind == BoxKind::Shop && !nearestDepot(world, box.Key, buffers)) {
      appendStarvedMark(mesh, box.At,
                        Rgba{STARVED_COLOR.R, STARVED_COLOR.G, STARVED_COLOR.B, alpha});
    }
  }
}

void buildParkMesh(const World &world, ParkMeshLists &lists) {
  ParkMesh &mesh = lists.Mesh;
  mesh.Vertices.clear();
  mesh.Indices.clear();
  parkEntrances(world, lists.Entrances);
  for (const ParkEntrance &entrance : lists.Entrances) {
    appendBox(mesh, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR);
  }
  appendSurface(mesh, walkedSurface(world), 0.0f, pathColor);
  parkBoxes(world, lists.Boxes);
  for (const ParkBox &box : lists.Boxes) {
    appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind));
  }
  parkFurniture(world, lists.Furniture);
  for (const FurniturePiece &piece : lists.Furniture) {
    appendBox(mesh, piece.At, furnitureSize(piece.Kind), furnitureHeight(piece.Kind),
              furnitureColor(piece.Kind));
  }
  // As appendStarvedMarks draws them, reading the supply routes through the kept lists.
  for (const ParkBox &box : lists.Boxes) {
    if (box.Kind == BoxKind::Shop && !nearestDepot(world, box.Key, lists.Buffers)) {
      appendStarvedMark(mesh, box.At, STARVED_COLOR);
    }
  }
}

void appendFurniture(ParkMesh &mesh, const World &world, float alpha) {
  for (const FurniturePiece &piece : parkFurniture(world)) {
    const Rgba base = furnitureColor(piece.Kind);
    appendBox(mesh, piece.At, furnitureSize(piece.Kind), furnitureHeight(piece.Kind),
              Rgba{base.R, base.G, base.B, alpha});
  }
}

std::optional<GroundBounds> meshBounds(const ParkMesh &mesh) {
  if (mesh.Vertices.empty()) {
    return std::nullopt;
  }
  GroundBounds bounds{mesh.Vertices[0].Position[0], mesh.Vertices[0].Position[2],
                      mesh.Vertices[0].Position[0], mesh.Vertices[0].Position[2]};
  for (const ParkVertex &vertex : mesh.Vertices) {
    bounds.MinX = std::min(bounds.MinX, vertex.Position[0]);
    bounds.MaxX = std::max(bounds.MaxX, vertex.Position[0]);
    bounds.MinZ = std::min(bounds.MinZ, vertex.Position[2]);
    bounds.MaxZ = std::max(bounds.MaxZ, vertex.Position[2]);
  }
  return bounds;
}

void appendMesh(ParkMesh &mesh, const ParkMesh &more) {
  const auto offset = static_cast<uint32_t>(mesh.Vertices.size());
  mesh.Vertices.insert(mesh.Vertices.end(), more.Vertices.begin(), more.Vertices.end());
  for (const uint32_t index : more.Indices) {
    mesh.Indices.push_back(offset + index);
  }
}

void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color) {
  appendEntity(mesh, world, parkBoxes(world), parkPathPoints(world), key, color);
}

void appendEntity(ParkMesh &mesh, const World &world, std::span<const ParkBox> boxes,
                  std::span<const ParkPathPoint> points, EntityKey key, Rgba color) {
  for (const ParkBox &box : boxes) {
    if (box.Key == key) {
      appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), color);
      return;
    }
  }
  const OwnerPaving *paving = pavingOf(resolvedSurface(world), key);
  if (paving != nullptr && !paving->Pieces.empty()) {
    for (const SurfacePiece &piece : paving->Pieces) {
      appendPiece(mesh, piece, GHOST_LIFT, color);
    }
    return;
  }
  for (const ParkPathPoint &point : points) {
    if (point.Key == key) {
      appendDisc(mesh, point.At, JOIN_MARK_RADIUS, JOIN_MARK_LIFT, color);
      return;
    }
  }
}

namespace {

// Puts in own the pieces and edge lines of the surface whose owners are among the keys, which are
// sorted. Empties own first, keeping its owners' memory for the owners it adds.
void ownedBy(const PavedSurface &surface, const std::vector<EntityKey> &owners, PavedSurface &own) {
  own.Owners.eraseIf([](const OwnerPaving & /*paving*/) { return true; });
  for (const EntityKey owner : owners) {
    const OwnerPaving *paving = pavingOf(surface, owner);
    if (paving == nullptr || (paving->Pieces.empty() && paving->Edges.empty())) {
      continue;
    }
    OwnerPaving &into = own.Owners.getOrAdd(owner).first;
    into.Pieces.assign(paving->Pieces.begin(), paving->Pieces.end());
    into.Edges.assign(paving->Edges.begin(), paving->Edges.end());
    into.EdgePoints.assign(paving->EdgePoints.begin(), paving->EdgePoints.end());
    into.Sides.clear();
    into.SidePoints.clear();
  }
}

// Puts the surface's owners, sorted, in owners, clearing it first.
void ownersOf(const PavedSurface &surface, std::vector<EntityKey> &owners) {
  owners.clear();
  for (const auto &[owner, paving] : surface.Owners) {
    owners.push_back(owner);
  }
}

// Whether any owner of the surface holds a piece.
bool hasPieces(const PavedSurface &surface) {
  return std::ranges::any_of(surface.Owners,
                             [](const auto &owned) { return !owned.second.Pieces.empty(); });
}

// Puts in lists.Owned the pieces and edge lines that belong to the edited segments in lists.Parts:
// from the candidate's surface with its walkways when there is one, so its walkways join their
// segments as the committed park's do, and from the edited surface otherwise.
void reshapedBy(const std::optional<World> &candidate, GhostLists &lists) {
  std::vector<EntityKey> &reshaped = lists.Owners;
  reshaped.clear();
  for (const ParkSegment &segment : lists.Parts.Segments) {
    reshaped.push_back(segment.Key);
  }
  std::ranges::sort(reshaped);
  if (candidate) {
    ownedBy(walkedSurface(*candidate), reshaped, lists.Owned);
    return;
  }
  ownedBy(lists.Parts.Surface, reshaped, lists.Owned);
}

// Adds the candidate's walkways and starved marks, and the furniture the edit changes.
void appendCandidate(GhostMesh &ghost, const World &world, const World &candidate,
                     GhostLists &lists) {
  appendWalkways(ghost.Lit, candidate, GHOST_ALPHA, lists.Walkways, lists.Line, lists.Buffers);
  appendStarvedMarks(ghost.Lit, candidate, GHOST_ALPHA, lists.Boxes, lists.Buffers);
  appendFurnitureChanges(ghost, world, candidate, lists);
}

// Adds the box an AddBox adds or a MoveBox moves, in its kind's color as the ghost shows it.
void appendBoxGhost(ParkMesh &shape, const World &world, const ParkEdit &edit, bool accepted,
                    GhostLists &lists) {
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(shape, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(accepted, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    parkBoxes(world, lists.Boxes);
    for (const ParkBox &box : lists.Boxes) {
      if (box.Key == move->Box) {
        appendBox(shape, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(accepted, boxColor(box.Kind)));
      }
    }
  }
}

} // namespace

void buildGhostMesh(const World &world, const ParkEdit &edit, const PlannedEdit &planned,
                    const std::optional<World> &candidate, GhostLists &lists) {
  GhostMesh &ghost = lists.Ghost;
  for (ParkMesh *part : {&ghost.Tints, &ghost.Lit, &ghost.Marks}) {
    part->Vertices.clear();
    part->Indices.clear();
  }
  const bool accepted = isAccepted(planned);
  editedParts(world, planned, lists.Parts, lists.Buffers);
  // What the edit adds or changes previews the park, lit as it is, unless the world refuses it and
  // it shows in the invalid tint.
  ParkMesh &shape = accepted ? ghost.Lit : ghost.Tints;
  appendBoxGhost(shape, world, edit, accepted, lists);
  const auto colorOf = [accepted](PathKind kind) { return ghostColor(accepted, pathColor(kind)); };
  if (const auto *del = std::get_if<DeleteSegment>(&edit)) {
    // A segment's deletion shows only what goes, and the segments whose line it reshapes.
    parkBoxes(world, lists.Boxes);
    parkPathPoints(world, lists.Points);
    appendEntity(ghost.Tints, world, lists.Boxes, lists.Points, del->Segment,
                 accepted ? DELETE_TINT : INVALID_TINT);
    reshapedBy(candidate, lists);
    appendSurface(shape, lists.Owned, GHOST_LIFT, colorOf);
    if (candidate) {
      appendCandidate(ghost, world, *candidate, lists);
    }
    return;
  }
  if (accepted) {
    appendDeleted(ghost.Tints, world, edit, lists);
  }
  const PavedSurface &edited = lists.Parts.Surface;
  if (const auto *path = std::get_if<AddPath>(&edit); path != nullptr && !hasPieces(edited)) {
    appendStraightPath(shape, world, *path, ghostColor(accepted, pathColor(path->Kind)), lists);
  }
  if (candidate && hasPieces(edited)) {
    ownersOf(edited, lists.Owners);
    ownedBy(walkedSurface(*candidate), lists.Owners, lists.Owned);
    appendSurface(shape, lists.Owned, GHOST_LIFT, colorOf);
  } else {
    appendSurface(shape, edited, GHOST_LIFT, colorOf);
  }
  // A narrower ghost lies inside the path it narrows, in the same color, so its new sides are
  // marked.
  if (std::holds_alternative<SetSegmentWidth>(edit)) {
    for (const ParkSegment &segment : lists.Parts.Segments) {
      appendSides(ghost.Marks, segment, JOIN_MARK_COLOR);
    }
  }
  appendJoinMarks(ghost.Marks, world, edit, planned, lists);
  if (candidate) {
    appendCandidate(ghost, world, *candidate, lists);
  }
}

GhostMesh buildGhostMesh(const World &world, const ParkEdit &edit,
                         const std::optional<World> &candidate) {
  GhostLists lists;
  PlannedEdit planned;
  planEdit(world, edit, planned, lists.Buffers);
  buildGhostMesh(world, edit, planned, candidate, lists);
  return std::move(lists.Ghost);
}

GhostMesh buildGhostMesh(const World &world, const ParkEdit &edit) {
  std::optional<World> candidate;
  if (isAccepted(world, edit)) {
    CommandQueue queue;
    queueEdit(queue, edit);
    candidate = makeCandidate(world, queue);
  }
  return buildGhostMesh(world, edit, candidate);
}

} // namespace tpj
