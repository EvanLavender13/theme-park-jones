#include "legible/needs.h"

#include "sim/medium/field.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"

namespace tpj {
namespace {

const AvailabilityDefinition FOOD{&sourceEntryAtNode<FoodOffer>, DISCOUNT_CURVE, MEAL_RELIEF};
const AvailabilityDefinition DRINK{&sourceEntryAtNode<DrinkOffer>, DISCOUNT_CURVE, DRINK_RELIEF};
const AvailabilityDefinition REST{&sourceEntryAtNode<RestOffer>, DISCOUNT_CURVE, REST_RELIEF};
const AvailabilityDefinition TOILET{&sourceEntryAtNode<ToiletOffer>, DISCOUNT_CURVE, TOILET_RELIEF};

} // namespace

const AvailabilityDefinition &needAvailability(GuestNeed need) {
  switch (need) {
  case GuestNeed::Hunger:
    return FOOD;
  case GuestNeed::Thirst:
    return DRINK;
  case GuestNeed::Tiredness:
    return REST;
  case GuestNeed::Bladder:
    return TOILET;
  }
  return FOOD;
}

std::string_view needAvailabilityName(GuestNeed need) {
  switch (need) {
  case GuestNeed::Hunger:
    return "Food";
  case GuestNeed::Thirst:
    return "Drink";
  case GuestNeed::Tiredness:
    return "Rest";
  case GuestNeed::Bladder:
    return "Toilet";
  }
  return "Food";
}

} // namespace tpj
