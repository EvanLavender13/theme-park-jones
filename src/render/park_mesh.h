#ifndef TPJ_RENDER_PARK_MESH_H
#define TPJ_RENDER_PARK_MESH_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
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

// Ghosts and highlights are translucent.
inline constexpr float GHOST_ALPHA = 0.5f;
inline constexpr Rgba INVALID_TINT{0.95f, 0.15f, 0.15f, 0.5f};
inline constexpr Rgba DELETE_TINT{1.0f, 0.55f, 0.1f, 0.6f};
inline constexpr Rgba HIGHLIGHT_TINT{1.0f, 1.0f, 1.0f, 0.35f};

// Adds the path's flat ribbon along its ground line, or nothing when the line is empty.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points);
// The same ribbon in the color given.
void appendPath(ParkMesh &mesh, PathKind kind, const std::vector<ParkPoint> &points, Rgba color);

// A walkway's round joint beyond its end is a half disc of this many segments.
inline constexpr uint32_t WALKWAY_JOINT_SEGMENTS = 16;

// Adds a connector's walkway: the ribbon along the points, pathWidth(kind) wide, in the color, and
// a round half disc of half that width beyond the last point. The points are each distinct from the
// next, as a connector's are. Nothing for fewer than two.
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   Rgba color);
// The same walkway, starting flush with the face whose unit normal is given: its first two
// vertices slide along the walkway onto the face's line. It starts square without a normal, or
// where sliding would fold the ribbon.
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   std::optional<ParkPoint> faceNormal, Rgba color);

// Adds a walkway for each connector of the world's guest network and then its backstage network,
// in the kind's path color with the alpha given. A connector is a carrier whose first stop's node
// is anchored.
void appendWalkways(ParkMesh &mesh, const World &world, float alpha);

// Adds an open-bottomed box of the height over the pose's footprint, or nothing when the pose has
// no footprint.
void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color);

// The world's entrances, paths, walkways, and boxes, in that order and each in key order.
ParkMesh buildParkMesh(const World &world);

// Adds the box or path the key holds as buildParkMesh draws it, in the color. Nothing when the key
// holds neither.
void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color);

// The ghost of an edit on a world: translucent in its kind's color when accepted, INVALID_TINT when
// not, and DELETE_TINT for a deletion, followed for an accepted edit by its candidate's walkways.
// See render/SPEC.md.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit);

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
