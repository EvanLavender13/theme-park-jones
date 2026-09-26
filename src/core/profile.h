#ifndef TPJ_CORE_PROFILE_H
#define TPJ_CORE_PROFILE_H

// Profiling macros. Code includes this header, never Tracy directly, so the profiler can be
// swapped or compiled out. With TPJ_TRACY off every macro expands to nothing.

#ifdef TPJ_TRACY
#include <tracy/Tracy.hpp>
#define TPJ_PROFILE_FRAME() FrameMark
#define TPJ_PROFILE_ZONE() ZoneScoped
#define TPJ_PROFILE_ZONE_NAMED(name) ZoneScopedN(name)
#else
#define TPJ_PROFILE_FRAME()
#define TPJ_PROFILE_ZONE()
#define TPJ_PROFILE_ZONE_NAMED(name)
#endif

#endif
