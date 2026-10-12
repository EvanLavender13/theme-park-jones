#include "legible/inspect.h"

#include "sim/clock.h"
#include "sim/furniture/furniture.h"
#include "sim/guests/guests.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"

#include <algorithm>
#include <format>
#include <stddef.h>
#include <utility>

namespace tpj {
namespace {

std::string keyText(EntityKey key) { return std::format("{}", static_cast<uint64_t>(key)); }

std::string twoDecimals(double value) { return std::format("{:.2f}", value); }

// The seconds a number of ticks lasts, with one decimal.
std::string seconds(uint64_t ticks) {
  return std::format("{:.1f}", static_cast<double>(ticks) * SIM_TICK_SECONDS);
}

// The ticks from a past tick to the world's, and 0 for a later one.
uint64_t ticksSince(const World &world, uint64_t tick) {
  return tick <= world.Tick ? world.Tick - tick : 0;
}

// An enum's name for display, each '-' a space.
template <typename Enum> std::string displayName(Enum value) {
  std::string name(enumNames(value)[static_cast<size_t>(value)]);
  std::ranges::replace(name, '-', ' ');
  return name;
}

// The box's or piece of furniture's kind and key, such as "bench 5" or "table 9", or "box <key>"
// when neither has the key.
std::string boxText(const World &world, EntityKey key) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      return displayName(box.Kind) + " " + keyText(key);
    }
  }
  for (const FurniturePiece &piece : parkFurniture(world)) {
    if (piece.Key == key) {
      return displayName(piece.Kind) + " " + keyText(key);
    }
  }
  return "box " + keyText(key);
}

// The pieces the guest remembers as full, each with how long ago it found it full, or "none".
std::string fullPiecesText(const World &world, const GuestRecord &record) {
  std::string text;
  for (const FullPiece &full : record.FullPieces) {
    if (full.Piece == NULL_KEY) {
      continue;
    }
    if (!text.empty()) {
      text += "; ";
    }
    text += std::format("{} {} s ago", boxText(world, full.Piece),
                        seconds(ticksSince(world, full.Found)));
  }
  return text.empty() ? "none" : text;
}

std::vector<InspectorRow> guestRows(const World &world, const GuestRecord &record) {
  std::vector<InspectorRow> rows;
  rows.push_back({"Activity", displayName(record.Activity)});
  rows.push_back({"Hunger", twoDecimals(record.Hunger)});
  rows.push_back({"Thirst", twoDecimals(record.Thirst)});
  rows.push_back({"Tiredness", twoDecimals(record.Tiredness)});
  rows.push_back({"Bladder", twoDecimals(record.Bladder)});
  const ParkTime arrived = parkTime(record.Arrived);
  rows.push_back({"Arrived", std::format("day {}, {}:{:02}", arrived.Day, arrived.Second / 3600,
                                         (arrived.Second % 3600) / 60)});
  if (record.Target != NULL_KEY) {
    rows.push_back(
        {"Target", boxText(world, record.Target) + " for " + displayName(record.TargetNeed)});
  } else if (record.Activity == GuestActivity::HeadingHome) {
    rows.push_back({"Target", "home for " + displayName(record.Reason)});
  } else {
    rows.push_back({"Target", "none"});
  }
  rows.push_back({"Meals eaten", std::format("{}", record.MealsEaten)});
  if (const std::optional<GuestMeal> &meal = record.LastMeal) {
    rows.push_back({"Last meal", std::format("{} s ago, hunger {:.2f} to {:.2f}",
                                             seconds(ticksSince(world, meal->Tick)), meal->Before,
                                             meal->After)});
  } else {
    rows.push_back({"Last meal", "none"});
  }
  rows.push_back({"Full pieces", fullPiecesText(world, record)});
  return rows;
}

// The name with its first letter capitalized.
std::string capitalized(std::string name) {
  if (!name.empty() && name[0] >= 'a' && name[0] <= 'z') {
    name[0] = static_cast<char>(name[0] - 'a' + 'A');
  }
  return name;
}

// The guest's choices as six rows: each need's best offer or seat and the chance of any of them,
// then carrying on and heading home.
std::vector<ChoiceRow> choiceRows(const World &world, const std::vector<ChoiceOption> &options) {
  std::vector<ChoiceRow> rows;
  for (const GuestNeed need : NEEDS) {
    // HomeReason lists the needs in NEEDS order, so its names give "toilet" for the bladder.
    ChoiceRow row;
    row.Choice = capitalized(displayName(static_cast<HomeReason>(need)));
    const ChoiceOption *best = nullptr;
    double probability = 0.0;
    for (const ChoiceOption &option : options) {
      if ((option.Kind == ChoiceKind::Offer || option.Kind == ChoiceKind::Seat) &&
          option.Need == need) {
        probability += option.Probability;
        if (best == nullptr || option.Score > best->Score) {
          best = &option;
        }
      }
    }
    if (best == nullptr) {
      row.Target = "no offer";
    } else {
      row.Target = boxText(world, best->Facility);
      row.Relief = twoDecimals(best->Relief);
      row.Distance = twoDecimals(best->Distance);
      row.Wait = twoDecimals(best->Wait);
      row.Commitment = twoDecimals(best->Commitment);
      row.Score = twoDecimals(best->Score);
      row.Probability = twoDecimals(probability);
    }
    rows.push_back(std::move(row));
  }
  ChoiceRow carryOn;
  carryOn.Choice = capitalized(displayName(ChoiceKind::CarryOn));
  const auto carry = std::ranges::find(options, ChoiceKind::CarryOn, &ChoiceOption::Kind);
  if (carry != options.end()) {
    carryOn.Score = twoDecimals(carry->Score);
    carryOn.Probability = twoDecimals(carry->Probability);
  }
  rows.push_back(std::move(carryOn));
  ChoiceRow headHome;
  headHome.Choice = capitalized(displayName(ChoiceKind::HeadHome));
  const auto home = std::ranges::find(options, ChoiceKind::HeadHome, &ChoiceOption::Kind);
  if (home != options.end()) {
    headHome.Target = "home for " + displayName(home->Reason);
    headHome.Commitment = twoDecimals(home->Commitment);
    headHome.Score = twoDecimals(home->Score);
    headHome.Probability = twoDecimals(home->Probability);
  } else {
    headHome.Target = "no route home";
  }
  rows.push_back(std::move(headHome));
  return rows;
}

// Heading home's base and each reason's term, or nothing when heading home is not an option.
std::vector<InspectorRow> homeTerms(const std::vector<ChoiceOption> &options) {
  std::vector<InspectorRow> rows;
  const auto home = std::ranges::find(options, ChoiceKind::HeadHome, &ChoiceOption::Kind);
  if (home == options.end()) {
    return rows;
  }
  rows.push_back({"Base", twoDecimals(home->Base)});
  for (const HomeReason reason : HOME_REASONS) {
    rows.push_back({capitalized(displayName(reason)),
                    twoDecimals(home->Reasons[static_cast<size_t>(reason)])});
  }
  return rows;
}

std::vector<InspectorRow> shopRows(const ShopRecord &record) {
  std::vector<InspectorRow> rows;
  for (const Good good : GOODS) {
    const std::string name = capitalized(displayName(good));
    const GoodRecord &holding = record.Goods[static_cast<size_t>(good)];
    rows.push_back({name + " stock", std::format("{}", holding.Stock)});
    rows.push_back({name + " on order", std::format("{}", holding.OnOrder)});
  }
  rows.push_back({"Queue", std::format("{}", record.Queue)});
  rows.push_back({"Limit", std::string(limitingFactorName(record.Limit))});
  rows.push_back({"Starved", record.Starved ? "yes" : "no"});
  return rows;
}

std::vector<InspectorRow> facilityRows(const FacilityRecord &record) {
  return {{"Places", std::format("{}", record.Places)},
          {"Taken", std::format("{}", record.Taken)},
          {"Queue", std::format("{}", record.Queue)}};
}

} // namespace

std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key) {
  if (guestRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Guest};
  }
  if (shopRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Shop};
  }
  if (facilityRecord(world, key)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == key && box.Kind == BoxKind::Bench) {
        return InspectorSubject{key, SubjectKind::Bench};
      }
      if (box.Key == key && box.Kind == BoxKind::ToiletBlock) {
        return InspectorSubject{key, SubjectKind::ToiletBlock};
      }
    }
  }
  return std::nullopt;
}

void pickSubject(std::optional<InspectorSubject> &subject, const World &world,
                 std::optional<EntityKey> key) {
  if (!key) {
    return;
  }
  if (const std::optional<InspectorSubject> picked = inspectorSubject(world, *key)) {
    subject = picked;
  }
}

Inspection inspectSubject(const World &world, const InspectorSubject &subject) {
  Inspection inspection;
  if (subject.Kind == SubjectKind::Guest) {
    inspection.Title = "Guest " + keyText(subject.Key);
    const std::optional<GuestRecord> record = guestRecord(world, subject.Key);
    inspection.Gone = !record;
    if (record) {
      inspection.Rows = guestRows(world, *record);
      if (const std::optional<std::vector<ChoiceOption>> options =
              guestOptions(world, subject.Key)) {
        inspection.Choices = choiceRows(world, *options);
        inspection.HomeTerms = homeTerms(*options);
      }
    }
    return inspection;
  }
  if (subject.Kind == SubjectKind::Bench || subject.Kind == SubjectKind::ToiletBlock) {
    const BoxKind kind = subject.Kind == SubjectKind::Bench ? BoxKind::Bench : BoxKind::ToiletBlock;
    inspection.Title = capitalized(displayName(kind)) + " " + keyText(subject.Key);
    const std::optional<FacilityRecord> record = facilityRecord(world, subject.Key);
    inspection.Gone = !record;
    if (record) {
      inspection.Rows = facilityRows(*record);
    }
    return inspection;
  }
  inspection.Title = "Shop " + keyText(subject.Key);
  const std::optional<ShopRecord> record = shopRecord(world, subject.Key);
  inspection.Gone = !record;
  if (record) {
    inspection.Rows = shopRows(*record);
  }
  return inspection;
}

} // namespace tpj
