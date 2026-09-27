# Implementation Plan: Text Saves

## Goal

Give tpj_sim saveWorld and loadWorld: a world's intent and state as canonical text, read back strictly with errors by line number, through each registered type's visitFields.

## Approach

A new header, sim/field_text.h, holds the field text machinery: a writer and a reader visitor that mirror emitField's field types, a bounds-checked line cursor, number formatting through std::to_chars and std::from_chars, enum names, and LoadError. IsVector and UNSUPPORTED_FIELD move there from schema.h, which includes it. Each ComponentType gains Write and Read functions that addComponent instantiates, so saveWorld and loadWorld in sim/save.cpp handle types opaquely, as the walk does. createDerivedEntity gives a loaded derived-key entity the origin it is resolved from.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Apply the three changes in FEATURE.md's "Spec changes" section exactly. Insert the createDerivedEntity sentences after "never meets the counter's range." Append the registration sentence to the end of the paragraph that begins "Component types are registered with a WorldSchema". Insert the three save paragraphs after the paragraph that begins "drawUniform is the draw's top 53 bits" and before the paragraph that begins "Stepping is deterministic".

### Task 2: Add the field text header and move the shared field traits

Files:
- Create: `src/sim/field_text.h`
- Modify: `src/sim/schema.h:60-64`
- Modify: `src/sim/schema.cpp:1-16`

Step 1: Create `src/sim/field_text.h`. The writer and reader come in Tasks 7 and 9.

```cpp
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

} // namespace tpj

#endif
```

Step 2: In `src/sim/schema.h`, add `#include "sim/field_text.h"` after `#include "sim/entity_key.h"`, and delete the definitions of `IsVector` (both lines of the primary template and the partial specialization) and `UNSUPPORTED_FIELD`, which now come from field_text.h.

Step 3: In `src/sim/schema.cpp`, delete the anonymous namespace that holds the local `isValidName`, so the calls in `addComponentType` and `addResolver` use the one in field_text.h.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 62 existing tests pass.

### Task 3: Declare the component text functions

Files:
- Modify: `src/sim/schema.h` (the `ComponentType` struct and `WorldSchema::addComponent`)

Step 1: In `ComponentType`, after the `Emit` member, add:

```cpp
  // Writes the component's fields for its save line, each as a space and name=value.
  void (*Write)(const entt::registry &registry, entt::entity entity, std::string &line) = nullptr;
  // Reads what Write wrote and adds the component to the entity. Throws LoadError.
  void (*Read)(TextCursor &cursor, entt::registry &registry, entt::entity entity) = nullptr;
```

Step 2: In `WorldSchema::addComponent`, before `addComponentType(std::move(type));`, add the stubs:

```cpp
  type.Write = [](const entt::registry &, entt::entity, std::string &) {};
  type.Read = [](TextCursor &, entt::registry &, entt::entity) {};
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 62 existing tests pass.

### Task 4: Declare saves

Files:
- Create: `src/sim/save.h`
- Create: `src/sim/save.cpp`
- Modify: `src/sim/world.h`
- Modify: `src/sim/CMakeLists.txt:3-8`

Step 1: Create `src/sim/save.h`:

```cpp
#ifndef TPJ_SIM_SAVE_H
#define TPJ_SIM_SAVE_H

#include "sim/field_text.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <string>
#include <string_view>

namespace tpj {

// The world's intent and state as canonical text: equal worlds give identical saves. Throws
// std::invalid_argument for an enum value with no name, and in debug builds WorldInvariantError for
// a world the walk cannot cover.
std::string saveWorld(const World &world);

// A world read from saveWorld's text, with resolution pending. Throws LoadError, naming the line,
// for text that is not a save in the form src/sim/SPEC.md defines.
World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text);

} // namespace tpj

#endif
```

Step 2: Create `src/sim/save.cpp` with stubs:

```cpp
#include "sim/save.h"

#include <utility>

namespace tpj {

std::string saveWorld(const World & /*world*/) { return {}; }

World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view /*text*/) {
  return World(std::move(schema), 0);
}

} // namespace tpj
```

Step 3: In `src/sim/world.h`, add `#include <string>` and `#include <string_view>` to the system includes, keeping them sorted. After the line `friend World makeCandidate(const World &world, const CommandQueue &commands);`, add:

```cpp
  friend std::string saveWorld(const World &world);
  friend World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text);
```

Step 4: In `src/sim/CMakeLists.txt`, add `save.cpp` to the tpj_sim sources after `draw.cpp`:

```cmake
add_library(tpj_sim STATIC
    cycle.cpp
    draw.cpp
    save.cpp
    schema.cpp
    walk.cpp
    world.cpp)
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 62 existing tests pass.

### Task 5: Test pass

Dispatch the test-writer agent through implementing-features' test pass template. Feature: this directory's FEATURE.md. Specs: `src/sim/SPEC.md`. Public headers: `src/sim/save.h`, `src/sim/field_text.h`, `src/sim/schema.h`, `src/sim/world.h`, `src/sim/entity_key.h`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 62 existing tests pass. The new tests of round trips, the save's form, canonical saves, derived data, malformed lines, line endings, save refusals, and the reserved name fail against the stubs. Tests that any stub meets, such as a loaded world having resolution pending, may pass.

### Task 6: Registration rules for saves

Files:
- Modify: `src/sim/schema.h` (`WorldSchema::addComponent`)
- Modify: `src/sim/schema.cpp` (`WorldSchema::addComponentType`)

Step 1: In `WorldSchema::addComponent`, after the existing static_asserts, add:

```cpp
  static_assert(std::is_default_constructible_v<T>,
                "a registered component must be default constructible, so that a load can fill it");
```

Step 2: In `WorldSchema::addComponentType`, after the isValidName check, add:

```cpp
  if (type.Name == ENTITIES_SECTION) {
    throw std::invalid_argument("component name 'entities' is reserved for saves");
  }
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the test of the reserved name passes.

### Task 7: The field writer

Files:
- Modify: `src/sim/field_text.h`
- Modify: `src/sim/schema.h` (`WorldSchema::addComponent`)

Step 1: In `src/sim/field_text.h`, after the `TextCursor` class, add:

```cpp
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
  } else if constexpr (std::is_same_v<Field, double>) {
    writeNumber(out, value);
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
  } else if constexpr (std::is_integral_v<Field>) {
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
```

Step 2: In `WorldSchema::addComponent` in `src/sim/schema.h`, replace the Write stub with:

```cpp
  type.Write = [](const entt::registry &registry, entt::entity entity, std::string &line) {
    if constexpr (!std::is_empty_v<T>) {
      // As in Emit, writing only reads, and the stored component is not itself const.
      FieldWriter writer(line, true);
      visitFields(writer, const_cast<T &>(registry.get<T>(entity)));
    }
  };
```

Run: `cmake --build --preset linux-debug`
Expected: the build succeeds with no warnings. Every registered enum field type now needs enumNames, which the test pass gave tests/synthetic_types.h's Mood; if the build instead stops at the static_assert naming enumNames, that is a deviation.

### Task 8: Saving

Files:
- Modify: `src/sim/save.cpp`

Step 1: Replace the contents of `src/sim/save.cpp` above the loadWorld stub with the includes, helpers, and saveWorld below, keeping the loadWorld stub:

```cpp
#include "sim/save.h"

#include "core/profile.h"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {

namespace {

constexpr std::string_view SAVE_HEADER = "tpj-park 1";

void appendSectionHeader(std::string &text, std::string_view name) {
  text += "\n[";
  text += name;
  text += "]\n";
}

} // namespace

std::string saveWorld(const World &world) {
  TPJ_PROFILE_ZONE();
  if constexpr (WORLD_CHECKS) {
    validateWorld(world);
  }
  const std::vector<ComponentType> &types = world.schema().components();
  std::string text(SAVE_HEADER);
  text += "\nseed ";
  writeNumber(text, world.Seed);
  text += "\ntick ";
  writeNumber(text, world.Tick);
  text += "\nnext-key ";
  writeNumber(text, world.NextKey);
  text += '\n';

  const auto holdsSaved = [&world, &types](entt::entity entity) {
    return std::any_of(types.begin(), types.end(), [&world, entity](const ComponentType &type) {
      return type.Kind != DataKind::Derived && type.Has(world.Registry, entity);
    });
  };
  bool headed = false;
  for (const auto &[key, record] : world.ByKey) {
    if (isDerivedKey(key) || holdsSaved(record.Entity)) {
      continue;
    }
    if (!headed) {
      appendSectionHeader(text, ENTITIES_SECTION);
      headed = true;
    }
    writeNumber(text, static_cast<uint64_t>(key));
    text += '\n';
  }

  for (const ComponentType &type : types) {
    if (type.Kind == DataKind::Derived) {
      continue;
    }
    headed = false;
    for (const auto &[key, record] : world.ByKey) {
      if (!type.Has(world.Registry, record.Entity)) {
        continue;
      }
      if (!headed) {
        appendSectionHeader(text, type.Name);
        headed = true;
      }
      writeNumber(text, static_cast<uint64_t>(key));
      type.Write(world.Registry, record.Entity, text);
      text += '\n';
    }
  }
  return text;
}
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests of the save's form, canonical saves, derived data, and the save refusals pass, as far as they do not load a save. Tests that load a save still fail.

### Task 9: The field reader

Files:
- Modify: `src/sim/field_text.h`
- Modify: `src/sim/schema.h` (`WorldSchema::addComponent`)

Step 1: In `src/sim/field_text.h`, after the `writeValue` definition, add:

```cpp
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
```

Every pass through the vector loop consumes at least one character or throws, so it ends on any input.

Step 2: In `WorldSchema::addComponent` in `src/sim/schema.h`, replace the Read stub with:

```cpp
  type.Read = [](TextCursor &cursor, entt::registry &registry, entt::entity entity) {
    if constexpr (std::is_empty_v<T>) {
      registry.emplace<T>(entity);
    } else {
      T value{};
      FieldReader reader(cursor, true);
      visitFields(reader, value);
      registry.emplace<T>(entity, std::move(value));
    }
  };
```

Run: `cmake --build --preset linux-debug`
Expected: the build succeeds with no warnings.

### Task 10: Loading

Files:
- Modify: `src/sim/save.cpp`

Step 1: In the anonymous namespace of `src/sim/save.cpp`, after `appendSectionHeader`, add:

```cpp
// A non-blank line of a save, numbered from 1.
struct SaveLine {
  size_t Number = 0;
  std::string_view Text;
};

// Hands out a save's non-blank lines in order, and names the line after the last when they run
// out. A carriage return that ends a line is dropped.
class LineReader {
public:
  explicit LineReader(std::string_view text) {
    size_t start = 0;
    size_t number = 0;
    while (start < text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string_view::npos) {
        end = text.size();
      }
      ++number;
      std::string_view line = text.substr(start, end - start);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      if (!line.empty()) {
        Lines.push_back(SaveLine{number, line});
      }
      start = end + 1;
    }
    EndNumber = number + 1;
  }

  [[nodiscard]] bool atEnd() const { return Next == Lines.size(); }
  [[nodiscard]] size_t endNumber() const { return EndNumber; }
  const SaveLine &next(std::string_view expected) {
    if (atEnd()) {
      throw LoadError(EndNumber, "the save ends before " + std::string(expected));
    }
    return Lines[Next++];
  }

private:
  std::vector<SaveLine> Lines;
  size_t EndNumber = 1;
  size_t Next = 0;
};

// Reads a header line of the form "label value".
uint64_t readHeaderNumber(const SaveLine &line, std::string_view label) {
  TextCursor cursor(line.Text, line.Number);
  cursor.expect(label);
  cursor.expect(' ');
  const uint64_t value = readNumber<uint64_t>(cursor, label);
  if (!cursor.atEnd()) {
    cursor.fail("unexpected text after " + std::string(label));
  }
  return value;
}

// Where the sections read so far allow the next one to be.
struct SectionOrder {
  bool EntitiesAllowed = true;
  size_t NextType = 0;
};

// Reads a section header line. Returns the component type it names, or nullptr for [entities], and
// throws unless the section may follow the ones before it.
const ComponentType *readSectionHeader(const SaveLine &line, const WorldSchema &schema,
                                       SectionOrder &order) {
  if (line.Text.size() < 3 || line.Text.back() != ']') {
    throw LoadError(line.Number, "malformed section header");
  }
  const std::string_view name = line.Text.substr(1, line.Text.size() - 2);
  const std::string section = "[" + std::string(name) + "]";
  if (name == ENTITIES_SECTION) {
    if (!order.EntitiesAllowed) {
      throw LoadError(line.Number, section + " must come first, and only once");
    }
    order.EntitiesAllowed = false;
    return nullptr;
  }
  order.EntitiesAllowed = false;
  const std::vector<ComponentType> &types = schema.components();
  const auto found = std::find_if(types.begin(), types.end(),
                                  [name](const ComponentType &type) { return type.Name == name; });
  if (found == types.end()) {
    throw LoadError(line.Number, "unknown section " + section);
  }
  if (found->Kind == DataKind::Derived) {
    throw LoadError(line.Number, section + " is a derived type, which saves never hold");
  }
  const auto index = static_cast<size_t>(found - types.begin());
  if (index < order.NextType) {
    throw LoadError(line.Number, section + " is repeated or out of order");
  }
  order.NextType = index + 1;
  return &*found;
}

// Reads a line's key, which must be above the previous key in its section and, for a counter key,
// below next-key.
EntityKey readKey(TextCursor &cursor, EntityKey previous, uint64_t nextKey) {
  const EntityKey key{readNumber<uint64_t>(cursor, "key")};
  if (key == NULL_KEY) {
    cursor.fail("key 0 is not an entity");
  }
  if (key <= previous) {
    cursor.fail("keys must ascend within a section");
  }
  if (!isDerivedKey(key) && static_cast<uint64_t>(key) >= nextKey) {
    cursor.fail("counter key " + std::to_string(static_cast<uint64_t>(key)) +
                " is not below next-key");
  }
  return key;
}
```

Step 2: Replace the loadWorld stub with:

```cpp
World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text) {
  TPJ_PROFILE_ZONE();
  LineReader reader(text);
  const SaveLine &header = reader.next("its header");
  if (header.Text != SAVE_HEADER) {
    throw LoadError(header.Number, "expected the header 'tpj-park 1'");
  }
  const uint64_t seed = readHeaderNumber(reader.next("seed"), "seed");
  World world(std::move(schema), seed);
  world.Tick = readHeaderNumber(reader.next("tick"), "tick");
  const SaveLine &nextKeyLine = reader.next("next-key");
  world.NextKey = readHeaderNumber(nextKeyLine, "next-key");
  if (world.NextKey == 0 || world.NextKey > DERIVED_KEY_BIT) {
    throw LoadError(nextKeyLine.Number, "next-key must be from 1 to 2^63");
  }

  SectionOrder order;
  bool inSection = false;
  size_t sectionLines = 0;
  const ComponentType *type = nullptr;
  EntityKey previous = NULL_KEY;
  // The keys [entities] lists, ascending, since its keys ascend.
  std::vector<EntityKey> bareKeys;
  // saveWorld never writes a section with no key lines. One is found at the next section header,
  // or at the end of the text.
  const auto requireKeyLines = [&inSection, &sectionLines](size_t number) {
    if (inSection && sectionLines == 0) {
      throw LoadError(number, "the section before holds no entities");
    }
  };
  while (!reader.atEnd()) {
    const SaveLine &line = reader.next("a line");
    if (line.Text.front() == '[') {
      requireKeyLines(line.Number);
      type = readSectionHeader(line, world.schema(), order);
      inSection = true;
      sectionLines = 0;
      previous = NULL_KEY;
      continue;
    }
    if (!inSection) {
      throw LoadError(line.Number, "expected a section header");
    }
    ++sectionLines;
    TextCursor cursor(line.Text, line.Number);
    const EntityKey key = readKey(cursor, previous, world.NextKey);
    previous = key;
    if (type == nullptr) {
      if (isDerivedKey(key)) {
        cursor.fail("[entities] lists only counter keys");
      }
      bareKeys.push_back(key);
    } else if (std::binary_search(bareKeys.begin(), bareKeys.end(), key)) {
      cursor.fail("key " + std::to_string(static_cast<uint64_t>(key)) +
                  " is listed in [entities], which holds entities with nothing saved");
    }
    entt::entity entity = world.findEntity(key);
    if (entity == entt::null) {
      entity = world.addEntity(key);
    }
    if (type != nullptr) {
      type->Read(cursor, world.Registry, entity);
    }
    if (!cursor.atEnd()) {
      cursor.fail("unexpected text after the line's last field");
    }
  }
  requireKeyLines(reader.endNumber());
  return world;
}
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests of malformed lines, line endings, and loaded headers pass. Round-trip tests whose worlds hold derived-key entities with state may still fail in debug builds, where resolving such a loaded entity reports two origins, until Task 11.

### Task 11: Loaded derived entities take their origin

Files:
- Modify: `src/sim/world.cpp` (`World::createDerivedEntity`)

Step 1: Replace the `else if` branch of `World::createDerivedEntity` with:

```cpp
  } else if (!found->second.Origin) {
    // A loaded entity takes the origin it is resolved from.
    found->second.Origin = origin;
  } else if (WORLD_CHECKS && *found->second.Origin != origin) {
    throw WorldInvariantError("derived key " + std::to_string(static_cast<uint64_t>(key)) +
                              " is shared by two origins");
  }
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: every test passes.

### Task 12: Verify on both builds

Step 1: Format the changed sources first, so the pre-commit hook changes nothing and the pre-push build finds nothing to rebuild.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds and every test passes.

### Task 13: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Sim: Add canonical text saves and strict loading`.
