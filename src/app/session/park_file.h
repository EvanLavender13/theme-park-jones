#ifndef TPJ_APP_SESSION_PARK_FILE_H
#define TPJ_APP_SESSION_PARK_FILE_H

#include "sim/world.h"

#include <optional>
#include <string>
#include <string_view>

namespace tpj {

// A park file's world, or why there is none.
struct OpenedPark {
  // Loaded with makeParkSchema and resolved.
  std::optional<World> Park;
  // Empty when Park holds a world.
  std::string Error;
};

// Reads the park file at the path, loads it, and resolves it. On failure, no world and a message
// naming the path: why it cannot be read, or the LoadError's message.
OpenedPark openParkFile(const char *path);
// Writes the world's save to the path, replacing any file there. Returns an empty message, or why
// it could not.
std::string saveParkFile(const World &world, const char *path);
// The path, followed by .park when its file name has no extension.
std::string withParkExtension(std::string_view path);
// True when a file or directory exists at the path.
bool fileExists(const char *path);

} // namespace tpj

#endif
