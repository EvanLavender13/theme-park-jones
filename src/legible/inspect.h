#ifndef TPJ_LEGIBLE_INSPECT_H
#define TPJ_LEGIBLE_INSPECT_H

#include "sim/entity_key.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// What an inspector explains: a guest, a shop, a bench, or a toilet block.
enum class SubjectKind : uint8_t { Guest, Shop, Bench, ToiletBlock };

// The entity an inspector explains and what it is, kept after the entity is gone.
struct InspectorSubject {
  EntityKey Key = NULL_KEY;
  SubjectKind Kind = SubjectKind::Guest;

  bool operator==(const InspectorSubject &) const = default;
};

// One line of an inspector: what it shows and its value, as text.
struct InspectorRow {
  std::string Label;
  std::string Value;

  bool operator==(const InspectorRow &) const = default;
};

// One row of a guest's choice table, as text: the choice, where it leads, the terms of its score,
// its score, and its probability. A need's row holds its best offer's facility and terms and the
// probability of any offer of that need. Carrying on has no target and no terms. Heading home's
// target is home and its reason, and its only term is its commitment.
struct ChoiceRow {
  std::string Choice;
  std::string Target;
  std::string Relief;
  std::string Distance;
  std::string Wait;
  std::string Commitment;
  std::string Score;
  std::string Probability;

  bool operator==(const ChoiceRow &) const = default;
};

// What an inspector shows: its title, whether its entity is gone, its lines, a guest's choices, and
// heading home's base and reason terms when heading home is an option.
struct Inspection {
  std::string Title;
  bool Gone = false;
  std::vector<InspectorRow> Rows;
  std::vector<ChoiceRow> Choices;
  std::vector<InspectorRow> HomeTerms;

  bool operator==(const Inspection &) const = default;
};

// The subject for the key: a guest when the key holds one, a shop, bench, or toilet block when it
// holds that box, and none otherwise.
std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key);

// Replaces the subject with the picked key's when it has one, and otherwise leaves it.
void pickSubject(std::optional<InspectorSubject> &subject, const World &world,
                 std::optional<EntityKey> key);

// The subject's inspector, built from its inspection record and the world's tick, or saying its
// entity is gone when the world gives no record. Changes nothing.
Inspection inspectSubject(const World &world, const InspectorSubject &subject);

} // namespace tpj

#endif
