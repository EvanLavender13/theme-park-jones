#ifndef TPJ_SIM_OPERATIONS_INTERNAL_SHOP_SERVICE_H
#define TPJ_SIM_OPERATIONS_INTERNAL_SHOP_SERVICE_H

#include "sim/entity_key.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// State, on a shop box's entity: one guest key for each visit unit the shop holds, in the order it
// took them in, and the first tick at which it may serve.
struct ShopService {
  std::vector<EntityKey> Queue;
  uint64_t FreeAt = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, ShopService &service) {
  visitor.field("queue", service.Queue);
  visitor.field("free-at", service.FreeAt);
}

} // namespace tpj

#endif
