#include "bench/park_file.h"

#include <array>
#include <fstream>
#include <stddef.h>

namespace tpj {

std::optional<std::string> readParkFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  // Reading through the file's own stream lets its state record a read that fails after the
  // open, such as of a directory, which a copy of its buffer into another stream would not.
  std::string text;
  std::array<char, 65536> buffer{};
  while (file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) ||
         file.gcount() > 0) {
    text.append(buffer.data(), static_cast<size_t>(file.gcount()));
  }
  if (file.bad() || !file.eof()) {
    return std::nullopt;
  }
  return text;
}

} // namespace tpj
