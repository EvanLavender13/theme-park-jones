#include "app/session/park_file.h"
#include "app/session/park_file_requests.h"
#include "app/session/park_session.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {
namespace {

std::filesystem::path scratchDirectory(std::string_view test) {
  const std::filesystem::path directory =
      std::filesystem::path(TPJ_APP_SCRATCH) / "park-session" / test;
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory);
  return directory;
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path &path, std::string_view text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

std::string sketchPath() { return (std::filesystem::path(TPJ_PARKS_DIR) / "sketch.park").string(); }

// The checked-in sketch park, which differs from the new park in every kind of intent.
World sketchPark() {
  OpenedPark opened = openParkFile(sketchPath().c_str());
  if (!opened.Park.has_value()) {
    throw std::runtime_error(opened.Error);
  }
  return std::move(*opened.Park);
}

// A shop on open ground in both the sketch and the new park, so stepping applies it.
void queueShop(CommandQueue &queue) {
  queueEdit(queue, AddBox{BoxKind::Shop, Pose{-40.0, -60.0, 0.0, -1.0}});
}

// Records what the session asks of the platform, and answers confirmReplace as told.
class RecordingEdge : public ParkFileEdge {
public:
  explicit RecordingEdge(bool confirms = false) : Confirms(confirms) {}

  bool confirmReplace(const std::string &path) override {
    Asked.push_back(path);
    return Confirms;
  }
  void reportError(const std::string &message) override { Errors.push_back(message); }

  bool Confirms;
  std::vector<std::string> Asked;
  std::vector<std::string> Errors;
};

TEST_CASE("A session's step gives the world stepWorld gives for the same world and queued "
          "commands, and empties the queue") {
  // The world steps only through stepWorld with the queued commands, so the session and the
  // simulation agree on every cycle.
  ParkSession session(sketchPark());
  World expected = sketchPark();
  CommandQueue expectedCommands;
  queueShop(session.commands());
  queueShop(expectedCommands);

  session.step();
  stepWorld(expected, expectedCommands);

  CHECK(worldsEqual(session.world(), expected));
  CHECK(session.commands().empty());
}

TEST_CASE("A session's generation changes exactly when the world is replaced, to a value it has "
          "not had") {
  const auto directory = scratchDirectory("generation");
  const std::string savePath = (directory / "saved.park").string();
  const std::string missingPath = (directory / "missing.park").string();
  RecordingEdge edge(true);
  ParkSession session(sketchPark());
  std::set<uint64_t> seen{session.generation()};
  const uint64_t current = session.generation();

  // Nothing here replaces the world.
  session.step();
  CHECK(session.generation() == current);
  session.useFileRequest(FileRequest{ParkAction::Save, savePath}, edge);
  CHECK(session.generation() == current);
  session.useFileRequest(FileRequest{ParkAction::Open, missingPath}, edge);
  CHECK(session.generation() == current);
  session.useFileRequest(FileRequest{}, edge);
  CHECK(session.generation() == current);

  // Each of these replaces it, the last returning to a world the session has held before, so a
  // generation derived from the world rather than from the replacement would repeat.
  const std::vector<FileRequest> replacements{
      FileRequest{ParkAction::New, ""},
      FileRequest{ParkAction::Open, sketchPath()},
      FileRequest{ParkAction::New, ""},
  };
  for (const FileRequest &request : replacements) {
    INFO("action " << static_cast<int>(request.Action) << " path " << request.Path);
    session.useFileRequest(request, edge);
    CHECK_FALSE(seen.contains(session.generation()));
    seen.insert(session.generation());
  }
}

TEST_CASE("useFileRequest with New makes the world resolvedNewPark() and empties the queue") {
  RecordingEdge edge;
  ParkSession session(sketchPark());
  queueShop(session.commands());

  session.useFileRequest(FileRequest{ParkAction::New, ""}, edge);

  CHECK(worldsEqual(session.world(), resolvedNewPark()));
  CHECK(session.commands().empty());
}

TEST_CASE("useFileRequest with Open of a file openParkFile opens makes the world that world and "
          "empties the queue") {
  RecordingEdge edge;
  ParkSession session(resolvedNewPark());
  queueShop(session.commands());
  const OpenedPark expected = openParkFile(sketchPath().c_str());
  REQUIRE(expected.Park.has_value());

  session.useFileRequest(FileRequest{ParkAction::Open, sketchPath()}, edge);

  if (expected.Park.has_value()) {
    CHECK(worldsEqual(session.world(), *expected.Park));
  }
  CHECK(session.commands().empty());
  CHECK(edge.Errors.empty());
}

TEST_CASE("useFileRequest with Open of a file openParkFile cannot open leaves the world and the "
          "queue and reports openParkFile's message once") {
  const auto directory = scratchDirectory("open-fails");
  const std::string path = (directory / "missing.park").string();
  RecordingEdge edge;
  ParkSession session(sketchPark());
  queueShop(session.commands());
  const World expected = sketchPark();

  session.useFileRequest(FileRequest{ParkAction::Open, path}, edge);

  CHECK(worldsEqual(session.world(), expected));
  CHECK(session.commands().commands().size() == 1);
  CHECK(edge.Errors == std::vector<std::string>{openParkFile(path.c_str()).Error});
}

TEST_CASE("useFileRequest with Save writes saveWorld of the world as it is to the path with the "
          "park extension, leaving the world and the queue") {
  // The queued shop is not yet in the world, so a save holding it would have saved a pending
  // command rather than the park.
  const auto directory = scratchDirectory("save-writes");
  const std::filesystem::path path = directory / "plain";
  RecordingEdge edge;
  ParkSession session(sketchPark());
  queueShop(session.commands());
  const World expected = sketchPark();

  session.useFileRequest(FileRequest{ParkAction::Save, path.string()}, edge);

  CHECK(readFile(path.string() + ".park") == saveWorld(expected));
  CHECK(worldsEqual(session.world(), expected));
  CHECK(session.commands().commands().size() == 1);
  CHECK(edge.Errors.empty());
}

TEST_CASE("When Save added the extension and a file exists there, it asks the edge to confirm, "
          "naming that path, and saves only when the edge confirms") {
  // The save dialog asked about the name as typed, not the name with the extension added.
  const auto directory = scratchDirectory("save-confirms");
  const std::string typed = (directory / "plain").string();
  const std::string extended = typed + ".park";
  constexpr std::string_view EARLIER = "an earlier file";
  writeFile(extended, EARLIER);
  ParkSession session(sketchPark());

  SECTION("The edge declines") {
    RecordingEdge edge(false);
    session.useFileRequest(FileRequest{ParkAction::Save, typed}, edge);

    CHECK(edge.Asked == std::vector<std::string>{extended});
    CHECK(readFile(extended) == EARLIER);
  }
  SECTION("The edge confirms") {
    RecordingEdge edge(true);
    session.useFileRequest(FileRequest{ParkAction::Save, typed}, edge);

    CHECK(edge.Asked == std::vector<std::string>{extended});
    CHECK(readFile(extended) == saveWorld(session.world()));
  }
}

TEST_CASE("Save asks the edge to confirm in no other case") {
  const auto directory = scratchDirectory("save-asks-not");
  ParkSession session(sketchPark());
  RecordingEdge edge(false);

  SECTION("The path already has an extension and a file exists there") {
    // The save dialog already asked about this exact name, so the save replaces it.
    const std::string path = (directory / "named.park").string();
    writeFile(path, "an earlier file");

    session.useFileRequest(FileRequest{ParkAction::Save, path}, edge);

    CHECK(readFile(path) == saveWorld(session.world()));
  }
  SECTION("The extension was added and no file exists there") {
    const std::string typed = (directory / "plain").string();

    session.useFileRequest(FileRequest{ParkAction::Save, typed}, edge);

    CHECK(readFile(typed + ".park") == saveWorld(session.world()));
  }

  CHECK(edge.Asked.empty());
}

} // namespace
} // namespace tpj
