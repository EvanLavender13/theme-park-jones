#ifndef TPJ_TESTS_TOOLS_SUPPORT_TOOL_PARK_H
#define TPJ_TESTS_TOOLS_SUPPORT_TOOL_PARK_H

#include "sim/entity_key.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <string_view>

namespace tpj::test {

inline constexpr EntityKey ENTRANCE{1};
inline constexpr EntityKey TEMPLATE_PATH{2};
// Backstage, along x = -40 from z = -50 to 50.
inline constexpr EntityKey BACKSTAGE_PATH{3};
// Guest, along z = 0 from x = -60 to -20, crossing the backstage path at (-40, 0).
inline constexpr EntityKey CROSSING_PATH{4};
// A shop facing (2, 0), so 6 m deep along x and 8 m wide along z: x from 37 to 43, z from -4 to 4.
inline constexpr EntityKey TURNED_SHOP{5};
// A shop facing (0, -1): x from 40 to 48, z from -3 to 3, overlapping the turned shop.
inline constexpr EntityKey OVERLAPPING_SHOP{6};
// A depot facing (0, -1): x from -46 to -34, z from 16 to 24, standing over the backstage path.
inline constexpr EntityKey DEPOT_ON_PATH{7};
// A shop at (80, -80) with a facing of zero length, so it has no footprint.
inline constexpr EntityKey UNFACING_SHOP{8};

// Overlaps and a box with no footprint, which only a save can hold, so the picking rules show
// where more than one entity, or none, could answer.
inline constexpr std::string_view TOOL_PARK =
    "tpj-park 1\nseed 1\ntick 0\nnext-key 9\n"
    "\n[entrance]\n"
    "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
    "\n[path]\n"
    "2 kind=guest points=[{x=0 z=123} {x=0 z=103}]\n"
    "3 kind=backstage points=[{x=-40 z=-50} {x=-40 z=50}]\n"
    "4 kind=guest points=[{x=-60 z=0} {x=-20 z=0}]\n"
    "\n[box]\n"
    "5 kind=shop x=40 z=0 facing-x=2 facing-z=0\n"
    "6 kind=shop x=44 z=0 facing-x=0 facing-z=-1\n"
    "7 kind=depot x=-40 z=20 facing-x=0 facing-z=-1\n"
    "8 kind=shop x=80 z=-80 facing-x=0 facing-z=0\n";

inline World toolPark() { return loadWorld(makeParkSchema(), TOOL_PARK); }

} // namespace tpj::test

#endif
