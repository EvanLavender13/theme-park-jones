#ifndef TPJ_APP_OPTIONS_H
#define TPJ_APP_OPTIONS_H

#include <optional>
#include <stdint.h>

namespace tpj {

// What the command line asks of the app. The paths point into the command line's arguments.
struct Options {
  // The frames to run before exiting, or 0 to run until the player quits.
  int FrameLimit = 0;
  // Where to write the last frame as a BMP, or null.
  const char *CapturePath = nullptr;
  // The park file to start from, or null for a new park.
  const char *ParkPath = nullptr;
  // The cycles to step before the first frame.
  uint64_t Ticks = 0;
  // Write the hash after the ticks and exit without a window.
  bool PrintHash = false;
  // Check the Debug panel's Graph checkbox at start.
  bool ShowGraph = false;
  // Check the Debug panel's Food overlay checkbox at start.
  bool ShowFoodOverlay = false;
  // Write each frame's cost after the last frame.
  bool FrameTimes = false;
};

// Reads the command line, argv[0] being the program's name. --park PATH starts from a park file,
// --ticks N steps it N ticks before the first frame, and --hash prints the state hash after them
// and exits. --frames N exits after N frames. --graph starts with the graph view on, and --overlay
// food with the food overlay on. --frame-times writes each frame's cost after the last frame.
// --capture PATH writes the last frame to PATH as a BMP and implies a limit of 3 frames when
// --frames gives none that is positive. None, after logging the usage, for an unknown option, an
// option missing its value, a --ticks value that is not a decimal count, an --overlay value other
// than food, --frame-times without a positive --frames, or --hash with --frames, --capture,
// --graph, or --overlay.
std::optional<Options> parseOptions(int argc, const char *const *argv);

} // namespace tpj

#endif
