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

// The values of a save's four header lines.
struct SaveHeader {
  uint64_t Seed = 0;
  uint64_t Tick = 0;
  uint64_t NextKey = 1;
};

SaveHeader readHeader(LineReader &reader) {
  const SaveLine &header = reader.next("its header");
  if (header.Text != SAVE_HEADER) {
    throw LoadError(header.Number, "expected the header 'tpj-park 1'");
  }
  SaveHeader values;
  values.Seed = readHeaderNumber(reader.next("seed"), "seed");
  values.Tick = readHeaderNumber(reader.next("tick"), "tick");
  const SaveLine &nextKeyLine = reader.next("next-key");
  values.NextKey = readHeaderNumber(nextKeyLine, "next-key");
  if (values.NextKey == 0 || values.NextKey > DERIVED_KEY_BIT) {
    throw LoadError(nextKeyLine.Number, "next-key must be from 1 to 2^63");
  }
  return values;
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

World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text) {
  TPJ_PROFILE_ZONE();
  LineReader reader(text);
  const SaveHeader header = readHeader(reader);
  World world(std::move(schema), header.Seed);
  world.Tick = header.Tick;
  world.NextKey = header.NextKey;

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

} // namespace tpj
