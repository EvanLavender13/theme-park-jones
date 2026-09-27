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
  for (auto &&[id, storage] : world.Registry.storage()) {
    if (!storage.empty() && world.Schema->findComponent(storage.info().hash()) == nullptr) {
      throw WorldInvariantError("component type " + std::string(storage.info().name()) +
                                " is not registered with the world's schema");
    }
  }
  for (auto &&[entity] : world.Registry.storage<entt::entity>()->each()) {
    if (!world.KeyByEntity.contains(entity)) {
      throw WorldInvariantError("an entity was created outside World::createEntity");
    }
  }
  for (const auto &[key, record] : world.ByKey) {
    if (!world.Registry.valid(record.Entity)) {
      throw WorldInvariantError("entity " + keyText(key) +
                                " was destroyed outside World::destroyEntity");
    }
  }
  for (const ComponentType &type : world.Schema->components()) {
    for (const auto &[key, record] : world.ByKey) {
      if (!type.Has(world.Registry, record.Entity)) {
        continue;
      }
      NanFinder finder;
      type.Emit(world.Registry, record.Entity, finder);
      if (finder.Field) {
        throw WorldInvariantError("NaN in field '" + *finder.Field + "' of component '" +
                                  type.Name + "' on entity " + keyText(key));
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
  for (const ComponentType &type : Schema->components()) {
    sink.word(hashName(type.Name));
    sink.word(static_cast<uint64_t>(type.Kind));
    uint64_t count = 0;
    for (const auto &entry : ByKey) {
      if (type.Has(Registry, entry.second.Entity)) {
        ++count;
      }
    }
    sink.word(count);
    for (const auto &[key, record] : ByKey) {
      if (type.Has(Registry, record.Entity)) {
        sink.word(static_cast<uint64_t>(key));
        type.Emit(Registry, record.Entity, sink);
      }
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
  for (const auto &[key, record] : world.ByKey) {
    copy.addEntity(key);
    copy.ByKey.at(key).Origin = record.Origin;
  }
  for (const ComponentType &type : world.Schema->components()) {
    for (const auto &[key, record] : world.ByKey) {
      if (type.Has(world.Registry, record.Entity)) {
        type.Copy(world.Registry, record.Entity, copy.Registry, copy.ByKey.at(key).Entity);
      }
    }
  }
  return copy;
}

} // namespace tpj
