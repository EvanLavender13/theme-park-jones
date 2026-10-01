#ifndef TPJ_TESTS_INTEGRATION_SUPPORT_PARK_FILES_H
#define TPJ_TESTS_INTEGRATION_SUPPORT_PARK_FILES_H

#include "sim/guests/guests.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

// The checked-in park files under tests/parks/, and opening them as the app and tpj_scenarios do.
namespace tpj::test {

// A park file's name, such as fed.park, and its text.
struct ParkFile {
  std::string Name;
  std::string Text;
};

inline std::string readParkText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The named file of tests/parks/.
inline ParkFile parkFile(std::string_view name) {
  ParkFile park{std::string(name),
                readParkText(std::filesystem::path(TPJ_PARKS_DIR) / std::string(name))};
  REQUIRE_FALSE(park.Text.empty());
  return park;
}

// Every .park file in tests/parks/, in name order, found by listing the directory, so a park
// checked in there is covered with no change to the tests.
inline std::vector<ParkFile> parkFiles() {
  std::vector<std::filesystem::path> paths;
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::directory_iterator(std::filesystem::path(TPJ_PARKS_DIR))) {
    if (entry.is_regular_file() && entry.path().extension() == ".park") {
      paths.push_back(entry.path());
    }
  }
  std::ranges::sort(paths);
  std::vector<ParkFile> parks;
  parks.reserve(paths.size());
  for (const std::filesystem::path &path : paths) {
    parks.push_back(ParkFile{path.filename().string(), readParkText(path)});
    REQUIRE_FALSE(parks.back().Text.empty());
  }
  REQUIRE_FALSE(parks.empty());
  return parks;
}

// The world a park's text opens as: loaded with makeParkSchema and resolved.
inline World openPark(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

inline World openPark(const ParkFile &park) { return openPark(park.Text); }

// The guest's inspection record, which the guest is required to have.
inline GuestRecord recordOf(const World &world, EntityKey guest) {
  const std::optional<GuestRecord> record = guestRecord(world, guest);
  REQUIRE(record.has_value());
  return record.value_or(GuestRecord{});
}

} // namespace tpj::test

#endif
