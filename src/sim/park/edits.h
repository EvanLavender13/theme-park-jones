#ifndef TPJ_SIM_PARK_EDITS_H
#define TPJ_SIM_PARK_EDITS_H

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <variant>
#include <vector>

namespace tpj {

// Conflicts no deeper than this are contact, not overlap.
inline constexpr double CONTACT_TOLERANCE = 0.001;

// A path of the kind through the points, which the path keeps as keptPoints gives them.
struct AddPath {
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;

  bool operator==(const AddPath &) const = default;
};

// A box of the kind at the pose, held exactly as given.
struct AddBox {
  BoxKind Kind = BoxKind::Shop;
  Pose At;

  bool operator==(const AddBox &) const = default;
};

// The box's new pose, held exactly as given.
struct MoveBox {
  EntityKey Box = NULL_KEY;
  Pose At;

  bool operator==(const MoveBox &) const = default;
};

struct DeletePath {
  EntityKey Path = NULL_KEY;

  bool operator==(const DeletePath &) const = default;
};

struct DeleteBox {
  EntityKey Box = NULL_KEY;

  bool operator==(const DeleteBox &) const = default;
};

// Registers AddPath, AddBox, MoveBox, DeletePath, and DeleteBox, in that order.
void addParkEdits(WorldSchema &schema);

// A tentative or committed edit to park intent: one of the five commands.
using ParkEdit = std::variant<AddPath, AddBox, MoveBox, DeletePath, DeleteBox>;

// Whether the command would be applied to the world: it describes a physical object, names what it
// acts on, and conflicts with nothing: no footprints overlap, no path comes within half its width
// of a footprint, and nothing leaves the park's square.
bool isAccepted(const World &world, const AddPath &command);
bool isAccepted(const World &world, const AddBox &command);
bool isAccepted(const World &world, const MoveBox &command);
bool isAccepted(const World &world, const DeletePath &command);
bool isAccepted(const World &world, const DeleteBox &command);

// Whether the command the edit holds would be applied to the world.
bool isAccepted(const World &world, const ParkEdit &edit);

// Pushes the command the edit holds, as its own type, so the cycle applies it as if pushed
// directly.
void queueEdit(CommandQueue &queue, const ParkEdit &edit);

// Apply the command when isAccepted, and change nothing otherwise.
void applyCommand(World &world, const AddPath &command);
void applyCommand(World &world, const AddBox &command);
void applyCommand(World &world, const MoveBox &command);
void applyCommand(World &world, const DeletePath &command);
void applyCommand(World &world, const DeleteBox &command);

// Whether every solid and line has its geometry, stays in the park, and conflicts with no other.
bool isPhysicallyValid(const World &world);

} // namespace tpj

#endif
