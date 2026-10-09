#include "sim/world.h"

#include "sim/mix.h"

#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

namespace {

// Records the first NaN among the doubles it is shown.
class NanFinder final : public WordSink {
public:
  void word(uint64_t) override {}
  void real(std::string_view field, double value) override {
    if (!Field && std::isnan(value)) {
      Field = std::string(field);
    }
  }

  std::optional<std::string> Field;
};

class WordList final : public WordSink {
public:
  void word(uint64_t value) override { Words.push_back(value); }
  void real(std::string_view, double) override {}

  std::vector<uint64_t> Words;
};

class WordHash final : public WordSink {
public:
  void word(uint64_t value) override { Hash.add(value); }
  void real(std::string_view, double) override {}

  Hasher Hash;
};

std::string keyText(EntityKey key) { return std::to_string(static_cast<uint64_t>(key)); }

} // namespace

void validateWorld(const World &world) {
  const std::vector<ComponentType> &types = world.Schema->components();
  for (size_t index = 0; index < types.size(); ++index) {
    const ComponentStore &store = *world.Stores[index];
    for (const EntityKey key : store.keys()) {
      NanFinder finder;
      store.emit(key, finder);
      if (finder.Field) {
        throw WorldInvariantError("NaN in field '" + *finder.Field + "' of component '" +
                                  types[index].Name + "' on entity " + keyText(key));
      }
    }
  }
}

void World::emitWords(WordSink &sink) const {
  sink.word(Tick);
  sink.word(Seed);
  sink.word(NextKey);
  sink.word(ResolvePending ? 1U : 0U);
  sink.word(ByKey.size());
  for (const auto &[key, record] : ByKey) {
    sink.word(static_cast<uint64_t>(key));
    if (record.Origin) {
      sink.word(1);
      sink.word(static_cast<uint64_t>(record.Origin->Owner));
      sink.word(record.Origin->Purpose);
      sink.word(record.Origin->Index);
    } else {
      sink.word(0);
    }
  }
  const std::vector<ComponentType> &types = Schema->components();
  for (size_t index = 0; index < types.size(); ++index) {
    sink.word(hashName(types[index].Name));
    sink.word(static_cast<uint64_t>(types[index].Kind));
    const ComponentStore &store = *Stores[index];
    sink.word(store.size());
    for (const EntityKey key : store.keys()) {
      sink.word(static_cast<uint64_t>(key));
      store.emit(key, sink);
    }
  }
}

bool worldsEqual(const World &left, const World &right) {
  if constexpr (WORLD_CHECKS) {
    validateWorld(left);
    validateWorld(right);
  }
  if (!left.Schema->sameComponents(*right.Schema)) {
    return false;
  }
  WordList leftWords;
  WordList rightWords;
  left.emitWords(leftWords);
  right.emitWords(rightWords);
  return leftWords.Words == rightWords.Words;
}

uint64_t hashWorld(const World &world) {
  if constexpr (WORLD_CHECKS) {
    validateWorld(world);
  }
  WordHash hash;
  world.emitWords(hash);
  return hash.Hash.value();
}

World copyWorld(const World &world) {
  if constexpr (WORLD_CHECKS) {
    validateWorld(world);
  }
  World copy(world.Schema, world.Seed);
  copy.Tick = world.Tick;
  copy.NextKey = world.NextKey;
  copy.ResolvePending = world.ResolvePending;
  copy.ByKey = world.ByKey;
  for (size_t index = 0; index < world.Stores.size(); ++index) {
    copy.Stores[index] = world.Stores[index]->clone();
  }
  return copy;
}

} // namespace tpj
