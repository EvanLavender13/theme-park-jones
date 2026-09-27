#include "sim/schema.h"

#include <algorithm>
#include <stdexcept>

namespace tpj {

namespace {

bool isValidName(std::string_view name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
  });
}

} // namespace

void WorldSchema::addComponentType(ComponentType type) {
  if (!isValidName(type.Name)) {
    throw std::invalid_argument("component name '" + type.Name +
                                "' must be lowercase letters, digits, and hyphens");
  }
  for (const ComponentType &existing : Components) {
    if (existing.Name == type.Name) {
      throw std::invalid_argument("component name '" + type.Name + "' is already registered");
    }
    if (existing.TypeId == type.TypeId) {
      throw std::invalid_argument("the type named '" + type.Name + "' is already registered as '" +
                                  existing.Name + "'");
    }
  }
  Components.push_back(std::move(type));
}

const ComponentType *WorldSchema::findComponent(entt::id_type typeId) const {
  const auto found =
      std::find_if(Components.begin(), Components.end(),
                   [typeId](const ComponentType &type) { return type.TypeId == typeId; });
  return found == Components.end() ? nullptr : &*found;
}

bool WorldSchema::sameComponents(const WorldSchema &other) const {
  return std::equal(
      Components.begin(), Components.end(), other.Components.begin(), other.Components.end(),
      [](const ComponentType &left, const ComponentType &right) {
        return left.Name == right.Name && left.Kind == right.Kind && left.TypeId == right.TypeId;
      });
}

} // namespace tpj
