#ifndef TPJ_SIM_PARK_EDITS_H
#define TPJ_SIM_PARK_EDITS_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <vector>

namespace tpj {

// Conflicts no deeper than this are contact, not overlap.
inline constexpr double CONTACT_TOLERANCE = 0.001;

// A path of the kind through the points, which the path keeps as keptPoints gives them.
struct AddPath {
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;
};

// A box of the kind at the pose, held exactly as given.
struct AddBox {
  BoxKind Kind = BoxKind::Shop;
  Pose At;
};

// The box's new pose, held exactly as given.
struct MoveBox {
  EntityKey Box = NULL_KEY;
  Pose At;
};

struct DeletePath {
  EntityKey Path = NULL_KEY;
};

struct DeleteBox {
  EntityKey Box = NULL_KEY;
};

// Registers AddPath, AddBox, MoveBox, DeletePath, and DeleteBox, in that order.
void addParkEdits(WorldSchema &schema);

// Whether the command would be applied to the world: it describes a physical object, names what it
// acts on, and conflicts with nothing. See sim/park/SPEC.md.
bool isAccepted(const World &world, const AddPath &command);
bool isAccepted(const World &world, const AddBox &command);
bool isAccepted(const World &world, const MoveBox &command);
bool isAccepted(const World &world, const DeletePath &command);
bool isAccepted(const World &world, const DeleteBox &command);

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
