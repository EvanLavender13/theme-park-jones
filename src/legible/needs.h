#ifndef TPJ_LEGIBLE_NEEDS_H
#define TPJ_LEGIBLE_NEEDS_H

#include "legible/availability.h"
#include "sim/guests/guests.h"

#include <string_view>

namespace tpj {

// The availability of a need: food from shops' food offers, drink from shops' drink offers, rest
// from benches' offers, and the toilet from toilet blocks' offers, each discounted by
// DISCOUNT_CURVE, with an overlay of it halfway up its ramp at one serving's relief with no walk
// and no wait.
const AvailabilityDefinition &needAvailability(GuestNeed need);

// The need's name as the Overlay row and its tooltip show it: Food, Drink, Rest, or Toilet.
std::string_view needAvailabilityName(GuestNeed need);

} // namespace tpj

#endif
