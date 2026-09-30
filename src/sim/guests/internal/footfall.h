#ifndef TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H
#define TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H

#include "sim/medium/field.h"

#include <vector>

namespace tpj {

class WorldSchema;

// State, on hungry footfall's field entity: each stretch's value at the place it is held at, in
// the order the stretches were held.
struct Footfall {
  std::vector<PlacedEntry<double>> Stretches;
};

template <typename Visitor> void visitFields(Visitor &visitor, Footfall &footfall) {
  visitor.field("stretches", footfall.Stretches);
}

// Registers hungry footfall: its field, its state, the system that averages it, and the finisher
// that carries it across each resolution. addGuests calls it after its own registrations.
void addFootfall(WorldSchema &schema);

} // namespace tpj

#endif
