#ifndef TPJ_TOOLS_TOOLS_H
#define TPJ_TOOLS_TOOLS_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <array>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {

enum class ToolKind : uint8_t {
  None,
  GuestPath,
  BackstagePath,
  PlaceShop,
  PlaceDepot,
  PlaceBench,
  PlaceToilet,
  MoveBox,
  EditPath,
  RemovePoint,
  Delete,
  Furnish
};

// A place drag nearer than this to where the box lands keeps the facing it had, in meters.
inline constexpr double MIN_FACING_DRAG = 1.0;
// A path tool's press this near its last stop finishes the path, in meters.
inline constexpr double FINISH_REACH = 1.0;
// A snap catches the pointer this near it, and holds it until it moves this far, in screen pixels.
inline constexpr double SNAP_CATCH_PIXELS = 10.0;
inline constexpr double SNAP_RELEASE_PIXELS = 14.0;
// A path that snaps to a door ends this far clear of the door's face, in meters.
inline constexpr double DOOR_CLEARANCE = 0.25;
// A box that fronts paving has its front face this far outside it, in meters.
inline constexpr double FRONTAGE_SETBACK = 1.0;
// Where a box fronts a corner of the paving's edge lines that turns by this much or less, in
// radians, it turns with the pointer, as along a curve. At a sharper corner it lines up with one
// side.
inline constexpr double FRONTAGE_CORNER_TURN = std::numbers::pi / 6.0;
// A fronting box that doesn't fit where it first fronts tries places this far apart along the
// edge lines, in meters.
inline constexpr double FRONTAGE_STEP = 0.25;
// A fronting box that doesn't fit moves straight out by at most this, in meters, so the middle of
// its front face stays within CONNECTION_REACH of the paving it fronts.
inline constexpr double FRONTAGE_PUSH = CONNECTION_REACH - FRONTAGE_SETBACK;
// A press or hover this near a path point picks it, in meters.
inline constexpr double POINT_REACH = 2.0;
// Edit path keeps a corner's radius handle at least this far from its point, in meters.
inline constexpr double RADIUS_HANDLE_DEPTH = 3.0;
// Edit path keeps a taper handle at least this far from where the narrower segment's sides would
// stop without curves, in meters, unless its curves can be no longer.
inline constexpr double TAPER_HANDLE_DEPTH = 3.0;
// Edit path sets a taper length no shorter than this, in meters. A length of 0 asks for the curve's
// natural length.
inline constexpr double MIN_TAPER_LENGTH = 0.5;
// A press or hover this near a radius handle takes it, in meters.
inline constexpr double HANDLE_REACH = 1.0;
// A press or hover this near a segment's side takes it, in meters.
inline constexpr double SIDE_REACH = 0.5;
// Edit path sets a segment's width between these, in meters.
inline constexpr double MIN_PATH_WIDTH = 1.5;
inline constexpr double MAX_PATH_WIDTH = 16.0;
// The Furnish tool's spacing lies between these, in meters.
inline constexpr double FURNISH_MIN_SPACING = 2.0;
inline constexpr double FURNISH_MAX_SPACING = 30.0;

// What the Furnish tool lays: a kind, or none to remove furnishing, the spacing, and whether on
// both sides or on the side the pointer is on.
struct FurnishChoice {
  std::optional<FurnitureKind> Kind = FurnitureKind::Bench;
  double Spacing = 10.0;
  bool BothSides = true;

  bool operator==(const FurnishChoice &) const = default;
};

// What an Edit path handle is: a path point's marker, which sets nothing, or what it sets: a
// corner's radius, an edge's radius, a segment's width, or a taper's length.
enum class HandleKind : uint8_t { Point, Radius, Edge, Side, Taper };

// A handle Edit path shows or takes hold of: a path point's marker, keyed by its point, at its
// position; a corner's radius handle, keyed by the corner's point, at At; an edge handle, keyed by
// the edge's point and its segment, at At; a segment's sides, keyed by the segment; or a taper
// handle, keyed by its point and the wider segment, at At.
struct EditHandle {
  HandleKind Kind = HandleKind::Radius;
  EntityKey Key = NULL_KEY;
  ParkPoint At;
  EntityKey Segment = NULL_KEY;

  bool operator==(const EditHandle &) const = default;
};

// What a snap guide shows: a point snapped to, a door's line to where the path ends, a segment
// snapped onto, a direction from an anchor, or whole meters from an anchor.
enum class GuideKind : uint8_t { Point, Door, Segment, Direction, Length };

// A guide to draw. Point: the point at To. Door: from the door at From to the target at To.
// Segment: the straight part of a segment from From to To. Direction: from its anchor at From
// through To, running on past it. Length: from its anchor at From to To, with a tick at each whole
// meter.
struct SnapGuide {
  GuideKind Kind = GuideKind::Point;
  ParkPoint From;
  ParkPoint To;

  bool operator==(const SnapGuide &) const = default;
};

// One thing a snap holds to. Key names a path point, a door's entity, a segment, or a rounded
// corner's point. Index names a drawn stop, a drawn segment, a door's face, a guide's place in the
// snap's list of guides, or a whole number of meters.
enum class HoldKind : uint8_t {
  Point,
  DrawnStop,
  Door,
  Segment,
  Bend,
  DrawnSegment,
  Guide,
  Length
};

struct SnapHold {
  HoldKind Kind = HoldKind::Point;
  EntityKey Key = NULL_KEY;
  uint64_t Index = 0;

  bool operator==(const SnapHold &) const = default;
};

// The snap in effect: where the stop or point lands, the stop it makes, the first GuideCount
// guides it shows, and the first HoldCount things it holds to. While snapping is off, it lands at
// the pointer as a new point, with no guides and no holds. No snap has more than two of either, so
// a snap is a plain value.
struct PointerSnap {
  ParkPoint At;
  PathStop Stop;
  std::array<SnapGuide, 2> Guides{};
  size_t GuideCount = 0;
  std::array<SnapHold, 2> Holds{};
  size_t HoldCount = 0;

  bool operator==(const PointerSnap &) const = default;
};

// Where the pointer meets the ground, if it does; how many meters of ground a screen pixel spans
// there; and whether the player is holding the key that turns snapping off.
struct PointerSample {
  std::optional<ParkPoint> Ground;
  double MetersPerPixel = 0.0;
  bool SnapsOff = false;
};

// A segment whose length the ghost shows, from one end to the other.
struct MeasuredLength {
  ParkPoint From;
  ParkPoint To;

  bool operator==(const MeasuredLength &) const = default;
};

// One tool and what it holds. Change it only through selectTool, movePointer, pressPointer, and
// releasePointer.
struct ToolState {
  ToolKind Kind = ToolKind::None;
  // Where the pointer meets the ground, if it does.
  std::optional<ParkPoint> Pointer;
  // The pointer's last sample's meters per pixel and whether it turned snapping off.
  double MetersPerPixel = 0.0;
  bool SnapsOff = false;
  // The snap in effect while a path tool is chosen with the pointer on the ground, or while Edit
  // path drags a point. None otherwise.
  std::optional<PointerSnap> Snapped;
  bool Holding = false;
  // Whether the pointer has met the ground anywhere new since the press took hold.
  bool Dragged = false;
  // The place tools' facing for the next box.
  double FacingX = 0.0;
  double FacingZ = -1.0;
  // Holding with a place tool: where the box lands and how it faces.
  Pose Landing;
  // The place tools' box pose while it fronts a segment, found when the pointer moves while not
  // holding, and kept through a hold. None while it fronts none.
  std::optional<Pose> Fronted;
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
  // What the Furnish tool lays. Selecting a tool keeps it.
  FurnishChoice Furnish;
};

// The lists the tools reuse from call to call, private to the tools.
struct ToolLists;

// The park as the tools read it: its path points, segments, corners, edges, tapers, and boxes, in
// the order sim/park gives them, and the doors serving each kind, as parkDoors gives them, with the
// lists the tools reuse from call to call. The caller keeps it and fills it again with keepToolPark
// whenever the world's intent changes. It can be moved but not copied.
struct ToolPark {
  ToolPark();
  ~ToolPark();
  ToolPark(ToolPark &&) noexcept;
  ToolPark &operator=(ToolPark &&) noexcept;
  ToolPark(const ToolPark &) = delete;
  ToolPark &operator=(const ToolPark &) = delete;

  std::vector<ParkPathPoint> Points;
  std::vector<ParkSegment> Segments;
  std::vector<ParkCorner> Corners;
  std::vector<ParkEdge> Edges;
  std::vector<ParkTaper> Tapers;
  std::vector<ParkBox> Boxes;
  std::vector<ParkDoor> GuestDoors;
  std::vector<ParkDoor> BackstageDoors;
  std::unique_ptr<ToolLists> Lists;
};

// Fills the park from the world's intent.
void keepToolPark(const World &world, ToolPark &park);

// Sets the tool, dropping any hold without committing.
void selectTool(ToolState &tool, ToolKind kind);
// Records the pointer's sample: where it meets the ground, or none off the ground or over a panel,
// the meters a pixel spans there, and whether snapping is off. Then finds the snap in effect.
void movePointer(ToolState &tool, ToolPark &park, const World &world, const PointerSample &pointer);
// The primary button went down. A path tool adds no stop that would make the world refuse its path.
void pressPointer(ToolState &tool, ToolPark &park, const World &world);
// The primary button went up: the edit to commit, which is the tentative edit of the moment
// before when the press took hold.
std::optional<ParkEdit> releasePointer(ToolState &tool, ToolPark &park, const World &world);

// Sets what the Furnish tool lays.
void setFurnishChoice(ToolState &tool, const FurnishChoice &choice);

// Puts the edit the ghost shows in edit. If edit holds an AddPath and the tool's edit is one, it
// refills that AddPath's stops in place, so they allocate nothing once grown.
void tentativeEdit(const ToolState &tool, ToolPark &park, const World &world,
                   std::optional<ParkEdit> &edit);
// The edit the ghost shows.
std::optional<ParkEdit> tentativeEdit(const ToolState &tool, ToolPark &park, const World &world);
// The entity the tool marks.
std::optional<EntityKey> highlightedEntity(const ToolState &tool, const ToolPark &park,
                                           const World &world);

// The first box in key order whose footprint holds the point.
std::optional<EntityKey> boxAt(const ToolPark &park, ParkPoint point);
// The first segment in key order whose ground line passes within half its width of the point.
std::optional<EntityKey> segmentAt(const ToolPark &park, ParkPoint point);
// The path point nearest the point within POINT_REACH, of either kind. Ties go to the lower key.
std::optional<EntityKey> pointAt(const ToolPark &park, ParkPoint point);
// Where the corner's radius handle lies.
ParkPoint radiusHandleOf(const ParkCorner &corner);
// Where the taper's handle lies.
ParkPoint taperHandleOf(const ParkTaper &taper);
// The radius handle, edge handle, or taper handle that lies nearest the point within HANDLE_REACH,
// with At where it lies. Ties go to a radius handle, then to the lower point key, then to the
// lower segment key.
std::optional<EditHandle> handleAt(const ToolPark &park, ParkPoint point);
// The segment with a ground line whose side lies nearest the point within SIDE_REACH. Ties go to
// the lower key.
std::optional<EntityKey> sideAt(const ToolPark &park, ParkPoint point);
// The pose of a box of the kind that fronts the paving near the point: backstage paving for a
// depot, and guest paving for every other kind. None when no such paving is near.
std::optional<Pose> frontagePose(const World &world, BoxKind kind, ParkPoint point);
// As the three-argument form, reading the edge lines from the park, as keepToolPark fills them,
// and keeping its lists in the park's.
std::optional<Pose> frontagePose(const World &world, ToolPark &park, BoxKind kind, ParkPoint point);
// Every path point's marker, in key order, then every corner's radius handle, in key order of its
// point, then every edge handle, in the order parkEdges gives the edges, then every taper handle,
// in the order parkTapers gives the tapers, while EditPath is chosen and nothing is held. Only the
// markers while RemovePoint is chosen and nothing is held. Empty otherwise. Into a list the caller
// keeps, which it clears first.
void editHandles(const ToolState &tool, const ToolPark &park, std::vector<EditHandle> &handles);
// The radius, edge, or taper handle, or the segment's sides under the pointer that a press with
// EditPath would take, while nothing is held. None otherwise.
std::optional<EditHandle> hoveredHandle(const ToolState &tool, const ToolPark &park);
// The guides of the snap in effect, or none.
std::span<const SnapGuide> snapGuides(const ToolState &tool);
// From the last stop of the path being drawn to where a press would add the next, or from the
// other end of each segment at a dragged point to where it would land. Empty otherwise. Into a list
// the caller keeps, which it clears first.
void measuredLengths(const ToolState &tool, ToolPark &park, const World &world,
                     std::vector<MeasuredLength> &lengths);

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
std::optional<PathStart> pathStart(const ToolState &tool, ToolPark &park, const World &world);

// Where a press would finish and commit the drawn path: its last stop's position, when at least two
// stops are drawn and the pointer lies within FINISH_REACH of it. None otherwise.
std::optional<ParkPoint> pathFinish(const ToolState &tool, ToolPark &park, const World &world);

} // namespace tpj

#endif
