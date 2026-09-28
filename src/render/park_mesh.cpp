#include "render/park_mesh.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <optional>

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

} // namespace

Rgba lightened(Rgba color) {
  return {color.R + (1.0f - color.R) * 0.4f, color.G + (1.0f - color.G) * 0.4f,
          color.B + (1.0f - color.B) * 0.4f, color.A};
}

void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points) {
  const std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return;
  }
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  const double half = 0.5 * pathWidth(kind);
  const Rgba color = pathColor(kind);
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

} // namespace tpj
