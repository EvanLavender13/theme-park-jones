#include "sim/park_schema.h"

#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"

namespace tpj {

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them, in dependency order, starting with park intent and its commands.
  addNetworkComponent(*schema);
  addParkIntent(*schema);
  addParkEdits(*schema);
  return schema;
}

} // namespace tpj
