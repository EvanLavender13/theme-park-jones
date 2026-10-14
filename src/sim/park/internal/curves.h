#ifndef TPJ_SIM_PARK_INTERNAL_CURVES_H
#define TPJ_SIM_PARK_INTERNAL_CURVES_H

#include "sim/entity_key.h"
#include "sim/park/internal/layout.h"
#include "sim/tick_buffers.h"

#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

// A segment end, by its segment's key and which end: 0 for From, 1 for To.
using SegmentEnd = std::pair<EntityKey, size_t>;

// The radii an edit writes last so that its joins and splits keep every curve: each point's corner
// radius, and each segment end's edge radius.
struct CurveWrites {
  std::vector<std::pair<EntityKey, double>> Radii;
  std::vector<std::pair<SegmentEnd, double>> Edges;
};

// Puts in writes the writes that keep the curves of before, whose ground is groundBefore, in
// after, the layout an edit leaves. added lists the segment ends the edit adds at points that were
// already path points. Refills writes and keeps the lists it works with in the scratch, so it
// allocates nothing once they have grown.
void keptCurves(const PathLayout &before, const PathGround &groundBefore, const PathLayout &after,
                const std::vector<SegmentEnd> &added, CurveWrites &writes, TickBuffers &scratch);

} // namespace tpj

#endif
