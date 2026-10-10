#ifndef TPJ_SIM_GUESTS_INTERNAL_DEPARTURES_H
#define TPJ_SIM_GUESTS_INTERNAL_DEPARTURES_H

#include "sim/guests/guests.h"

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace tpj {

// State, on the departure tally's entity: how many guests have walked out of an entrance since the
// park last opened, by the reason they left, in HOME_REASONS order.
struct Departures {
  std::array<uint64_t, HOME_REASONS.size()> ByReason{};
};

template <typename Visitor> void visitFields(Visitor &visitor, Departures &departures) {
  for (const HomeReason reason : HOME_REASONS) {
    const size_t index = static_cast<size_t>(reason);
    visitor.field(enumNames(reason)[index], departures.ByReason[index]);
  }
}

} // namespace tpj

#endif
