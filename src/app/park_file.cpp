#include "app/park_file.h"

#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <SDL3/SDL.h>

namespace tpj {

OpenedPark openParkFile(const char *path) {
  size_t size = 0;
  void *text = SDL_LoadFile(path, &size);
  if (text == nullptr) {
    return {std::nullopt, std::string("Cannot read ") + path + ": " + SDL_GetError()};
  }
  OpenedPark opened;
  try {
    opened.Park =
        loadWorld(makeParkSchema(), std::string_view(static_cast<const char *>(text), size));
  } catch (const LoadError &error) {
    opened.Error = std::string("Cannot load ") + path + ": " + error.what();
  }
  SDL_free(text);
  if (opened.Park) {
    resolveWorld(*opened.Park);
  }
  return opened;
}

std::string saveParkFile(const World &world, const char *path) {
  const std::string text = saveWorld(world);
  if (!SDL_SaveFile(path, text.data(), text.size())) {
    return std::string("Cannot save ") + path + ": " + SDL_GetError();
  }
  return {};
}

std::string withParkExtension(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  std::string result(path);
  if (name.find('.') == std::string_view::npos) {
    result += ".park";
  }
  return result;
}

} // namespace tpj
