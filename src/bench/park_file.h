#ifndef TPJ_BENCH_PARK_FILE_H
#define TPJ_BENCH_PARK_FILE_H

#include <optional>
#include <string>

namespace tpj {

// The file's whole text, read in binary mode, or none when it cannot be read.
std::optional<std::string> readParkFile(const std::string &path);

} // namespace tpj

#endif
