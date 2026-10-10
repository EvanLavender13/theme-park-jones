#ifndef TPJ_SIM_OPERATIONS_INTERNAL_FACILITY_SERVICE_H
#define TPJ_SIM_OPERATIONS_INTERNAL_FACILITY_SERVICE_H

#include "sim/entity_key.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// State, on a bench or toilet block box's entity: one guest key for each visit unit it holds,
// waiting at its door in the order it took them in, and the tick each taken place frees,
// ascending.
struct FacilityService {
  std::vector<EntityKey> Queue;
  std::vector<uint64_t> FreeAt;
};

template <typename Visitor> void visitFields(Visitor &visitor, FacilityService &service) {
  visitor.field("queue", service.Queue);
  visitor.field("free-at", service.FreeAt);
}

} // namespace tpj

#endif
