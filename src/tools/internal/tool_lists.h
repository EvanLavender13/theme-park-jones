#ifndef TPJ_TOOLS_INTERNAL_TOOL_LISTS_H
#define TPJ_TOOLS_INTERNAL_TOOL_LISTS_H

#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/tick_buffers.h"
#include "tools/internal/frontage.h"
#include "tools/internal/snapping.h"

#include <vector>

namespace tpj {

// The lists the tools reuse from call to call: the stop positions a path tool works out,
// snapping's lists, the frontage search's, and the lists the sim's readers keep for the tool park.
// Pressed holds the path a press asks the world about, as an AddPath refilled in place, and
// Planned its plan.
struct ToolLists {
  std::vector<ParkPoint> Positions;
  ParkEdit Pressed = AddPath{};
  PlannedEdit Planned;
  SnapLists Snapping;
  FrontageLists Frontage;
  TickBuffers Buffers;
};

} // namespace tpj

#endif
