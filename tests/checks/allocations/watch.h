#ifndef TPJ_TESTS_CHECKS_ALLOCATIONS_WATCH_H
#define TPJ_TESTS_CHECKS_ALLOCATIONS_WATCH_H

#include <span>
#include <stddef.h>
#include <stdint.h>

// Watches the program's heap: while watching, every block allocated is noted, and freeing a noted
// block is found. watch.cpp replaces the global operator new and operator delete to do it, so it
// sees every allocation in the program.
namespace tpj::check {

// A block allocated and freed while watching, with the ticks the watch was at for each.
struct FreedBlock {
  size_t Size = 0;
  uint64_t Allocated = 0;
  uint64_t Freed = 0;
};

inline constexpr size_t REPORTED_BLOCKS = 20;

// Starts watching at the tick: every block allocated from now is noted. Forgets earlier findings.
void startWatching(uint64_t tick);
// The tick the next allocations and frees are counted at.
void watchTick(uint64_t tick);
void stopWatching();
// How many noted blocks were freed while watching, and the first REPORTED_BLOCKS of them.
size_t freedCount();
std::span<const FreedBlock> freedBlocks();
// True when more blocks were noted at once than the table holds.
bool tableOverflowed();
// Called at each noted block freed while watching. Stop here in a debugger to see who frees it.
void blockFreed(const FreedBlock &block);

} // namespace tpj::check

#endif
