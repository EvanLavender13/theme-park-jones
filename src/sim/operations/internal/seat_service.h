#ifndef TPJ_SIM_OPERATIONS_INTERNAL_SEAT_SERVICE_H
#define TPJ_SIM_OPERATIONS_INTERNAL_SEAT_SERVICE_H

#include "sim/fixed_list.h"
#include "sim/furniture/furniture.h"

#include <stdint.h>

namespace tpj {

// State, on a piece of furniture's entity: the tick each taken seat frees, ascending.
struct SeatService {
  FixedList<uint64_t, MOST_FURNITURE_SEATS> FreeAt;
};

template <typename Visitor> void visitFields(Visitor &visitor, SeatService &service) {
  visitor.field("free-at", service.FreeAt);
}

} // namespace tpj

#endif
