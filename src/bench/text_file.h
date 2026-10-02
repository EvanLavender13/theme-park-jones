#ifndef TPJ_BENCH_TEXT_FILE_H
#define TPJ_BENCH_TEXT_FILE_H

#include <optional>
#include <string>
#include <string_view>

namespace tpj {

// The file's whole text, read in binary mode, or none when it cannot be read.
std::optional<std::string> readTextFile(const std::string &path);
// Writes the text to the file in binary mode, replacing it. False when it cannot be written.
bool writeTextFile(const std::string &path, std::string_view text);

} // namespace tpj

#endif
