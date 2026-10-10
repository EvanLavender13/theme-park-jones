#include "app/options.h"

#include <SDL3/SDL_log.h>

#include <charconv>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <system_error>

namespace tpj {
namespace {

bool parseCount(const char *text, uint64_t &count) {
  const std::string_view value(text);
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, count);
  return !value.empty() && result.ec == std::errc() && result.ptr == end;
}

// Whether the options make sense together: --hash opens no window, so it refuses every option that
// needs one, and --frame-times needs a positive --frames.
bool consistent(const Options &options, bool framesGiven, bool speedGiven) {
  const bool needsWindow = framesGiven || speedGiven || options.CapturePath != nullptr ||
                           options.ShowGraph || options.ShowFoodOverlay ||
                           options.LightHour.has_value();
  const bool hashRefuses = options.PrintHash && needsWindow;
  const bool timesUnbounded = options.FrameTimes && options.FrameLimit <= 0;
  return !hashRefuses && !timesUnbounded;
}

} // namespace

std::optional<Options> parseOptions(int argc, const char *const *argv) {
  Options options;
  bool framesGiven = false;
  bool speedGiven = false;
  bool valid = true;
  for (int i = 1; i < argc && valid; ++i) {
    const bool hasValue = i + 1 < argc;
    if (strcmp(argv[i], "--frames") == 0 && hasValue) {
      options.FrameLimit = static_cast<int>(strtol(argv[++i], nullptr, 10));
      framesGiven = true;
    } else if (strcmp(argv[i], "--capture") == 0 && hasValue) {
      options.CapturePath = argv[++i];
    } else if (strcmp(argv[i], "--park") == 0 && hasValue) {
      options.ParkPath = argv[++i];
    } else if (strcmp(argv[i], "--ticks") == 0 && hasValue) {
      valid = parseCount(argv[++i], options.Ticks);
    } else if (strcmp(argv[i], "--speed") == 0 && hasValue) {
      uint64_t speed = 0;
      valid =
          parseCount(argv[++i], speed) && (speed == 0 || speed == 1 || speed == 2 || speed == 4);
      options.Speed = static_cast<uint32_t>(speed);
      speedGiven = true;
    } else if (strcmp(argv[i], "--hash") == 0) {
      options.PrintHash = true;
    } else if (strcmp(argv[i], "--graph") == 0) {
      options.ShowGraph = true;
    } else if (strcmp(argv[i], "--overlay") == 0 && hasValue) {
      // food is the only overlay.
      valid = strcmp(argv[++i], "food") == 0;
      options.ShowFoodOverlay = valid;
    } else if (strcmp(argv[i], "--light-hour") == 0 && hasValue) {
      uint64_t hour = 0;
      valid = parseCount(argv[++i], hour) && hour < 24;
      options.LightHour = hour;
    } else if (strcmp(argv[i], "--frame-times") == 0) {
      options.FrameTimes = true;
    } else {
      valid = false;
    }
  }
  if (!valid || !consistent(options, framesGiven, speedGiven)) {
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--speed N] [--hash] [--frames N] [--capture "
            "PATH] [--graph] "
            "[--overlay food] [--light-hour H] [--frame-times]",
            argv[0]);
    return std::nullopt;
  }
  if (options.CapturePath != nullptr && options.FrameLimit <= 0) {
    options.FrameLimit = 3;
  }
  return options;
}

} // namespace tpj
