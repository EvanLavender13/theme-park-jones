#include "sim/park_schema.h"

namespace tpj {

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  // Capabilities register their park types here as they land, in dependency order.
  return schema;
}

} // namespace tpj
