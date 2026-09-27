#ifndef TPJ_SIM_MIX_H
#define TPJ_SIM_MIX_H

#include <stdint.h>
#include <string_view>

namespace tpj {

// SplitMix64's finalizer (Steele, Lea, and Flood, with the constants of Vigna's splitmix64.c): a
// bijection on 64-bit words whose output depends on every input bit.
constexpr uint64_t mix64(uint64_t x) {
  x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31U);
}

// Folds a sequence of 64-bit words into one, for the state hash, derived keys, and names.
class Hasher {
public:
  constexpr void add(uint64_t word) { State = mix64((State ^ word) + GAMMA); }
  [[nodiscard]] constexpr uint64_t value() const { return State; }

private:
  static constexpr uint64_t GAMMA = 0x9e3779b97f4a7c15ULL;
  uint64_t State = 0x6a09e667f3bcc909ULL;
};

// A stable 64-bit identifier for a name, the same on every build.
constexpr uint64_t hashName(std::string_view name) {
  Hasher hasher;
  for (const char ch : name) {
    hasher.add(static_cast<unsigned char>(ch));
  }
  hasher.add(name.size());
  return hasher.value();
}

} // namespace tpj

#endif
