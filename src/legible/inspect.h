#ifndef TPJ_LEGIBLE_INSPECT_H
#define TPJ_LEGIBLE_INSPECT_H

#include "sim/entity_key.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// What an inspector explains: a guest or a shop.
enum class SubjectKind : uint8_t { Guest, Shop };

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

// One option of a guest's last choice, as text: the option, its terms, score, and probability,
// and whether the guest picked it. Carrying on and heading home have empty terms.
struct ChoiceRow {
  std::string Option;
  std::string Relief;
  std::string Distance;
  std::string Wait;
  std::string Commitment;
  std::string Score;
  std::string Probability;
  bool Picked = false;

  bool operator==(const ChoiceRow &) const = default;
};

// What an inspector shows: its title, whether its entity is gone, its lines, and a guest's last
// choice.
struct Inspection {
  std::string Title;
  bool Gone = false;
  std::vector<InspectorRow> Rows;
  std::vector<ChoiceRow> Choices;

  bool operator==(const Inspection &) const = default;
};

// The subject for the key: a guest when the key holds one, a shop when it holds a shop box, and
// none otherwise.
std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key);

// Replaces the subject with the picked key's when it has one, and otherwise leaves it.
void pickSubject(std::optional<InspectorSubject> &subject, const World &world,
                 std::optional<EntityKey> key);

// The subject's inspector, built from its inspection record and the world's tick, or saying its
// entity is gone when the world gives no record. Changes nothing.
Inspection inspectSubject(const World &world, const InspectorSubject &subject);

} // namespace tpj

#endif
