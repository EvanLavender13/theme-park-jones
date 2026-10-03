#ifndef TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H
#define TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H

namespace tpj {

class WorldSchema;

// Registers hungry footfall: its kept field, the system that averages it, and the finisher that
// carries it across each resolution. addGuests calls it after its own registrations.
void addFootfall(WorldSchema &schema);

} // namespace tpj

#endif
