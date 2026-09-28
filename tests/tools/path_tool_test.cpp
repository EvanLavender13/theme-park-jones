#include "support/tool_park.h"

#include "tools/tools.h"

#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::toolPark;

using Edit = std::optional<ParkEdit>;

constexpr std::optional<ParkPoint> NO_GROUND = std::nullopt;

ToolState toolOf(ToolKind kind) {
  ToolState tool;
  selectTool(tool, kind);
  return tool;
}

Edit addPath(PathKind kind, std::vector<ParkPoint> points) {
  return ParkEdit{AddPath{kind, std::move(points)}};
}

void clickAt(ToolState &tool, const World &world, ParkPoint at) {
  movePointer(tool, at);
  pressPointer(tool, world);
}

double distance(ParkPoint from, ParkPoint to) { return std::hypot(to.X - from.X, to.Z - from.Z); }

// Far from every path and box in the tool park, so snapping leaves them as they are.
constexpr ParkPoint OPEN_A{60.0, 60.0};
constexpr ParkPoint OPEN_B{80.0, 60.0};

TEST_CASE("A path tool's press with a ground position, unless it finishes, appends the pointer "
          "snapped for the tool's kind to its drawn points and takes no hold") {
  const World world = toolPark();
  // Within reach of both the crossing guest path and the backstage path.
  const ParkPoint nearBoth{-40.5, 1.5};
  REQUIRE(snapToPath(world, PathKind::Guest, nearBoth) !=
          snapToPath(world, PathKind::Backstage, nearBoth));

  for (const auto &[toolKind, pathKind] :
       {std::pair{ToolKind::GuestPath, PathKind::Guest},
        std::pair{ToolKind::BackstagePath, PathKind::Backstage}}) {
    INFO("path kind " << static_cast<int>(pathKind));
    ToolState tool = toolOf(toolKind);
    REQUIRE(tool.Drawn.empty());
    clickAt(tool, world, nearBoth);
    CHECK(tool.Drawn == std::vector{snapToPath(world, pathKind, nearBoth)});
    CHECK_FALSE(tool.Holding);
    clickAt(tool, world, OPEN_A);
    CHECK(tool.Drawn == std::vector{snapToPath(world, pathKind, nearBoth), OPEN_A});
    CHECK_FALSE(tool.Holding);
  }
}

TEST_CASE("A path tool not holding shows an AddPath through its drawn points and the snapped "
          "pointer when that lies farther than FINISH_REACH from the last, through the drawn "
          "points alone otherwise, and none below two points") {
  CHECK(FINISH_REACH == 1.0);
  const World world = toolPark();

  SECTION("by the pointer's distance from the last point") {
    ToolState tool = toolOf(ToolKind::GuestPath);
    movePointer(tool, ParkPoint{10.0, 10.0});
    CHECK_FALSE(tentativeEdit(tool, world).has_value());

    clickAt(tool, world, {10.0, 10.0});
    // Beside the crossing guest path, so the ghost shows the point snapped.
    const ParkPoint nearPath{-50.0, 1.2};
    REQUIRE(snapToPath(world, PathKind::Guest, nearPath) != nearPath);
    movePointer(tool, nearPath);
    CHECK(tentativeEdit(tool, world) ==
          addPath(PathKind::Guest, {{10.0, 10.0}, snapToPath(world, PathKind::Guest, nearPath)}));
    // Exactly FINISH_REACH from the last point, which is not farther.
    movePointer(tool, ParkPoint{10.0, 11.0});
    CHECK_FALSE(tentativeEdit(tool, world).has_value());
    movePointer(tool, NO_GROUND);
    CHECK_FALSE(tentativeEdit(tool, world).has_value());

    clickAt(tool, world, {30.0, 10.0});
    movePointer(tool, ParkPoint{30.0, 11.0});
    CHECK(tentativeEdit(tool, world) == addPath(PathKind::Guest, {{10.0, 10.0}, {30.0, 10.0}}));
    movePointer(tool, ParkPoint{30.0, 11.01});
    CHECK(tentativeEdit(tool, world) ==
          addPath(PathKind::Guest, {{10.0, 10.0}, {30.0, 10.0}, {30.0, 11.01}}));
    movePointer(tool, NO_GROUND);
    CHECK(tentativeEdit(tool, world) == addPath(PathKind::Guest, {{10.0, 10.0}, {30.0, 10.0}}));
  }

  SECTION("measured from the snapped pointer, not the pointer") {
    ToolState tool = toolOf(ToolKind::GuestPath);
    clickAt(tool, world, {-70.0, 10.0});
    // Snapped onto the crossing path.
    clickAt(tool, world, {-50.0, 0.5});
    REQUIRE(tool.Drawn.size() == 2);
    const ParkPoint pointer{-50.3, 1.9};
    REQUIRE(distance(pointer, tool.Drawn.back()) > FINISH_REACH);
    REQUIRE(distance(snapToPath(world, PathKind::Guest, pointer), tool.Drawn.back()) <
            FINISH_REACH);
    movePointer(tool, pointer);
    CHECK(tentativeEdit(tool, world) == addPath(PathKind::Guest, tool.Drawn));
  }
}

TEST_CASE("A press whose snapped pointer lies within FINISH_REACH of the last drawn point takes "
          "hold, and while it holds its edit is the AddPath through exactly the drawn points, "
          "whatever the pointer does") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::BackstagePath);
  clickAt(tool, world, OPEN_A);
  clickAt(tool, world, OPEN_B);
  const std::vector<ParkPoint> drawn{OPEN_A, OPEN_B};
  REQUIRE(tool.Drawn == drawn);

  SECTION("clicking the last point again") { clickAt(tool, world, OPEN_B); }
  SECTION("clicking within FINISH_REACH of it") { clickAt(tool, world, {80.6, 60.7}); }

  CHECK(tool.Holding);
  CHECK(tool.Drawn == drawn);
  const Edit finished = addPath(PathKind::Backstage, drawn);
  CHECK(tentativeEdit(tool, world) == finished);
  movePointer(tool, ParkPoint{-100.0, -100.0});
  CHECK(tentativeEdit(tool, world) == finished);
  movePointer(tool, NO_GROUND);
  CHECK(tentativeEdit(tool, world) == finished);
  clickAt(tool, world, {10.0, 10.0});
  CHECK(tentativeEdit(tool, world) == finished);
  CHECK(tool.Drawn == drawn);
}

TEST_CASE("A path tool finishes on the snapped pointer, not the pointer") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::GuestPath);
  clickAt(tool, world, {-70.0, 10.0});
  // Snapped onto the crossing path.
  clickAt(tool, world, {-50.0, 0.5});
  const std::vector<ParkPoint> drawn = tool.Drawn;
  REQUIRE(drawn.size() == 2);
  const ParkPoint pointer{-50.3, 1.9};
  REQUIRE(distance(pointer, drawn.back()) > FINISH_REACH);
  REQUIRE(distance(snapToPath(world, PathKind::Guest, pointer), drawn.back()) < FINISH_REACH);

  clickAt(tool, world, pointer);
  CHECK(tool.Holding);
  CHECK(tool.Drawn == drawn);
}

TEST_CASE("A path tool finishing on a lone first point holds no edit, whatever the pointer does") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::GuestPath);
  clickAt(tool, world, OPEN_A);
  clickAt(tool, world, OPEN_A);
  CHECK(tool.Holding);
  CHECK_FALSE(tentativeEdit(tool, world).has_value());
  movePointer(tool, OPEN_B);
  CHECK_FALSE(tentativeEdit(tool, world).has_value());
}

TEST_CASE("After a release a path tool has no drawn points") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::GuestPath);

  SECTION("after committing a path") {
    clickAt(tool, world, OPEN_A);
    clickAt(tool, world, OPEN_B);
    clickAt(tool, world, OPEN_B);
    REQUIRE(releasePointer(tool, world) == addPath(PathKind::Guest, {OPEN_A, OPEN_B}));
  }
  SECTION("after finishing on a lone first point, which commits nothing") {
    clickAt(tool, world, OPEN_A);
    clickAt(tool, world, OPEN_A);
    REQUIRE_FALSE(releasePointer(tool, world).has_value());
  }

  CHECK(tool.Drawn.empty());
  CHECK_FALSE(tool.Holding);
  movePointer(tool, ParkPoint{10.0, 10.0});
  CHECK_FALSE(tentativeEdit(tool, world).has_value());
}

TEST_CASE("selectTool leaves a path tool with no drawn points, so no later release commits the "
          "points drawn before it") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::GuestPath);
  clickAt(tool, world, OPEN_A);
  clickAt(tool, world, OPEN_B);
  const ParkPoint nextA{10.0, 10.0};
  const ParkPoint nextB{30.0, 10.0};

  // Draws two new points and finishes on the second.
  const auto drawAgain = [&] {
    clickAt(tool, world, nextA);
    clickAt(tool, world, nextB);
    clickAt(tool, world, nextB);
    return releasePointer(tool, world);
  };

  SECTION("the same kind") {
    selectTool(tool, ToolKind::GuestPath);
    CHECK(tool.Drawn.empty());
    CHECK(drawAgain() == addPath(PathKind::Guest, {nextA, nextB}));
  }
  SECTION("the other path kind") {
    selectTool(tool, ToolKind::BackstagePath);
    CHECK(tool.Drawn.empty());
    CHECK(drawAgain() == addPath(PathKind::Backstage, {nextA, nextB}));
  }
  SECTION("while holding a finished path") {
    clickAt(tool, world, OPEN_B);
    REQUIRE(tool.Holding);
    selectTool(tool, ToolKind::GuestPath);
    CHECK(tool.Drawn.empty());
    CHECK_FALSE(releasePointer(tool, world).has_value());
  }
}

TEST_CASE("A path tool highlights nothing") {
  const World world = toolPark();
  ToolState tool = toolOf(ToolKind::GuestPath);
  // Over a box, and over a path of the tool's kind.
  for (const ParkPoint over : {ParkPoint{47.0, 0.0}, ParkPoint{-50.0, 1.0}}) {
    INFO("pointer at " << over.X << ", " << over.Z);
    movePointer(tool, over);
    CHECK_FALSE(highlightedEntity(tool, world).has_value());
  }
  clickAt(tool, world, OPEN_A);
  clickAt(tool, world, OPEN_A);
  REQUIRE(tool.Holding);
  movePointer(tool, ParkPoint{47.0, 0.0});
  CHECK_FALSE(highlightedEntity(tool, world).has_value());
}

} // namespace
} // namespace tpj
