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

TEST_CASE("tests/parks/routes.park is physically valid") {
  CHECK(isPhysicallyValid(loadWorld(makeParkSchema(), routesText())));
}

TEST_CASE("tests/parks/routes.park loads with makeParkSchema and saves back to identical text") {
  const std::string text = routesText();
  CHECK(saveWorld(loadWorld(makeParkSchema(), text)) == text);
}

TEST_CASE("Every door of tests/parks/routes.park has a connector to the network it serves") {
  World routes = loadWorld(makeParkSchema(), routesText());
  resolveWorld(routes);
  const std::vector<ParkEntrance> entrances = parkEntrances(routes);
  const std::vector<ParkBox> boxes = parkBoxes(routes);
  REQUIRE(entrances.size() == 1);
  REQUIRE(std::ranges::count_if(boxes,
                                [](const ParkBox &box) { return box.Kind == BoxKind::Shop; }) == 1);
  REQUIRE(std::ranges::count_if(
              boxes, [](const ParkBox &box) { return box.Kind == BoxKind::Depot; }) == 1);

  // The entrance's front, the shop's front and back, and the depot's front.
  std::vector<std::tuple<EntityKey, Face, PathKind>> doors{
      {entrances.front().Key, Face::Front, PathKind::Guest}};
  for (const ParkBox &box : boxes) {
    if (box.Kind == BoxKind::Shop) {
      doors.emplace_back(box.Key, Face::Front, PathKind::Guest);
      doors.emplace_back(box.Key, Face::Back, PathKind::Backstage);
    } else {
      doors.emplace_back(box.Key, Face::Front, PathKind::Backstage);
    }
  }
  for (const auto &[entity, face, kind] : doors) {
    INFO("entity " << static_cast<uint64_t>(entity) << ", face " << static_cast<int>(face));
    const std::vector<Carrier> &carriers = parkNetwork(routes, kind).carriers();
    CHECK(std::ranges::any_of(carriers, [key = connectorKey(entity, face)](const Carrier &carrier) {
      return carrier.Key == key;
    }));
  }
}

TEST_CASE("Each of tests/parks/routes.park's two networks has every node reachable from every "
          "other along its edges") {
  World routes = loadWorld(makeParkSchema(), routesText());
  resolveWorld(routes);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    const Network &network = parkNetwork(routes, kind);
    REQUIRE(network.nodeCount() > 0);

    // Edges join their two nodes whichever way they are walked, so reaching every node from node 0
    // reaches every node from every other.
    std::vector<bool> reached(network.nodeCount(), false);
    reached[0] = true;
    bool grew = true;
    while (grew) {
      grew = false;
      for (const NetworkEdge &edge : network.edges()) {
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
}

} // namespace
} // namespace tpj
