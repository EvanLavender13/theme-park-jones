// The world-as-value standards over the park schema's real types. The checked worlds are every
// park file directly in tests/parks/, opened as the app opens it and stepped, and the worlds a few
// edits of warm.park reach, each chosen for what it makes the capabilities do. A park file checked
// in there is covered with no change here, and a type the park schema registers that no checked
// world holds fails the checks, so a capability's new state is covered once it is registered.

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// Cycles a park is stepped for its stepped world, and each of two runs is stepped: a second of
// play, in which warm.park's guests walk.
constexpr uint64_t CYCLES = 30;

// A registered type no checked world can hold, and why.
struct UnheldType {
  std::string_view Name;
  std::string_view Reason;
};

constexpr std::array UNHELD_TYPES{
    UnheldType{"previous-network",
               "no resolution ends holding a previous network, so no world between cycles, its "
               "copies and saves, or any candidate holds one"},
};

// A second shop beside warm.park's shop, facing the guest segments along x = 0 and backing onto
// the backstage segment along x = 12: its front door, at (3.5, 106.8), is 3.5 m from the guest
// segments, and its back door, at (9.5, 106.8), is 2.5 m from the backstage segment, so both doors
// are within a connector's reach. Its footprint, z from 102.8 to 110.8, clears the first shop's,
// which starts at z = 111, and the guest segment along z = 100.
constexpr Pose SECOND_SHOP{6.5, 106.8, -1.0, 0.0};

struct ParkFile {
  std::string Name;
  std::string Text;
};

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Every .park file directly in tests/parks/, in name order, found by listing the directory.
std::vector<ParkFile> parkFiles() {
  std::vector<std::filesystem::path> paths;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::directory_iterator(std::filesystem::path(TPJ_PARKS_DIR))) {
    if (entry.is_regular_file() && entry.path().extension() == ".park") {
      paths.push_back(entry.path());
    }
  }
  std::ranges::sort(paths);
  std::vector<ParkFile> parks;
  for (const std::filesystem::path &path : paths) {
    parks.push_back(ParkFile{path.filename().string(), readText(path)});
    REQUIRE_FALSE(parks.back().Text.empty());
  }
  REQUIRE_FALSE(parks.empty());
  return parks;
}

// A park's text as the app opens it: loaded with the park's schema and resolved.
World openPark(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

World openWarmPark() {
  const std::string text = readText(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park");
  REQUIRE_FALSE(text.empty());
  return openPark(text);
}

World steppedOnce(const World &world) {
  World stepped = copyWorld(world);
  stepWorld(stepped);
  return stepped;
}

CommandQueue queueOf(const ParkEdit &edit) {
  CommandQueue commands;
  queueEdit(commands, edit);
  return commands;
}

std::vector<GuestRecord> guestsOf(const World &world) {
  std::vector<GuestRecord> records;
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    REQUIRE(record.has_value());
    records.push_back(record.value_or(GuestRecord{}));
  }
  return records;
}

bool someGuest(const World &world, GuestActivity activity, EntityKey target) {
  return std::ranges::any_of(guestsOf(world), [&](const GuestRecord &record) {
    return record.Activity == activity && record.Target == target;
  });
}

// The guest segments that guests heading to a shop stand on, in key order of the guests, each
// once.
std::vector<EntityKey> guestSegmentsUnderGuestsHeadingToShop(const World &world) {
  std::vector<EntityKey> keys;
  for (const GuestRecord &record : guestsOf(world)) {
    if (record.Activity != GuestActivity::HeadingToShop) {
      continue;
    }
    for (const ParkSegment &segment : parkSegments(world)) {
      if (segment.Kind == PathKind::Guest && segment.Key == record.At.Carrier &&
          std::ranges::find(keys, segment.Key) == keys.end()) {
        keys.push_back(segment.Key);
      }
    }
  }
  return keys;
}

// A guest segment that a guest heading to a shop stands on.
EntityKey guestSegmentUnderGuestHeadingToShop(const World &world) {
  const std::vector<EntityKey> keys = guestSegmentsUnderGuestsHeadingToShop(world);
  return keys.empty() ? NULL_KEY : keys.front();
}

EntityKey firstShop(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      return box.Key;
    }
  }
  return NULL_KEY;
}

// An edit of warm.park, named for why it was chosen.
struct ChosenEdit {
  std::string Name;
  ParkEdit Edit;
};

// Guests standing on the segment, some heading to the shop, are carried off a network that loses a
// carrier, and route distance and the networks are derived again.
ChosenEdit deletingGuestSegment(const World &before) {
  const EntityKey segment = guestSegmentUnderGuestHeadingToShop(before);
  REQUIRE(segment != NULL_KEY);
  return {"deleting a guest segment guests stand on, heading to the shop", DeleteSegment{segment}};
}

// The unit direction square to the segment, from its From point to its To point turned a quarter.
ParkPoint squareToSegment(const World &world, EntityKey segmentKey) {
  std::optional<ParkPoint> from;
  std::optional<ParkPoint> to;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key == segmentKey) {
      from = stopPosition(world, OnPoint{segment.From});
      to = stopPosition(world, OnPoint{segment.To});
    }
  }
  REQUIRE(from.has_value());
  REQUIRE(to.has_value());
  const ParkPoint fromPoint = from.value_or(ParkPoint{});
  const ParkPoint toPoint = to.value_or(ParkPoint{});
  const double alongX = toPoint.X - fromPoint.X;
  const double alongZ = toPoint.Z - fromPoint.Z;
  const double length = std::sqrt((alongX * alongX) + (alongZ * alongZ));
  REQUIRE(length > 0.0);
  return {-alongZ / length, alongX / length};
}

// The segment guests stand on splits at its middle and its far part takes a new key, so the guests
// there are carried onto the new carrier, and the networks and route distance are derived again.
// The new point lies 6 m from the middle, square to the segment, on whichever side the world
// accepts, so the path meets nothing but the segment it ends on.
ChosenEdit addingPathEndingOnGuestSegment(const World &before) {
  const EntityKey segmentKey = guestSegmentUnderGuestHeadingToShop(before);
  REQUIRE(segmentKey != NULL_KEY);
  const OnSegment middle{segmentKey, 0.5};
  const std::optional<ParkPoint> middlePosition = stopPosition(before, middle);
  REQUIRE(middlePosition.has_value());
  const ParkPoint middlePoint = middlePosition.value_or(ParkPoint{});
  const ParkPoint square = squareToSegment(before, segmentKey);
  constexpr double OFFSET = 6.0;
  for (const double side : {1.0, -1.0}) {
    const ParkPoint start{middlePoint.X + (side * OFFSET * square.X),
                          middlePoint.Z + (side * OFFSET * square.Z)};
    const AddPath add{PathKind::Guest, {NewPoint{start}, middle}};
    if (isAccepted(before, ParkEdit{add})) {
      return {"a guest path ending on the middle of a segment guests stand on, splitting the "
              "segment under them",
              add};
    }
  }
  FAIL("neither side of the guest segment accepts a path ending on its middle");
  return {};
}

// A guest path drawn across a segment guests stand on joins it where they cross: both split at a
// new point the path adds, so the guests on the segment's far part are carried onto a new carrier,
// and the networks and route distance are derived again. The path's ends lie 6 m from the
// segment's middle, square to it, one on each side. The segment is the first under a guest heading
// to a shop where the world accepts such a path, since one may run into a box.
ChosenEdit addingPathCrossingGuestSegment(const World &before) {
  for (const EntityKey segmentKey : guestSegmentsUnderGuestsHeadingToShop(before)) {
    const std::optional<ParkPoint> middlePosition =
        stopPosition(before, OnSegment{segmentKey, 0.5});
    REQUIRE(middlePosition.has_value());
    const ParkPoint middlePoint = middlePosition.value_or(ParkPoint{});
    const ParkPoint square = squareToSegment(before, segmentKey);
    constexpr double OFFSET = 6.0;
    const ParkPoint oneSide{middlePoint.X + (OFFSET * square.X),
                            middlePoint.Z + (OFFSET * square.Z)};
    const ParkPoint otherSide{middlePoint.X - (OFFSET * square.X),
                              middlePoint.Z - (OFFSET * square.Z)};
    const AddPath add{PathKind::Guest, {NewPoint{oneSide}, NewPoint{otherSide}}};
    if (!isAccepted(before, ParkEdit{add})) {
      continue;
    }
    // Without a crossing, this edit would only add a free-standing path and check nothing new.
    REQUIRE_FALSE(pathCrossings(before, add).empty());
    return {"a guest path crossing the middle of a segment guests stand on, splitting the segment "
            "under them",
            add};
  }
  FAIL("no guest segment under a guest heading to a shop accepts a path crossing its middle");
  return {};
}

// A new shop is connected to both networks, supplied, and offered.
ChosenEdit addingShop(const World &before) {
  const AddBox add{BoxKind::Shop, SECOND_SHOP};
  const EntityKey added{before.nextKey()};
  REQUIRE(nearestDepot(makeCandidate(before, queueOf(ParkEdit{add})), added).has_value());
  return {"adding a shop touching both a guest and the backstage path", add};
}

// The guests waiting at the shop and heading to it, and the supplies addressed to it, held and in
// transit, outlive the box.
ChosenEdit deletingShop(const World &before) {
  const EntityKey shop = firstShop(before);
  REQUIRE(shop != NULL_KEY);
  REQUIRE(someGuest(before, GuestActivity::Waiting, shop));
  REQUIRE(someGuest(before, GuestActivity::HeadingToShop, shop));
  const FlowAddressed supplies = addressedTo<Supplies>(before, shop);
  REQUIRE_FALSE(supplies.Packets.empty());
  REQUIRE_FALSE(supplies.Stocks.empty());
  return {"deleting the shop", DeleteBox{shop}};
}

bool someGuestStandsOn(const World &world, EntityKey segment) {
  return std::ranges::any_of(
      guestsOf(world), [&](const GuestRecord &record) { return record.At.Carrier == segment; });
}

// The first path point, in key order, that exactly two segments use, both of guest kind, where a
// guest stands on at least one of the two.
EntityKey guestPointBetweenTwoSegmentsUnderGuests(const World &world) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    std::vector<ParkSegment> atPoint;
    for (const ParkSegment &segment : segments) {
      if (segment.From == point.Key || segment.To == point.Key) {
        atPoint.push_back(segment);
      }
    }
    if (atPoint.size() != 2) {
      continue;
    }
    const bool bothGuest = std::ranges::all_of(
        atPoint, [](const ParkSegment &segment) { return segment.Kind == PathKind::Guest; });
    const bool guestOnOne = std::ranges::any_of(
        atPoint, [&](const ParkSegment &segment) { return someGuestStandsOn(world, segment.Key); });
    if (bothGuest && guestOnOne) {
      return point.Key;
    }
  }
  return NULL_KEY;
}

ParkPoint pointPosition(const World &world, EntityKey pointKey) {
  const std::optional<ParkPoint> position = stopPosition(world, OnPoint{pointKey});
  REQUIRE(position.has_value());
  return position.value_or(ParkPoint{});
}

// Both segments at the point follow it to a new shape and keep their keys, so the guests on them
// are carried onto the new shape, and the networks and route distance are derived again. The point
// moves 3 m along whichever axis direction the world accepts first.
ChosenEdit movingPointUnderGuests(const World &before) {
  const EntityKey pointKey = guestPointBetweenTwoSegmentsUnderGuests(before);
  REQUIRE(pointKey != NULL_KEY);
  const ParkPoint at = pointPosition(before, pointKey);
  constexpr double OFFSET = 3.0;
  const std::array<ParkPoint, 4> directions{ParkPoint{1.0, 0.0}, ParkPoint{-1.0, 0.0},
                                            ParkPoint{0.0, 1.0}, ParkPoint{0.0, -1.0}};
  for (const ParkPoint &direction : directions) {
    const MovePoint move{pointKey, NewPoint{ParkPoint{at.X + (OFFSET * direction.X),
                                                      at.Z + (OFFSET * direction.Z)}}};
    if (isAccepted(before, ParkEdit{move})) {
      return {"moving a guest point between two segments guests stand on, reshaping the segments "
              "under them",
              move};
    }
  }
  FAIL("no direction along an axis accepts moving the guest point 3 m");
  return {};
}

// A point inserted in the middle of the segment guests stand on splits it under them, so the
// guests on its far part are carried onto a new carrier, and the networks and route distance are
// derived again.
ChosenEdit insertingPointUnderGuests(const World &before) {
  const EntityKey segmentKey = guestSegmentUnderGuestHeadingToShop(before);
  REQUIRE(segmentKey != NULL_KEY);
  return {"inserting a point in the middle of a segment guests stand on, heading to the shop, "
          "splitting the segment under them",
          InsertPoint{OnSegment{segmentKey, 0.5}}};
}

// The point's two segments merge into one and the other segment goes, so the guests on it are
// carried onto the merged segment, and the networks and route distance are derived again.
ChosenEdit removingPointUnderGuests(const World &before) {
  const EntityKey pointKey = guestPointBetweenTwoSegmentsUnderGuests(before);
  REQUIRE(pointKey != NULL_KEY);
  const RemovePoint remove{pointKey};
  // Without a merge, this edit would only delete segments, as deletingGuestSegment already does.
  // The point merges its two segments when their far ends differ.
  std::vector<EntityKey> farEnds;
  for (const ParkSegment &segment : parkSegments(before)) {
    if (segment.From == pointKey || segment.To == pointKey) {
      farEnds.push_back(segment.From == pointKey ? segment.To : segment.From);
    }
  }
  REQUIRE(farEnds.size() == 2);
  REQUIRE(farEnds[0] != farEnds[1]);
  return {"removing a guest point between two segments guests stand on, merging the segments "
          "under them",
          remove};
}

// The first corner, in key order of its point, whose two segments are both of guest kind, where a
// guest stands on at least one of the two.
std::optional<ParkCorner> guestCornerUnderGuests(const World &world) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  for (const ParkCorner &corner : parkCorners(world)) {
    std::vector<ParkSegment> atPoint;
    for (const ParkSegment &segment : segments) {
      if (segment.From == corner.Point || segment.To == corner.Point) {
        atPoint.push_back(segment);
      }
    }
    if (atPoint.size() != 2) {
      continue;
    }
    const bool bothGuest = std::ranges::all_of(
        atPoint, [](const ParkSegment &segment) { return segment.Kind == PathKind::Guest; });
    const bool guestOnOne = std::ranges::any_of(
        atPoint, [&](const ParkSegment &segment) { return someGuestStandsOn(world, segment.Key); });
    if (bothGuest && guestOnOne) {
      return corner;
    }
  }
  return std::nullopt;
}

// Both segments at the corner keep their keys and take a new shape along the arc, so the guests on
// them are carried onto it, and the networks and route distance are derived again.
ChosenEdit roundingCornerUnderGuests(const World &before) {
  const std::optional<ParkCorner> corner = guestCornerUnderGuests(before);
  REQUIRE(corner.has_value());
  const ParkCorner found = corner.value_or(ParkCorner{});
  // With no radius that fits, the corner would stay sharp, and no ground line would change.
  REQUIRE(found.MaxRadius > 0.0);
  return {"rounding a guest corner between two segments guests stand on to half the largest radius "
          "that fits, reshaping the segments under them",
          SetCornerRadius{found.Point, found.MaxRadius / 2.0}};
}

// The segment keeps its key and its ground line and only its width changes, so the guests on it
// stay on the same carrier.
ChosenEdit narrowingSegmentUnderGuests(const World &before) {
  const EntityKey segmentKey = guestSegmentUnderGuestHeadingToShop(before);
  REQUIRE(segmentKey != NULL_KEY);
  std::optional<double> width;
  for (const ParkSegment &segment : parkSegments(before)) {
    if (segment.Key == segmentKey) {
      width = segment.Width;
    }
  }
  REQUIRE(width.has_value());
  return {"narrowing a guest segment guests stand on, heading to the shop, to half its width",
          SetSegmentWidth{segmentKey, width.value_or(0.0) / 2.0}};
}

// The first edge, in the order parkEdges gives, whose point three or more guest segments use,
// where a guest stands on one of those segments, and which has a radius above 0 that fits.
std::optional<ParkEdge> guestJunctionEdgeUnderGuests(const World &world) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  for (const ParkEdge &edge : parkEdges(world)) {
    if (!(edge.MaxRadius > 0.0)) {
      continue;
    }
    std::vector<ParkSegment> guestAtPoint;
    for (const ParkSegment &segment : segments) {
      if (segment.Kind == PathKind::Guest &&
          (segment.From == edge.Point || segment.To == edge.Point)) {
        guestAtPoint.push_back(segment);
      }
    }
    if (guestAtPoint.size() < 3) {
      continue;
    }
    const bool guestOnOne = std::ranges::any_of(guestAtPoint, [&](const ParkSegment &segment) {
      return someGuestStandsOn(world, segment.Key);
    });
    if (guestOnOne) {
      return edge;
    }
  }
  return std::nullopt;
}

// The segment keeps its key and its ground line, and only its edge radius at the junction changes,
// so the guests at the junction stay where they stand and the save holds a curved edge.
ChosenEdit curvingJunctionEdgeUnderGuests(const World &before) {
  const std::optional<ParkEdge> edge = guestJunctionEdgeUnderGuests(before);
  REQUIRE(edge.has_value());
  const ParkEdge found = edge.value_or(ParkEdge{});
  const SetEdgeRadius set{found.Segment, found.Point, found.MaxRadius / 2.0};
  REQUIRE(isAccepted(before, ParkEdit{set}));
  return {"curving an inside corner of a guest junction whose segments guests stand on to half the "
          "largest radius that fits, leaving the segments' ground lines as they were",
          set};
}

// The first path point, in the order parkPathPoints gives, that two or more guest segments use,
// where a guest stands on one of those segments.
EntityKey guestJunctionPointUnderGuests(const World &world) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    std::vector<ParkSegment> guestAtPoint;
    for (const ParkSegment &segment : segments) {
      if (segment.Kind == PathKind::Guest &&
          (segment.From == point.Key || segment.To == point.Key)) {
        guestAtPoint.push_back(segment);
      }
    }
    if (guestAtPoint.size() < 2) {
      continue;
    }
    const bool guestOnOne = std::ranges::any_of(guestAtPoint, [&](const ParkSegment &segment) {
      return someGuestStandsOn(world, segment.Key);
    });
    if (guestOnOne) {
      return point.Key;
    }
  }
  return NULL_KEY;
}

// The point keeps its key and position and only its taper length changes. No two segments in
// warm.park run straight on at a point, so no ground line or paving changes, the guests at the
// point stay where they stand, and the save holds a taper length above 0.
ChosenEdit settingTaperAtGuestJunctionUnderGuests(const World &before) {
  const EntityKey pointKey = guestJunctionPointUnderGuests(before);
  REQUIRE(pointKey != NULL_KEY);
  constexpr double LENGTH = 4.0;
  const SetTaperLength set{pointKey, LENGTH};
  REQUIRE(isAccepted(before, ParkEdit{set}));
  // Without the length held on the point, the checked worlds would hold no taper but 0.
  const World candidate = makeCandidate(before, queueOf(ParkEdit{set}));
  const std::vector<ParkPathPoint> points = parkPathPoints(candidate);
  const auto found = std::ranges::find_if(
      points, [&](const ParkPathPoint &point) { return point.Key == pointKey; });
  REQUIRE(found != points.end());
  REQUIRE(found->Taper == LENGTH);
  return {"setting a taper length on a guest junction point whose segments guests stand on, "
          "leaving the segments' ground lines and paving as they were",
          set};
}

// The shop keeps its key, kind, and pose, and only its stock setting changes, while guests wait at
// it and head to it, so the save holds a shop-stock line.
ChosenEdit settingShopToDrinkUnderGuests(const World &before) {
  const EntityKey shop = firstShop(before);
  REQUIRE(shop != NULL_KEY);
  REQUIRE(someGuest(before, GuestActivity::Waiting, shop));
  REQUIRE(someGuest(before, GuestActivity::HeadingToShop, shop));
  const SetShopStock set{shop, ShopStock::Drink};
  REQUIRE(isAccepted(before, ParkEdit{set}));
  // Without the setting held on the shop, this edit would leave the world as it was.
  const World candidate = makeCandidate(before, queueOf(ParkEdit{set}));
  const std::vector<ParkBox> boxes = parkBoxes(candidate);
  const auto found =
      std::ranges::find_if(boxes, [&](const ParkBox &box) { return box.Key == shop; });
  REQUIRE(found != boxes.end());
  REQUIRE(found->Stock == ShopStock::Drink);
  return {"setting the shop, while guests wait at it and head to it, to sell drink", set};
}

// The park's hours change from the default while guests are in the park, so the save holds an
// opening-hours line and nothing else in the world changes.
ChosenEdit settingOpeningHoursUnderGuests(const World &before) {
  REQUIRE_FALSE(parkGuests(before).empty());
  // 10:00 to 22:00.
  const OpeningHours hours{36000, 79200};
  REQUIRE(parkHours(before) != hours);
  const SetOpeningHours set{hours};
  REQUIRE(isAccepted(before, ParkEdit{set}));
  // Without the hours held by the park, this edit would leave the world as it was.
  const World candidate = makeCandidate(before, queueOf(ParkEdit{set}));
  REQUIRE(parkHours(candidate) == hours);
  return {"setting the park's opening hours to 10:00 to 22:00 while guests are in the park", set};
}

// The world's segment with the key, requiring that there is one.
ParkSegment segmentWithKey(const World &world, EntityKey key) {
  std::optional<ParkSegment> found;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key == key) {
      found = segment;
    }
  }
  REQUIRE(found.has_value());
  return found.value_or(ParkSegment{});
}

// The key of the candidate's segment with the old segment's kind, points, width, and edge radii,
// requiring that no segment there holds the old key.
std::optional<EntityKey> segmentEqualTo(const World &candidate, const ParkSegment &old,
                                        EntityKey oldKey) {
  std::optional<EntityKey> restoredKey;
  for (const ParkSegment &segment : parkSegments(candidate)) {
    REQUIRE(segment.Key != oldKey);
    if (segment.Kind == old.Kind && segment.From == old.From && segment.To == old.To &&
        segment.Width == old.Width && segment.FromEdge == old.FromEdge &&
        segment.ToEdge == old.ToEdge) {
      restoredKey = segment.Key;
    }
  }
  return restoredKey;
}

// The segment guests stand on goes and an equal one comes back under a fresh key, as an undo of
// its deletion puts it back, so the guests on it are carried onto the new carrier, and the
// networks and route distance are derived again. The added record is named by the world's next
// key, which holds no entity.
ChosenEdit restoringGuestSegmentUnderFreshKey(const World &before) {
  const EntityKey segmentKey = guestSegmentUnderGuestHeadingToShop(before);
  REQUIRE(segmentKey != NULL_KEY);
  const ParkSegment old = segmentWithKey(before, segmentKey);
  const EntityKey name{before.nextKey()};
  RestoreIntent restore;
  restore.Remove = {segmentKey};
  restore.Add.Segments = {
      SegmentRecord{name, old.Kind, old.From, old.To, old.Width, old.FromEdge, old.ToEdge}};
  REQUIRE(isAccepted(before, ParkEdit{restore}));
  // Without the segment back under a new key with guests on it, this edit would check nothing
  // that deletingGuestSegment doesn't.
  const World candidate = makeCandidate(before, queueOf(ParkEdit{restore}));
  const std::optional<EntityKey> restoredKey = segmentEqualTo(candidate, old, segmentKey);
  REQUIRE(restoredKey.has_value());
  const EntityKey restored = restoredKey.value_or(NULL_KEY);
  REQUIRE(static_cast<uint64_t>(restored) >= before.nextKey());
  REQUIRE(someGuestStandsOn(candidate, restored));
  return {"putting back a guest segment guests stand on under a fresh key, as an undo of its "
          "deletion does, carrying the guests on it onto the new key",
          restore};
}

// The edits of warm.park, made from the world as a cycle leaves it. Each requires the state it was
// chosen for, so a change to warm.park that loses that state fails here rather than quietly
// checking less.
std::vector<ChosenEdit> chosenEdits(const World &before) {
  std::vector<ChosenEdit> edits{deletingGuestSegment(before),
                                addingPathEndingOnGuestSegment(before),
                                addingPathCrossingGuestSegment(before),
                                addingShop(before),
                                deletingShop(before),
                                movingPointUnderGuests(before),
                                insertingPointUnderGuests(before),
                                removingPointUnderGuests(before),
                                roundingCornerUnderGuests(before),
                                narrowingSegmentUnderGuests(before),
                                curvingJunctionEdgeUnderGuests(before),
                                settingTaperAtGuestJunctionUnderGuests(before),
                                settingShopToDrinkUnderGuests(before),
                                settingOpeningHoursUnderGuests(before),
                                restoringGuestSegmentUnderFreshKey(before)};
  for (const ChosenEdit &chosen : edits) {
    INFO(chosen.Name);
    REQUIRE(isAccepted(before, chosen.Edit));
  }
  return edits;
}

// A world the checks hold, named for a failure's message.
struct CheckedWorld {
  std::string Name;
  World Value;
};

// Each park file opened and stepped, and for each chosen edit of warm.park, the candidate made
// with it from the world a cycle leaves and the world that queuing it for that cycle gives.
std::vector<CheckedWorld> checkedWorlds() {
  std::vector<CheckedWorld> worlds;
  for (const ParkFile &park : parkFiles()) {
    World opened = openPark(park.Text);
    World stepped = copyWorld(opened);
    for (uint64_t cycle = 0; cycle < CYCLES; ++cycle) {
      stepWorld(stepped);
    }
    worlds.push_back(CheckedWorld{park.Name + ", opened", std::move(opened)});
    worlds.push_back(CheckedWorld{park.Name + ", stepped", std::move(stepped)});
  }
  const World warm = openWarmPark();
  const World before = steppedOnce(warm);
  for (const ChosenEdit &chosen : chosenEdits(before)) {
    CommandQueue commands = queueOf(chosen.Edit);
    World candidate = makeCandidate(before, commands);
    World committed = copyWorld(warm);
    stepWorld(committed, commands);
    worlds.push_back(CheckedWorld{"warm.park, candidate of " + chosen.Name, std::move(candidate)});
    worlds.push_back(CheckedWorld{"warm.park, committing " + chosen.Name, std::move(committed)});
  }
  return worlds;
}

TEST_CASE("A copy of every checked world equals it and hashes equal") {
  for (const CheckedWorld &checked : checkedWorlds()) {
    INFO(checked.Name);
    const World copy = copyWorld(checked.Value);
    CHECK(worldsEqual(copy, checked.Value));
    CHECK(hashWorld(copy) == hashWorld(checked.Value));
  }
}

TEST_CASE("The save of every checked world loads back, resolves equal to the world saved, and "
          "saves again to identical text") {
  for (const CheckedWorld &checked : checkedWorlds()) {
    INFO(checked.Name);
    const std::string text = saveWorld(checked.Value);
    World loaded = loadWorld(makeParkSchema(), text);
    CHECK(saveWorld(loaded) == text);
    resolveWorld(loaded);
    CHECK(worldsEqual(loaded, checked.Value));
  }
}

// A preview is exact only if the candidate is the world its commit makes, whether a cycle commits
// it or, while the game is paused, commitCommands does.
TEST_CASE("Each chosen edit's candidate, made from warm.park as a cycle leaves it, equals the "
          "world that queuing the edit for that cycle gives and the world that committing it "
          "without a cycle gives") {
  const World warm = openWarmPark();
  const World before = steppedOnce(warm);
  for (const ChosenEdit &chosen : chosenEdits(before)) {
    INFO(chosen.Name);
    CommandQueue commands = queueOf(chosen.Edit);
    const World candidate = makeCandidate(before, commands);
    World committed = copyWorld(warm);
    stepWorld(committed, commands);
    CHECK(worldsEqual(candidate, committed));

    CommandQueue pausedCommands = queueOf(chosen.Edit);
    World committedPaused = copyWorld(before);
    commitCommands(committedPaused, pausedCommands);
    CHECK(worldsEqual(candidate, committedPaused));
    CHECK(committedPaused.Tick == before.Tick);
  }
}

// Whether some guest in both worlds stands at a different place in the second.
bool someGuestMoved(const World &start, const World &end) {
  std::map<EntityKey, Place> startPlaces;
  for (const EntityKey guest : parkGuests(start)) {
    startPlaces.emplace(guest, guestRecord(start, guest).value_or(GuestRecord{}).At);
  }
  for (const EntityKey guest : parkGuests(end)) {
    const auto found = startPlaces.find(guest);
    if (found != startPlaces.end() &&
        !(guestRecord(end, guest).value_or(GuestRecord{}).At == found->second)) {
      return true;
    }
  }
  return false;
}

// Two runs, each loaded from the text, stepped side by side, with the edit, if any, queued for the
// first cycle of each, so a result that depends on anything outside the world, such as an
// uninitialized value, an order by address, or a cache shared between worlds, shows as a
// difference.
bool twoRunsEndEqual(std::string_view text, const std::optional<ParkEdit> &edit,
                     bool &sawGuestMove) {
  const World start = openPark(text);
  World first = openPark(text);
  World second = openPark(text);
  for (uint64_t cycle = 0; cycle < CYCLES; ++cycle) {
    CommandQueue firstCommands;
    CommandQueue secondCommands;
    if (cycle == 0 && edit.has_value()) {
      queueEdit(firstCommands, *edit);
      queueEdit(secondCommands, *edit);
    }
    stepWorld(first, firstCommands);
    stepWorld(second, secondCommands);
  }
  sawGuestMove = sawGuestMove || someGuestMoved(start, first);
  return worldsEqual(first, second);
}

TEST_CASE("Two runs from the same park file, given the same edits at the same ticks, end in "
          "equal worlds") {
  bool sawGuestMove = false;
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    CHECK(twoRunsEndEqual(park.Text, std::nullopt, sawGuestMove));
  }
  const std::string warmText = readText(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park");
  for (const ChosenEdit &chosen : chosenEdits(steppedOnce(openWarmPark()))) {
    INFO("warm.park, " << chosen.Name);
    CHECK(twoRunsEndEqual(warmText, chosen.Edit, sawGuestMove));
  }
  CHECK(sawGuestMove);
}

TEST_CASE("Every component type the park schema registers is held by some checked world, except "
          "those named with the reason none can hold them") {
  std::set<std::string> held;
  for (const CheckedWorld &checked : checkedWorlds()) {
    const World &world = checked.Value;
    for (const ComponentType &type : world.schema().components()) {
      for (const EntityKey key : world.keys()) {
        if (type.Has(world.Registry, world.findEntity(key))) {
          held.insert(type.Name);
          break;
        }
      }
    }
  }
  const std::shared_ptr<const WorldSchema> schema = makeParkSchema();
  for (const ComponentType &type : schema->components()) {
    const bool named = std::ranges::any_of(
        UNHELD_TYPES, [&](const UnheldType &unheld) { return unheld.Name == type.Name; });
    if (named) {
      continue;
    }
    INFO("registered type held by no checked world: " << type.Name);
    CHECK(held.contains(type.Name));
  }
}

} // namespace
} // namespace tpj
