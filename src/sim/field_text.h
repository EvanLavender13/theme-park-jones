#ifndef TPJ_SIM_FIELD_TEXT_H
#define TPJ_SIM_FIELD_TEXT_H

#include "sim/entity_key.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace tpj {

template <typename T> struct IsVector : std::false_type {};
template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename> inline constexpr bool UNSUPPORTED_FIELD = false;

// Lowercase letters, digits, and hyphens, and not empty: the rule for component, resolver, and
// enum value names.
constexpr bool isValidName(std::string_view name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
  });
}

// The save section that lists entities holding nothing saved. No component type may take its name.
constexpr std::string_view ENTITIES_SECTION = "entities";

// A save loadWorld cannot read, with the number of the line where the problem was found.
class LoadError : public std::runtime_error {
public:
  LoadError(size_t line, const std::string &message)
      : std::runtime_error("line " + std::to_string(line) + ": " + message), Line(line) {}

  [[nodiscard]] size_t line() const { return Line; }

private:
  size_t Line;
};

// Reads one line of a save. Every read checks its bounds, and every failure throws LoadError
// naming the line.
class TextCursor {
public:
  TextCursor(std::string_view text, size_t line) : Text(text), Line(line) {}

  [[nodiscard]] bool atEnd() const { return Position == Text.size(); }
  [[nodiscard]] bool peek(char ch) const { return Position < Text.size() && Text[Position] == ch; }
  // Consumes ch, or throws.
  void expect(char ch) {
    if (!peek(ch)) {
      fail(std::string("expected '") + ch + "'");
    }
    ++Position;
  }
  // Consumes word, or throws.
  void expect(std::string_view word) {
    if (Text.substr(Position, word.size()) != word) {
      fail("expected '" + std::string(word) + "'");
    }
    Position += word.size();
  }
  // Consumes and returns the characters up to the next space, bracket, or brace, or the end of the
  // line. Throws if there are none.
  std::string_view token() {
    const size_t start = Position;
    while (Position < Text.size() && !isDelimiter(Text[Position])) {
      ++Position;
    }
    if (Position == start) {
      fail("expected a value");
    }
    return Text.substr(start, Position - start);
  }
  [[noreturn]] void fail(const std::string &message) const { throw LoadError(Line, message); }

private:
  static bool isDelimiter(char ch) {
    return ch == ' ' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
  }

  std::string_view Text;
  size_t Position = 0;
  size_t Line;
};

// Appends a number as std::to_chars writes it: decimal for integers, and for doubles the shortest
// form that reads back to the same bits.
template <typename Number> void writeNumber(std::string &out, Number value) {
  std::array<char, 32> buffer{};
  const std::to_chars_result result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  out.append(buffer.data(), result.ptr);
}

// An enum type names its values with a constexpr function found by argument-dependent lookup,
//   constexpr std::array<std::string_view, 2> enumNames(Mood) { return {"calm", "excited"}; }
// returning the names of the values 0, 1, and so on.
template <typename T>
concept HasEnumNames = std::is_enum_v<T> && requires(T value) { enumNames(value); };

// True when the names are distinct and each follows isValidName.
template <typename Names> constexpr bool areValidEnumNames(const Names &names) {
  for (size_t i = 0; i < names.size(); ++i) {
    if (!isValidName(names[i])) {
      return false;
    }
    for (size_t j = 0; j < i; ++j) {
      if (names[j] == names[i]) {
        return false;
      }
    }
  }
  return true;
}

template <typename Field> void writeValue(std::string &out, std::string_view name, Field &value);

// The visitor saveWorld passes to visitFields. Writes each field as name=value, separated by single
// spaces, with a space before the first as well when leadingSpace is set.
class FieldWriter {
public:
  FieldWriter(std::string &out, bool leadingSpace) : Out(out), NeedSpace(leadingSpace) {}
  template <typename Field> void field(std::string_view name, Field &value) {
    if (NeedSpace) {
      Out += ' ';
    }
    NeedSpace = true;
    Out += name;
    Out += '=';
    writeValue(Out, name, value);
  }

private:
  std::string &Out;
  bool NeedSpace;
};

template <typename T>
concept HasTextFields = requires(FieldWriter &writer, T &value) { visitFields(writer, value); };

// The field types emitField takes, written as the save format in src/sim/SPEC.md spells them.
template <typename Field> void writeValue(std::string &out, std::string_view name, Field &value) {
  if constexpr (std::is_same_v<Field, bool>) {
    out += value ? "true" : "false";
  } else if constexpr (std::is_same_v<Field, EntityKey>) {
    writeNumber(out, static_cast<uint64_t>(value));
  } else if constexpr (std::is_enum_v<Field>) {
    static_assert(HasEnumNames<Field>,
                  "an enum field needs an enumNames function; see sim/field_text.h");
    static_assert(areValidEnumNames(enumNames(Field{})),
                  "enum names must be distinct, and lowercase letters, digits, and hyphens");
    constexpr auto names = enumNames(Field{});
    // A negative value becomes a large one, which has no name either.
    const auto index = static_cast<uint64_t>(
        static_cast<std::make_unsigned_t<std::underlying_type_t<Field>>>(value));
    if (index >= names.size()) {
      throw std::invalid_argument("enum field '" + std::string(name) + "' holds " +
                                  std::to_string(index) + ", which has no name");
    }
    out += names[index];
  } else if constexpr (std::is_same_v<Field, double> || std::is_integral_v<Field>) {
    writeNumber(out, value);
  } else if constexpr (IsVector<Field>::value) {
    out += '[';
    bool first = true;
    for (auto &&element : value) {
      if (!first) {
        out += ' ';
      }
      first = false;
      writeValue(out, name, element);
    }
    out += ']';
  } else if constexpr (HasTextFields<Field>) {
    out += '{';
    FieldWriter writer(out, false);
    visitFields(writer, value);
    out += '}';
  } else {
    static_assert(UNSUPPORTED_FIELD<Field>,
                  "unsupported field type; see emitField in sim/schema.h");
  }
}

// Reads a number that fills the whole token, as std::from_chars reads it.
template <typename Number> Number readNumber(TextCursor &cursor, std::string_view what) {
  const std::string_view token = cursor.token();
  Number number{};
  const std::from_chars_result result =
      std::from_chars(token.data(), token.data() + token.size(), number);
  if (result.ec == std::errc::result_out_of_range) {
    cursor.fail("'" + std::string(what) + "' is out of range");
  }
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
    cursor.fail("'" + std::string(what) + "' is not a number");
  }
  return number;
}

template <typename Field> void readValue(TextCursor &cursor, std::string_view name, Field &value);

// The visitor loadWorld passes to visitFields. Reads what FieldWriter writes.
class FieldReader {
public:
  FieldReader(TextCursor &cursor, bool leadingSpace) : Cursor(cursor), NeedSpace(leadingSpace) {}
  template <typename Field> void field(std::string_view name, Field &value) {
    if (NeedSpace) {
      Cursor.expect(' ');
    }
    NeedSpace = true;
    Cursor.expect(name);
    Cursor.expect('=');
    readValue(Cursor, name, value);
  }

private:
  TextCursor &Cursor;
  bool NeedSpace;
};

// Reads what writeValue writes, refusing anything that does not fit the field.
template <typename Field> void readValue(TextCursor &cursor, std::string_view name, Field &value) {
  if constexpr (std::is_same_v<Field, bool>) {
    const std::string_view token = cursor.token();
    if (token == "true") {
      value = true;
    } else if (token == "false") {
      value = false;
    } else {
      cursor.fail("'" + std::string(name) + "' must be true or false");
    }
  } else if constexpr (std::is_same_v<Field, double>) {
    value = readNumber<double>(cursor, name);
    if (std::isnan(value)) {
      cursor.fail("'" + std::string(name) + "' is NaN");
    }
  } else if constexpr (std::is_same_v<Field, EntityKey>) {
    value = EntityKey{readNumber<uint64_t>(cursor, name)};
  } else if constexpr (std::is_enum_v<Field>) {
    constexpr auto names = enumNames(Field{});
    const std::string_view token = cursor.token();
    for (size_t i = 0; i < names.size(); ++i) {
      if (names[i] == token) {
        value = static_cast<Field>(i);
        return;
      }
    }
    cursor.fail("'" + std::string(token) + "' is not a value of '" + std::string(name) + "'");
  } else if constexpr (std::is_integral_v<Field>) {
    value = readNumber<Field>(cursor, name);
  } else if constexpr (IsVector<Field>::value) {
    value.clear();
    cursor.expect('[');
    bool first = true;
    while (!cursor.peek(']')) {
      if (!first) {
        cursor.expect(' ');
      }
      first = false;
      readValue(cursor, name, value.emplace_back());
    }
    cursor.expect(']');
  } else if constexpr (HasTextFields<Field>) {
    cursor.expect('{');
    FieldReader reader(cursor, false);
    visitFields(reader, value);
    cursor.expect('}');
  } else {
    static_assert(UNSUPPORTED_FIELD<Field>,
                  "unsupported field type; see emitField in sim/schema.h");
  }
}

} // namespace tpj

#endif
