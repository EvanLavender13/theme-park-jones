#include "app/input/interaction.h"
#include "app/session/park_file.h"
#include "legible/inspect.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <filesystem>
#include <optional>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

// The checked-in sketch park, which holds shops, a depot, and an entrance.
World sketchPark() {
  const std::string path = (std::filesystem::path(TPJ_PARKS_DIR) / "sketch.park").string();
  OpenedPark opened = openParkFile(path.c_str());
  if (!opened.Park.has_value()) {
    throw std::runtime_error(opened.Error);
  }
  return std::move(*opened.Park);
}

ParkBox firstBox(const World &world, BoxKind kind) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == kind) {
      return box;
    }
  }
  throw std::runtime_error("the park holds no box of the kind");
}

// Open ground in the sketch park, away from its paths and boxes.
constexpr ParkPoint OPEN_GROUND{-40.0, -60.0};
constexpr ParkPoint OTHER_GROUND{-40.0, -80.0};

constexpr PointerButtons PRESS{.Pressed = true, .Released = false};
constexpr PointerButtons RELEASE{.Pressed = false, .Released = true};
constexpr PointerButtons CLICK{.Pressed = true, .Released = true};
constexpr PointerButtons NO_BUTTONS{};

// The kind, the pointer, and the place tools' state.
void checkSamePlacing(const ToolState &actual, const ToolState &expected) {
  CHECK(actual.Kind == expected.Kind);
  CHECK(actual.Pointer == expected.Pointer);
  CHECK(actual.Holding == expected.Holding);
  CHECK(actual.FacingX == expected.FacingX);
  CHECK(actual.FacingZ == expected.FacingZ);
  CHECK(actual.Landing == expected.Landing);
}

// MoveBox's hold and a path tool's drawn points.
void checkSameMovingAndDrawing(const ToolState &actual, const ToolState &expected) {
  CHECK(actual.Held == expected.Held);
  CHECK(actual.HeldFrom == expected.HeldFrom);
  CHECK(actual.GrabOffset == expected.GrabOffset);
  CHECK(actual.Target == expected.Target);
  CHECK(actual.Drawn == expected.Drawn);
}

void checkSameTool(const ToolState &actual, const ToolState &expected) {
  checkSamePlacing(actual, expected);
  checkSameMovingAndDrawing(actual, expected);
}

// A guest path tool holding a finished path of two drawn points, and a shop as the subject.
void holdAndInspect(Interaction &interaction, const World &world, CommandQueue &commands) {
  interaction.selectTool(ToolKind::GuestPath);
  interaction.movePointer(OPEN_GROUND);
  interaction.useButtons(world, commands, PRESS);
  interaction.movePointer(OTHER_GROUND);
  interaction.useButtons(world, commands, PRESS);
  // A press on the last drawn point finishes the path, which takes hold.
  interaction.useButtons(world, commands, PRESS);
  interaction.pick(world, firstBox(world, BoxKind::Shop).Key);
}

TEST_CASE("An Interaction starts with the tool at start and no subject") {
  const Interaction interaction(3);

  CHECK(interaction.tool().Kind == ToolKind::None);
  CHECK_FALSE(interaction.tool().Holding);
  CHECK(interaction.tool().Drawn.empty());
  CHECK_FALSE(interaction.subject().has_value());
}

TEST_CASE("follow with the generation the interaction last saw changes nothing") {
  const World world = sketchPark();
  CommandQueue commands;
  Interaction interaction(3);
  uint64_t lastSeen = 3;
  SECTION("The generation it was constructed with") {}
  SECTION("The generation it last followed") {
    // A world replaced once must not reset the tool at every frame after.
    interaction.follow(4);
    lastSeen = 4;
  }
  holdAndInspect(interaction, world, commands);
  REQUIRE(interaction.tool().Holding);
  REQUIRE_FALSE(interaction.tool().Drawn.empty());
  REQUIRE(interaction.subject().has_value());
  const ToolState before = interaction.tool();
  const std::optional<InspectorSubject> subject = interaction.subject();

  interaction.follow(lastSeen);

  checkSameTool(interaction.tool(), before);
  CHECK(interaction.subject() == subject);
}

TEST_CASE("follow with another generation leaves the tool as selectTool of its own kind leaves it "
          "and forgets the subject") {
  const World world = sketchPark();
  CommandQueue commands;
  Interaction interaction(3);
  holdAndInspect(interaction, world, commands);
  REQUIRE(interaction.tool().Holding);
  REQUIRE_FALSE(interaction.tool().Drawn.empty());
  REQUIRE(interaction.subject().has_value());
  ToolState expected = interaction.tool();
  selectTool(expected, expected.Kind);

  interaction.follow(4);

  checkSameTool(interaction.tool(), expected);
  CHECK_FALSE(interaction.tool().Holding);
  CHECK(interaction.tool().Drawn.empty());
  CHECK_FALSE(interaction.subject().has_value());
}

// The queue holds exactly the command queueEdit pushes for the edit.
void checkQueued(const CommandQueue &commands, const ParkEdit &edit) {
  CommandQueue expected;
  queueEdit(expected, edit);
  REQUIRE(commands.commands().size() == expected.commands().size());
  REQUIRE(commands.commands().size() == 1);
  CHECK(commands.commands()[0].TypeId == expected.commands()[0].TypeId);
  std::visit(
      [&commands](const auto &command) {
        using Command = std::decay_t<decltype(command)>;
        const Command *queued = std::any_cast<Command>(&commands.commands()[0].Value);
        REQUIRE(queued != nullptr);
        CHECK(*queued == command);
      },
      edit);
}

TEST_CASE("useButtons gives the tool the frame's press as pressPointer does, then its release as "
          "releasePointer does, and queues the edit the release commits") {
  // A place tool takes hold on a press and commits on the release, so a release given before the
  // press would leave it holding and queue nothing.
  const World world = sketchPark();
  CommandQueue commands;
  Interaction interaction(3);
  ToolState expected;
  interaction.selectTool(ToolKind::PlaceShop);
  selectTool(expected, ToolKind::PlaceShop);
  interaction.movePointer(OPEN_GROUND);
  movePointer(expected, OPEN_GROUND);
  std::optional<ParkEdit> edit;

  SECTION("A press and a release in one frame") {
    interaction.useButtons(world, commands, CLICK);
    pressPointer(expected, world);
    edit = releasePointer(expected, world);
  }
  SECTION("A press in one frame and its release after a drag in a later one") {
    interaction.useButtons(world, commands, PRESS);
    pressPointer(expected, world);
    checkSameTool(interaction.tool(), expected);
    CHECK(commands.empty());
    interaction.movePointer(OTHER_GROUND);
    movePointer(expected, OTHER_GROUND);
    interaction.useButtons(world, commands, RELEASE);
    edit = releasePointer(expected, world);
  }

  checkSameTool(interaction.tool(), expected);
  REQUIRE(edit.has_value());
  if (edit.has_value()) {
    checkQueued(commands, *edit);
  }
}

TEST_CASE("useButtons queues nothing for a release that commits no edit") {
  const World world = sketchPark();
  CommandQueue commands;
  Interaction interaction(3);
  ToolState expected;
  interaction.selectTool(ToolKind::PlaceShop);
  selectTool(expected, ToolKind::PlaceShop);
  interaction.movePointer(OPEN_GROUND);
  movePointer(expected, OPEN_GROUND);

  interaction.useButtons(world, commands, RELEASE);
  REQUIRE_FALSE(releasePointer(expected, world).has_value());

  checkSameTool(interaction.tool(), expected);
  CHECK(commands.empty());
}

TEST_CASE("useButtons with neither a press nor a release changes nothing") {
  const World world = sketchPark();
  CommandQueue commands;
  Interaction interaction(3);
  interaction.selectTool(ToolKind::PlaceShop);
  interaction.movePointer(OPEN_GROUND);
  interaction.useButtons(world, commands, PRESS);
  REQUIRE(interaction.tool().Holding);
  const ToolState before = interaction.tool();

  interaction.useButtons(world, commands, NO_BUTTONS);

  checkSameTool(interaction.tool(), before);
  CHECK(commands.empty());
}

TEST_CASE("picks is true exactly when the frame's buttons hold a press and the tool is Look") {
  Interaction interaction(3);

  CHECK(interaction.picks(PRESS));
  CHECK(interaction.picks(CLICK));
  CHECK_FALSE(interaction.picks(RELEASE));
  CHECK_FALSE(interaction.picks(NO_BUTTONS));

  interaction.selectTool(ToolKind::MoveBox);

  CHECK_FALSE(interaction.picks(PRESS));
}

TEST_CASE("pick sets the subject as pickSubject does for the world and the entity") {
  // Nothing, a shop, another shop, a depot, the entrance, and nothing: picks that set, replace,
  // and keep the subject.
  const World world = sketchPark();
  std::vector<std::optional<EntityKey>> picked{std::nullopt};
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      picked.emplace_back(box.Key);
    }
  }
  REQUIRE(picked.size() >= 3);
  picked.emplace_back(firstBox(world, BoxKind::Depot).Key);
  picked.emplace_back(parkEntrances(world).at(0).Key);
  picked.emplace_back(std::nullopt);
  Interaction interaction(3);
  std::optional<InspectorSubject> expected;

  for (size_t index = 0; index < picked.size(); ++index) {
    INFO("pick " << index);
    interaction.pick(world, picked[index]);
    pickSubject(expected, world, picked[index]);

    CHECK(interaction.subject() == expected);
  }
  CHECK(expected.has_value());
}

TEST_CASE("forgetSubject leaves no subject") {
  const World world = sketchPark();
  Interaction interaction(3);
  interaction.pick(world, firstBox(world, BoxKind::Shop).Key);
  REQUIRE(interaction.subject().has_value());

  interaction.forgetSubject();

  CHECK_FALSE(interaction.subject().has_value());
}

TEST_CASE("selectTool, movePointer, tentativeEdit, and highlighted give what tools.h's functions "
          "give for the interaction's tool") {
  const World world = sketchPark();
  Interaction interaction(3);
  ToolState expected;

  SECTION("A place tool's edit at the pointer") {
    interaction.selectTool(ToolKind::PlaceShop);
    selectTool(expected, ToolKind::PlaceShop);
    interaction.movePointer(OPEN_GROUND);
    movePointer(expected, OPEN_GROUND);

    REQUIRE(tentativeEdit(expected, world).has_value());
    CHECK(interaction.tentativeEdit(world) == tentativeEdit(expected, world));
  }
  SECTION("MoveBox's highlight of the box under the pointer") {
    const Pose shop = firstBox(world, BoxKind::Shop).At;
    interaction.selectTool(ToolKind::MoveBox);
    selectTool(expected, ToolKind::MoveBox);
    interaction.movePointer(ParkPoint{shop.X, shop.Z});
    movePointer(expected, ParkPoint{shop.X, shop.Z});

    REQUIRE(highlightedEntity(expected, world).has_value());
    CHECK(interaction.highlighted(world) == highlightedEntity(expected, world));
  }
  SECTION("Selecting the tool again over drawn points") {
    CommandQueue commands;
    interaction.selectTool(ToolKind::GuestPath);
    selectTool(expected, ToolKind::GuestPath);
    interaction.movePointer(OPEN_GROUND);
    movePointer(expected, OPEN_GROUND);
    interaction.useButtons(world, commands, PRESS);
    pressPointer(expected, world);
    REQUIRE_FALSE(expected.Drawn.empty());

    interaction.selectTool(ToolKind::GuestPath);
    selectTool(expected, ToolKind::GuestPath);
  }

  checkSameTool(interaction.tool(), expected);
}

} // namespace
} // namespace tpj
