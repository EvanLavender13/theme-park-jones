#ifndef TPJ_SIM_PARK_EDITS_H
#define TPJ_SIM_PARK_EDITS_H

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <optional>
#include <stddef.h>
#include <variant>
#include <vector>

namespace tpj {

// Conflicts no deeper than this are contact, not overlap.
inline constexpr double CONTACT_TOLERANCE = 0.001;

// A drawn path's stop at a new point.
struct NewPoint {
  ParkPoint At;

  bool operator==(const NewPoint &) const = default;
};

// A drawn path's stop at an existing path point.
struct OnPoint {
  EntityKey Point = NULL_KEY;

  bool operator==(const OnPoint &) const = default;
};

// A drawn path's stop on an existing segment, Along of the way from its From point to its To point.
struct OnSegment {
  EntityKey Segment = NULL_KEY;
  double Along = 0.0;

  bool operator==(const OnSegment &) const = default;
};

// A drawn path's stop at the point an earlier stop of the same path makes, given by that stop's
// index, as when a path closes a loop on its own start.
struct RepeatStop {
  size_t Stop = 0;

  bool operator==(const RepeatStop &) const = default;
};

// A drawn path's stop on one of its own earlier segments, the one from stop Stop to stop Stop + 1,
// Along of the way from the first to the second.
struct OnDrawnSegment {
  size_t Stop = 0;
  double Along = 0.0;

  bool operator==(const OnDrawnSegment &) const = default;
};

using PathStop = std::variant<NewPoint, OnPoint, OnSegment, RepeatStop, OnDrawnSegment>;

// A path of the kind through the stops, in order, joining the points and segments they name.
struct AddPath {
  PathKind Kind = PathKind::Guest;
  std::vector<PathStop> Stops;

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

struct DeleteSegment {
  EntityKey Segment = NULL_KEY;

  bool operator==(const DeleteSegment &) const = default;
};

struct DeleteBox {
  EntityKey Box = NULL_KEY;

  bool operator==(const DeleteBox &) const = default;
};

// Where MovePoint moves a point: a new position, another path point, or a place on a segment.
using PointTarget = std::variant<NewPoint, OnPoint, OnSegment>;

// The path point moves to the target, and every segment at it follows.
struct MovePoint {
  EntityKey Point = NULL_KEY;
  PointTarget To;

  bool operator==(const MovePoint &) const = default;
};

// A new point on the segment, Along of the way from its From point to its To point, splitting it
// there.
struct InsertPoint {
  EntityKey Segment = NULL_KEY;
  double Along = 0.0;

  bool operator==(const InsertPoint &) const = default;
};

// The path point goes, and its two segments merge when it has exactly two.
struct DeletePoint {
  EntityKey Point = NULL_KEY;

  bool operator==(const DeletePoint &) const = default;
};

// The path point's corner radius, held exactly as given.
struct SetCornerRadius {
  EntityKey Point = NULL_KEY;
  double Radius = 0.0;

  bool operator==(const SetCornerRadius &) const = default;
};

// The segment's width, held exactly as given.
struct SetSegmentWidth {
  EntityKey Segment = NULL_KEY;
  double Width = 0.0;

  bool operator==(const SetSegmentWidth &) const = default;
};

// Registers AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint,
// DeletePoint, SetCornerRadius, and SetSegmentWidth, in that order.
void addParkEdits(WorldSchema &schema);

// A tentative or committed edit to park intent: one of the ten commands.
using ParkEdit = std::variant<AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint,
                              InsertPoint, DeletePoint, SetCornerRadius, SetSegmentWidth>;

// The stop's ground position: a NewPoint's own, an OnPoint's point's, or the place Along of the way
// from an OnSegment's segment's From point to its To point. None when the stop names no path point,
// or no segment whose two ends are path points, and for a RepeatStop or an OnDrawnSegment, which
// only its path places.
std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop);
// Each of the path's stops' ground positions, a RepeatStop's being its earlier stop's, and an
// OnDrawnSegment's the place Along of the way between its two stops. None when a stop has none, or
// a RepeatStop or an OnDrawnSegment names a stop that is not earlier.
std::optional<std::vector<ParkPoint>> stopPositions(const World &world, const AddPath &path);
// The ground position of each point where the path splits a segment of its kind, or one of its
// own segments, because the two meet: a new point where they cross, or an end of one lying on the
// other. Leaves out the points of OnSegment and OnDrawnSegment stops. In the order the meetings
// are found. Empty when the world refuses the path.
std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path);
// The ground position of each point where the move or the merge splits a segment of its kind
// because the two meet, leaving out a MovePoint's OnSegment target. In the order the meetings are
// found. Empty when the world refuses the command.
std::vector<ParkPoint> pathCrossings(const World &world, const MovePoint &move);
std::vector<ParkPoint> pathCrossings(const World &world, const DeletePoint &del);

// Each segment the edit adds, or whose ground line or width it changes, as it would lie after the
// edit, in ascending key order, a new segment with the key the commit would give it. Given whether
// or not the world refuses the edit for meeting a solid. Empty when the edit names nothing it acts
// on, when a segment it adds or reshapes has no ground line, and for edits of boxes.
std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit);

// Whether the command would be applied to the world: it describes a physical object, names what it
// acts on, and conflicts with nothing: no footprints overlap, no segment comes within half its
// width of a footprint, and nothing leaves the park's square.
bool isAccepted(const World &world, const AddPath &command);
bool isAccepted(const World &world, const AddBox &command);
bool isAccepted(const World &world, const MoveBox &command);
bool isAccepted(const World &world, const DeleteSegment &command);
bool isAccepted(const World &world, const DeleteBox &command);
bool isAccepted(const World &world, const MovePoint &command);
bool isAccepted(const World &world, const InsertPoint &command);
bool isAccepted(const World &world, const DeletePoint &command);
bool isAccepted(const World &world, const SetCornerRadius &command);
bool isAccepted(const World &world, const SetSegmentWidth &command);

// Whether the command the edit holds would be applied to the world.
bool isAccepted(const World &world, const ParkEdit &edit);

// Pushes the command the edit holds, as its own type, so the cycle applies it as if pushed
// directly.
void queueEdit(CommandQueue &queue, const ParkEdit &edit);

// Apply the command when isAccepted, and change nothing otherwise.
void applyCommand(World &world, const AddPath &command);
void applyCommand(World &world, const AddBox &command);
void applyCommand(World &world, const MoveBox &command);
void applyCommand(World &world, const DeleteSegment &command);
void applyCommand(World &world, const DeleteBox &command);
void applyCommand(World &world, const MovePoint &command);
void applyCommand(World &world, const InsertPoint &command);
void applyCommand(World &world, const DeletePoint &command);
void applyCommand(World &world, const SetCornerRadius &command);
void applyCommand(World &world, const SetSegmentWidth &command);

// Whether every solid and line has its geometry, stays in the park, and conflicts with no other.
bool isPhysicallyValid(const World &world);

} // namespace tpj

#endif
