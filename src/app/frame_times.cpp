#include "app/frame_times.h"

namespace tpj {

void FrameTimes::add(int64_t nanoseconds) {
  // The first covers no frame, and the second the first frame, which follows loading.
  if (++Given > 2) {
    Kept.push_back(nanoseconds);
  }
}

std::span<const int64_t> FrameTimes::kept() const { return Kept; }

std::string FrameTimes::line() const {
  std::string text = "frame-times";
  for (const int64_t duration : Kept) {
    text += ' ';
    text += std::to_string(duration);
  }
  return text;
}

} // namespace tpj
