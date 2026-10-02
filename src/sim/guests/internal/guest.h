#ifndef TPJ_SIM_GUESTS_INTERNAL_GUEST_H
#define TPJ_SIM_GUESTS_INTERNAL_GUEST_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <stdint.h>

namespace tpj {

// State, on a guest's entity: where it stands on the guest network, whether it walks toward
// higher distances along that place's carrier, what it is doing, how hungry it is and how fast
// that rises, the tick its stay ends, the shop it is heading to or waiting at, the relief of the
// offer it last picked, and the meals it has eaten and the last of them.
struct Guest {
  Place At;
  bool Forward = true;
  GuestActivity Activity = GuestActivity::Wandering;
  double Hunger = 0.0;
  double HungerRate = 0.0;
  uint64_t StayUntil = 0;
  EntityKey Target = NULL_KEY;
  double MealRelief = 0.0;
  uint64_t MealsEaten = 0;
  GuestMeal LastMeal;
};

template <typename Visitor> void visitFields(Visitor &visitor, Guest &guest) {
  visitor.field("at", guest.At);
  visitor.field("forward", guest.Forward);
  visitor.field("activity", guest.Activity);
  visitor.field("hunger", guest.Hunger);
  visitor.field("hunger-rate", guest.HungerRate);
  visitor.field("stay-until", guest.StayUntil);
  visitor.field("target", guest.Target);
  visitor.field("meal-relief", guest.MealRelief);
  visitor.field("meals-eaten", guest.MealsEaten);
  visitor.field("last-meal", guest.LastMeal);
}

} // namespace tpj

#endif
