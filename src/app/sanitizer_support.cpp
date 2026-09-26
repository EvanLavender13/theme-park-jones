// Compiled only into sanitized builds of the app (TPJ_SANITIZE), which are Linux-only.
//
// The X11 client libraries and the Vulkan loader and drivers that SDL loads keep allocations
// alive until exit. Leaks whose stack passes through SDL's entry points for them are
// suppressed, so the app exits cleanly while project code keeps full leak checking. Driver
// threads leak from inside the driver alone, so the Vulkan loader is told not to unload
// drivers; otherwise their frames cannot be symbolized and matched by the suppressions.

#include <stdlib.h>

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
extern "C" const char *__lsan_default_suppressions() {
  return "leak:X11_VideoInit\n"
         "leak:SDL_CreateGPUDevice\n"
         "leak:libvulkan\n";
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

namespace {

// Runs before main, and so before SDL loads the Vulkan loader.
const int KEEP_VULKAN_DRIVERS_LOADED =
    setenv("VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING", "1", 0); // NOLINT(concurrency-mt-unsafe)

} // namespace
