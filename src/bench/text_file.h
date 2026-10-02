#ifndef TPJ_BENCH_TEXT_FILE_H
#define TPJ_BENCH_TEXT_FILE_H

#include <optional>
#include <string>

namespace tpj {

// The file's whole text, read in binary mode, or none when it cannot be read.
std::optional<std::string> readTextFile(const std::string &path);

} // namespace tpj

#endif
