#include "sim/park_schema.h"

#include "sim/medium/network.h"

namespace tpj {

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them as they land, in dependency order.
  addNetworkComponent(*schema);
  return schema;
}

} // namespace tpj
