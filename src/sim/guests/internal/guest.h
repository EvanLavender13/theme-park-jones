#ifndef TPJ_SIM_GUESTS_INTERNAL_GUEST_H
#define TPJ_SIM_GUESTS_INTERNAL_GUEST_H

#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <stdint.h>

namespace tpj {

// State, on a guest's entity: where it stands on the guest network, whether it walks toward
// higher distances along that place's carrier, what it is doing, how hungry it is and how fast
// that rises, and the tick its stay ends.
struct Guest {
  Place At;
  bool Forward = true;
  GuestActivity Activity = GuestActivity::Wandering;
  double Hunger = 0.0;
  double HungerRate = 0.0;
  uint64_t StayUntil = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Guest &guest) {
  visitor.field("at", guest.At);
  visitor.field("forward", guest.Forward);
  visitor.field("activity", guest.Activity);
  visitor.field("hunger", guest.Hunger);
  visitor.field("hunger-rate", guest.HungerRate);
  visitor.field("stay-until", guest.StayUntil);
}

} // namespace tpj

#endif
