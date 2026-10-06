#include "render/park_mesh.h"

#include "sim/command_queue.h"
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

// The normal of an entrance's or box's front and back faces: its footprint's Forward. None when the
// key holds neither, or its pose has no footprint.
std::optional<ParkPoint> faceNormalOf(const World &world, EntityKey entity) {
  std::optional<Footprint> footprint;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (entrance.Key == entity) {
      footprint = footprintOf(entrance.At, ENTRANCE_SIZE);
    }
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == entity) {
      footprint = footprintOf(box.At, boxSize(box.Kind));
    }
  }
  if (!footprint) {
    return std::nullopt;
  }
  return footprint->Forward;
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

// Adds each piece of the surface, the lift above its kind's paths, in the color colorOf gives for
// its kind, and then each edge line EDGE_LIFT higher in that color's edgeColor.
template <typename ColorOf>
void appendSurface(ParkMesh &mesh, const PavedSurface &surface, float lift, ColorOf colorOf) {
  for (const SurfacePiece &piece : surface.Pieces) {
    const float height = pathLift(piece.Kind) + lift;
    const Rgba color = colorOf(piece.Kind);
    const auto first = static_cast<uint32_t>(mesh.Vertices.size());
    for (const ParkPoint &corner : pieceCorners(piece)) {
      addVertex(mesh, corner.X, height, corner.Z, {}, 1.0f, color);
    }
    for (uint32_t k = 1; k + 1 < piece.Count; ++k) {
      mesh.Indices.insert(mesh.Indices.end(), {first, first + k, first + k + 1});
    }
  }
  for (const EdgeLine &edge : surface.Edges) {
    appendEdgeLine(mesh, edgePoints(surface, edge), pathLift(edge.Kind) + lift + EDGE_LIFT,
                   edgeColor(colorOf(edge.Kind)));
  }
}

// Adds a segment's ribbon and round joints along the line, GHOST_LIFT above its kind's paths.
void appendGhostSegment(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &line,
                        double width, Rgba color) {
  if (line.size() >= 2) {
    appendSegmentAt(mesh, line, width, pathLift(kind) + GHOST_LIFT, color);
  }
}

// Adds an added path's straight segments between its stops, for a path the world refuses so that
// editedSegments gives none. Nothing when a stop has no position.
void appendStraightPath(ParkMesh &mesh, const World &world, const AddPath &path, Rgba color) {
  const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, path);
  if (!positions) {
    return;
  }
  for (size_t i = 0; i + 1 < positions->size(); ++i) {
    appendGhostSegment(mesh, path.Kind, segmentLine((*positions)[i], (*positions)[i + 1]),
                       pathWidth(path.Kind), color);
  }
}

// Adds, in the delete tint, the box or segment a deletion removes, or every segment at a point
// DeletePoint deletes.
void appendDeleted(ParkMesh &mesh, const World &world, const ParkEdit &edit) {
  if (const auto *del = std::get_if<DeletePoint>(&edit)) {
    for (const ParkSegment &segment : parkSegments(world)) {
      if (segment.From == del->Point || segment.To == del->Point) {
        appendEntity(mesh, world, segment.Key, DELETE_TINT);
      }
    }
  } else if (const auto *segment = std::get_if<DeleteSegment>(&edit)) {
    appendEntity(mesh, world, segment->Segment, DELETE_TINT);
  } else if (const auto *box = std::get_if<DeleteBox>(&edit)) {
    appendEntity(mesh, world, box->Box, DELETE_TINT);
  }
}

// Adds a join mark at each stop where an added path joins a point, a segment, or an earlier stop,
// at a moved point's target when it is a point or segment, at an inserted point's place, and at
// each place pathCrossings gives.
void appendJoinMarks(ParkMesh &mesh, const World &world, const ParkEdit &edit) {
  if (const auto *path = std::get_if<AddPath>(&edit)) {
    if (const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, *path)) {
      for (size_t i = 0; i < positions->size(); ++i) {
        if (!std::holds_alternative<NewPoint>(path->Stops[i])) {
          appendJoinMark(mesh, (*positions)[i]);
        }
      }
    }
    for (const ParkPoint &at : pathCrossings(world, *path)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *move = std::get_if<MovePoint>(&edit)) {
    if (!std::holds_alternative<NewPoint>(move->To)) {
      const PathStop target = std::visit([](const auto &stop) { return PathStop{stop}; }, move->To);
      if (const std::optional<ParkPoint> at = stopPosition(world, target)) {
        appendJoinMark(mesh, *at);
      }
    }
    for (const ParkPoint &at : pathCrossings(world, *move)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *insert = std::get_if<InsertPoint>(&edit)) {
    if (const std::optional<ParkPoint> at =
            stopPosition(world, OnSegment{insert->Segment, insert->Along})) {
      appendJoinMark(mesh, *at);
    }
  } else if (const auto *del = std::get_if<DeletePoint>(&edit)) {
    for (const ParkPoint &at : pathCrossings(world, *del)) {
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

void appendSides(ParkMesh &mesh, const World &world, EntityKey segment, Rgba color) {
  for (const ParkSegment &candidate : parkSegments(world)) {
    if (candidate.Key == segment) {
      appendSides(mesh, candidate, color);
    }
  }
}

void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   Rgba color) {
  appendWalkway(mesh, kind, points, std::nullopt, color);
}

void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   std::optional<ParkPoint> faceNormal, Rgba color) {
  if (points.size() < 2) {
    return;
  }
  const auto first = mesh.Vertices.size();
  appendRibbon(mesh, points, pathWidth(kind), pathLift(kind), color);
  if (faceNormal) {
    startFlush(mesh, first, points, 0.5 * pathWidth(kind), *faceNormal);
  }
  appendJoint(mesh, points.back(), unitStep(points[points.size() - 2], points.back()),
              0.5 * pathWidth(kind), pathLift(kind), color);
}

void appendWalkways(ParkMesh &mesh, const World &world, float alpha) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const Network &network = parkNetwork(world, kind);
    const Rgba base = pathColor(kind);
    const Rgba color{base.R, base.G, base.B, alpha};
    for (const Carrier &carrier : network.carriers()) {
      // path-networks anchors each connector's door node, and no other node.
      if (carrier.Stops.empty()) {
        continue;
      }
      const EntityKey entity = network.nodeAnchor(carrier.Stops.front().Node);
      if (entity != NULL_KEY) {
        appendWalkway(mesh, kind, carrier.Points, faceNormalOf(world, entity), color);
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

void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha) {
  for (const ParkBox &box : parkBoxes(world)) {
    const std::optional<ShopRecord> record = shopRecord(world, box.Key);
    if (record && record->Starved) {
      appendStarvedMark(mesh, box.At,
                        Rgba{STARVED_COLOR.R, STARVED_COLOR.G, STARVED_COLOR.B, alpha});
    }
  }
}

ParkMesh buildParkMesh(const World &world) {
  ParkMesh mesh;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    appendBox(mesh, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR);
  }
  appendSurface(mesh, parkSurface(world), 0.0f, pathColor);
  appendWalkways(mesh, world, 1.0f);
  for (const ParkBox &box : parkBoxes(world)) {
    appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind));
  }
  appendStarvedMarks(mesh, world, 1.0f);
  return mesh;
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

void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), color);
      return;
    }
  }
  PavedSurface own;
  for (const SurfacePiece &piece : parkSurface(world).Pieces) {
    if (piece.Owner == key) {
      own.Pieces.push_back(piece);
    }
  }
  if (!own.Pieces.empty()) {
    appendSurface(mesh, own, GHOST_LIFT, [color](PathKind /*kind*/) { return color; });
    return;
  }
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    if (point.Key == key) {
      appendDisc(mesh, point.At, JOIN_MARK_RADIUS, JOIN_MARK_LIFT, color);
      return;
    }
  }
}

ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit,
                        const std::optional<World> &candidate) {
  ParkMesh mesh;
  const bool accepted = isAccepted(world, edit);
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(mesh, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(accepted, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == move->Box) {
        appendBox(mesh, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(accepted, boxColor(box.Kind)));
      }
    }
  }
  if (accepted) {
    appendDeleted(mesh, world, edit);
  }
  const PavedSurface surface = editedSurface(world, edit);
  if (const auto *path = std::get_if<AddPath>(&edit); path != nullptr && surface.Pieces.empty()) {
    appendStraightPath(mesh, world, *path, ghostColor(accepted, pathColor(path->Kind)));
  }
  appendSurface(mesh, surface, GHOST_LIFT,
                [accepted](PathKind kind) { return ghostColor(accepted, pathColor(kind)); });
  // A narrower ghost lies inside the path it narrows, in the same color, so its new sides are
  // marked.
  if (std::holds_alternative<SetSegmentWidth>(edit)) {
    for (const ParkSegment &segment : editedSegments(world, edit)) {
      appendSides(mesh, segment, JOIN_MARK_COLOR);
    }
  }
  appendJoinMarks(mesh, world, edit);
  if (candidate) {
    appendWalkways(mesh, *candidate, GHOST_ALPHA);
    appendStarvedMarks(mesh, *candidate, GHOST_ALPHA);
  }
  return mesh;
}

ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit) {
  std::optional<World> candidate;
  if (isAccepted(world, edit)) {
    CommandQueue queue;
    queueEdit(queue, edit);
    candidate = makeCandidate(world, queue);
  }
  return buildGhostMesh(world, edit, candidate);
}

} // namespace tpj
