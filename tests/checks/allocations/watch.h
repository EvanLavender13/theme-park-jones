#ifndef TPJ_TESTS_CHECKS_ALLOCATIONS_WATCH_H
#define TPJ_TESTS_CHECKS_ALLOCATIONS_WATCH_H

#include <span>
#include <stddef.h>
#include <stdint.h>

// Watches the program's heap: while watching, every block allocated is noted, and freeing a noted
// block is found. watch.cpp replaces the global operator new and operator delete, and gives SDL its
// allocation functions, to see every allocation in the program and in SDL.
namespace tpj::check {

// A block allocated and freed while watching, with the ticks the watch was at for each, and the
// work that allocated it and the work that freed it.
struct FreedBlock {
  size_t Size = 0;
  uint64_t Allocated = 0;
  uint64_t Freed = 0;
  const char *AllocatedBy = "";
  const char *FreedBy = "";
};

inline constexpr size_t REPORTED_BLOCKS = 20;

// Starts watching at the tick: every block allocated from now is noted. Forgets earlier findings.
void startWatching(uint64_t tick);
// The tick the next allocations and frees are counted at.
void watchTick(uint64_t tick);
// The work the next allocations and frees are counted against. The name must outlive the watch, as
// a string literal does.
void watchWork(const char *work);
void stopWatching();
// How many noted blocks were freed while watching, and the first REPORTED_BLOCKS of them.
size_t freedCount();
std::span<const FreedBlock> freedBlocks();
// True when more blocks were noted at once than the table holds.
bool tableOverflowed();
// Allocation functions with the C library's signatures, which watch the blocks they allocate and
// free as operator new and delete do. Give them to SDL with SDL_SetMemoryFunctions before SDL
// allocates anything. A realloc counts as its block growing, as a list moving to a larger block
// does, so it is never found.
void *watchedMalloc(size_t size);
void *watchedCalloc(size_t count, size_t size);
void *watchedRealloc(void *block, size_t size);
void watchedFree(void *block);

// Called at each noted block freed while watching. Stop here in a debugger to see who frees it.
void blockFreed(const FreedBlock &block);

} // namespace tpj::check

#endif
