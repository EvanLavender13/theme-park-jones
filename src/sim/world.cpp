#include "sim/world.h"

#include "core/profile.h"

namespace tpj {

void stepWorld(World &world) {
  TPJ_PROFILE_ZONE();
  ++world.Tick;
}

} // namespace tpj
