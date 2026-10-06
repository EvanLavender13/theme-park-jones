#ifndef TPJ_TOOLS_TOOLS_H
#define TPJ_TOOLS_TOOLS_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

enum class ToolKind : uint8_t {
  None,
  GuestPath,
  BackstagePath,
  PlaceShop,
  PlaceDepot,
  MoveBox,
  EditPath,
  RemovePoint,
  Delete
};

// A place drag nearer than this to where the box lands keeps the facing it had, in meters.
inline constexpr double MIN_FACING_DRAG = 1.0;
// A path tool snaps a click onto a point or segment of its kind this near it, in meters.
inline constexpr double SNAP_REACH = 2.0;
// A path tool's press this near its last stop finishes the path, in meters.
inline constexpr double FINISH_REACH = 1.0;
// A press or hover this near a path point picks it, in meters.
inline constexpr double POINT_REACH = 2.0;
// Edit path keeps a corner's radius handle at least this far from its point, in meters.
inline constexpr double RADIUS_HANDLE_DEPTH = 3.0;
// A press or hover this near a radius handle takes it, in meters.
inline constexpr double HANDLE_REACH = 1.0;
// A press or hover this near a segment's side takes it, in meters.
inline constexpr double SIDE_REACH = 0.5;
// Edit path sets a segment's width between these, in meters.
inline constexpr double MIN_PATH_WIDTH = 1.5;
inline constexpr double MAX_PATH_WIDTH = 16.0;

// What an Edit path handle is: a path point's marker, which sets nothing, or what it sets: a
// corner's radius, an edge's radius, or a segment's width.
enum class HandleKind : uint8_t { Point, Radius, Edge, Side };

// A handle Edit path shows or takes hold of: a path point's marker, keyed by its point, at its
// position; a corner's radius handle, keyed by the corner's point, at At; an edge handle, keyed by
// the edge's point and its segment, at At; or a segment's sides, keyed by the segment.
struct EditHandle {
  HandleKind Kind = HandleKind::Radius;
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  EntityKey Segment = NULL_KEY;

  bool operator==(const EditHandle &) const = default;
};

// One tool and what it holds. Change it only through selectTool, movePointer, pressPointer, and
// releasePointer.
struct ToolState {
  ToolKind Kind = ToolKind::None;
  // Where the pointer meets the ground, if it does.
  std::optional<ParkPoint> Pointer;
  bool Holding = false;
  // Whether the pointer has met the ground anywhere new since the press took hold.
  bool Dragged = false;
  // The place tools' facing for the next box.
  double FacingX = 0.0;
  double FacingZ = -1.0;
  // Holding with a place tool: where the box lands and how it faces.
  Pose Landing;
  // Holding with MoveBox: the box, the offset of its position from the pointer at the press, and
  // where it would move.
  EntityKey Held = NULL_KEY;
  ParkPoint GrabOffset;
  Pose Target;
  // Holding with EditPath on a point: Held is the point, and PointTo where the drag would put it
  // before snapping. GrabOffset is the offset of its position from the pointer at the press.
  ParkPoint PointTo;
  // Holding with EditPath on a segment: the point the release inserts.
  std::optional<InsertPoint> Inserting;
  // Holding with EditPath on a radius handle, an edge handle, or a segment's sides.
  std::optional<EditHandle> Grabbed;
  // For a radius handle, how much deeper the arc's middle lay than the pointer at the press, along
  // the corner's Toward. For an edge handle, how much farther the edge's middle lay than the
  // pointer at the press, along the edge's Toward from its Sharp. For a side, the width at the
  // press less twice the pointer's distance from the ground line.
  double GrabFrom = 0.0;
  // A path tool's stops so far.
  std::vector<PathStop> Drawn;
};

// Sets the tool, dropping any hold without committing.
void selectTool(ToolState &tool, ToolKind kind);
// Where the pointer meets the ground, or none off the ground or over a panel.
void movePointer(ToolState &tool, std::optional<ParkPoint> ground);
// The primary button went down. A path tool adds no stop that would make the world refuse its path.
void pressPointer(ToolState &tool, const World &world);
// The primary button went up: the edit to commit, which is the tentative edit of the moment
// before when the press took hold.
std::optional<ParkEdit> releasePointer(ToolState &tool, const World &world);

// The edit the ghost shows.
std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world);
// The entity the tool marks.
std::optional<EntityKey> highlightedEntity(const ToolState &tool, const World &world);

// The first box in key order whose footprint holds the point.
std::optional<EntityKey> boxAt(const World &world, ParkPoint point);
// The first segment in key order whose ground line passes within half its width of the point.
std::optional<EntityKey> segmentAt(const World &world, ParkPoint point);
// The stop a click at the point makes for a path of the kind whose stops so far are drawn: the
// nearest within SNAP_REACH of the path points of the kind and the points the drawn stops make,
// leaving out the last, or else the nearest place within SNAP_REACH on a segment of the kind or a
// segment between two drawn stops, or that segment's end point when the place lies within
// MIN_SEGMENT_LENGTH of it, or else a new point at the click. A point is the kind's when at least
// one segment uses it and every segment at it has the kind.
PathStop snapStop(const World &world, PathKind kind, const std::vector<PathStop> &drawn,
                  ParkPoint point);
// The path point nearest the point within POINT_REACH, of either kind. Ties go to the lower key.
std::optional<EntityKey> pointAt(const World &world, ParkPoint point);
// Where a dragged path point at the place moves to: snapStop's target for a path of the point's
// kind with no stops drawn, leaving out the point and every segment at it, or a new point at the
// place when no segment uses the point.
PointTarget snapPoint(const World &world, EntityKey point, ParkPoint at);
// Where the corner's radius handle lies.
ParkPoint radiusHandleOf(const ParkCorner &corner);
// The radius handle or edge handle that lies nearest the point within HANDLE_REACH, with At where
// it lies. Ties go to a radius handle, then to the lower point key, then to the lower segment key.
std::optional<EditHandle> handleAt(const World &world, ParkPoint point);
// The segment with a ground line whose side lies nearest the point within SIDE_REACH. Ties go to
// the lower key.
std::optional<EntityKey> sideAt(const World &world, ParkPoint point);
// Every path point's marker, in key order, then every corner's radius handle, in key order of its
// point, then every edge handle, in the order parkEdges gives the edges, while EditPath is chosen
// and nothing is held. Only the markers while RemovePoint is chosen and nothing is held. Empty
// otherwise.
std::vector<EditHandle> editHandles(const ToolState &tool, const World &world);
// The radius handle, edge handle, or segment's sides under the pointer that a press with EditPath
// would take, while nothing is held. None otherwise.
std::optional<EditHandle> hoveredHandle(const ToolState &tool, const World &world);

// Where a path tool's first click would start the path, its kind, and whether it would join a
// point or segment there.
struct PathStart {
  ParkPoint At;
  PathKind Kind = PathKind::Guest;
  bool Joins = false;

  bool operator==(const PathStart &) const = default;
};

// The path's start before its first click, or none when the tool draws no path, has a stop drawn,
// or the pointer is off the ground.
std::optional<PathStart> pathStart(const ToolState &tool, const World &world);

// Where a press would finish and commit the drawn path: its last stop's position, when at least two
// stops are drawn and the pointer lies within FINISH_REACH of it. None otherwise.
std::optional<ParkPoint> pathFinish(const ToolState &tool, const World &world);

} // namespace tpj

#endif
