#include "bench/report/internal/lines.h"

#include "bench/report/report_error.h"
#include "bench/timing.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace tpj {

namespace {

std::string fieldName(size_t index) { return "field " + std::to_string(index + 1); }

bool isLowerHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

} // namespace

std::vector<TextLine> readLines(std::string_view text) {
  std::vector<TextLine> lines;
  size_t number = 0;
  while (!text.empty()) {
    ++number;
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      continue;
    }
    TextLine read{number, {}};
    size_t start = 0;
    while (true) {
      const size_t space = line.find(' ', start);
      read.Fields.push_back(
          line.substr(start, space == std::string_view::npos ? space : space - start));
      if (space == std::string_view::npos) {
        break;
      }
      start = space + 1;
    }
    if (std::ranges::any_of(read.Fields, [](std::string_view field) { return field.empty(); })) {
      refuseLine(read, "fields are separated by single spaces");
    }
    lines.push_back(std::move(read));
  }
  return lines;
}

void refuseLine(const TextLine &line, const std::string &problem) {
  throw ReportError("line " + std::to_string(line.Number) + ": " + problem);
}

void expectWord(const TextLine &line, size_t index, std::string_view word) {
  if (index >= line.Fields.size() || line.Fields[index] != word) {
    refuseLine(line, "expected '" + std::string(word) + "' as " + fieldName(index));
  }
}

uint64_t readCount(const TextLine &line, size_t index) {
  if (index < line.Fields.size()) {
    const std::string_view field = line.Fields[index];
    uint64_t value = 0;
    const char *end = field.data() + field.size();
    const auto result = std::from_chars(field.data(), end, value);
    if (result.ec == std::errc() && result.ptr == end) {
      return value;
    }
  }
  refuseLine(line, "expected an unsigned decimal count as " + fieldName(index));
}

int64_t readTime(const TextLine &line, size_t index) {
  const uint64_t value = readCount(line, index);
  if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    refuseLine(line, "the time in " + fieldName(index) + " is too large");
  }
  return static_cast<int64_t>(value);
}

uint64_t readHash(const TextLine &line, size_t index) {
  if (index < line.Fields.size()) {
    const std::string_view field = line.Fields[index];
    if (field.size() == 16 && std::ranges::all_of(field, isLowerHexDigit)) {
      uint64_t value = 0;
      const auto result = std::from_chars(field.data(), field.data() + field.size(), value, 16);
      if (result.ec == std::errc()) {
        return value;
      }
    }
  }
  refuseLine(line, "expected 16 lowercase hexadecimal digits as " + fieldName(index));
}

StageResult readStageLine(const TextLine &line) {
  if (line.Fields.size() != 12) {
    refuseLine(line, "a stage line has 12 fields");
  }
  expectWord(line, 0, "stage");
  expectWord(line, 2, "count");
  expectWord(line, 4, "median");
  expectWord(line, 6, "least");
  expectWord(line, 8, "greatest");
  StageResult stage;
  stage.Name = std::string(line.Fields[1]);
  stage.Times = TimeSummary{static_cast<size_t>(readCount(line, 3)), readTime(line, 5),
                            readTime(line, 7), readTime(line, 9)};
  if (line.Fields[10] == "hash") {
    stage.Kind = ResultKind::Hash;
    stage.Result = readHash(line, 11);
  } else if (line.Fields[10] == "vertices") {
    stage.Kind = ResultKind::Vertices;
    stage.Result = readCount(line, 11);
  } else {
    refuseLine(line, "expected 'hash' or 'vertices' as " + fieldName(10));
  }
  return stage;
}

} // namespace tpj
