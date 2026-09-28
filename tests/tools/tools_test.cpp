#include "support/tool_park.h"

#include "tools/tools.h"

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdint.h>
#include <string>

namespace tpj {
namespace {

using test::BACKSTAGE_PATH;
using test::CROSSING_PATH;
using test::DEPOT_ON_PATH;
using test::OVERLAPPING_SHOP;
using test::toolPark;
using test::TURNED_SHOP;

using Edit = std::optional<ParkEdit>;

constexpr std::optional<ParkPoint> NO_GROUND = std::nullopt;

ToolState toolOf(ToolKind kind) {
  ToolState tool;
  selectTool(tool, kind);
  return tool;
}

Edit addBox(BoxKind kind, Pose at) { return ParkEdit{AddBox{kind, at}}; }

// The tentative edit just before the release, which the release must give back exactly.
void checkReleaseGivesGhost(ToolState &tool, const World &world, bool accepted) {
  const Edit shown = tentativeEdit(tool, world);
  REQUIRE(shown.has_value());
  REQUIRE(isAccepted(world, shown.value_or(ParkEdit{})) == accepted);
  CHECK(releasePointer(tool, world) == shown);
}

TEST_CASE("The None tool gives no edit and no highlight") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::None);
  for (const ParkPoint over : {ParkPoint{47.0, 0.0}, ParkPoint{-50.0, 1.0}}) {
    INFO("pointer at " << over.X << ", " << over.Z);
    movePointer(tool, over);
    CHECK(!tentativeEdit(tool, world).has_value());
    CHECK(!highlightedEntity(tool, world).has_value());
  }
}

// Principles 5 and 8: the tool refuses nothing, and what is committed is what the ghost showed.
TEST_CASE("A release gives exactly the tentative edit of the moment before it when its press took "
          "hold, whether or not that edit is accepted") {
  const World world = toolPark();

  SECTION("PlaceShop, accepted") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{80.0, 80.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{80.0, 85.0});
    checkReleaseGivesGhost(tool, world, true);
  }
  SECTION("PlaceDepot, refused over a shop") {
    ToolState tool = toolOf(ToolKind::PlaceDepot);
    movePointer(tool, ParkPoint{40.0, 0.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{43.0, 4.0});
    checkReleaseGivesGhost(tool, world, false);
  }
  SECTION("MoveBox, accepted") {
    ToolState tool = toolOf(ToolKind::MoveBox);
    movePointer(tool, ParkPoint{47.0, 0.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{83.0, -40.0});
    checkReleaseGivesGhost(tool, world, true);
  }
  SECTION("MoveBox, refused onto the depot") {
    ToolState tool = toolOf(ToolKind::MoveBox);
    movePointer(tool, ParkPoint{47.0, 0.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{-37.0, 20.0});
    checkReleaseGivesGhost(tool, world, false);
  }
  SECTION("Delete, a box") {
    ToolState tool = toolOf(ToolKind::Delete);
    movePointer(tool, ParkPoint{-40.0, 20.0});
    pressPointer(tool, world);
    checkReleaseGivesGhost(tool, world, true);
  }
  SECTION("Delete, a path") {
    ToolState tool = toolOf(ToolKind::Delete);
    movePointer(tool, ParkPoint{-50.0, 1.0});
    pressPointer(tool, world);
    checkReleaseGivesGhost(tool, world, true);
  }
}

TEST_CASE("A release gives none when no press before it took hold, even while a ghost shows") {
  const World world = toolPark();

  SECTION("None, pressed over a box") {
    ToolState tool = toolOf(ToolKind::None);
    movePointer(tool, ParkPoint{47.0, 0.0});
    pressPointer(tool, world);
    CHECK(!releasePointer(tool, world).has_value());
  }
  SECTION("PlaceShop, pressed with no ground position") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, NO_GROUND);
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{80.0, 80.0});
    REQUIRE(tentativeEdit(tool, world).has_value());
    CHECK(!releasePointer(tool, world).has_value());
  }
  SECTION("MoveBox, pressed over no box") {
    ToolState tool = toolOf(ToolKind::MoveBox);
    movePointer(tool, ParkPoint{80.0, 80.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{47.0, 0.0});
    CHECK(!releasePointer(tool, world).has_value());
  }
  SECTION("PlaceShop, never pressed") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{80.0, 80.0});
    REQUIRE(tentativeEdit(tool, world).has_value());
    CHECK(!releasePointer(tool, world).has_value());
  }
}

TEST_CASE("After a release the tool holds nothing, so a second release gives none") {
  const World world = toolPark();

  SECTION("PlaceShop") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{80.0, 80.0});
    pressPointer(tool, world);
    REQUIRE(releasePointer(tool, world).has_value());
    REQUIRE(tentativeEdit(tool, world).has_value());
    CHECK(!releasePointer(tool, world).has_value());
  }
  SECTION("Delete, whose ghost still shows over the box") {
    ToolState tool = toolOf(ToolKind::Delete);
    movePointer(tool, ParkPoint{-40.0, 20.0});
    pressPointer(tool, world);
    REQUIRE(releasePointer(tool, world).has_value());
    REQUIRE(tentativeEdit(tool, world).has_value());
    CHECK(!releasePointer(tool, world).has_value());
  }
}

TEST_CASE("A press while holding changes nothing") {
  const World world = toolPark();

  SECTION("PlaceShop, pressed again away from where the box lands") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{80.0, 80.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{80.0, 85.0});
    const Edit before = tentativeEdit(tool, world);
    REQUIRE(before == addBox(BoxKind::Shop, Pose{80.0, 80.0, 0.0, 5.0}));
    pressPointer(tool, world);
    CHECK(tentativeEdit(tool, world) == before);
    CHECK(releasePointer(tool, world) == before);
  }
  SECTION("MoveBox, pressed again over another box") {
    ToolState tool = toolOf(ToolKind::MoveBox);
    movePointer(tool, ParkPoint{47.0, 0.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{38.0, 2.0});
    const Edit before = tentativeEdit(tool, world);
    REQUIRE(before == Edit{ParkEdit{MoveBox{OVERLAPPING_SHOP, Pose{35.0, 2.0, 0.0, -1.0}}}});
    pressPointer(tool, world);
    CHECK(tentativeEdit(tool, world) == before);
    CHECK(releasePointer(tool, world) == before);
  }
}

TEST_CASE("selectTool drops a hold without committing, and keeps the pointer and the place tools' "
          "facing") {
  const World world = toolPark();

  SECTION("a hold dropped keeps the facing from before it") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{80.0, 80.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{80.0, 85.0});
    REQUIRE(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{80.0, 80.0, 0.0, 5.0}));
    selectTool(tool, ToolKind::PlaceDepot);
    CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Depot, Pose{80.0, 85.0, 0.0, -1.0}));
    CHECK(!releasePointer(tool, world).has_value());
  }
  SECTION("a committed facing survives other tools") {
    ToolState tool = toolOf(ToolKind::PlaceShop);
    movePointer(tool, ParkPoint{10.0, 20.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{13.0, 24.0});
    REQUIRE(releasePointer(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 3.0, 4.0}));
    selectTool(tool, ToolKind::MoveBox);
    selectTool(tool, ToolKind::PlaceDepot);
    CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Depot, Pose{13.0, 24.0, 3.0, 4.0}));
  }
}

TEST_CASE("A place tool not holding shows an AddBox of its kind at the pointer with the tool's "
          "facing, and nothing off the ground") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::PlaceShop);
  movePointer(tool, ParkPoint{80.0, 80.0});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{80.0, 80.0, 0.0, -1.0}));

  selectTool(tool, ToolKind::PlaceDepot);
  movePointer(tool, ParkPoint{-70.25, 12.5});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Depot, Pose{-70.25, 12.5, 0.0, -1.0}));

  movePointer(tool, NO_GROUND);
  CHECK(!tentativeEdit(tool, world).has_value());
}

TEST_CASE("A place tool's press lands the box at the pointer, and while it holds the landing "
          "facing follows the pointer only from MIN_FACING_DRAG away") {
  CHECK(MIN_FACING_DRAG == 1.0);
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::PlaceShop);
  movePointer(tool, ParkPoint{10.0, 20.0});
  pressPointer(tool, world);
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 0.0, -1.0}));

  // Nearer than MIN_FACING_DRAG: the facing is kept, and the box stays where it landed.
  movePointer(tool, ParkPoint{10.6, 20.79});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 0.0, -1.0}));
  // Exactly MIN_FACING_DRAG away.
  movePointer(tool, ParkPoint{10.0, 21.0});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 0.0, 1.0}));
  // The facing is the pointer minus the landing position, not normalized.
  movePointer(tool, ParkPoint{13.0, 24.0});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 3.0, 4.0}));
  // Back nearer than MIN_FACING_DRAG, and then off the ground: the last facing is kept.
  movePointer(tool, ParkPoint{10.5, 20.5});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 3.0, 4.0}));
  movePointer(tool, NO_GROUND);
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Shop, Pose{10.0, 20.0, 3.0, 4.0}));
}

TEST_CASE("After a release, a place tool's facing is the one that release committed") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::PlaceDepot);
  movePointer(tool, ParkPoint{10.0, 20.0});
  pressPointer(tool, world);
  movePointer(tool, ParkPoint{8.0, 21.5});
  REQUIRE(releasePointer(tool, world) == addBox(BoxKind::Depot, Pose{10.0, 20.0, -2.0, 1.5}));

  movePointer(tool, ParkPoint{-70.0, -70.0});
  CHECK(tentativeEdit(tool, world) == addBox(BoxKind::Depot, Pose{-70.0, -70.0, -2.0, 1.5}));
}

TEST_CASE("The move tool not holding highlights the box under the pointer and shows no edit") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::MoveBox);
  const auto checkHover = [&](std::optional<ParkPoint> pointer, std::optional<EntityKey> box) {
    movePointer(tool, pointer);
    CHECK(highlightedEntity(tool, world) == box);
    CHECK(!tentativeEdit(tool, world).has_value());
  };
  checkHover(ParkPoint{47.0, 0.0}, OVERLAPPING_SHOP);
  // A box standing over a path, which the move tool marks rather than the path.
  checkHover(ParkPoint{-40.0, 20.0}, DEPOT_ON_PATH);
  // Over a path alone.
  checkHover(ParkPoint{-50.0, 1.0}, std::nullopt);
  checkHover(NO_GROUND, std::nullopt);
}

TEST_CASE("The move tool carries the box it pressed on by its grab point, keeping its facing, and "
          "shows no edit while the box is at its pose from the press") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::MoveBox);
  // Inside the turned shop alone, 2 m and -2 m from its position at (40, 0).
  movePointer(tool, ParkPoint{38.0, 2.0});
  pressPointer(tool, world);
  CHECK(!tentativeEdit(tool, world).has_value());

  // The pointer plus the offset (2, -2), with the shop's facing (2, 0) as it was given.
  const Edit moved = ParkEdit{MoveBox{TURNED_SHOP, Pose{64.0, 10.0, 2.0, 0.0}}};
  movePointer(tool, ParkPoint{62.0, 12.0});
  CHECK(tentativeEdit(tool, world) == moved);
  movePointer(tool, NO_GROUND);
  CHECK(tentativeEdit(tool, world) == moved);
  movePointer(tool, ParkPoint{38.0, 2.0});
  CHECK(!tentativeEdit(tool, world).has_value());
}

TEST_CASE("The move tool highlights nothing while it holds") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::MoveBox);
  // Over a box at the press, and over another where the carried box would land.
  movePointer(tool, ParkPoint{47.0, 0.0});
  pressPointer(tool, world);
  CHECK(!highlightedEntity(tool, world).has_value());
  movePointer(tool, ParkPoint{-37.0, 20.0});
  // The edit shows only while the tool holds the box.
  REQUIRE(tentativeEdit(tool, world).has_value());
  CHECK(!highlightedEntity(tool, world).has_value());
}

TEST_CASE("The delete tool shows DeleteBox for the box under the pointer, else DeletePath for the "
          "path under it, else nothing, holding or not, and highlights nothing") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::Delete);
  const auto checkShows = [&](std::optional<ParkPoint> pointer, Edit expected) {
    movePointer(tool, pointer);
    CHECK(tentativeEdit(tool, world) == expected);
    CHECK(!highlightedEntity(tool, world).has_value());
  };
  const auto checkAll = [&] {
    // A box standing over a path.
    checkShows(ParkPoint{-40.0, 20.0}, ParkEdit{DeleteBox{DEPOT_ON_PATH}});
    checkShows(ParkPoint{-50.0, 1.0}, ParkEdit{DeletePath{CROSSING_PATH}});
    checkShows(ParkPoint{-39.5, -30.0}, ParkEdit{DeletePath{BACKSTAGE_PATH}});
    checkShows(ParkPoint{100.0, 100.0}, std::nullopt);
    checkShows(NO_GROUND, std::nullopt);
  };

  SECTION("not holding") { checkAll(); }
  SECTION("holding, from a press with no ground position") {
    pressPointer(tool, world);
    checkAll();
  }
}

// Principle 10: tools read the world and never write it; edits reach it only through the queue.
TEST_CASE("Driving every tool leaves the world's save and hash unchanged") {
  const World world = toolPark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);

  for (const ToolKind kind : {ToolKind::None, ToolKind::PlaceShop, ToolKind::PlaceDepot,
                              ToolKind::MoveBox, ToolKind::Delete}) {
    ToolState tool = toolOf(kind);
    movePointer(tool, ParkPoint{47.0, 0.0});
    pressPointer(tool, world);
    movePointer(tool, ParkPoint{83.0, -40.0});
    static_cast<void>(tentativeEdit(tool, world));
    static_cast<void>(highlightedEntity(tool, world));
    static_cast<void>(releasePointer(tool, world));
  }
  static_cast<void>(boxAt(world, {47.0, 0.0}));
  static_cast<void>(pathAt(world, {-50.0, 1.0}));

  CHECK(saveWorld(world) == save);
  CHECK(hashWorld(world) == hash);
}

} // namespace
} // namespace tpj
