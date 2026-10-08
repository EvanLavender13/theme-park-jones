#ifndef TPJ_SIM_GUESTS_INTERNAL_PRESENCE_H
#define TPJ_SIM_GUESTS_INTERNAL_PRESENCE_H

#include "sim/entity_key.h"
#include "sim/medium/ground_field.h"
#include "sim/medium/network.h"

#include <span>
#include <string_view>

namespace tpj {

// How a guest moved over the last tick it walked, in meters per second.
struct Stride {
  double X = 0.0;
  double Z = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Stride &stride) {
  visitor.field("x", stride.X);
  visitor.field("z", stride.Z);
}

// Guest presence: each walking guest's entry, where it stands and how it moved over the last tick
// it walked. Only guests publish and sample it.
struct GuestPresence {
  using Entry = Stride;
  static constexpr std::string_view Name = "guest-presence";
};

// The guest's step aside over one tick, as an offset on the ground, from the guests sampled near
// it. Heading is the unit direction it walks in at its speed. The offset is square to the heading,
// at most SIDESTEP_SPEED times SIM_TICK_SECONDS long, and zero when no guest ahead would come
// within SIDESTEP_DISTANCE of it within SIDESTEP_HORIZON. Allocates nothing.
GroundPoint sidestep(EntityKey self, GroundPoint at, GroundDirection heading, double speed,
                     std::span<const GroundSample<Stride>> near);

} // namespace tpj

#endif
