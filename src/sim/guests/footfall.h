#ifndef TPJ_SIM_GUESTS_FOOTFALL_H
#define TPJ_SIM_GUESTS_FOOTFALL_H

#include "sim/medium/field.h"
#include "sim/medium/kept_field.h"

#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// The hungry footfall field: each guest-network stretch's moving average of the summed hunger of
// the guests on it, kept at the midpoints of the stretches guests have stood in, and at each node
// the mean of the stretches meeting it, worked out when read.
struct HungryFootfall {
  using Entry = double;
  static constexpr std::string_view Name = "hungry-footfall";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  // A value the ticks after it was kept, decayed as an empty stretch's:
  // value * simExp(ticks * simLog(1 - 1 / FOOTFALL_TIME)), so the value itself at 0 ticks.
  static double readKept(double value, uint64_t ticks);

  // The values of the source's entries strictly inside the sampled edge, in order, ignoring those
  // at its nodes.
  static std::vector<double> sampleEdge(const EdgeSample<double> &sample);

  // The mean, over the node's edge ends, of each end's stretch value: the sum of the values of
  // the entries strictly inside its edge.
  static std::vector<double> sampleNode(const NodeSample &sample);
};

// The moving average's time constant, in ticks: each tick a stretch's value moves 1/FOOTFALL_TIME
// of the way toward its guests' summed hunger.
inline constexpr uint64_t FOOTFALL_TIME = 300;

} // namespace tpj

#endif
