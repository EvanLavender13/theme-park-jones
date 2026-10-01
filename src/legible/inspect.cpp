#include "legible/inspect.h"

#include "sim/guests/guests.h"
#include "sim/operations/operations.h"

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

std::vector<InspectorRow> guestRows(const World &world, const GuestRecord &record) {
  std::vector<InspectorRow> rows;
  rows.push_back({"Activity", displayName(record.Activity)});
  rows.push_back({"Hunger", twoDecimals(record.Hunger)});
  rows.push_back({"Target", record.Target == NULL_KEY ? "none" : "shop " + keyText(record.Target)});
  rows.push_back({"Stay", record.StayUntil > world.Tick
                              ? seconds(record.StayUntil - world.Tick) + " s left"
                              : "over"});
  rows.push_back({"Meals eaten", std::format("{}", record.MealsEaten)});
  if (const std::optional<GuestMeal> &meal = record.LastMeal) {
    rows.push_back({"Last meal", std::format("{} s ago, hunger {:.2f} to {:.2f}",
                                             seconds(ticksSince(world, meal->Tick)), meal->Before,
                                             meal->After)});
  } else {
    rows.push_back({"Last meal", "none"});
  }
  if (const std::optional<GuestChoice> &choice = record.LastChoice) {
    rows.push_back(
        {"Last choice", std::format("{} s ago at hunger {:.2f}",
                                    seconds(ticksSince(world, choice->Tick)), choice->Hunger)});
  } else {
    rows.push_back({"Last choice", "none"});
  }
  return rows;
}

// Each option the guest weighed, its terms only for an offer, and the one it picked.
std::vector<ChoiceRow> choiceRows(const GuestChoice &choice) {
  std::vector<ChoiceRow> rows;
  for (size_t index = 0; index < choice.Options.size(); ++index) {
    const ChoiceOption &option = choice.Options[index];
    ChoiceRow row;
    if (option.Kind == ChoiceKind::Offer) {
      row.Option = "shop " + keyText(option.Shop);
      row.Relief = twoDecimals(option.Relief);
      row.Distance = twoDecimals(option.Distance);
      row.Wait = twoDecimals(option.Wait);
      row.Commitment = twoDecimals(option.Commitment);
    } else {
      row.Option = displayName(option.Kind);
    }
    row.Score = twoDecimals(option.Score);
    row.Probability = twoDecimals(option.Probability);
    row.Picked = index == choice.Picked;
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<InspectorRow> shopRows(const ShopRecord &record) {
  return {{"Stock", std::format("{}", record.Stock)},
          {"Queue", std::format("{}", record.Queue)},
          {"On order", std::format("{}", record.OnOrder)},
          {"Limit", std::string(limitingFactorName(record.Limit))},
          {"Starved", record.Starved ? "yes" : "no"}};
}

} // namespace

std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key) {
  if (guestRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Guest};
  }
  if (shopRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Shop};
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
      if (record->LastChoice) {
        inspection.Choices = choiceRows(*record->LastChoice);
      }
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
