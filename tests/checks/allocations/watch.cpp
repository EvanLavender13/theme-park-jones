#include "watch.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace tpj::check {

namespace {

// The most blocks the table notes at once. Blocks leave it when they are freed, so it holds only
// the blocks allocated while watching that are still live.
constexpr size_t TRACKED_BLOCKS = size_t{1} << 16;

struct Noted {
  const void *Address = nullptr;
  size_t Size = 0;
  uint64_t Tick = 0;
  const char *Work = "";
};

// Static storage, so noting a block allocates nothing. A null address marks an empty entry.
std::array<Noted, TRACKED_BLOCKS> Table{};
size_t NotedCount = 0;
std::array<FreedBlock, REPORTED_BLOCKS> Reported{};
size_t FreedCount = 0;
bool Watching = false;
// While paused, no block is noted, and a noted block freed loses its note without a finding.
bool Paused = false;
bool Overflowed = false;
uint64_t Tick = 0;
const char *Work = "";
void (*FreedHandler)(const FreedBlock &block) = nullptr;
// The block the last heap call allocated while watching, or a null address when the last call
// freed one.
Noted LastAllocated{};

size_t homeOf(const void *block) {
  // Blocks are aligned, so the low bits say little; Fibonacci hashing spreads the rest.
  const auto address = reinterpret_cast<uintptr_t>(block);
  const uint64_t mixed = static_cast<uint64_t>(address >> 4U) * 0x9E3779B97F4A7C15ULL;
  return static_cast<size_t>(mixed >> 48U) & (TRACKED_BLOCKS - 1);
}

void note(void *block, size_t size) {
  if (!Watching || Paused || block == nullptr) {
    return;
  }
  if (NotedCount + 1 >= TRACKED_BLOCKS) {
    Overflowed = true;
    return;
  }
  size_t at = homeOf(block);
  while (Table[at].Address != nullptr) {
    at = (at + 1) & (TRACKED_BLOCKS - 1);
  }
  Table[at] = Noted{block, size, Tick, Work};
  ++NotedCount;
  LastAllocated = Table[at];
}

// Removes the entry at the index, moving later entries of its probe run back so lookups still
// find them.
void removeAt(size_t at) {
  size_t hole = at;
  size_t next = (hole + 1) & (TRACKED_BLOCKS - 1);
  while (Table[next].Address != nullptr) {
    const size_t home = homeOf(Table[next].Address);
    // The entry may fill the hole when its home doesn't lie cyclically in (hole, next].
    const bool homeAfterHole =
        hole <= next ? (home > hole && home <= next) : (home > hole || home <= next);
    if (!homeAfterHole) {
      Table[hole] = Table[next];
      hole = next;
    }
    next = (next + 1) & (TRACKED_BLOCKS - 1);
  }
  Table[hole] = Noted{};
  --NotedCount;
}

// Whether the larger block holds the smaller one's bytes as a growing vector leaves them: all at
// its start, or, when it grew by inserting, those before the insertion at its start and the rest
// further along.
bool holdsMoved(const unsigned char *moved, size_t size, const unsigned char *larger,
                size_t largerSize) {
  size_t same = 0;
  while (same < size && moved[same] == larger[same]) {
    ++same;
  }
  if (same == size) {
    return true;
  }
  const size_t rest = size - same;
  for (size_t shift = 1; same + shift + rest <= largerSize; ++shift) {
    if (std::memcmp(moved + same, larger + same + shift, rest) == 0) {
      return true;
    }
  }
  return false;
}

// Whether freeing the noted block is a list moving to a larger block: the last heap call, last,
// allocated a larger block, still live, that holds this block's bytes as a growing vector leaves
// them. The conventions allow a list to grow while it is smaller than its largest. A temporary or
// a list rebuilt each tick is freed without that, so it is still found.
bool movesToLarger(const Noted &last, const Noted &freed) {
  return last.Address != nullptr && last.Size > freed.Size &&
         holdsMoved(static_cast<const unsigned char *>(freed.Address), freed.Size,
                    static_cast<const unsigned char *>(last.Address), last.Size);
}

void moved(void *block);

void forget(void *block) {
  if (!Watching || block == nullptr) {
    return;
  }
  if (Paused) {
    moved(block);
    return;
  }
  const Noted last = LastAllocated;
  LastAllocated = Noted{};
  if (NotedCount == 0) {
    return;
  }
  for (size_t at = homeOf(block); Table[at].Address != nullptr;
       at = (at + 1) & (TRACKED_BLOCKS - 1)) {
    if (Table[at].Address == block) {
      const Noted noted = Table[at];
      removeAt(at);
      if (movesToLarger(last, noted)) {
        return;
      }
      const FreedBlock freed{noted.Size, noted.Tick, Tick, noted.Work, Work};
      if (FreedCount < REPORTED_BLOCKS) {
        Reported[FreedCount] = freed;
      }
      ++FreedCount;
      blockFreed(freed);
      if (FreedHandler != nullptr) {
        FreedHandler(freed);
      }
      return;
    }
  }
}

// Removes the block's entry, if it has one, without reporting it, for a block that realloc moved or
// resized.
void moved(void *block) {
  if (!Watching || block == nullptr) {
    return;
  }
  LastAllocated = Noted{};
  if (NotedCount == 0) {
    return;
  }
  for (size_t at = homeOf(block); Table[at].Address != nullptr;
       at = (at + 1) & (TRACKED_BLOCKS - 1)) {
    if (Table[at].Address == block) {
      removeAt(at);
      return;
    }
  }
}

void *allocate(size_t size) {
  void *block = std::malloc(size == 0 ? 1 : size);
  note(block, size);
  return block;
}

void *allocateAligned(size_t size, std::align_val_t alignment) {
  const auto align = static_cast<size_t>(alignment);
  const size_t rounded = ((size == 0 ? 1 : size) + align - 1) / align * align;
#ifdef _WIN32
  void *block = _aligned_malloc(rounded, align);
#else
  void *block = std::aligned_alloc(align, rounded);
#endif
  note(block, size);
  return block;
}

void release(void *block) {
  forget(block);
  std::free(block);
}

void releaseAligned(void *block) {
  forget(block);
#ifdef _WIN32
  _aligned_free(block);
#else
  std::free(block);
#endif
}

void *orThrow(void *block) {
  if (block == nullptr) {
    throw std::bad_alloc();
  }
  return block;
}

} // namespace

void *watchedMalloc(size_t size) { return allocate(size); }

void *watchedCalloc(size_t count, size_t size) {
  void *block = allocate(count * size);
  if (block != nullptr) {
    std::memset(block, 0, count * size);
  }
  return block;
}

void *watchedRealloc(void *block, size_t size) {
  // The entry goes first, since the old block can't be read once realloc moved it. If realloc
  // fails, the block stays unnoted, which can hide a free but never reports a false one.
  moved(block);
  void *resized = std::realloc(block, size == 0 ? 1 : size);
  if (resized != nullptr) {
    note(resized, size);
    // A realloc frees nothing the next free could be a move to.
    LastAllocated = Noted{};
  }
  return resized;
}

void watchedFree(void *block) { release(block); }

void startWatching(uint64_t tick) {
  Table.fill(Noted{});
  NotedCount = 0;
  FreedCount = 0;
  Overflowed = false;
  LastAllocated = Noted{};
  Tick = tick;
  Watching = true;
  Paused = false;
}

void watchTick(uint64_t tick) { Tick = tick; }

void watchWork(const char *work) { Work = work; }

const char *currentWork() { return Work; }

void stopWatching() {
  Watching = false;
  Paused = false;
}

void pauseWatching() {
  Paused = true;
  LastAllocated = Noted{};
}

void resumeWatching() {
  Paused = false;
  LastAllocated = Noted{};
}

size_t freedCount() { return FreedCount; }

std::span<const FreedBlock> freedBlocks() {
  return std::span<const FreedBlock>(Reported).first(
      FreedCount < REPORTED_BLOCKS ? FreedCount : REPORTED_BLOCKS);
}

bool tableOverflowed() { return Overflowed; }

[[gnu::noinline]] void blockFreed(const FreedBlock & /*block*/) {
  // An empty body the compiler keeps, so a debugger can stop here.
  asm volatile("");
}

void onFreed(void (*handler)(const FreedBlock &block)) { FreedHandler = handler; }

} // namespace tpj::check

// The replaced allocation functions: every form of operator new and operator delete.

void *operator new(size_t size) { return tpj::check::orThrow(tpj::check::allocate(size)); }

void *operator new[](size_t size) { return tpj::check::orThrow(tpj::check::allocate(size)); }

void *operator new(size_t size, const std::nothrow_t & /*tag*/) noexcept {
  return tpj::check::allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t & /*tag*/) noexcept {
  return tpj::check::allocate(size);
}

void *operator new(size_t size, std::align_val_t alignment) {
  return tpj::check::orThrow(tpj::check::allocateAligned(size, alignment));
}

void *operator new[](size_t size, std::align_val_t alignment) {
  return tpj::check::orThrow(tpj::check::allocateAligned(size, alignment));
}

void *operator new(size_t size, std::align_val_t alignment,
                   const std::nothrow_t & /*tag*/) noexcept {
  return tpj::check::allocateAligned(size, alignment);
}

void *operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t & /*tag*/) noexcept {
  return tpj::check::allocateAligned(size, alignment);
}

void operator delete(void *block) noexcept { tpj::check::release(block); }

void operator delete[](void *block) noexcept { tpj::check::release(block); }

void operator delete(void *block, size_t /*size*/) noexcept { tpj::check::release(block); }

void operator delete[](void *block, size_t /*size*/) noexcept { tpj::check::release(block); }

void operator delete(void *block, const std::nothrow_t & /*tag*/) noexcept {
  tpj::check::release(block);
}

void operator delete[](void *block, const std::nothrow_t & /*tag*/) noexcept {
  tpj::check::release(block);
}

void operator delete(void *block, std::align_val_t /*alignment*/) noexcept {
  tpj::check::releaseAligned(block);
}

void operator delete[](void *block, std::align_val_t /*alignment*/) noexcept {
  tpj::check::releaseAligned(block);
}

void operator delete(void *block, size_t /*size*/, std::align_val_t /*alignment*/) noexcept {
  tpj::check::releaseAligned(block);
}

void operator delete[](void *block, size_t /*size*/, std::align_val_t /*alignment*/) noexcept {
  tpj::check::releaseAligned(block);
}

void operator delete(void *block, std::align_val_t /*alignment*/,
                     const std::nothrow_t & /*tag*/) noexcept {
  tpj::check::releaseAligned(block);
}

void operator delete[](void *block, std::align_val_t /*alignment*/,
                       const std::nothrow_t & /*tag*/) noexcept {
  tpj::check::releaseAligned(block);
}
