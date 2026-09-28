#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdint.h>
#include <string>

namespace tpj {
namespace {

std::string sketchText() {
  std::ifstream file(TPJ_PARKS_DIR "/sketch.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

TEST_CASE("tests/parks/sketch.park holds an entrance, both kinds of path, and both kinds of box") {
  const World sketch = loadWorld(makeParkSchema(), sketchText());
  const auto paths = parkPaths(sketch);
  const auto boxes = parkBoxes(sketch);
  CHECK_FALSE(parkEntrances(sketch).empty());
  CHECK(std::ranges::any_of(paths,
                            [](const ParkPath &path) { return path.Kind == PathKind::Guest; }));
  CHECK(std::ranges::any_of(paths,
                            [](const ParkPath &path) { return path.Kind == PathKind::Backstage; }));
  CHECK(std::ranges::any_of(boxes, [](const ParkBox &box) { return box.Kind == BoxKind::Shop; }));
  CHECK(std::ranges::any_of(boxes, [](const ParkBox &box) { return box.Kind == BoxKind::Depot; }));
}

TEST_CASE("tests/parks/sketch.park loads with makeParkSchema and saves back to identical text") {
  const std::string text = sketchText();
  CHECK(saveWorld(loadWorld(makeParkSchema(), text)) == text);
}

TEST_CASE("tests/parks/sketch.park is physically valid, with every path's ground line non-empty") {
  const World sketch = loadWorld(makeParkSchema(), sketchText());
  for (const ParkPath &path : parkPaths(sketch)) {
    INFO("path " << static_cast<uint64_t>(path.Key));
    CHECK_FALSE(groundLine(path.Points).empty());
  }
  CHECK(isPhysicallyValid(sketch));
}

} // namespace
} // namespace tpj
