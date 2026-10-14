#include "sim/schema.h"

#include <algorithm>
#include <stdexcept>

namespace tpj {

void WorldSchema::addComponentType(ComponentType type) {
  if (!isValidName(type.Name)) {
    throw std::invalid_argument("component name '" + type.Name +
                                "' must be lowercase letters, digits, and hyphens");
  }
  if (type.Name == ENTITIES_SECTION) {
    throw std::invalid_argument("component name 'entities' is reserved for saves");
  }
  for (const ComponentType &existing : Components) {
    if (existing.Name == type.Name) {
      throw std::invalid_argument("component name '" + type.Name + "' is already registered");
    }
    if (existing.Id == type.Id) {
      throw std::invalid_argument("the type named '" + type.Name + "' is already registered as '" +
                                  existing.Name + "'");
    }
  }
  IndexById.emplace(type.Id, Components.size());
  Components.push_back(std::move(type));
}

std::optional<size_t> WorldSchema::indexOf(TypeId id) const {
  const auto found = IndexById.find(id);
  if (found == IndexById.end()) {
    return std::nullopt;
  }
  return found->second;
}

bool WorldSchema::sameComponents(const WorldSchema &other) const {
  return std::equal(
      Components.begin(), Components.end(), other.Components.begin(), other.Components.end(),
      [](const ComponentType &left, const ComponentType &right) {
        return left.Name == right.Name && left.Kind == right.Kind && left.Id == right.Id;
      });
}

void WorldSchema::addSystem(StepFunction step) { Systems.push_back(step); }

void WorldSchema::addSwap(StepFunction swap) { Swaps.push_back(swap); }

void WorldSchema::addFinisher(ResolveFunction finish) { Finishers.push_back(finish); }

void WorldSchema::addResolver(std::string_view name, ResolveFunction resolve,
                              std::vector<std::string> dependencies) {
  ResolverType resolver{std::string(name), resolve, std::move(dependencies)};
  const auto isRegistered = [this](std::string_view resolverName) {
    return std::any_of(Resolvers.begin(), Resolvers.end(),
                       [resolverName](const ResolverType &r) { return r.Name == resolverName; });
  };
  if (!isValidName(resolver.Name)) {
    throw std::invalid_argument("resolver name '" + resolver.Name +
                                "' must be lowercase letters, digits, and hyphens");
  }
  if (isRegistered(resolver.Name)) {
    throw std::invalid_argument("resolver name '" + resolver.Name + "' is already registered");
  }
  for (const std::string &dependency : resolver.Dependencies) {
    if (!isRegistered(dependency)) {
      throw std::invalid_argument("resolver '" + resolver.Name + "' depends on '" + dependency +
                                  "', which is not an already registered resolver");
    }
  }
  Resolvers.push_back(std::move(resolver));
}

void WorldSchema::addCommandType(CommandType type) {
  if (findCommand(type.Id) != nullptr) {
    throw std::invalid_argument("a command type is already registered");
  }
  Commands.push_back(type);
}

const CommandType *WorldSchema::findCommand(TypeId id) const {
  const auto found = std::find_if(Commands.begin(), Commands.end(),
                                  [id](const CommandType &type) { return type.Id == id; });
  return found == Commands.end() ? nullptr : &*found;
}

} // namespace tpj
