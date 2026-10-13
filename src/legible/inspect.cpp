#include "legible/inspect.h"

#include "sim/clock.h"
#include "sim/furniture/furniture.h"
#include "sim/guests/guests.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <stddef.h>
#include <string>
#include <string_view>
#include <utility>

namespace tpj {
namespace {

// The most rows any inspection shows, which a guest's do, the rows of a guest's choice table, and
// heading home's terms. Each list is reserved to its size, so none grows after the first fill.
constexpr size_t MOST_ROWS = 11;
constexpr size_t CHOICE_ROWS = NEEDS.size() + 2;
constexpr size_t HOME_TERMS = HOME_REASONS.size() + 1;

// Writes the text in place of what the string held, keeping its memory.
template <typename... Args>
void setText(std::string &text, std::format_string<Args...> format, Args &&...args) {
  text.clear();
  std::format_to(std::back_inserter(text), format, std::forward<Args>(args)...);
}

// Writes the text after what the string holds.
template <typename... Args>
void appendText(std::string &text, std::format_string<Args...> format, Args &&...args) {
  std::format_to(std::back_inserter(text), format, std::forward<Args>(args)...);
}

// The ticks from a past tick to the world's, and 0 for a later one.
uint64_t ticksSince(const World &world, uint64_t tick) {
  return tick <= world.Tick ? world.Tick - tick : 0;
}

// The seconds a number of ticks lasts.
double seconds(uint64_t ticks) { return static_cast<double>(ticks) * SIM_TICK_SECONDS; }

// Appends an enum's name for display, each '-' a space.
template <typename Enum> void appendDisplayName(std::string &text, Enum value) {
  const size_t start = text.size();
  text += enumNames(value)[static_cast<size_t>(value)];
  std::replace(text.begin() + static_cast<std::ptrdiff_t>(start), text.end(), '-', ' ');
}

// Capitalizes the text's first letter.
void capitalize(std::string &text) {
  if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
    text[0] = static_cast<char>(text[0] - 'a' + 'A');
  }
}

// Appends the box's or piece of furniture's kind and key, such as "bench 5" or "table 9", or
// "box <key>" when neither has the key.
void appendBoxText(std::string &text, const World &world, EntityKey key) {
  if (const std::optional<ParkBox> box = parkBox(world, key)) {
    appendDisplayName(text, box->Kind);
  } else if (const std::optional<FurniturePiece> piece = furniturePiece(world, key)) {
    appendDisplayName(text, piece->Kind);
  } else {
    text += "box";
  }
  appendText(text, " {}", static_cast<uint64_t>(key));
}

// The list's next row, its text cleared for the caller to write, reusing a row the list holds.
template <typename Row> Row &nextRow(std::vector<Row> &rows, size_t &used) {
  if (used == rows.size()) {
    rows.emplace_back();
  }
  return rows[used++];
}

InspectorRow &clearedRow(std::vector<InspectorRow> &rows, size_t &used) {
  InspectorRow &row = nextRow(rows, used);
  row.Label.clear();
  row.Value.clear();
  return row;
}

// The next row with its label, giving its value cleared for the caller to write.
std::string &valueOf(std::vector<InspectorRow> &rows, size_t &used, std::string_view label) {
  InspectorRow &row = clearedRow(rows, used);
  row.Label.assign(label);
  return row.Value;
}

ChoiceRow &clearedChoice(std::vector<ChoiceRow> &rows, size_t &used) {
  ChoiceRow &row = nextRow(rows, used);
  for (std::string *cell : {&row.Choice, &row.Target, &row.Relief, &row.Distance, &row.Wait,
                            &row.Commitment, &row.Score, &row.Probability}) {
    cell->clear();
  }
  return row;
}

// The pieces the guest remembers as full, each with how long ago it found it full, or "none".
void fullPiecesText(std::string &text, const World &world, const GuestRecord &record) {
  for (const FullPiece &full : record.FullPieces) {
    if (full.Piece == NULL_KEY) {
      continue;
    }
    if (!text.empty()) {
      text += "; ";
    }
    appendBoxText(text, world, full.Piece);
    appendText(text, " {:.1f} s ago", seconds(ticksSince(world, full.Found)));
  }
  if (text.empty()) {
    text.assign("none");
  }
}

void guestRows(const World &world, const GuestRecord &record, std::vector<InspectorRow> &rows) {
  size_t used = 0;
  appendDisplayName(valueOf(rows, used, "Activity"), record.Activity);
  setText(valueOf(rows, used, "Hunger"), "{:.2f}", record.Hunger);
  setText(valueOf(rows, used, "Thirst"), "{:.2f}", record.Thirst);
  setText(valueOf(rows, used, "Tiredness"), "{:.2f}", record.Tiredness);
  setText(valueOf(rows, used, "Bladder"), "{:.2f}", record.Bladder);
  const ParkTime arrived = parkTime(record.Arrived);
  setText(valueOf(rows, used, "Arrived"), "day {}, {}:{:02}", arrived.Day, arrived.Second / 3600,
          (arrived.Second % 3600) / 60);
  std::string &target = valueOf(rows, used, "Target");
  if (record.Target != NULL_KEY) {
    appendBoxText(target, world, record.Target);
    target += " for ";
    appendDisplayName(target, record.TargetNeed);
  } else if (record.Activity == GuestActivity::HeadingHome) {
    target += "home for ";
    appendDisplayName(target, record.Reason);
  } else {
    target.assign("none");
  }
  valueOf(rows, used, "Carrying").assign(record.Carrying ? "food" : "nothing");
  setText(valueOf(rows, used, "Meals eaten"), "{}", record.MealsEaten);
  std::string &lastMeal = valueOf(rows, used, "Last meal");
  if (const std::optional<GuestMeal> &meal = record.LastMeal) {
    setText(lastMeal, "{:.1f} s ago, hunger {:.2f} to {:.2f}",
            seconds(ticksSince(world, meal->Tick)), meal->Before, meal->After);
  } else {
    lastMeal.assign("none");
  }
  fullPiecesText(valueOf(rows, used, "Full pieces"), world, record);
  rows.resize(used);
}

// The guest's choices as six rows: each need's best offer, seat, or eating now and the chance of
// any of them, then carrying on and heading home. While the guest carries food, every row but
// hunger's says so.
void choiceRows(const World &world, const std::vector<ChoiceOption> &options, bool carrying,
                std::vector<ChoiceRow> &rows) {
  size_t used = 0;
  for (const GuestNeed need : NEEDS) {
    // HomeReason lists the needs in NEEDS order, so its names give "toilet" for the bladder.
    ChoiceRow &row = clearedChoice(rows, used);
    appendDisplayName(row.Choice, static_cast<HomeReason>(need));
    capitalize(row.Choice);
    if (carrying && need != GuestNeed::Hunger) {
      row.Target.assign("carrying food");
      continue;
    }
    const ChoiceOption *best = nullptr;
    double probability = 0.0;
    for (const ChoiceOption &option : options) {
      if ((option.Kind == ChoiceKind::Offer || option.Kind == ChoiceKind::Seat ||
           option.Kind == ChoiceKind::EatNow) &&
          option.Need == need) {
        probability += option.Probability;
        if (best == nullptr || option.Score > best->Score) {
          best = &option;
        }
      }
    }
    if (best == nullptr) {
      row.Target.assign("no offer");
      continue;
    }
    if (best->Kind == ChoiceKind::EatNow) {
      row.Target.assign("eat now");
    } else {
      appendBoxText(row.Target, world, best->Facility);
    }
    setText(row.Relief, "{:.2f}", best->Relief);
    setText(row.Distance, "{:.2f}", best->Distance);
    setText(row.Wait, "{:.2f}", best->Wait);
    setText(row.Commitment, "{:.2f}", best->Commitment);
    setText(row.Score, "{:.2f}", best->Score);
    setText(row.Probability, "{:.2f}", probability);
  }
  ChoiceRow &carryOn = clearedChoice(rows, used);
  appendDisplayName(carryOn.Choice, ChoiceKind::CarryOn);
  capitalize(carryOn.Choice);
  if (carrying) {
    carryOn.Target.assign("carrying food");
  }
  const auto carry = std::ranges::find(options, ChoiceKind::CarryOn, &ChoiceOption::Kind);
  if (carry != options.end()) {
    setText(carryOn.Score, "{:.2f}", carry->Score);
    setText(carryOn.Probability, "{:.2f}", carry->Probability);
  }
  ChoiceRow &headHome = clearedChoice(rows, used);
  appendDisplayName(headHome.Choice, ChoiceKind::HeadHome);
  capitalize(headHome.Choice);
  const auto home = std::ranges::find(options, ChoiceKind::HeadHome, &ChoiceOption::Kind);
  if (home != options.end()) {
    headHome.Target += "home for ";
    appendDisplayName(headHome.Target, home->Reason);
    setText(headHome.Commitment, "{:.2f}", home->Commitment);
    setText(headHome.Score, "{:.2f}", home->Score);
    setText(headHome.Probability, "{:.2f}", home->Probability);
  } else {
    headHome.Target.assign(carrying ? "carrying food" : "no route home");
  }
  rows.resize(used);
}

// Heading home's base and each reason's term, or nothing when heading home is not an option.
void homeTerms(const std::vector<ChoiceOption> &options, std::vector<InspectorRow> &rows) {
  size_t used = 0;
  const auto home = std::ranges::find(options, ChoiceKind::HeadHome, &ChoiceOption::Kind);
  if (home != options.end()) {
    setText(valueOf(rows, used, "Base"), "{:.2f}", home->Base);
    for (const HomeReason reason : HOME_REASONS) {
      InspectorRow &row = clearedRow(rows, used);
      appendDisplayName(row.Label, reason);
      capitalize(row.Label);
      setText(row.Value, "{:.2f}", home->Reasons[static_cast<size_t>(reason)]);
    }
  }
  rows.resize(used);
}

void shopRows(const ShopRecord &record, std::vector<InspectorRow> &rows) {
  size_t used = 0;
  for (const Good good : GOODS) {
    const GoodRecord &holding = record.Goods[static_cast<size_t>(good)];
    InspectorRow &stock = clearedRow(rows, used);
    appendDisplayName(stock.Label, good);
    capitalize(stock.Label);
    stock.Label += " stock";
    setText(stock.Value, "{}", holding.Stock);
    InspectorRow &onOrder = clearedRow(rows, used);
    appendDisplayName(onOrder.Label, good);
    capitalize(onOrder.Label);
    onOrder.Label += " on order";
    setText(onOrder.Value, "{}", holding.OnOrder);
  }
  setText(valueOf(rows, used, "Queue"), "{}", record.Queue);
  valueOf(rows, used, "Limit").assign(limitingFactorName(record.Limit));
  valueOf(rows, used, "Starved").assign(record.Starved ? "yes" : "no");
  rows.resize(used);
}

void facilityRows(const FacilityRecord &record, std::vector<InspectorRow> &rows) {
  size_t used = 0;
  setText(valueOf(rows, used, "Places"), "{}", record.Places);
  setText(valueOf(rows, used, "Taken"), "{}", record.Taken);
  setText(valueOf(rows, used, "Queue"), "{}", record.Queue);
  rows.resize(used);
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

void inspectSubject(const World &world, const InspectorSubject &subject, InspectLists &lists,
                    Inspection &inspection) {
  inspection.Rows.reserve(MOST_ROWS);
  inspection.Choices.reserve(CHOICE_ROWS);
  inspection.HomeTerms.reserve(HOME_TERMS);
  const auto key = static_cast<uint64_t>(subject.Key);
  inspection.Gone = true;
  bool hasChoices = false;
  if (subject.Kind == SubjectKind::Guest) {
    setText(inspection.Title, "Guest {}", key);
    if (const std::optional<GuestRecord> record = guestRecord(world, subject.Key)) {
      inspection.Gone = false;
      guestRows(world, *record, inspection.Rows);
      hasChoices = guestOptions(world, subject.Key, lists.Buffers, lists.Options);
      if (hasChoices) {
        choiceRows(world, lists.Options, record->Carrying, inspection.Choices);
        homeTerms(lists.Options, inspection.HomeTerms);
      }
    }
  } else if (subject.Kind == SubjectKind::Bench || subject.Kind == SubjectKind::ToiletBlock) {
    const BoxKind kind = subject.Kind == SubjectKind::Bench ? BoxKind::Bench : BoxKind::ToiletBlock;
    inspection.Title.clear();
    appendDisplayName(inspection.Title, kind);
    capitalize(inspection.Title);
    appendText(inspection.Title, " {}", key);
    if (const std::optional<FacilityRecord> record = facilityRecord(world, subject.Key)) {
      inspection.Gone = false;
      facilityRows(*record, inspection.Rows);
    }
  } else {
    setText(inspection.Title, "Shop {}", key);
    if (const std::optional<ShopRecord> record = shopRecord(world, subject.Key, lists.Buffers)) {
      inspection.Gone = false;
      shopRows(*record, inspection.Rows);
    }
  }
  if (inspection.Gone) {
    inspection.Rows.clear();
  }
  if (!hasChoices) {
    inspection.Choices.clear();
    inspection.HomeTerms.clear();
  }
}

} // namespace tpj
