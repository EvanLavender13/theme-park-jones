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
// for text that saveWorld could not have written for the schema.
World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text);

} // namespace tpj

#endif
