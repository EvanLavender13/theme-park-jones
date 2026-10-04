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
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
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

// A second shop beside warm.park's shop, facing guest path 2 along x = 0 and backing onto
// backstage path 6 along x = 12: its front door, at (3.5, 106.8), is 3.5 m from the guest path,
// and its back door, at (9.5, 106.8), is 2.5 m from the backstage path, so both doors are within
// a connector's reach. Its footprint, z from 102.8 to 110.8, clears the first shop's, which
// starts at z = 111, and guest path 5, which leaves guest path 2's end at z = 100.
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

// A guest path that a guest heading to a shop stands on.
EntityKey guestPathUnderGuestHeadingToShop(const World &world) {
  for (const GuestRecord &record : guestsOf(world)) {
    if (record.Activity != GuestActivity::HeadingToShop) {
      continue;
    }
    for (const ParkPath &path : parkPaths(world)) {
      if (path.Kind == PathKind::Guest && path.Key == record.At.Carrier) {
        return path.Key;
      }
    }
  }
  return NULL_KEY;
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

// Guests standing on the path, some heading to the shop, are carried off a network that loses a
// carrier, and route distance and the networks are derived again.
ChosenEdit deletingGuestPath(const World &before) {
  const EntityKey path = guestPathUnderGuestHeadingToShop(before);
  REQUIRE(path != NULL_KEY);
  return {"deleting a guest path guests stand on, heading to the shop", DeletePath{path}};
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

// The edits of warm.park, made from the world as a cycle leaves it. Each requires the state it was
// chosen for, so a change to warm.park that loses that state fails here rather than quietly
// checking less.
std::vector<ChosenEdit> chosenEdits(const World &before) {
  std::vector<ChosenEdit> edits{deletingGuestPath(before), addingShop(before),
                                deletingShop(before)};
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

// A park file is itself a save, so it too must save again to its own text; a hand-written one
// whose numbers a save spells unusually shows they read back to the same bits.
TEST_CASE("The save of every checked world loads back, resolves equal to the world saved, and "
          "saves again to identical text, as every park file does") {
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    CHECK(saveWorld(loadWorld(makeParkSchema(), park.Text)) == park.Text);
  }
  for (const CheckedWorld &checked : checkedWorlds()) {
    INFO(checked.Name);
    const std::string text = saveWorld(checked.Value);
    World loaded = loadWorld(makeParkSchema(), text);
    CHECK(saveWorld(loaded) == text);
    resolveWorld(loaded);
    CHECK(worldsEqual(loaded, checked.Value));
  }
}

// A preview is exact only if the candidate is the world its commit makes.
TEST_CASE("Each chosen edit's candidate, made from warm.park as a cycle leaves it, equals the "
          "world that queuing the edit for that cycle gives") {
  const World warm = openWarmPark();
  const World before = steppedOnce(warm);
  for (const ChosenEdit &chosen : chosenEdits(before)) {
    INFO(chosen.Name);
    CommandQueue commands = queueOf(chosen.Edit);
    const World candidate = makeCandidate(before, commands);
    World committed = copyWorld(warm);
    stepWorld(committed, commands);
    CHECK(worldsEqual(candidate, committed));
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
