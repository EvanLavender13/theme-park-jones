#ifndef TPJ_SIM_GUESTS_FOOTFALL_H
#define TPJ_SIM_GUESTS_FOOTFALL_H

#include "sim/medium/field.h"

#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// The hungry footfall field: each guest-network stretch's moving average of the summed hunger of
// the guests on it, one entry per stretch at its midpoint, and at each node the mean of the
// stretches meeting it.
struct HungryFootfall {
  using Entry = double;
  static constexpr std::string_view Name = "hungry-footfall";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  // The values of the source's entries strictly inside the sampled edge, in order, ignoring those
  // at its nodes.
  static std::vector<double> sampleEdge(const EdgeSample<double> &sample);
};

// The moving average's time constant, in ticks: each tick a stretch's value moves 1/FOOTFALL_TIME
// of the way toward its guests' summed hunger.
inline constexpr uint64_t FOOTFALL_TIME = 300;

} // namespace tpj

#endif
