#ifndef TPJ_SIM_PARK_EDITS_H
#define TPJ_SIM_PARK_EDITS_H

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/world.h"

#include <memory>
#include <optional>
#include <span>
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

// A place on the arc of the corner at the path point, Along of the way along it, as bendPlace
// gives.
struct OnBend {
  EntityKey Point = NULL_KEY;
  double Along = 0.0;

  bool operator==(const OnBend &) const = default;
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

using PathStop = std::variant<NewPoint, OnPoint, OnSegment, RepeatStop, OnDrawnSegment, OnBend>;

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

// Where MovePoint moves a point: a new position, another path point, a place on a segment, or a
// place on an arc.
using PointTarget = std::variant<NewPoint, OnPoint, OnSegment, OnBend>;

// The path point moves to the target, and every segment at it follows.
struct MovePoint {
  EntityKey Point = NULL_KEY;
  PointTarget To;

  bool operator==(const MovePoint &) const = default;
};

// Where InsertPoint puts a point: a place on a segment or a place on an arc.
using InsertPlace = std::variant<OnSegment, OnBend>;

// A new point at the place, splitting the segment or corner there.
struct InsertPoint {
  InsertPlace At;

  bool operator==(const InsertPoint &) const = default;
};

// The path point goes, and its two segments merge into one.
struct RemovePoint {
  EntityKey Point = NULL_KEY;

  bool operator==(const RemovePoint &) const = default;
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

// The segment's edge radius at the point, one of its two points, held exactly as given.
struct SetEdgeRadius {
  EntityKey Segment = NULL_KEY;
  EntityKey Point = NULL_KEY;
  double Radius = 0.0;

  bool operator==(const SetEdgeRadius &) const = default;
};

// The path point's taper length, held exactly as given.
struct SetTaperLength {
  EntityKey Point = NULL_KEY;
  double Length = 0.0;

  bool operator==(const SetTaperLength &) const = default;
};

// The shop's stock: what it sells.
struct SetShopStock {
  EntityKey Shop = NULL_KEY;
  ShopStock Stock = ShopStock::Food;

  bool operator==(const SetShopStock &) const = default;
};

// The park's opening hours.
struct SetOpeningHours {
  OpeningHours Hours;

  bool operator==(const SetOpeningHours &) const = default;
};

// The segment's furnishing on both sides.
struct SetFurnishing {
  EntityKey Segment = NULL_KEY;
  Furnishing To;

  bool operator==(const SetFurnishing &) const = default;
};

// A path segment's intent as RestoreIntent puts it: a ParkSegment without its ground line.
struct SegmentRecord {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  double FromEdge = DEFAULT_EDGE_RADIUS;
  double ToEdge = DEFAULT_EDGE_RADIUS;
  Furnishing Furnished;

  bool operator==(const SegmentRecord &) const = default;
};

// Path points, segments, and boxes, each list in ascending key order unless its user says
// otherwise, and the park's opening hours, when the records hold them.
struct IntentRecords {
  std::vector<ParkPathPoint> Points;
  std::vector<SegmentRecord> Segments;
  std::vector<ParkBox> Boxes;
  std::optional<OpeningHours> Hours;

  bool operator==(const IntentRecords &) const = default;
};

// Each segment's record, in ascending key order, into a list the caller keeps, which it clears
// first: what parkSegments gives for it, without its line. Doesn't derive the lines, and allocates
// nothing once the list has grown.
void segmentRecords(const World &world, std::vector<SegmentRecord> &records);

// Intent put back as an undo or redo finds it. The path points, segments, and boxes Remove names
// go. Each record in Set replaces the values of the one holding its key. Each record in Add is
// added with a key from the counter, in the order listed, points first, then segments, then boxes.
// Add's keys are names only: a segment that names one as From or To ends at the point added under
// it. When Set holds hours, they become the park's hours, set after the adds. Add holds none.
struct RestoreIntent {
  std::vector<EntityKey> Remove;
  IntentRecords Set;
  IntentRecords Add;

  bool operator==(const RestoreIntent &) const = default;
};

// Registers AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint,
// RemovePoint, SetCornerRadius, SetSegmentWidth, SetEdgeRadius, SetTaperLength, SetShopStock,
// SetOpeningHours, SetFurnishing, and RestoreIntent, in that order.
void addParkEdits(WorldSchema &schema);

// A tentative or committed edit to park intent: one of the sixteen commands.
using ParkEdit =
    std::variant<AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint,
                 RemovePoint, SetCornerRadius, SetSegmentWidth, SetEdgeRadius, SetTaperLength,
                 SetShopStock, SetOpeningHours, SetFurnishing, RestoreIntent>;

// The stop's ground position: a NewPoint's own, an OnPoint's point's, the place Along of the way
// from an OnSegment's segment's From point to its To point, or the place an OnBend names on its
// corner's arc. None when the stop names no path point, no segment whose two ends are path points,
// or no corner with a radius above 0 at a fraction strictly between 0 and 1, and for a RepeatStop
// or an OnDrawnSegment, which only its path places.
std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop);
// Each of the path's stops' ground positions, a RepeatStop's being its earlier stop's, and an
// OnDrawnSegment's the place Along of the way between its two stops. None when a stop has none, or
// a RepeatStop or an OnDrawnSegment names a stop that is not earlier.
std::optional<std::vector<ParkPoint>> stopPositions(const World &world, const AddPath &path);
// Appends the stop's ground position to the positions of the path's earlier stops, which positions
// holds, as stopPositions gives it, reading the corners from a list as parkCorners gives them.
// Gives false and appends nothing where stopPositions gives none. Allocates nothing once the list
// has grown.
bool appendStopPosition(const World &world, std::span<const ParkCorner> corners,
                        const PathStop &stop, std::vector<ParkPoint> &positions);
// The ground position of each point where the path splits a segment of its kind, or one of its
// own segments, because the two meet: a new point where they cross, or an end of one lying on the
// other, or a split of a corner where the path meets its arc. Leaves out the points of OnSegment,
// OnBend, and OnDrawnSegment stops. In the order the meetings
// are found. Empty when the world refuses the path.
std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path);
// The ground position of each point where the move or the merge splits a segment of its kind
// or an arc because the two meet, leaving out a MovePoint's OnSegment or OnBend target. In the
// order the meetings are found. Empty when the world refuses the command.
std::vector<ParkPoint> pathCrossings(const World &world, const MovePoint &move);
std::vector<ParkPoint> pathCrossings(const World &world, const RemovePoint &remove);

// Each segment the edit adds, or whose ground line or width it changes, as it would lie after the
// edit, in ascending key order, a new segment with the key the commit would give it. Given whether
// or not the world refuses the edit for meeting a solid. Empty when the edit names nothing it acts
// on, when a segment it adds or reshapes has no ground line, for edits of boxes, of the opening
// hours, and of furnishing, and for RestoreIntent, which shows no ghost.
std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit);

// The pieces and edge lines of each segment and point whose part of the surface the edit changes,
// as they would lie after the edit, in ascending key order of their owners. Empty when the edit
// names nothing it acts on, when a segment it adds or reshapes has no ground line, for edits of
// boxes, of the opening hours, and of furnishing, and for RestoreIntent, which shows no ghost.
PavedSurface editedSurface(const World &world, const ParkEdit &edit);

struct PlanLists;

// An edit planned on a world, which the caller keeps from frame to frame. planEdit fills it, and
// planning again into it allocates nothing once its lists have grown. It can be moved but not
// copied.
struct PlannedEdit {
  PlannedEdit();
  ~PlannedEdit();
  PlannedEdit(PlannedEdit &&) noexcept;
  PlannedEdit &operator=(PlannedEdit &&) noexcept;
  PlannedEdit(const PlannedEdit &) = delete;
  PlannedEdit &operator=(const PlannedEdit &) = delete;

  std::unique_ptr<PlanLists> Lists;
};

// Plans the edit on the world into planned, keeping the lists it works with in the buffers.
void planEdit(const World &world, const ParkEdit &edit, PlannedEdit &planned, TickBuffers &buffers);
// Whether the world the edit was planned on accepts it, as isAccepted on the edit gives.
bool isAccepted(const PlannedEdit &planned);
// The crossings pathCrossings on the edit gives, from its plan.
std::span<const ParkPoint> pathCrossings(const PlannedEdit &planned);
// Makes the change applyCommand makes for the edit, from its plan, without planning it again. The
// world is the one the edit was planned on, or a copy of it. Changes nothing when the plan is
// refused. Keeps the lists it works with in the buffers.
void applyPlanned(World &world, const ParkEdit &edit, const PlannedEdit &planned,
                  TickBuffers &buffers);

// What editedSegments and editedSurface give for an edit, kept by the caller.
struct EditedParts {
  std::vector<ParkSegment> Segments;
  PavedSurface Surface;
};
// Puts in parts what editedSegments and editedSurface give for the edit the plan holds, from the
// plan, clearing them first and keeping the lists it works with in the buffers.
void editedParts(const World &world, const PlannedEdit &planned, EditedParts &parts,
                 TickBuffers &buffers);
// As above, from the edit's candidate: a copy of the world, which is resolved, with the edit
// applied and then resolved once, as makeCandidate and the preview make it, and not resolved
// again since. It compares the paving and ground lines the candidate's last resolution derived
// with the world's, and derives none.
void editedParts(const World &world, const PlannedEdit &planned, const World &candidate,
                 EditedParts &parts, TickBuffers &buffers);

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
bool isAccepted(const World &world, const RemovePoint &command);
bool isAccepted(const World &world, const SetCornerRadius &command);
bool isAccepted(const World &world, const SetSegmentWidth &command);
bool isAccepted(const World &world, const SetEdgeRadius &command);
bool isAccepted(const World &world, const SetTaperLength &command);
bool isAccepted(const World &world, const SetShopStock &command);
bool isAccepted(const World &world, const SetOpeningHours &command);
bool isAccepted(const World &world, const SetFurnishing &command);
bool isAccepted(const World &world, const RestoreIntent &command);

// Whether the command the edit holds would be applied to the world.
bool isAccepted(const World &world, const ParkEdit &edit);

// Whether applying the edit can change a path point or segment: false for AddBox, MoveBox,
// DeleteBox, SetShopStock, and SetOpeningHours, and true for every other edit.
bool changesPaths(const ParkEdit &edit);

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
void applyCommand(World &world, const RemovePoint &command);
void applyCommand(World &world, const SetCornerRadius &command);
void applyCommand(World &world, const SetSegmentWidth &command);
void applyCommand(World &world, const SetEdgeRadius &command);
void applyCommand(World &world, const SetTaperLength &command);
void applyCommand(World &world, const SetShopStock &command);
void applyCommand(World &world, const SetOpeningHours &command);
void applyCommand(World &world, const SetFurnishing &command);
void applyCommand(World &world, const RestoreIntent &command);
// As above, keeping the lists applying works with in the buffers, so applying allocates nothing
// once they have grown.
void applyCommand(World &world, const AddPath &command, TickBuffers &buffers);
void applyCommand(World &world, const AddBox &command, TickBuffers &buffers);
void applyCommand(World &world, const MoveBox &command, TickBuffers &buffers);
void applyCommand(World &world, const DeleteSegment &command, TickBuffers &buffers);
void applyCommand(World &world, const DeleteBox &command, TickBuffers &buffers);
void applyCommand(World &world, const MovePoint &command, TickBuffers &buffers);
void applyCommand(World &world, const InsertPoint &command, TickBuffers &buffers);
void applyCommand(World &world, const RemovePoint &command, TickBuffers &buffers);
void applyCommand(World &world, const SetCornerRadius &command, TickBuffers &buffers);
void applyCommand(World &world, const SetSegmentWidth &command, TickBuffers &buffers);
void applyCommand(World &world, const SetEdgeRadius &command, TickBuffers &buffers);
void applyCommand(World &world, const SetTaperLength &command, TickBuffers &buffers);
void applyCommand(World &world, const SetShopStock &command, TickBuffers &buffers);
void applyCommand(World &world, const SetOpeningHours &command, TickBuffers &buffers);
void applyCommand(World &world, const SetFurnishing &command, TickBuffers &buffers);
void applyCommand(World &world, const RestoreIntent &command, TickBuffers &buffers);

// Whether every solid and line has its geometry, stays in the park, and conflicts with no other.
bool isPhysicallyValid(const World &world);

} // namespace tpj

#endif
