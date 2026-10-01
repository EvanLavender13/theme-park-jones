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
           row.Probability + "\", " + (row.Picked ? "picked" : "not picked") + "}";
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
// Guest 9 is heading to shop 7 with hunger 0.6654..., its stay ends at tick 2373, it has eaten one
// meal, at tick 623 from hunger 0.4836... to 0, and its last choice, at tick 1857 and hunger
// 0.6336..., weighed an offer of shop 7 (relief 1.3677..., distance -0.33499..., wait -0.2573...,
// commitment 0, score 0.7753..., probability 0.7505...), which it picked, against carrying on
// (score 0.5, probability 0.2494...).
constexpr EntityKey ENTRANCE{1};
constexpr EntityKey GUEST_PATH{2};
constexpr EntityKey BACKSTAGE_PATH{6};
constexpr EntityKey SHOP{7};
constexpr EntityKey DEPOT{8};
constexpr EntityKey HEADING_GUEST{9};
// Wandering with no target, and its last choice picked its second option, carrying on.
constexpr EntityKey WANDERING_GUEST{10};
// Admitted in the last cycle: it has neither eaten nor chosen.
constexpr EntityKey NEW_GUEST{40};
constexpr EntityKey MISSING{99};
constexpr std::array<EntityKey, 4> GUEST_PATHS{EntityKey{2}, EntityKey{3}, EntityKey{4},
                                               EntityKey{5}};
constexpr uint64_t HEADING_STAY_UNTIL = 2373;

const std::vector<std::string> GUEST_LABELS{"Activity",    "Hunger",    "Target",     "Stay",
                                            "Meals eaten", "Last meal", "Last choice"};

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

// warm.park after a cycle deleting every guest path: its guests remain, but with no guest network
// their places no longer resolve.
World cutWarm() {
  World world = openWarm();
  CommandQueue cut;
  for (const EntityKey path : GUEST_PATHS) {
    queueEdit(cut, DeletePath{path});
  }
  stepWorld(world, cut);
  return world;
}

std::vector<std::string> labelsOf(const Inspection &inspection) {
  std::vector<std::string> labels;
  labels.reserve(inspection.Rows.size());
  for (const InspectorRow &row : inspection.Rows) {
    labels.push_back(row.Label);
  }
  return labels;
}

std::string valueOf(const Inspection &inspection, std::string_view label) {
  for (const InspectorRow &row : inspection.Rows) {
    if (row.Label == label) {
      return row.Value;
    }
  }
  FAIL("no row labeled " << label);
  return {};
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

TEST_CASE("A guest's inspector shows its title and the seven rows formatted from its record and "
          "the world's tick") {
  const World world = openWarm();
  const Inspection inspection =
      inspectSubject(world, InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  CHECK(inspection.Title == "Guest 9");
  CHECK_FALSE(inspection.Gone);
  // Stay: 453 ticks left is 15.1 s. Last meal: 1297 ticks ago is 43.2 s. Last choice: 63 ticks
  // ago is 2.1 s.
  const std::vector<InspectorRow> expected{
      {"Activity", "heading to shop"},
      {"Hunger", "0.67"},
      {"Target", "shop 7"},
      {"Stay", "15.1 s left"},
      {"Meals eaten", "1"},
      {"Last meal", "43.2 s ago, hunger 0.48 to 0.00"},
      {"Last choice", "2.1 s ago at hunger 0.63"},
  };
  CHECK(inspection.Rows == expected);
}

TEST_CASE("A guest with no target, no meal, and no choice shows none for each") {
  const Inspection fresh =
      inspectSubject(openWarm(), InspectorSubject{NEW_GUEST, SubjectKind::Guest});
  CHECK(labelsOf(fresh) == GUEST_LABELS);
  CHECK(valueOf(fresh, "Target") == "none");
  CHECK(valueOf(fresh, "Meals eaten") == "0");
  CHECK(valueOf(fresh, "Last meal") == "none");
  CHECK(valueOf(fresh, "Last choice") == "none");
}

TEST_CASE("A guest's stay reads the seconds left while StayUntil is later than the world's tick, "
          "and over from StayUntil on") {
  const World world = openWarm();
  const InspectorSubject guest{HEADING_GUEST, SubjectKind::Guest};
  // One tick left is 1/30 s.
  CHECK(valueOf(inspectSubject(atTick(world, HEADING_STAY_UNTIL - 1), guest), "Stay") ==
        "0.0 s left");
  CHECK(valueOf(inspectSubject(atTick(world, HEADING_STAY_UNTIL), guest), "Stay") == "over");
  CHECK(valueOf(inspectSubject(atTick(world, HEADING_STAY_UNTIL + 300), guest), "Stay") == "over");
}

TEST_CASE("A meal or choice later than the world's tick reads as 0 s ago") {
  // Tick 600 is before guest 9's meal at 623 and its choice at 1857; its stay has 1773 ticks left.
  const Inspection inspection =
      inspectSubject(atTick(openWarm(), 600), InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  CHECK(valueOf(inspection, "Last meal") == "0.0 s ago, hunger 0.48 to 0.00");
  CHECK(valueOf(inspection, "Last choice") == "0.0 s ago at hunger 0.63");
  CHECK(valueOf(inspection, "Stay") == "59.1 s left");
}

TEST_CASE("A guest's choice table has a row for each option it weighed, in order, with an offer's "
          "terms, every score and probability, and its pick marked") {
  const World world = openWarm();
  const Inspection inspection =
      inspectSubject(world, InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  // Carrying on has no terms, so its term cells are empty.
  const std::vector<ChoiceRow> expected{
      {"shop 7", "1.37", "-0.33", "-0.26", "0.00", "0.78", "0.75", true},
      {"carry on", "", "", "", "", "0.50", "0.25", false},
  };
  CHECK(inspection.Choices == expected);
}

TEST_CASE("Exactly the choice row at the record's Picked index is marked picked") {
  // Guest 10 picked its second option, carrying on.
  const World world = openWarm();
  const std::optional<GuestRecord> record = guestRecord(world, WANDERING_GUEST);
  REQUIRE(record.has_value());
  const GuestChoice choice = record.value_or(GuestRecord{}).LastChoice.value_or(GuestChoice{});
  REQUIRE(choice.Options.size() == 2);
  REQUIRE(choice.Picked == 1);
  const Inspection inspection =
      inspectSubject(world, InspectorSubject{WANDERING_GUEST, SubjectKind::Guest});
  std::vector<bool> picked;
  picked.reserve(inspection.Choices.size());
  for (const ChoiceRow &row : inspection.Choices) {
    picked.push_back(row.Picked);
  }
  CHECK(picked == std::vector<bool>{false, true});
}

TEST_CASE("A guest that has not chosen has an empty choice table") {
  const World world = openWarm();
  CHECK(inspectSubject(world, InspectorSubject{NEW_GUEST, SubjectKind::Guest}).Choices.empty());
}

// The rows state the record's own values, so they are compared with the record rather than with
// the ledger arithmetic behind it.
std::vector<InspectorRow> shopRows(const ShopRecord &record) {
  return {
      {"Stock", std::to_string(record.Stock)},
      {"Queue", std::to_string(record.Queue)},
      {"On order", std::to_string(record.OnOrder)},
      {"Limit", std::string(limitingFactorName(record.Limit))},
      {"Starved", record.Starved ? "yes" : "no"},
  };
}

TEST_CASE("A shop's inspector shows its title and its record's stock, queue, supplies on order, "
          "limiting factor, and whether it is starved, and no choices") {
  const World world = openWarm();
  // Deleting the backstage path leaves the shop with no depot, so both answers of Starved show.
  const World starved = candidateOf(world, DeletePath{BACKSTAGE_PATH});
  for (const World *shown : {&world, &starved}) {
    const std::optional<ShopRecord> record = shopRecord(*shown, SHOP);
    REQUIRE(record.has_value());
    const Inspection inspection = inspectSubject(*shown, InspectorSubject{SHOP, SubjectKind::Shop});
    CHECK(inspection.Title == "Shop 7");
    CHECK_FALSE(inspection.Gone);
    CHECK(inspection.Rows == shopRows(record.value_or(ShopRecord{})));
    CHECK(inspection.Choices.empty());
  }
  CHECK(valueOf(inspectSubject(world, InspectorSubject{SHOP, SubjectKind::Shop}), "Starved") ==
        "no");
  CHECK(valueOf(inspectSubject(starved, InspectorSubject{SHOP, SubjectKind::Shop}), "Starved") ==
        "yes");
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

TEST_CASE("A guest that has left the park is inspected as gone, keeping its title") {
  // With no guest network, every guest leaves the park when it next steps.
  World left = cutWarm();
  stepWorld(left);
  REQUIRE_FALSE(guestRecord(left, HEADING_GUEST).has_value());
  const Inspection inspection =
      inspectSubject(left, InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  CHECK(inspection.Title == "Guest 9");
  CHECK(inspection.Gone);
  CHECK(inspection.Rows.empty());
  CHECK(inspection.Choices.empty());
}

TEST_CASE("A guest whose place does not resolve is inspected like any other") {
  const World cut = cutWarm();
  const std::optional<GuestRecord> record = guestRecord(cut, HEADING_GUEST);
  REQUIRE(record.has_value());
  const GuestRecord held = record.value_or(GuestRecord{});
  REQUIRE_FALSE(held.Position.has_value());
  const Inspection inspection =
      inspectSubject(cut, InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  CHECK_FALSE(inspection.Gone);
  CHECK(labelsOf(inspection) == GUEST_LABELS);
  CHECK(inspection.Choices.size() == held.LastChoice.value_or(GuestChoice{}).Options.size());
}

TEST_CASE("Inspecting changes nothing in the world") {
  const World world = openWarm();
  const uint64_t before = hashWorld(world);
  std::optional<InspectorSubject> subject;
  for (const EntityKey key : {HEADING_GUEST, SHOP, DEPOT, MISSING}) {
    (void)inspectorSubject(world, key);
    pickSubject(subject, world, key);
  }
  (void)inspectSubject(world, InspectorSubject{HEADING_GUEST, SubjectKind::Guest});
  (void)inspectSubject(world, InspectorSubject{SHOP, SubjectKind::Shop});
  (void)inspectSubject(world, InspectorSubject{MISSING, SubjectKind::Guest});
  CHECK(hashWorld(world) == before);
}

} // namespace
} // namespace tpj
