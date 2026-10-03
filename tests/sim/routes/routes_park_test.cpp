#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

TEST_CASE("tests/parks/routes.park loads with makeParkSchema and saves back to identical text") {
  const std::string text = routesText();
  CHECK(saveWorld(loadWorld(makeParkSchema(), text)) == text);
}

} // namespace
} // namespace tpj
