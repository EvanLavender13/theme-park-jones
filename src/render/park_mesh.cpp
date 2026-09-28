#include "render/park_mesh.h"

#include "sim/command_queue.h"
#include "sim/medium/network.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <numbers>
#include <optional>
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

// An edit's ghost color for a kind's color: translucent when accepted, the invalid tint when not.
Rgba ghostColor(const World &world, const ParkEdit &edit, Rgba color) {
  return isAccepted(world, edit) ? Rgba{color.R, color.G, color.B, GHOST_ALPHA} : INVALID_TINT;
}

// Adds the flat ribbon of the width along the line, PATH_LIFT above the ground. The line has at
// least two points, each distinct from the next.
void appendRibbon(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width, Rgba color) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  const double half = 0.5 * width;
  for (size_t i = 0; i < line.size(); ++i) {
    const ParkPoint tangent = tangentAt(line, i);
    const ParkPoint right{-tangent.Z, tangent.X};
    addVertex(mesh, line[i].X - right.X * half, PATH_LIFT, line[i].Z - right.Z * half, {}, 1.0f,
              color);
    addVertex(mesh, line[i].X + right.X * half, PATH_LIFT, line[i].Z + right.Z * half, {}, 1.0f,
              color);
  }
  for (uint32_t i = 0; i + 1 < line.size(); ++i) {
    const uint32_t left0 = first + 2 * i;
    const uint32_t right0 = left0 + 1;
    const uint32_t left1 = left0 + 2;
    const uint32_t right1 = left0 + 3;
    mesh.Indices.insert(mesh.Indices.end(), {left0, right0, left1, left1, right0, right1});
  }
}

// Adds a flat half disc of the radius beyond a ribbon's end at the point, PATH_LIFT above the
// ground, for the ribbon's unit direction there: its center, then WALKWAY_JOINT_SEGMENTS + 1
// vertices on its rim from the ribbon's right end corner around to its left, and a triangle facing
// up from the center to each rim vertex and the next.
void appendJoint(ParkMesh &mesh, const CarrierPoint &point, ParkPoint direction, double radius,
                 Rgba color) {
  const ParkPoint right{-direction.Z, direction.X};
  const auto center = static_cast<uint32_t>(mesh.Vertices.size());
  addVertex(mesh, point.X, PATH_LIFT, point.Z, {}, 1.0f, color);
  for (uint32_t k = 0; k <= WALKWAY_JOINT_SEGMENTS; ++k) {
    const double angle = std::numbers::pi * static_cast<double>(k) / WALKWAY_JOINT_SEGMENTS;
    const double across = radius * cos(angle);
    const double along = radius * sin(angle);
    addVertex(mesh, point.X + across * right.X + along * direction.X, PATH_LIFT,
              point.Z + across * right.Z + along * direction.Z, {}, 1.0f, color);
  }
  for (uint32_t k = 0; k < WALKWAY_JOINT_SEGMENTS; ++k) {
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

} // namespace

Rgba lightened(Rgba color) {
  return {color.R + (1.0f - color.R) * 0.4f, color.G + (1.0f - color.G) * 0.4f,
          color.B + (1.0f - color.B) * 0.4f, color.A};
}

void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points) {
  appendPath(mesh, kind, points, pathColor(kind));
}

void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color) {
  const std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return;
  }
  appendRibbon(mesh, line, pathWidth(kind), color);
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
  appendRibbon(mesh, points, pathWidth(kind), color);
  if (faceNormal) {
    startFlush(mesh, first, points, 0.5 * pathWidth(kind), *faceNormal);
  }
  appendJoint(mesh, points.back(), unitStep(points[points.size() - 2], points.back()),
              0.5 * pathWidth(kind), color);
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
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return;
  }
  const auto &[frontLeft, frontRight, backRight, backLeft] = footprint->Corners;
  const ParkPoint forward = footprint->Forward;
  const ParkPoint right = footprint->Right;
  const float h = height;
  const std::array<float, 4> sideHeights{0.0f, 0.0f, h, h};

  addQuad(mesh, {frontLeft, backLeft, backRight, frontRight}, {h, h, h, h}, {}, 1.0f, color);
  // Each side runs along its edge counter-clockwise seen from above, then rises back over it.
  addQuad(mesh, {frontLeft, backLeft, backLeft, frontLeft}, sideHeights, {-right.X, -right.Z}, 0.0f,
          color);
  addQuad(mesh, {backLeft, backRight, backRight, backLeft}, sideHeights, {-forward.X, -forward.Z},
          0.0f, color);
  addQuad(mesh, {backRight, frontRight, frontRight, backRight}, sideHeights, right, 0.0f, color);
  addQuad(mesh, {frontRight, frontLeft, frontLeft, frontRight}, sideHeights, forward, 0.0f,
          lightened(color));
}

ParkMesh buildParkMesh(const World &world) {
  ParkMesh mesh;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    appendBox(mesh, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR);
  }
  for (const ParkPath &path : parkPaths(world)) {
    appendPath(mesh, path.Kind, path.Points);
  }
  appendWalkways(mesh, world, 1.0f);
  for (const ParkBox &box : parkBoxes(world)) {
    appendBox(mesh, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind));
  }
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
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Key == key) {
      appendPath(mesh, path.Kind, path.Points, color);
      return;
    }
  }
}

ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit) {
  ParkMesh mesh;
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(mesh, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(world, edit, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == move->Box) {
        appendBox(mesh, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(world, edit, boxColor(box.Kind)));
      }
    }
  } else if (const auto *path = std::get_if<AddPath>(&edit)) {
    appendPath(mesh, path->Kind, path->Points, ghostColor(world, edit, pathColor(path->Kind)));
  } else if (isAccepted(world, edit)) {
    const EntityKey key = std::holds_alternative<DeletePath>(edit) ? std::get<DeletePath>(edit).Path
                                                                   : std::get<DeleteBox>(edit).Box;
    appendEntity(mesh, world, key, DELETE_TINT);
  }
  if (isAccepted(world, edit)) {
    CommandQueue queue;
    queueEdit(queue, edit);
    appendWalkways(mesh, makeCandidate(world, queue), GHOST_ALPHA);
  }
  return mesh;
}

} // namespace tpj
