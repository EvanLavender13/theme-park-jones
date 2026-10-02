#include "app/session/park_file.h"
#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace tpj {
namespace {

using Catch::Matchers::StartsWith;

std::filesystem::path scratchDirectory(std::string_view test) {
  const std::filesystem::path directory =
      std::filesystem::path(TPJ_APP_SCRATCH) / "park-file" / test;
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

// The checked-in sketch park, resolved and stepped past tick 0, so its save holds a tick the
// template's does not and paths and boxes of both kinds.
World resolvedSketchPark() {
  const std::string text = readFile(std::filesystem::path(TPJ_PARKS_DIR) / "sketch.park");
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  for (int tick = 0; tick < 3; ++tick) {
    stepWorld(world);
  }
  return world;
}

TEST_CASE("saveParkFile writes exactly saveWorld's text, replacing any file there") {
  const auto directory = scratchDirectory("save-writes-save");
  const std::string path = (directory / "sketch.park").string();
  const World world = resolvedSketchPark();
  const std::string expected = saveWorld(world);
  // Longer than the save, so a write that did not replace the file would leave its tail behind.
  writeFile(path, std::string(expected.size() * 4, 'x'));

  const std::string message = saveParkFile(world, path.c_str());

  CHECK(message.empty());
  CHECK(readFile(path) == expected);
}

TEST_CASE("A park saved with saveParkFile and opened with openParkFile is unchanged") {
  const auto directory = scratchDirectory("round-trip");
  const std::string path = (directory / "sketch.park").string();
  const World world = resolvedSketchPark();
  REQUIRE(saveParkFile(world, path.c_str()).empty());
  const std::string file = readFile(path);

  const OpenedPark opened = openParkFile(path.c_str());

  CHECK(opened.Error.empty());
  REQUIRE(opened.Park.has_value());
  if (opened.Park.has_value()) {
    // The walk covers whether resolution is pending, so an equal hash also shows the opened world
    // was resolved before anything could step it.
    CHECK(saveWorld(*opened.Park) == file);
    CHECK(hashWorld(*opened.Park) == hashWorld(world));
  }
}

TEST_CASE("openParkFile of a path it cannot read gives no world and a message naming the path") {
  const auto directory = scratchDirectory("open-unreadable");
  const std::string path = (directory / "missing.park").string();

  const OpenedPark opened = openParkFile(path.c_str());

  CHECK_FALSE(opened.Park.has_value());
  CHECK_THAT(opened.Error, StartsWith("Cannot read " + path + ": "));
}

TEST_CASE("openParkFile of text loadWorld refuses gives no world and a message naming the path and "
          "the load error") {
  const auto directory = scratchDirectory("open-unloadable");
  const std::string path = (directory / "broken.park").string();
  // Key 5 is not below next-key 3, an error past the header, so the file is read in full first.
  constexpr std::string_view BROKEN =
      "tpj-park 1\nseed 1\ntick 0\nnext-key 3\n\n[entities]\n1\n5\n";
  writeFile(path, BROKEN);
  std::string loadError;
  try {
    loadWorld(makeParkSchema(), BROKEN);
  } catch (const LoadError &error) {
    loadError = error.what();
  }
  REQUIRE_FALSE(loadError.empty());

  const OpenedPark opened = openParkFile(path.c_str());

  CHECK_FALSE(opened.Park.has_value());
  CHECK(opened.Error == "Cannot load " + path + ": " + loadError);
}

TEST_CASE("saveParkFile to a path it cannot write gives a message naming the path") {
  const auto directory = scratchDirectory("save-unwritable");
  // A file in a directory that does not exist cannot be created.
  const std::string path = (directory / "no-such-directory" / "sketch.park").string();
  const World world = resolvedSketchPark();

  const std::string message = saveParkFile(world, path.c_str());

  CHECK_THAT(message, StartsWith("Cannot save " + path + ": "));
}

TEST_CASE("withParkExtension adds .park exactly when the file name after the last separator holds "
          "no dot") {
  // A file name with no dot gains .park after the whole path.
  CHECK(withParkExtension("sketch") == "sketch.park");

  // A file name holding a dot comes back unchanged, whatever follows the dot, so the player's own
  // extension is kept, and wherever the dot falls, since the rule asks only for a dot.
  CHECK(withParkExtension("sketch.park") == "sketch.park");
  CHECK(withParkExtension("sketch.txt") == "sketch.txt");
  CHECK(withParkExtension(".sketch") == ".sketch");

  // Only the file name decides: putting a dotted directory in front of a name changes nothing but
  // the prefix, for either separator. Each directory holds the other separator, so a separator
  // that is not the last one must not end the directory part.
  const std::string slashDirectory = "a.b\\c.d/";
  const std::string backslashDirectory = "a.b/c.d\\";
  for (const std::string_view name : {"sketch", "sketch.txt"}) {
    INFO("file name: " << name);
    CHECK(withParkExtension(slashDirectory + std::string(name)) ==
          slashDirectory + withParkExtension(name));
    CHECK(withParkExtension(backslashDirectory + std::string(name)) ==
          backslashDirectory + withParkExtension(name));
  }
}

TEST_CASE("fileExists is true exactly when a file or directory exists at the path") {
  const auto directory = scratchDirectory("file-exists");
  const std::filesystem::path file = directory / "present.park";
  writeFile(file, "");
  const std::filesystem::path missing = directory / "missing.park";

  CHECK(fileExists(file.string().c_str()));
  CHECK(fileExists(directory.string().c_str()));
  CHECK_FALSE(fileExists(missing.string().c_str()));
}

} // namespace
} // namespace tpj
