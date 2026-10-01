// Laws that hold with every capability running together, checked over every park file in
// tests/parks/. Each module's tests prove its laws on synthetic worlds; these run the park as the
// player and the cross-build check see it, with intent, networks, shops, depots, and guests all
// interacting, and a park checked in to tests/parks/ is covered with no code change.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::ParkFile;
using test::parkFiles;

// Ticks each conservation run steps every park: long enough for each flow to reach its consumer
// in warm.park, whose shop serves and whose served guest eats within the run.
constexpr uint64_t CONSERVATION_TICKS = 60;
// Ticks each determinism run steps every park: long enough for a guest of warm.park to eat.
constexpr uint64_t DETERMINISM_TICKS = 60;

// A flow's units as its producer and its consumer account for them: created, in transit, held by
// a shop box or a guest, and consumed by the one cause its consumer consumes them with.
struct FlowBooks {
  int64_t Created = 0;
  int64_t InTransit = 0;
  int64_t HeldByShops = 0;
  int64_t HeldByGuests = 0;
  int64_t Consumed = 0;
};

int64_t unitsOf(const std::vector<FlowHolding> &holdings) {
  int64_t units = 0;
  for (const FlowHolding &holding : holdings) {
    units += holding.Units;
  }
  return units;
}

template <FlowDefinition K> FlowBooks booksOf(const World &world, std::string_view consumedAs) {
  FlowBooks books;
  books.Created = unitsCreated<K>(world);
  books.InTransit = unitsInTransit<K>(world);
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      books.HeldByShops += unitsOf(stockOf<K>(world, box.Key));
    }
  }
  for (const EntityKey guest : parkGuests(world)) {
    books.HeldByGuests += unitsOf(stockOf<K>(world, guest));
  }
  books.Consumed = unitsConsumed<K>(world, consumedAs);
  return books;
}

bool balances(const FlowBooks &books) {
  return books.Created == books.InTransit + books.HeldByShops + books.HeldByGuests + books.Consumed;
}

// A flow that crosses capabilities, and how its consumer's books are read.
struct ConservedFlow {
  std::string_view Name;
  FlowBooks (*Books)(const World &world);
};

// The flows whose units pass from one capability to another, each with the cause its consumer
// consumes them with. With no park edit, no shop, depot, or guest is removed with units
// outstanding, so a unit consumed with any other cause, or held anywhere else, has left the
// path from its producer to its consumer.
const std::array<ConservedFlow, 3> CONSERVED_FLOWS{{
    // A depot ships supplies to a shop, which turns them into meals.
    {Supplies::Name, [](const World &world) { return booksOf<Supplies>(world, SERVED_CAUSE); }},
    // A shop sends a meal to the guest it served, which eats it.
    {Meals::Name, [](const World &world) { return booksOf<Meals>(world, EATEN_CAUSE); }},
    // A guest sends a visit to a shop, which returns it, served or not.
    {GuestVisits::Name,
     [](const World &world) { return booksOf<GuestVisits>(world, FINISHED_CAUSE); }},
}};

// Catches a unit that leaves its producer's books without reaching its consumer's: a guest that
// leaves the park with a visit or meal still addressed to it, a meal sent to a key that is not the
// guest that queued, or a shipment delivered somewhere other than its shop's stock, any of which
// ends as abandoned, discarded, or undeliverable instead.
TEST_CASE("In every tick of a run of every park file, each flow between capabilities is conserved "
          "from its producer to its consumer") {
  std::array<bool, CONSERVED_FLOWS.size()> consumedInRun{};
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    World world = openPark(park);
    std::array<int64_t, CONSERVED_FLOWS.size()> consumedAtStart{};
    for (size_t flow = 0; flow < CONSERVED_FLOWS.size(); ++flow) {
      consumedAtStart.at(flow) = CONSERVED_FLOWS.at(flow).Books(world).Consumed;
    }
    bool conserved = true;
    for (uint64_t cycle = 0; cycle <= CONSERVATION_TICKS && conserved; ++cycle) {
      if (cycle > 0) {
        stepWorld(world);
      }
      for (const ConservedFlow &flow : CONSERVED_FLOWS) {
        const FlowBooks books = flow.Books(world);
        if (!balances(books)) {
          INFO("tick " << world.Tick << ", " << flow.Name << ": created " << books.Created
                       << ", in transit " << books.InTransit << ", held by shops "
                       << books.HeldByShops << ", held by guests " << books.HeldByGuests
                       << ", consumed by its consumer " << books.Consumed);
          CHECK(balances(books));
          conserved = false;
        }
      }
    }
    for (size_t flow = 0; flow < CONSERVED_FLOWS.size(); ++flow) {
      consumedInRun.at(flow) =
          consumedInRun.at(flow) ||
          CONSERVED_FLOWS.at(flow).Books(world).Consumed > consumedAtStart.at(flow);
    }
  }
  for (size_t flow = 0; flow < CONSERVED_FLOWS.size(); ++flow) {
    INFO(CONSERVED_FLOWS.at(flow).Name << " reached its consumer in some park's run");
    CHECK(consumedInRun.at(flow));
  }
}

// Catches a park type whose text does not read back as it was written, such as a guest's last
// choice, a ledger's causes, or a field's stepped entries, which no synthetic save holds.
TEST_CASE("Every park file loads and saves back to identical text") {
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    CHECK(saveWorld(loadWorld(makeParkSchema(), park.Text)) == park.Text);
  }
}

// Whether the world holds supplies in transit to a shop that has no supply route, as a shipment
// does when its backstage path is deleted on the way.
bool holdsStrandedShipment(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind != BoxKind::Shop || nearestDepot(world, box.Key).has_value()) {
      continue;
    }
    for (const FlowPacket &packet : addressedTo<Supplies>(world, box.Key).Packets) {
      if (packet.To == box.Key) {
        return true;
      }
    }
  }
  return false;
}

// Catches state a module keeps that no save holds, and a resolver or finisher that changes state
// when a park is reopened, such as one that carries guests, clears stepped offers, or drops
// shipments whose route is gone, so the reopened park differs from the one saved.
TEST_CASE("A world stepped from every park file equals the world regenerated from its save, "
          "shipments in transit to a shop with no supply route included") {
  bool sawStrandedShipment = false;
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    World world = openPark(park);
    // A world the cycle has stepped, not one a load has just resolved.
    stepWorld(world);
    const World regenerated = openPark(saveWorld(world));
    CHECK(worldsEqual(regenerated, world));
    sawStrandedShipment = sawStrandedShipment || holdsStrandedShipment(world);
  }
  CHECK(sawStrandedShipment);
}

// Catches a result that depends on something outside the world: an uninitialized value, a
// container ordered by address, or a cache one module keeps between worlds, which two worlds
// stepped side by side in one process would not share.
TEST_CASE("Two runs of every park file, stepped side by side, end with the same state hash") {
  bool sawGuestsEat = false;
  for (const ParkFile &park : parkFiles()) {
    INFO(park.Name);
    World first = openPark(park);
    World second = openPark(park);
    const int64_t eatenAtStart = unitsConsumed<Meals>(first, EATEN_CAUSE);
    for (uint64_t cycle = 0; cycle < DETERMINISM_TICKS; ++cycle) {
      stepWorld(first);
      stepWorld(second);
    }
    CHECK(hashWorld(first) == hashWorld(second));
    sawGuestsEat = sawGuestsEat || unitsConsumed<Meals>(first, EATEN_CAUSE) > eatenAtStart;
  }
  CHECK(sawGuestsEat);
}

} // namespace
} // namespace tpj
