#ifndef TPJ_SIM_PARK_INTERNAL_AROUND_H
#define TPJ_SIM_PARK_INTERNAL_AROUND_H

#include "sim/park/intent.h"

namespace tpj {

// Positive when b lies counter-clockwise of a, seen from above.
inline double ccwOf(ParkPoint a, ParkPoint b) { return a.Z * b.X - a.X * b.Z; }

// Whether a's direction comes before b's, counter-clockwise from +x seen from above. The paving and
// the curves kept through edits order the segment ends around a point by it.
inline bool comesBefore(ParkPoint a, ParkPoint b) {
  const auto halfOf = [](ParkPoint d) { return d.Z < 0.0 || (d.Z == 0.0 && d.X > 0.0) ? 0 : 1; };
  if (halfOf(a) != halfOf(b)) {
    return halfOf(a) < halfOf(b);
  }
  return ccwOf(a, b) > 0.0;
}

} // namespace tpj

#endif
