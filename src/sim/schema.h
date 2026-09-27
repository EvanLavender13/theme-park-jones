#ifndef TPJ_SIM_SCHEMA_H
#define TPJ_SIM_SCHEMA_H

#include "sim/entity_key.h"

#include <entt/entity/registry.hpp>

#include <bit>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {

// What a registered component is to the save (principle 1): intent the player authored, state the
// simulation changes, or data derived from those, which is never saved.
enum class DataKind { Intent, State, Derived };

// Receives a component's fields as 64-bit words, in the order its visitFields lists them.
class WordSink {
public:
  WordSink() = default;
  WordSink(const WordSink &) = delete;
  WordSink &operator=(const WordSink &) = delete;
  WordSink(WordSink &&) = delete;
  WordSink &operator=(WordSink &&) = delete;
  virtual ~WordSink() = default;

  virtual void word(uint64_t value) = 0;
  // Called with each double field, just before its bits are passed to word.
  virtual void real(std::string_view field, double value) = 0;
};

template <typename Field> void emitField(WordSink &sink, std::string_view name, Field &value);

// The visitor the walk passes to a component's visitFields.
class FieldEmitter {
public:
  explicit FieldEmitter(WordSink &sink) : Sink(sink) {}
  template <typename Field> void field(std::string_view name, Field &value) {
    emitField(Sink, name, value);
  }

private:
  WordSink &Sink;
};

// A type lists its fields with a function found by argument-dependent lookup,
//   template <typename Visitor> void visitFields(Visitor &visitor, Shop &shop);
// calling visitor.field("stock", shop.Stock) once per field, in a fixed order.
template <typename T>
concept HasVisitFields = requires(FieldEmitter &emitter, T &value) { visitFields(emitter, value); };

template <typename T> struct IsVector : std::false_type {};
template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename> inline constexpr bool UNSUPPORTED_FIELD = false;

// A registered component type, handled opaquely through functions its owning module instantiates
// (principle 6).
struct ComponentType {
  std::string Name;
  DataKind Kind = DataKind::State;
  entt::id_type TypeId = 0;
  bool (*Has)(const entt::registry &registry, entt::entity entity) = nullptr;
  void (*Copy)(const entt::registry &from, entt::entity source, entt::registry &to,
               entt::entity target) = nullptr;
  void (*Emit)(const entt::registry &registry, entt::entity entity, WordSink &sink) = nullptr;
};

// The component types a world may hold, in registration order. Built once by explicit calls in a
// written order, never by static self-registration, then shared by worlds as a const value.
class WorldSchema {
public:
  // Registers T. The name is lowercase letters, digits, and hyphens. Throws std::invalid_argument
  // for a malformed or repeated name, or for a type already registered.
  template <typename T> void addComponent(std::string_view name, DataKind kind);

  [[nodiscard]] const std::vector<ComponentType> &components() const { return Components; }
  [[nodiscard]] const ComponentType *findComponent(entt::id_type typeId) const;
  // True when both list the same names, kinds, and types in the same order.
  [[nodiscard]] bool sameComponents(const WorldSchema &other) const;

private:
  void addComponentType(ComponentType type);

  std::vector<ComponentType> Components;
};

// Field types: bool, integers, double, EntityKey, enums, std::vector of a field type (not bool),
// and structs with their own visitFields. Integers are widened to 64 bits, doubles pass their bits.
template <typename Field> void emitField(WordSink &sink, std::string_view name, Field &value) {
  if constexpr (std::is_same_v<Field, bool>) {
    sink.word(value ? 1U : 0U);
  } else if constexpr (std::is_same_v<Field, double>) {
    sink.real(name, value);
    sink.word(std::bit_cast<uint64_t>(value));
  } else if constexpr (std::is_enum_v<Field>) {
    // Includes EntityKey, whose underlying type is uint64_t.
    auto underlying = static_cast<std::underlying_type_t<Field>>(value);
    emitField(sink, name, underlying);
  } else if constexpr (std::is_integral_v<Field> && std::is_signed_v<Field>) {
    sink.word(static_cast<uint64_t>(static_cast<int64_t>(value)));
  } else if constexpr (std::is_integral_v<Field>) {
    sink.word(static_cast<uint64_t>(value));
  } else if constexpr (IsVector<Field>::value) {
    sink.word(value.size());
    // A std::vector<bool> element is a proxy, which falls through to the static_assert below.
    for (auto &&element : value) {
      emitField(sink, name, element);
    }
  } else if constexpr (HasVisitFields<Field>) {
    FieldEmitter emitter(sink);
    visitFields(emitter, value);
  } else {
    static_assert(UNSUPPORTED_FIELD<Field>,
                  "unsupported field type; see emitField in sim/schema.h");
  }
}

template <typename T> void WorldSchema::addComponent(std::string_view name, DataKind kind) {
  static_assert(std::is_copy_constructible_v<T>, "a registered component must be copyable");
  static_assert(std::is_empty_v<T> || HasVisitFields<T>,
                "a registered component needs a visitFields function");
  ComponentType type;
  type.Name = std::string(name);
  type.Kind = kind;
  type.TypeId = entt::type_id<T>().hash();
  type.Has = [](const entt::registry &registry, entt::entity entity) {
    return registry.all_of<T>(entity);
  };
  type.Copy = [](const entt::registry &from, entt::entity source, entt::registry &to,
                 entt::entity target) {
    if constexpr (std::is_empty_v<T>) {
      // EnTT stores no instance of an empty type, so there is nothing to copy.
      to.emplace<T>(target);
    } else {
      to.emplace<T>(target, from.get<T>(source));
    }
  };
  type.Emit = [](const entt::registry &registry, entt::entity entity, WordSink &sink) {
    if constexpr (!std::is_empty_v<T>) {
      // visitFields takes a mutable reference so that one function can also decode. Emitting only
      // reads, and the stored component is not itself const.
      FieldEmitter emitter(sink);
      visitFields(emitter, const_cast<T &>(registry.get<T>(entity)));
    }
  };
  addComponentType(std::move(type));
}

} // namespace tpj

#endif
