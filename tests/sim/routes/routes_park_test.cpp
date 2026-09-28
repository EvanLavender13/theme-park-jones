#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdint.h>
#include <string>
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

TEST_CASE("tests/parks/routes.park is physically valid") {
  CHECK(isPhysicallyValid(loadWorld(makeParkSchema(), routesText())));
}

TEST_CASE("tests/parks/routes.park loads with makeParkSchema and saves back to identical text") {
  const std::string text = routesText();
  CHECK(saveWorld(loadWorld(makeParkSchema(), text)) == text);
}

TEST_CASE("Every node of tests/parks/routes.park's guest network is reachable from every other "
          "along its edges") {
  World routes = loadWorld(makeParkSchema(), routesText());
  resolveWorld(routes);
  const Network &guest = parkNetwork(routes, PathKind::Guest);
  REQUIRE(guest.nodeCount() > 0);

  // Edges join their two nodes whichever way they are walked, so reaching every node from node 0
  // reaches every node from every other.
  std::vector<bool> reached(guest.nodeCount(), false);
  reached[0] = true;
  bool grew = true;
  while (grew) {
    grew = false;
    for (const NetworkEdge &edge : guest.edges()) {
      if (reached[edge.From] != reached[edge.To]) {
        reached[edge.From] = true;
        reached[edge.To] = true;
        grew = true;
      }
    }
  }
  for (std::size_t node = 0; node < reached.size(); ++node) {
    INFO("node " << node);
    CHECK(reached[node]);
  }
}

} // namespace
} // namespace tpj
