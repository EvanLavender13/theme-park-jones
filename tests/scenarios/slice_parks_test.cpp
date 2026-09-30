#include "scenarios/slice_parks.h"

#include "sim/command_queue.h"
#include "sim/field_text.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdint.h>
#include <string>
#include <string_view>

namespace tpj {
namespace {

std::string readPark(std::string_view name) {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / name, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

World openPark(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

TEST_CASE("makeSliceParks saves the opened park, after the warm-up, and after a cycle cutting "
          "every backstage path") {
  // A short warm-up that still admits a guest, so Warm holds stepped state as well as a later tick.
  constexpr uint64_t WARM = 60;
  const std::string fed = readPark("fed.park");
  REQUIRE_FALSE(fed.empty());

  World world = openPark(fed);
  const std::string expectedFed = saveWorld(world);
  for (uint64_t cycle = 0; cycle < WARM; ++cycle) {
    stepWorld(world);
  }
  const std::string expectedWarm = saveWorld(world);
  CommandQueue cuts;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Backstage) {
      cuts.push(DeletePath{path.Key});
    }
  }
  stepWorld(world, cuts);
  const std::string expectedCut = saveWorld(world);

  const SliceParks parks = makeSliceParks(fed, WARM);
  CHECK(parks.Fed == expectedFed);
  CHECK(parks.Warm == expectedWarm);
  CHECK(parks.Cut == expectedCut);
}

TEST_CASE("makeSliceParks throws LoadError for a text loadWorld refuses") {
  // Key 5 is not below next-key 3.
  constexpr std::string_view BROKEN =
      "tpj-park 1\nseed 1\ntick 0\nnext-key 3\n\n[entities]\n1\n5\n";
  CHECK_THROWS_AS(makeSliceParks(BROKEN, 1), LoadError);
}

TEST_CASE("the checked-in slice parks are makeSliceParks of fed.park with WARM_TICKS") {
  const std::string fed = readPark("fed.park");
  REQUIRE_FALSE(fed.empty());
  const SliceParks parks = makeSliceParks(fed, WARM_TICKS);
  CHECK(parks.Fed == fed);
  CHECK(parks.Warm == readPark("warm.park"));
  CHECK(parks.Cut == readPark("cut.park"));
}

} // namespace
} // namespace tpj
