#ifndef TPJ_SCENARIOS_STALLS_H
#define TPJ_SCENARIOS_STALLS_H

#include "sim/medium/field.h"

#include <string_view>

namespace tpj {

// The stalls scenario's fields and flow kinds, declared here so tests can audit its world.

// Each stall's offer: its distance along the track over 10 when resolved, and the goods it holds
// when stepped.
struct StallOffer {
  using Entry = double;
  static constexpr std::string_view Name = "stall-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// Crowding: 1 at each stall when resolved, and 1 at each visitor when stepped.
struct Crowd {
  using Entry = double;
  static constexpr std::string_view Name = "crowd";
  static constexpr FieldKind Kind = FieldKind::Scalar;
};

// Goods the depot makes and ships to stalls, which sell them.
struct Goods {
  static constexpr std::string_view Name = "goods";
};

// Visits a visitor sends to a stall, addressed to itself, and the stall sends back.
struct Visits {
  static constexpr std::string_view Name = "visits";
};

} // namespace tpj

#endif
