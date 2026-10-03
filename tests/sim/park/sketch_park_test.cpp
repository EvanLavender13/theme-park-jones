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

TEST_CASE("tests/parks/sketch.park loads with makeParkSchema and saves back to identical text") {
  const std::string text = sketchText();
  CHECK(saveWorld(loadWorld(makeParkSchema(), text)) == text);
}

} // namespace
} // namespace tpj
