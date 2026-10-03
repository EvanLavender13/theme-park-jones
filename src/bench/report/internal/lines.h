#ifndef TPJ_BENCH_REPORT_INTERNAL_LINES_H
#define TPJ_BENCH_REPORT_INTERNAL_LINES_H

#include "bench/stages.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// A line that is not empty once a carriage return at its end is removed, split at single spaces,
// numbered counting every line of its text from 1. Its fields view the text it was read from.
struct TextLine {
  size_t Number = 0;
  std::vector<std::string_view> Fields;
};

// The lines of text separated by line feeds, without the empty ones. Throws ReportError for a line
// with an empty field.
std::vector<TextLine> readLines(std::string_view text);

// Throws ReportError with the message `line <n>: <problem>`.
[[noreturn]] void refuseLine(const TextLine &line, const std::string &problem);

// Refuses the line unless the field at index is word.
void expectWord(const TextLine &line, size_t index, std::string_view word);

// The field at index as an unsigned decimal count, or refuses the line.
uint64_t readCount(const TextLine &line, size_t index);

// The field at index as a duration in nanoseconds that fits int64_t, or refuses the line.
int64_t readTime(const TextLine &line, size_t index);

// The field at index as 16 lowercase hexadecimal digits, or refuses the line.
uint64_t readHash(const TextLine &line, size_t index);

// The first 12 fields of a line as stageLine writes them, or refuses the line.
StageResult readStageLine(const TextLine &line);

} // namespace tpj

#endif
