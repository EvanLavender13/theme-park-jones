#ifndef TPJ_SIM_GUESTS_INTERNAL_GUEST_H
#define TPJ_SIM_GUESTS_INTERNAL_GUEST_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/routes/route_distance.h"

#include <stdint.h>

namespace tpj {

// State, on a guest's entity: where it stands on the guest network, where it stands on the
// ground, the place on its route it walks toward, the way it leaves the next node it has chosen
// for (an empty step at the node when it isn't wandering, and with carrier NULL_KEY until it has
// chosen), whether it walks toward higher distances along its place's carrier, what it is doing,
// how hungry it is and how fast that rises, how thirsty it is and how fast that rises, how tired it
// is and how fast that rises with each meter walked, how full its bladder is, how fast it walks in
// meters per second, its share of the width on its side, the tick its stay ends, the facility it
// is heading to or waiting at and the need it goes there for, the relief of the offer it last
// picked, and the meals it has eaten and the last of them.
struct Guest {
  Place At;
  GroundPoint Ground;
  Place Aim;
  RouteStep Turn;
  bool Forward = true;
  GuestActivity Activity = GuestActivity::Wandering;
  double Hunger = 0.0;
  double HungerRate = 0.0;
  double Thirst = 0.0;
  double ThirstRate = 0.0;
  double Tiredness = 0.0;
  double TirednessRate = 0.0;
  double Bladder = 0.0;
  double Speed = WALK_SPEED;
  double Share = 0.5;
  uint64_t StayUntil = 0;
  EntityKey Target = NULL_KEY;
  GuestNeed TargetNeed = GuestNeed::Hunger;
  double ServingRelief = 0.0;
  uint64_t MealsEaten = 0;
  GuestMeal LastMeal;
};

template <typename Visitor> void visitFields(Visitor &visitor, Guest &guest) {
  visitor.field("at", guest.At);
  visitor.field("x", guest.Ground.X);
  visitor.field("z", guest.Ground.Z);
  visitor.field("aim", guest.Aim);
  visitor.field("turn", guest.Turn);
  visitor.field("forward", guest.Forward);
  visitor.field("activity", guest.Activity);
  visitor.field("hunger", guest.Hunger);
  visitor.field("hunger-rate", guest.HungerRate);
  visitor.field("thirst", guest.Thirst);
  visitor.field("thirst-rate", guest.ThirstRate);
  visitor.field("tiredness", guest.Tiredness);
  visitor.field("tiredness-rate", guest.TirednessRate);
  visitor.field("bladder", guest.Bladder);
  visitor.field("speed", guest.Speed);
  visitor.field("share", guest.Share);
  visitor.field("stay-until", guest.StayUntil);
  visitor.field("target", guest.Target);
  visitor.field("target-need", guest.TargetNeed);
  visitor.field("serving-relief", guest.ServingRelief);
  visitor.field("meals-eaten", guest.MealsEaten);
  visitor.field("last-meal", guest.LastMeal);
}

} // namespace tpj

#endif
