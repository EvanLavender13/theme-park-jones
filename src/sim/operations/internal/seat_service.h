#ifndef TPJ_SIM_OPERATIONS_INTERNAL_SEAT_SERVICE_H
#define TPJ_SIM_OPERATIONS_INTERNAL_SEAT_SERVICE_H

#include <stdint.h>
#include <vector>

namespace tpj {

// State, on a piece of furniture's entity: the tick each taken seat frees, ascending.
struct SeatService {
  std::vector<uint64_t> FreeAt;
};

template <typename Visitor> void visitFields(Visitor &visitor, SeatService &service) {
  visitor.field("free-at", service.FreeAt);
}

} // namespace tpj

#endif
