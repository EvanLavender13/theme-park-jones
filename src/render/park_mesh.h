#ifndef TPJ_RENDER_PARK_MESH_H
#define TPJ_RENDER_PARK_MESH_H

#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// A color with alpha, each channel in [0, 1].
struct Rgba {
  float R = 0.0f;
  float G = 0.0f;
  float B = 0.0f;
  float A = 1.0f;

  bool operator==(const Rgba &) const = default;
};

// Matches the vertex inputs of park.vert.
struct ParkVertex {
  float Position[3] = {};
  float Normal[3] = {};
  Rgba Color;
};

// Triangles as three indices each, wound counter-clockwise seen from the side their normal
// points to.
struct ParkMesh {
  std::vector<ParkVertex> Vertices;
  std::vector<uint32_t> Indices;
};

// A path's ribbon lies this far above the ground.
inline constexpr float PATH_LIFT = 0.02f;
inline constexpr float ENTRANCE_HEIGHT = 5.0f;
inline constexpr Rgba ENTRANCE_COLOR{0.92f, 0.80f, 0.28f, 1.0f};

constexpr float boxHeight(BoxKind kind) { return kind == BoxKind::Shop ? 4.0f : 6.0f; }

constexpr Rgba pathColor(PathKind kind) {
  return kind == PathKind::Guest ? Rgba{0.82f, 0.74f, 0.58f, 1.0f}
                                 : Rgba{0.46f, 0.46f, 0.50f, 1.0f};
}

constexpr Rgba boxColor(BoxKind kind) {
  return kind == BoxKind::Shop ? Rgba{0.84f, 0.42f, 0.30f, 1.0f} : Rgba{0.34f, 0.44f, 0.62f, 1.0f};
}

// The color a box's front face takes: red, green, and blue moved 0.4 of the way to 1.
Rgba lightened(Rgba color);

// Adds the path's flat ribbon along its ground line, or nothing when the line is empty.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points);

// Adds an open-bottomed box of the height over the pose's footprint, or nothing when the pose has
// no footprint.
void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color);

// The world's entrances, paths, and boxes, in that order and each in key order.
ParkMesh buildParkMesh(const World &world);

// A rectangle on the ground.
struct GroundBounds {
  float MinX = 0.0f;
  float MinZ = 0.0f;
  float MaxX = 0.0f;
  float MaxZ = 0.0f;
};

// The least rectangle holding every vertex's x and z, or none for a mesh with no vertices.
std::optional<GroundBounds> meshBounds(const ParkMesh &mesh);

} // namespace tpj

#endif
