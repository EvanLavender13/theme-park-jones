#include "legible/inspect.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

// Failures show rows and subjects as their text.
template <> struct Catch::StringMaker<tpj::InspectorRow> {
  static std::string convert(const tpj::InspectorRow &row) {
    return "{\"" + row.Label + "\", \"" + row.Value + "\"}";
  }
};

template <> struct Catch::StringMaker<tpj::ChoiceRow> {
  static std::string convert(const tpj::ChoiceRow &row) {
    return "{\"" + row.Option + "\", \"" + row.Relief + "\", \"" + row.Distance + "\", \"" +
           row.Wait + "\", \"" + row.Commitment + "\", \"" + row.Score + "\", \"" +
           row.Probability + "\"}";
  }
};

template <> struct Catch::StringMaker<tpj::InspectorSubject> {
  static std::string convert(const tpj::InspectorSubject &subject) {
    return std::string(subject.Kind == tpj::SubjectKind::Guest ? "Guest " : "Shop ") +
           std::to_string(static_cast<uint64_t>(subject.Key));
  }
};

namespace tpj {
namespace {

// tests/parks/warm.park, saved at tick 1920: entrance 1, guest paths 2 to 5, backstage path 6,
// shop 7 supplied by depot 8, and guests 9 to 40.
//
// Guest 9 is heading to shop 7 with hunger 0.6654..., its stay ends at tick 2373, and it has eaten
// one meal, at tick 623 from hunger 0.4836... to 0.
constexpr EntityKey ENTRANCE{1};
constexpr EntityKey GUEST_PATH{2};
constexpr EntityKey SHOP{7};
constexpr EntityKey DEPOT{8};
constexpr EntityKey HEADING_GUEST{9};
// Wandering with no target.
constexpr EntityKey WANDERING_GUEST{10};
// Admitted in the last cycle: it has not eaten.
constexpr EntityKey NEW_GUEST{40};
constexpr EntityKey MISSING{99};
constexpr uint64_t HEADING_STAY_UNTIL = 2373;

const std::vector<std::string> GUEST_LABELS{"Activity", "Hunger",      "Target",
                                            "Stay",     "Meals eaten", "Last meal"};

World openWarm() {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

World candidateOf(const World &world, const ParkEdit &edit) {
  CommandQueue queue;
  queueEdit(queue, edit);
  return makeCandidate(world, queue);
}

// A world's tick is what an inspector measures elapsed and remaining time from, so moving it alone
// moves those rows.
World atTick(const World &world, uint64_t tick) {
  World moved = copyWorld(world);
  moved.Tick = tick;
  return moved;
}

TEST_CASE("inspectorSubject names a guest or a shop box by its kind and gives none for any other "
          "key") {
  const World world = openWarm();
  CHECK(inspectorSubject(world, HEADING_GUEST) ==
        InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  CHECK(inspectorSubject(world, SHOP) == InspectorSubject{SHOP, SubjectKind::Shop});
  // A depot, an entrance, a path, a key holding nothing, and the null key have no inspector.
  for (const EntityKey key : {DEPOT, ENTRANCE, GUEST_PATH, MISSING, NULL_KEY}) {
    CAPTURE(key);
    CHECK_FALSE(inspectorSubject(world, key).has_value());
  }
}

TEST_CASE("pickSubject replaces the subject with a picked guest's or shop's") {
  const World world = openWarm();
  std::optional<InspectorSubject> subject;
  pickSubject(subject, world, HEADING_GUEST);
  CHECK(subject == InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  pickSubject(subject, world, SHOP);
  CHECK(subject == InspectorSubject{SHOP, SubjectKind::Shop});
  pickSubject(subject, world, WANDERING_GUEST);
  CHECK(subject == InspectorSubject{WANDERING_GUEST, SubjectKind::Guest});
}

TEST_CASE("pickSubject leaves the subject as it was for no key or a key with no inspector") {
  const World world = openWarm();
  const std::optional<InspectorSubject> held = InspectorSubject{SHOP, SubjectKind::Shop};
  for (const std::optional<EntityKey> key :
       {std::optional<EntityKey>{}, std::optional<EntityKey>{DEPOT},
        std::optional<EntityKey>{ENTRANCE}, std::optional<EntityKey>{MISSING}}) {
    std::optional<InspectorSubject> subject = held;
    pickSubject(subject, world, key);
    CHECK(subject == held);
  }
  // With no subject yet, a pick of nothing to inspect leaves none.
  std::optional<InspectorSubject> none;
  pickSubject(none, world, DEPOT);
  CHECK_FALSE(none.has_value());
}

std::string twoDecimals(double value) { return std::format("{:.2f}", value); }

// The rows of a choice table showing the options: an offer as its shop with its terms, and
// carrying on and heading home by name with no terms.
std::vector<ChoiceRow> choiceRows(const std::vector<ChoiceOption> &options) {
  std::vector<ChoiceRow> rows;
  for (const ChoiceOption &option : options) {
    if (option.Kind == ChoiceKind::Offer) {
      rows.push_back({"shop " + std::to_string(static_cast<uint64_t>(option.Shop)),
                      twoDecimals(option.Relief), twoDecimals(option.Distance),
                      twoDecimals(option.Wait), twoDecimals(option.Commitment),
                      twoDecimals(option.Score), twoDecimals(option.Probability)});
    } else {
      rows.push_back({option.Kind == ChoiceKind::CarryOn ? "carry on" : "head home", "", "", "", "",
                      twoDecimals(option.Score), twoDecimals(option.Probability)});
    }
  }
  return rows;
}

TEST_CASE("A guest's choice table has a row for each option guestOptions gives for it, in order, "
          "an offer's with its shop and terms and every row with its score and probability") {
  const World world = openWarm();
  // At its StayUntil, guest 9 would also weigh heading home.
  const World stayOver = atTick(world, HEADING_STAY_UNTIL);
  bool sawOffer = false;
  bool sawCarryOn = false;
  bool sawHeadHome = false;
  for (const World *shown : {&world, &stayOver}) {
    for (const EntityKey guest : {HEADING_GUEST, WANDERING_GUEST, NEW_GUEST}) {
      CAPTURE(shown->Tick, guest);
      const std::optional<std::vector<ChoiceOption>> options = guestOptions(*shown, guest);
      REQUIRE(options.has_value());
      const std::vector<ChoiceOption> held = options.value_or(std::vector<ChoiceOption>{});
      const Inspection inspection =
          inspectSubject(*shown, InspectorSubject{guest, SubjectKind::Guest});
      CHECK(inspection.Choices == choiceRows(held));
      for (const ChoiceOption &option : held) {
        sawOffer = sawOffer || option.Kind == ChoiceKind::Offer;
        sawCarryOn = sawCarryOn || option.Kind == ChoiceKind::CarryOn;
        sawHeadHome = sawHeadHome || option.Kind == ChoiceKind::HeadHome;
      }
    }
  }
  CHECK(sawOffer);
  CHECK(sawCarryOn);
  CHECK(sawHeadHome);
}

TEST_CASE("A shop whose box is deleted is inspected as gone, keeping its title") {
  const World deleted = candidateOf(openWarm(), DeleteBox{SHOP});
  REQUIRE_FALSE(shopRecord(deleted, SHOP).has_value());
  const Inspection inspection = inspectSubject(deleted, InspectorSubject{SHOP, SubjectKind::Shop});
  CHECK(inspection.Title == "Shop 7");
  CHECK(inspection.Gone);
  CHECK(inspection.Rows.empty());
  CHECK(inspection.Choices.empty());
}

} // namespace
} // namespace tpj
