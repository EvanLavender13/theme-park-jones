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
  Delete
};

// A place drag nearer than this to where the box lands keeps the facing it had, in meters.
inline constexpr double MIN_FACING_DRAG = 1.0;
// A path tool snaps the pointer onto a same-kind path's ground line this near it, in meters.
inline constexpr double SNAP_REACH = 2.0;
// A path tool's press this near its last drawn point finishes the path, in meters.
inline constexpr double FINISH_REACH = 1.0;

// One tool and what it holds. Change it only through selectTool, movePointer, pressPointer, and
// releasePointer.
struct ToolState {
  ToolKind Kind = ToolKind::None;
  // Where the pointer meets the ground, if it does.
  std::optional<ParkPoint> Pointer;
  bool Holding = false;
  // The place tools' facing for the next box.
  double FacingX = 0.0;
  double FacingZ = -1.0;
  // Holding with a place tool: where the box lands and how it faces.
  Pose Landing;
  // Holding with MoveBox: the box, its pose at the press, the offset of its position from the
  // pointer then, and where it would move.
  EntityKey Held = NULL_KEY;
  Pose HeldFrom;
  ParkPoint GrabOffset;
  Pose Target;
  // A path tool's points drawn so far.
  std::vector<ParkPoint> Drawn;
};

// Sets the tool, dropping any hold without committing.
void selectTool(ToolState &tool, ToolKind kind);
// Where the pointer meets the ground, or none off the ground or over a panel.
void movePointer(ToolState &tool, std::optional<ParkPoint> ground);
// The primary button went down.
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
// The first path in key order whose ground line passes within half its width of the point.
std::optional<EntityKey> pathAt(const World &world, ParkPoint point);
// The nearest point to the given one on the ground line of a path of the kind, when one lies within
// SNAP_REACH, and the point itself otherwise.
ParkPoint snapToPath(const World &world, PathKind kind, ParkPoint point);

} // namespace tpj

#endif
