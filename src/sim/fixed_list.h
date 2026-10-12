#ifndef TPJ_SIM_FIXED_LIST_H
#define TPJ_SIM_FIXED_LIST_H

#include <array>
#include <stddef.h>
#include <stdexcept>
#include <utility>

namespace tpj {

// A list of at most N values, held in place, so making, copying, and dropping one allocates
// nothing. A component may hold one wherever it may hold a vector: walks, saves, and loads treat
// it exactly as a vector.
template <typename T, size_t N> class FixedList {
public:
  [[nodiscard]] size_t size() const { return Count; }
  [[nodiscard]] bool empty() const { return Count == 0; }
  [[nodiscard]] static constexpr size_t capacity() { return N; }

  T *begin() { return Values.data(); }
  T *end() { return Values.data() + Count; }
  [[nodiscard]] const T *begin() const { return Values.data(); }
  [[nodiscard]] const T *end() const { return Values.data() + Count; }

  T &operator[](size_t index) { return Values[index]; }
  const T &operator[](size_t index) const { return Values[index]; }
  T &front() { return Values[0]; }
  [[nodiscard]] const T &front() const { return Values[0]; }
  T &back() { return Values[Count - 1]; }
  [[nodiscard]] const T &back() const { return Values[Count - 1]; }

  // Throws std::length_error when the list is full.
  void push_back(const T &value) { emplace_back() = value; }
  // Appends a default value and gives it. Throws std::length_error when the list is full.
  T &emplace_back() {
    if (Count == N) {
      throw std::length_error("a fixed list is full");
    }
    Values[Count] = T{};
    return Values[Count++];
  }
  void clear() { Count = 0; }
  // Removes the values from first up to last, keeping the order of the rest.
  T *erase(T *first, T *last) {
    T *kept = first;
    for (T *moved = last; moved != end(); ++moved, ++kept) {
      *kept = std::move(*moved);
    }
    Count -= static_cast<size_t>(last - first);
    return first;
  }

private:
  std::array<T, N> Values{};
  size_t Count = 0;
};

} // namespace tpj

#endif
