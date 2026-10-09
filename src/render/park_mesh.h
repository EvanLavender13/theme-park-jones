#ifndef TPJ_RENDER_PARK_MESH_H
#define TPJ_RENDER_PARK_MESH_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
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

// A path's ribbon lies this far above the ground: guest paths above backstage ones, by as much as
// backstage ones lie above the terrain, so crossing ribbons of the two kinds never tie in depth.
constexpr float pathLift(PathKind kind) { return kind == PathKind::Guest ? 0.04f : 0.02f; }
inline constexpr float ENTRANCE_HEIGHT = 5.0f;
inline constexpr Rgba ENTRANCE_COLOR{0.92f, 0.80f, 0.28f, 1.0f};

constexpr float boxHeight(BoxKind kind) {
  switch (kind) {
  case BoxKind::Shop:
    return 4.0f;
  case BoxKind::Depot:
    return 6.0f;
  case BoxKind::Bench:
    return 1.0f;
  case BoxKind::ToiletBlock:
    return 3.5f;
  }
  return 4.0f;
}

constexpr Rgba pathColor(PathKind kind) {
  return kind == PathKind::Guest ? Rgba{0.82f, 0.74f, 0.58f, 1.0f}
                                 : Rgba{0.46f, 0.46f, 0.50f, 1.0f};
}

constexpr Rgba boxColor(BoxKind kind) {
  switch (kind) {
  case BoxKind::Shop:
    return {0.84f, 0.42f, 0.30f, 1.0f};
  case BoxKind::Depot:
    return {0.34f, 0.44f, 0.62f, 1.0f};
  case BoxKind::Bench:
    return {0.55f, 0.38f, 0.22f, 1.0f};
  case BoxKind::ToiletBlock:
    return {0.45f, 0.72f, 0.70f, 1.0f};
  }
  return {0.84f, 0.42f, 0.30f, 1.0f};
}

// The color a box's front face takes: red, green, and blue moved 0.4 of the way to 1.
Rgba lightened(Rgba color);

// Ghosts and highlights are translucent.
inline constexpr float GHOST_ALPHA = 0.5f;
inline constexpr Rgba INVALID_TINT{0.95f, 0.15f, 0.15f, 0.5f};
inline constexpr Rgba DELETE_TINT{1.0f, 0.55f, 0.1f, 0.6f};
inline constexpr Rgba HIGHLIGHT_TINT{1.0f, 1.0f, 1.0f, 0.35f};

// A paved surface's edge lines are strips this wide on the paving's side of each line, this far
// above its pieces.
inline constexpr double EDGE_LINE_WIDTH = 0.15;
inline constexpr float EDGE_LIFT = 0.005f;

// The color of an edge line on paving of the color: red, green, and blue scaled by 0.6.
Rgba edgeColor(Rgba color);

// A ghost marks each stop where an added path joins a point or segment, and each place where it
// crosses one, with a flat disc of this radius, this high, in this color.
inline constexpr double JOIN_MARK_RADIUS = 1.0;
inline constexpr float JOIN_MARK_LIFT = 0.06f;
inline constexpr Rgba JOIN_MARK_COLOR{0.15f, 0.85f, 0.95f, 0.9f};

// Adds a join mark centered on the point.
void appendJoinMark(ParkMesh &mesh, ParkPoint at);

// A ghost marks the stop where a press would finish the path with a flat disc of this radius, this
// high, in this color. It lies under a join mark there, so the join mark still shows.
inline constexpr double FINISH_MARK_RADIUS = 1.6;
inline constexpr float FINISH_MARK_LIFT = 0.05f;
inline constexpr Rgba FINISH_MARK_COLOR{0.25f, 0.95f, 0.35f, 0.9f};

// Adds a finish mark centered on the point.
void appendFinishMark(ParkMesh &mesh, ParkPoint at);

// Edit path's radius handles are flat discs of this radius, this high, in this color when not
// hovered.
inline constexpr double RADIUS_HANDLE_SIZE = 0.6;
inline constexpr float HANDLE_LIFT = 0.07f;
inline constexpr Rgba HANDLE_COLOR{0.95f, 0.95f, 0.95f, 0.8f};
// The color of edge handles, apart from radius handles.
inline constexpr Rgba EDGE_HANDLE_COLOR{0.95f, 0.70f, 0.25f, 0.8f};
// The color of taper handles.
inline constexpr Rgba TAPER_HANDLE_COLOR{0.30f, 0.85f, 0.90f, 0.8f};

// Adds a radius, edge, or taper handle centered on the point.
void appendHandle(ParkMesh &mesh, ParkPoint at, Rgba color);

// Path point markers are flat discs of this radius, this high, under handles and above join marks.
inline constexpr double POINT_MARK_SIZE = 0.35;
inline constexpr float POINT_MARK_LIFT = 0.065f;
inline constexpr Rgba POINT_MARK_COLOR{0.15f, 0.2f, 0.3f, 0.9f};

// Adds a path point's marker centered on the point.
void appendPointMark(ParkMesh &mesh, ParkPoint at, Rgba color);

// A segment's side marks are strips this wide along the inside of each side.
inline constexpr double SIDE_MARK_WIDTH = 0.3;

// Adds a strip along the inside of each side of the segment, in the color, twice GHOST_LIFT above
// its kind's paths. Nothing when it has no ground line.
void appendSides(ParkMesh &mesh, const ParkSegment &segment, Rgba color);
// The same strips for the segment the key holds. Nothing when the key holds no segment.
void appendSides(ParkMesh &mesh, const World &world, EntityKey segment, Rgba color);

// Ghost and highlighted segments lie this far above the park's paths, so they never tie in depth
// with the ribbons and joints beneath them.
inline constexpr float GHOST_LIFT = 0.01f;

// Adds the ghost of where a path of the kind would start before its first click: a translucent
// disc of its width in its color, GHOST_LIFT above its paths, with a join mark when it would join a
// point or segment there.
void appendPathStart(ParkMesh &mesh, PathKind kind, ParkPoint at, bool joins);

// A round joint beyond a ribbon's end is a half disc of this many segments.
inline constexpr uint32_t JOINT_SEGMENTS = 16;

// Adds a connector's walkway: the ribbon along the points, width wide at pathLift(kind), in the
// color, and a round half disc of half that width beyond the last point. The points are each
// distinct from the next, as a connector's are. Nothing for fewer than two.
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   double width, Rgba color);
// The same walkway, starting flush with the face whose unit normal is given: its first two
// vertices slide along the walkway onto the face's line. It starts square without a normal, or
// where sliding would fold the ribbon.
void appendWalkway(ParkMesh &mesh, PathKind kind, const std::vector<CarrierPoint> &points,
                   double width, std::optional<ParkPoint> faceNormal, Rgba color);

// Adds a walkway for each connector of the world's guest network and then its backstage network,
// in the kind's path color with the alpha given. A connector is a carrier whose first stop's node
// is anchored.
void appendWalkways(ParkMesh &mesh, const World &world, float alpha);

// Every door's walkway in the world, guest then backstage, as connectorWalkways gives them.
std::vector<Walkway> parkWalkways(const World &world);

// Adds an open-bottomed box of the height over the pose's footprint, or nothing when the pose has
// no footprint.
void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color);

// A starved shop's mark: a cube of this size whose bottom floats this high, a meter over the
// shop's roof, in a violet no other part of the park is drawn in.
inline constexpr float STARVED_MARK_SIZE = 1.5f;
inline constexpr float STARVED_MARK_BASE = 5.0f;
inline constexpr Rgba STARVED_COLOR{0.56f, 0.24f, 0.86f, 1.0f};

// Adds a closed cube of STARVED_MARK_SIZE over the pose, its bottom at STARVED_MARK_BASE, or
// nothing when the pose has no footprint.
void appendStarvedMark(ParkMesh &mesh, const Pose &pose, Rgba color);
// Adds a mark over each shop box whose inspection record says it is starved, in key order, in
// STARVED_COLOR with the alpha given.
void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha);

// The world's entrances, paths, walkways, and boxes, in that order and each in key order, then a
// starved mark over each starved shop.
ParkMesh buildParkMesh(const World &world);

// Adds the box, segment, or path point the key holds, in the color: a box shaped as buildParkMesh
// draws it, a segment as its own pieces of the paved surface GHOST_LIFT higher, and a path point
// as a flat disc of JOIN_MARK_RADIUS at JOIN_MARK_LIFT. Nothing when the key holds none of these.
void appendEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color);

// The ghost of an edit on a world: the box it adds or moves, and the paved surface it adds or
// changes, drawn as buildParkMesh draws paving GHOST_LIFT higher, translucent in its kind's color
// when accepted and INVALID_TINT when not; what an accepted deletion removes, in DELETE_TINT; and
// its join marks; followed by the candidate's walkways and starved marks when a candidate is given.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit,
                        const std::optional<World> &candidate);
// The same ghost with the candidate makeCandidate gives for the edit when it is accepted, and none
// when it is not.
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
