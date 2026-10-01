#include "legible/park_summary.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

// Failures show a shop line as its key and record.
template <> struct Catch::StringMaker<tpj::ShopLine> {
  static std::string convert(const tpj::ShopLine &line) {
    return "{shop " + std::to_string(static_cast<uint64_t>(line.Shop)) + ": stock " +
           std::to_string(line.Record.Stock) + ", queue " + std::to_string(line.Record.Queue) +
           ", on order " + std::to_string(line.Record.OnOrder) + ", " +
           std::string(tpj::limitingFactorName(line.Record.Limit)) +
           (line.Record.Starved ? ", starved}" : "}");
  }
};

namespace tpj {
namespace {

// tests/parks/warm.park, saved at tick 1920: shop 7 supplied by depot 8, and guests 9 to 40, five
// of them waiting at the shop, four queued and one served, with meals eaten.
constexpr uint64_t WARM_GUESTS = 32;
constexpr uint64_t WARM_WAITING = 5;

// tests/parks/supply.park, at tick 0 with no guests: shop 7, depot 8, and shop 9, so the depot's
// box lies between the two shops' in parkBoxes' order.
constexpr EntityKey FIRST_SHOP{7};
constexpr EntityKey SECOND_SHOP{9};

World openPark(std::string_view name) {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / name, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

TEST_CASE("summarizePark lists a line for each shop box, in parkBoxes' order, with its key and "
          "record, and no other box") {
  const World world = openPark("supply.park");
  const std::optional<ShopRecord> first = shopRecord(world, FIRST_SHOP);
  const std::optional<ShopRecord> second = shopRecord(world, SECOND_SHOP);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  if (!first.has_value() || !second.has_value()) {
    return;
  }
  // The depot, a box with no shop record, gives no line.
  const std::vector<ShopLine> expected{{FIRST_SHOP, *first}, {SECOND_SHOP, *second}};
  CHECK(summarizePark(world).Shops == expected);
}

TEST_CASE("summarizePark counts the guests, the mean of their hunger, and how many wait") {
  const World world = openPark("warm.park");
  // Hunger is summed from 0.0 in parkGuests' order, so the mean is exact, bit for bit.
  double hungerTotal = 0.0;
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    REQUIRE(record.has_value());
    if (!record.has_value()) {
      continue;
    }
    hungerTotal += record->Hunger;
  }
  const ParkSummary summary = summarizePark(world);
  CHECK(summary.Guests == WARM_GUESTS);
  CHECK(summary.MeanHunger == hungerTotal / static_cast<double>(WARM_GUESTS));
  CHECK(summary.Waiting == WARM_WAITING);
}

TEST_CASE("summarizePark gives no guests, a mean hunger of 0, and none waiting for a park with "
          "no guests") {
  const ParkSummary summary = summarizePark(openPark("supply.park"));
  CHECK(summary.Guests == 0);
  CHECK(summary.MeanHunger == 0.0);
  CHECK(summary.Waiting == 0);
}

TEST_CASE("summarizePark's meals eaten are the meals units consumed with the cause eaten") {
  const World world = openPark("warm.park");
  const int64_t eaten = unitsConsumed<Meals>(world, EATEN_CAUSE);
  // warm.park's guests have eaten, so a summary that ignored the meals ledger would differ.
  REQUIRE(eaten > 0);
  CHECK(summarizePark(world).MealsEaten == eaten);
}

TEST_CASE("summarizePark is a function of the world alone and changes nothing in it") {
  const World world = openPark("warm.park");
  const World copy = copyWorld(world);
  const World reloaded = openPark("warm.park");
  const uint64_t before = hashWorld(world);
  const ParkSummary summary = summarizePark(world);
  CHECK(hashWorld(world) == before);
  CHECK(worldsEqual(world, copy));
  CHECK(summarizePark(copy) == summary);
  CHECK(summarizePark(reloaded) == summary);
}

} // namespace
} // namespace tpj
